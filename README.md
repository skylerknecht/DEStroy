# DEStroy - NetNTLMv1 GPU Rainbow Table Recovery Tool

A GPU-accelerated tool for recovering DES keys from NetNTLMv1 authentication hashes using rainbow tables.

## Quick Start
```bash
# Linux
make
./gpu_lookup /path/to/tables/ 535549550D915078

# Windows (cross-compile from Linux)
make windows
# Copy gpu_lookup.exe and kernels/ folder to Windows
gpu_lookup.exe C:\tables 535549550D915078
```

## Overview

NetNTLMv1 uses DES encryption internally. This tool:
1. Takes an 8-byte ciphertext (one third of a NetNTLMv1 response)
2. Searches rainbow tables for the 7-byte DES key that produced it
3. Uses GPU acceleration for both precomputation and false alarm checking

---

## How NetNTLMv1 Works
```
User password → NTLM hash (16 bytes)

NTLM hash split into 3 parts:
  - Bytes 0-6   → DES key 1 (7 bytes)
  - Bytes 7-13  → DES key 2 (7 bytes)  
  - Bytes 14-15 + 5 null bytes → DES key 3 (7 bytes)

Each 7-byte key is expanded to 8 bytes (parity bits added)
Each DES key encrypts the SERVER CHALLENGE (8 bytes)

Result: 3 × 8-byte ciphertexts = 24-byte NetNTLMv1 response
```

**Real Example:**
```
Password:             Password123
NTLM hash (MD4):      58A478135A93AC3BF058A5EA0E8FDB71
Challenge:            1122334455667788

Split NTLM hash into DES keys:
  Key1: 58A478135A93AC     (bytes 0-6)
  Key2: 3BF058A5EA0E8F     (bytes 7-13)
  Key3: DB71000000000000   (bytes 14-15 + padding)

DES encrypt challenge with each key:
  DES(Key1, challenge) = CT1 = 535549550D915078
  DES(Key2, challenge) = CT2 = B4F2F4334C1992E0
  DES(Key3, challenge) = CT3 = 0B3693107DC5A855

NetNTLMv1 response:   535549550D915078B4F2F4334C1992E00B3693107DC5A855
```

**Responder format:**
```
user::domain:535549550D915078B4F2F4334C1992E00B3693107DC5A855:1122334455667788
```

To recover the NTLM hash, run this tool 3 times (once per ciphertext):
```bash
./gpu_lookup /tables/ 535549550D915078   # → Key1: 58A478135A93AC
./gpu_lookup /tables/ B4F2F4334C1992E0   # → Key2: 3BF058A5EA0E8F
./gpu_lookup /tables/ 0B3693107DC5A855   # → Key3: DB71 (+ padding)
```

Then concatenate: `58A478135A93AC` + `3BF058A5EA0E8F` + `DB71` = `58A478135A93AC3BF058A5EA0E8FDB71`

We recover each 8-byte ciphertext independently to get the 7-byte DES key portions, then reconstruct the NTLM hash.

---

## Hardcoded Challenge

> **IMPORTANT:** These rainbow tables only work with challenge `1122334455667788`.

This challenge is hardcoded in two kernel files:

**`kernels/precompute.cl`** and **`kernels/false_alarm.cl`**:
```c
// Challenge 1122334455667788 after DES initial permutation
uint X = 0xf0aaf0aa;
uint Y = 0x00cd00cd;
```

The DES algorithm applies an "initial permutation" to input data before encryption. The values above are what `1122334455667788` becomes after this permutation.

---

## How Rainbow Tables Work

### The Problem

- 7-byte keyspace = 256^7 = 72 quadrillion possible keys
- Storing all (key → ciphertext) pairs = 1.1 exabytes
- Impossible to store

### The Solution: Rainbow Chains

A rainbow table stores compressed chains:
```
Chain structure:

  start_index → key₀ → hash₀ → reduce₀ → 
                key₁ → hash₁ → reduce₁ → 
                ...
                key_n → hash_n → reduce_n → end_index

Only store: (start_index, end_index)
Chain length: 881,689 steps
```

- **Hash function:** DES encrypt challenge `1122334455667788` with the key
- **Reduce function:** Convert ciphertext back to a key index
```c
index = (ciphertext_as_number + reduction_offset + position) % keyspace
```

The `position` parameter makes each reduction different, creating a "rainbow" of functions.

### Table Parameters

Filename format:
```
netntlmv1_byte#7-7_0_881689x134217668_3574.rt
             │ │   │ │       │         │
             │ │   │ │       │         └─ Table index (identifier)
             │ │   │ │       └─ Number of chains (134M)
             │ │   │ └─ Chain length (881,689)
             │ │   └─ Character set (0 = all bytes 0x00-0xFF)
             │ └─ Key length (7 bytes)
             └─ Hash type
```

### Lookup Algorithm

**Step 1: Precompute End Indices (GPU)**

Given target ciphertext, compute where it would land at each chain position:
```
For position p in [0, chain_length-1]:
    index = reduce(target_ciphertext, p)
    for step in [p+1, chain_length-1]:
        key = index_to_key(index)
        ct = DES(key, challenge)
        index = reduce(ct, step)
    end_indices[p] = index
```

This generates 881,688 potential end indices. If target exists in a chain at position p, `end_indices[p]` will match that chain's stored end_index.

**Step 2: Binary Search Tables (CPU)**

For each precomputed end_index:
- Binary search the table's sorted end indices
- If match found, record (start_index, position) as candidate

**Step 3: Verify Candidates (GPU)**

For each candidate:
```
index = start_index
for step in [0, position]:
    key = index_to_key(index)
    ct = DES(key, challenge)
    if step == position and ct == target:
        FOUND! Return key
    index = reduce(ct, step)
```

Most matches are "false alarms" - the end indices matched by coincidence, not because target was actually in the chain.

---

## Build & Run

### Prerequisites

- OpenCL headers: included in `deps/CL/` (no SDK needed)
- OpenCL runtime: installed with GPU drivers

### Linux
```bash
# Build
make

# Install OpenCL runtime (pick one)
sudo apt install nvidia-opencl-icd    # NVIDIA
sudo apt install mesa-opencl-icd      # AMD
sudo apt install intel-opencl-icd     # Intel

# Verify OpenCL works
clinfo

# Run
./gpu_lookup /path/to/tables/ 535549550D915078
```

### Windows

Cross-compile from Linux:
```bash
# Install cross-compiler
sudo apt install mingw-w64

# Build
make windows
```

Copy these to Windows:
```
gpu_lookup.exe
kernels/
├── precompute.cl
└── false_alarm.cl
```

Run (GPU drivers include OpenCL runtime):
```cmd
gpu_lookup.exe C:\path\to\tables 535549550D915078
```

### Makefile Targets

| Target | Description |
|--------|-------------|
| `make` | Build for Linux |
| `make windows` | Cross-compile for Windows |
| `make clean` | Remove binaries |

---

## Usage
```bash
# Single table
./gpu_lookup table.rt 535549550D915078

# Directory of tables  
./gpu_lookup /path/to/tables/ 535549550D915078
```

The ciphertext is ONE of the three 8-byte blocks from a NetNTLMv1 response. Run separately for each block to recover the full NTLM hash.

### Example Output
```
+--------------------------------------------------------------+
|           NetNTLMv1 Rainbow Table Lookup (GPU)               |
+--------------------------------------------------------------+
| Started: 13:36:10                                            |
+--------------------------------------------------------------+

Target: 535549550D915078

[13:36:10] Scanning directory...
           Found 80 table(s)

[13:36:10] Initializing GPU...
           NVIDIA GeForce RTX 3080 Ti (80 CUs) - 0.1 sec

[13:36:10] Loading kernels...
           Done - 0.2 sec

[13:36:10] Precomputing end indices...
           Loaded from cache

[13:36:10] Collecting candidates from 80 tables...
           Collected 58.39 K candidates - 1 min 44 sec

[13:37:54] Checking candidates on GPU...
           KEY FOUND - 47.9 sec

==============================================================

+--------------------------------------------------------------+
|                      KEY FOUND!                              |
+--------------------------------------------------------------+
|  Ciphertext:   535549550D915078                              |
|  DES Key:      58a478135a93ac                                |
|  Candidates:   58390                                         |
|  Tables:       80                                            |
|  Total time:   2 min 32 sec                                  |
|  Finished:     13:38:42                                      |
+--------------------------------------------------------------+
```

---

## Performance

### Breakdown (80 tables × 2GB each)

| Step | Time | Bottleneck |
|------|------|------------|
| GPU init | 0.1s | - |
| Kernel load | 0.2s | - |
| Precompute end indices | cached | GPU (first run: ~2s) |
| Load tables + binary search | ~100s | NVMe read + search |
| GPU false alarm check | ~48s | GPU |
| **Total** | **~2.5 min** | |

### Hardware Requirements

| Component | Minimum | Notes |
|-----------|---------|-------|
| CPU | Any | Single-threaded, not the bottleneck |
| RAM | 4 GB | 2GB for table + buffers |
| GPU | GTX 1050+ | Any OpenCL GPU, VRAM doesn't matter |
| Storage | SATA SSD | NVMe preferred, HDD too slow |

**Why these specs?**

- **CPU doesn't matter:** Single-threaded and I/O bound
- **GPU VRAM doesn't matter:** Only ~1MB sent to GPU
- **Storage matters most:** 160GB of tables to read

---

## Project Structure
```
DEStroy/
├── Makefile
├── README.md
├── deps/
│   └── CL/
│       ├── cl.h
│       └── cl_platform.h
├── kernels/
│   ├── precompute.cl
│   └── false_alarm.cl
├── include/
│   ├── utils.h
│   ├── des.h
│   ├── netntlmv1.h
│   ├── rainbow.h
│   ├── table.h
│   ├── lookup.h
│   ├── opencl_dyn.h
│   └── opencl_host.h
├── src/
│   ├── main.c
│   ├── utils.c
│   ├── des.c
│   ├── netntlmv1.c
│   ├── rainbow.c
│   ├── table.c
│   ├── lookup.c
│   ├── opencl_dyn.c
│   └── opencl_host.c
└── cache/
```

---

## Optimizations

1. **GPU precomputation** - 880K parallel chain walks
2. **Precompute caching** - Same ciphertext reuses cached end indices
3. **Batch candidate collection** - All tables loaded, then one GPU call
4. **Direct table data access** - No memory copying after file read
5. **Dynamic OpenCL loading** - Works without OpenCL SDK