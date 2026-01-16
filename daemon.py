#!/usr/bin/env python3
"""
DEStroy Daemon
"""
import argparse
import os
import sys
import time
import queue
import threading
import subprocess
from datetime import datetime

PRECOMPUTE_BIN = "./precompute.exe" if sys.platform == "win32" else "./precompute"
LOOKUP_BIN = "./candidate_lookup.exe" if sys.platform == "win32" else "./candidate_lookup"
CHECK_BIN = "./candidate_check.exe" if sys.platform == "win32" else "./candidate_check"

gpu_queue = queue.Queue()
cpu_queue = queue.Queue()
lookups_started = set()

lookup_progress = {}
lookup_lock = threading.Lock()


def log(job_type: str, ct: str, msg: str):
    timestamp = datetime.now().strftime('%H:%M:%S')
    print(f"[{timestamp}] {job_type:<10} {ct[:8]:<8} {msg}", flush=True)


gpu_in_progress = set()

def gpu_worker(working_dir: str):
    while True:
        job = gpu_queue.get()
        if job is None:
            break
        job_type, cipher_text = job
        
        if job_type == "precompute":
            log("PRECOMPUTE", cipher_text, "Starting...")
            start = time.time()
            result = subprocess.run([PRECOMPUTE_BIN, cipher_text, working_dir], capture_output=True, text=True)
            elapsed = time.time() - start
            if result.returncode == 0:
                log("PRECOMPUTE", cipher_text, f"Done ({elapsed:.1f}s)")
            else:
                log("PRECOMPUTE", cipher_text, f"FAILED ({elapsed:.1f}s)")
        
        elif job_type == "candidate_check":
            log("CHECK", cipher_text, "Starting...")
            start = time.time()
            result = subprocess.run([CHECK_BIN, cipher_text, working_dir], capture_output=True, text=True)
            elapsed = time.time() - start
            if result.returncode == 0:
                result_path = os.path.join(working_dir, f"{cipher_text.upper()}.result")
                with open(result_path, 'r') as f:
                    key = f.read().strip()
                log("CHECK", cipher_text, f"Success: {key} ({elapsed:.1f}s)")
            else:
                log("CHECK", cipher_text, f"Failed ({elapsed:.1f}s)")        
        gpu_in_progress.discard(cipher_text)
        gpu_queue.task_done()


def start_gpu_worker(working_dir: str):
    t = threading.Thread(target=gpu_worker, args=(working_dir,), daemon=True)
    t.start()


def cpu_worker(working_dir: str):
    while True:
        job = cpu_queue.get()
        if job is None:
            break
        cipher_text, batch, batch_num, total_batches, total_tables = job
        
        cmd = [LOOKUP_BIN, cipher_text, working_dir] + batch
        result = subprocess.run(cmd, capture_output=True, text=True)
        
        lines = result.stdout.strip().split('\n') if result.stdout.strip() else []
        candidates = len(lines)
        
        with lookup_lock:
            if cipher_text not in lookup_progress:
                lookup_progress[cipher_text] = {"done": 0, "candidates": 0, "start": time.time()}
            lookup_progress[cipher_text]["done"] += 1
            lookup_progress[cipher_text]["candidates"] += candidates
            done = lookup_progress[cipher_text]["done"]
            total_candidates = lookup_progress[cipher_text]["candidates"]
            elapsed = time.time() - lookup_progress[cipher_text]["start"]
        
        tables_done = min((batch_num + 1) * len(batch), total_tables)
        log("LOOKUP", cipher_text, f"[{tables_done}/{total_tables}] {total_candidates} candidates ({elapsed:.1f}s)")
        
        if done == total_batches:
            log("LOOKUP", cipher_text, f"Complete - {total_candidates} candidates ({elapsed:.1f}s)")
        
        cpu_queue.task_done()


def start_cpu_workers(working_dir: str, num_workers: int):
    for _ in range(num_workers):
        t = threading.Thread(target=cpu_worker, args=(working_dir,), daemon=True)
        t.start()


def get_tables(tables_dir: str):
    tables = []
    for root, dirs, files in os.walk(tables_dir):
        for f in files:
            if f.endswith(".rt") or f.endswith(".rtc"):
                tables.append(os.path.join(root, f))
    return sorted(tables)


def get_unfinished_cipher_texts(working_dir: str):
    cipher_texts = []
    cipher_texts_finished = []
    for file in os.listdir(working_dir):
        if file.endswith(".result"):
            cipher_texts_finished.append(file.replace(".result", ""))
            continue
        elif not file.endswith(".ct"):
            continue
        cipher_text = file.split(".ct")[0]
        cipher_texts.append(cipher_text)
    return [ct for ct in cipher_texts if ct not in cipher_texts_finished]


def does_candidates_exist(working_dir: str, cipher_text: str):
    candidates_name = f'{cipher_text.upper()}.candidates'
    return os.path.exists(os.path.join(working_dir, candidates_name))


def does_endpoints_exist(working_dir: str, cipher_text: str):
    endpoints_name = f'{cipher_text.upper()}.endpoints'
    return os.path.exists(os.path.join(working_dir, endpoints_name))


def main(args, poll_rate: int = 5):
    working_dir = args.directory
    tables_dir = args.rainbow_tables
    num_workers = args.workers

    os.makedirs(working_dir, exist_ok=True)

    tables = get_tables(tables_dir)
    
    print("""
+--------------------------------------------------------------+
|              DEStroy Daemon - NetNTLMv1 Recovery             |
+--------------------------------------------------------------+
""")
    print(f"  Tables:   {len(tables)}")
    print(f"  Workers:  {num_workers} CPU + 1 GPU")
    print(f"  Watching: {working_dir}/")
    print()

    if len(tables) == 0:
        print(f"ERROR: No tables found in {tables_dir}")
        sys.exit(1)

    start_gpu_worker(working_dir)
    start_cpu_workers(working_dir, num_workers)

    batch_size = 10

    while True:
        try:
            unfinished_cipher_texts = get_unfinished_cipher_texts(working_dir)
            for ct in unfinished_cipher_texts:
                if does_candidates_exist(working_dir, ct):
                    if gpu_queue.empty() and ct not in gpu_in_progress:
                        gpu_in_progress.add(ct)
                        gpu_queue.put(("candidate_check", ct))
                elif not does_endpoints_exist(working_dir, ct):
                    if gpu_queue.empty() and ct not in gpu_in_progress:
                        gpu_in_progress.add(ct)
                        gpu_queue.put(("precompute", ct))

                # CPU work
                if does_endpoints_exist(working_dir, ct) and not does_candidates_exist(working_dir, ct):
                    if ct not in lookups_started:
                        lookups_started.add(ct)
                        lookup_progress[ct] = {"done": 0, "candidates": 0, "start": time.time()}
                        log("LOOKUP", ct, f"Starting ({len(tables)} tables)")
                        batches = [tables[i:i + batch_size] for i in range(0, len(tables), batch_size)]
                        for i, batch in enumerate(batches):
                            cpu_queue.put((ct, batch, i, len(batches), len(tables)))

            time.sleep(poll_rate)
        except KeyboardInterrupt:
            print("\nShutting down...")
            break


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description="DEStroy Daemon - NetNTLMv1 Recovery")
    parser.add_argument("-d", "--directory", default="working", help="Working directory")
    parser.add_argument("-rt", "--rainbow-tables", default="tables", help="Rainbow tables directory")
    parser.add_argument("-w", "--workers", type=int, default=2, help="CPU workers (1 per ciphertext)")
    args = parser.parse_args()
    main(args)