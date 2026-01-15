#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <dirent.h>
#include <sys/stat.h>

#ifdef _WIN32
#include <direct.h>
#include <windows.h>
#define mkdir(path, mode) _mkdir(path)
#else
#include <sys/time.h>
#endif

#include "utils.h"
#include "table.h"
#include "rainbow.h"
#include "netntlmv1.h"
#include "opencl_host.h"

#define CHARSET_LEN 256
#define PLAINTEXT_LEN_MAX 7
#define CHAIN_LEN 881689
#define REDUCTION_OFFSET 0
#define MAX_TABLES 4096
#define CACHE_DIR "cache"
#define MAX_CANDIDATES (4 * 1024 * 1024)

void print_usage(const char *prog) {
    printf("Usage: %s <table.rt | table_directory> <ciphertext_hex>\n", prog);
}

double get_time_sec(void) {
#ifdef _WIN32
    LARGE_INTEGER freq, count;
    QueryPerformanceFrequency(&freq);
    QueryPerformanceCounter(&count);
    return (double)count.QuadPart / (double)freq.QuadPart;
#else
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return tv.tv_sec + tv.tv_usec / 1000000.0;
#endif
}

void get_timestamp(char *buf, size_t size) {
    time_t now = time(NULL);
    struct tm *t = localtime(&now);
    strftime(buf, size, "%H:%M:%S", t);
}

void format_time(double seconds, char *buf, size_t buf_size) {
    if (seconds < 60) {
        snprintf(buf, buf_size, "%.1f sec", seconds);
    } else if (seconds < 3600) {
        int mins = (int)(seconds / 60);
        int secs = (int)seconds % 60;
        snprintf(buf, buf_size, "%d min %d sec", mins, secs);
    } else {
        int hours = (int)(seconds / 3600);
        int mins = ((int)seconds % 3600) / 60;
        snprintf(buf, buf_size, "%d hr %d min", hours, mins);
    }
}

void format_number(uint64_t n, char *buf, size_t buf_size) {
    if (n >= 1000000000) {
        snprintf(buf, buf_size, "%.2f B", n / 1000000000.0);
    } else if (n >= 1000000) {
        snprintf(buf, buf_size, "%.2f M", n / 1000000.0);
    } else if (n >= 1000) {
        snprintf(buf, buf_size, "%.2f K", n / 1000.0);
    } else {
        snprintf(buf, buf_size, "%lu", (unsigned long)n);
    }
}

int is_directory(const char *path) {
    struct stat st;
    if (stat(path, &st) != 0) return 0;
    return S_ISDIR(st.st_mode);
}

int is_rainbow_table(const char *filename) {
    size_t len = strlen(filename);
    if (len < 3) return 0;
    return (strcmp(filename + len - 3, ".rt") == 0) ||
           (len >= 4 && strcmp(filename + len - 4, ".rtc") == 0);
}

int find_tables(const char *dir_path, char **table_paths, int max_tables, int *count) {
    DIR *dir = opendir(dir_path);
    if (!dir) return -1;

    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL && *count < max_tables) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
            continue;

        size_t path_len = strlen(dir_path) + strlen(entry->d_name) + 2;
        char *full_path = malloc(path_len);
        if (!full_path) continue;

#ifdef _WIN32
        snprintf(full_path, path_len, "%s\\%s", dir_path, entry->d_name);
#else
        snprintf(full_path, path_len, "%s/%s", dir_path, entry->d_name);
#endif

        if (is_directory(full_path)) {
            find_tables(full_path, table_paths, max_tables, count);
            free(full_path);
        } else if (is_rainbow_table(entry->d_name)) {
            table_paths[*count] = full_path;
            (*count)++;
        } else {
            free(full_path);
        }
    }
    closedir(dir);
    return 0;
}

void get_cache_path(const char *ct_hex, char *cache_path, size_t size) {
#ifdef _WIN32
    snprintf(cache_path, size, "%s\\%s.bin", CACHE_DIR, ct_hex);
#else
    snprintf(cache_path, size, "%s/%s.bin", CACHE_DIR, ct_hex);
#endif
}

void ensure_cache_dir(void) {
    mkdir(CACHE_DIR, 0755);
}

int load_cache(const char *ct_hex, uint64_t *end_indices, uint32_t num_indices) {
    char cache_path[256];
    get_cache_path(ct_hex, cache_path, sizeof(cache_path));
    
    FILE *f = fopen(cache_path, "rb");
    if (!f) return -1;
    
    uint32_t stored_chain_len, stored_num_indices;
    if (fread(&stored_chain_len, sizeof(uint32_t), 1, f) != 1 ||
        fread(&stored_num_indices, sizeof(uint32_t), 1, f) != 1) {
        fclose(f);
        return -1;
    }
    
    if (stored_chain_len != CHAIN_LEN || stored_num_indices != num_indices) {
        fclose(f);
        return -1;
    }
    
    size_t read = fread(end_indices, sizeof(uint64_t), num_indices, f);
    fclose(f);
    return (read != num_indices) ? -1 : 0;
}

int save_cache(const char *ct_hex, uint64_t *end_indices, uint32_t num_indices) {
    ensure_cache_dir();
    char cache_path[256];
    get_cache_path(ct_hex, cache_path, sizeof(cache_path));
    
    FILE *f = fopen(cache_path, "wb");
    if (!f) return -1;
    
    uint32_t chain_len = CHAIN_LEN;
    fwrite(&chain_len, sizeof(uint32_t), 1, f);
    fwrite(&num_indices, sizeof(uint32_t), 1, f);
    fwrite(end_indices, sizeof(uint64_t), num_indices, f);
    fclose(f);
    return 0;
}

int collect_candidates(const char *table_path, 
                       uint64_t *end_indices, uint32_t num_indices,
                       uint64_t *start_indices, uint32_t *positions,
                       uint32_t *num_candidates, uint32_t max_candidates,
                       double *search_time) {
    
    rt_table table = {0};
    if (table_load(&table, table_path) != 0) return -1;

    double t0 = get_time_sec();
    
    uint32_t found = 0;
    for (uint32_t pos = 0; pos < num_indices && *num_candidates < max_candidates; pos++) {
        int search_found = 0;
        uint64_t start_index = table_search(&table, end_indices[pos], &search_found);
        if (search_found) {
            start_indices[*num_candidates] = start_index;
            positions[*num_candidates] = pos;
            (*num_candidates)++;
            found++;
        }
    }
    
    *search_time = get_time_sec() - t0;
    printf("      [search: %.2fs]\n", *search_time);
    
    table_free(&table);
    return found;
}

int main(int argc, char **argv) {
    if (argc < 3) {
        print_usage(argv[0]);
        return 1;
    }

    const char *path = argv[1];
    const char *ct_hex = argv[2];
    double total_start = get_time_sec();
    char time_buf[64], num_buf[64], ts[16];

    uint8_t ciphertext[8];
    if (hex_to_bytes(ct_hex, ciphertext, 8) != 8) {
        fprintf(stderr, "Error: Invalid ciphertext (need 16 hex chars)\n");
        return 1;
    }

    get_timestamp(ts, sizeof(ts));
    printf("\n");
    printf("+--------------------------------------------------------------+\n");
    printf("|           NetNTLMv1 Rainbow Table Lookup (GPU)               |\n");
    printf("+--------------------------------------------------------------+\n");
    printf("| Started: %-52s|\n", ts);
    printf("+--------------------------------------------------------------+\n\n");
    printf("Target: %s\n\n", ct_hex);

    char **table_paths = calloc(MAX_TABLES, sizeof(char *));
    int num_tables = 0;

    get_timestamp(ts, sizeof(ts));
    if (is_directory(path)) {
        printf("[%s] Scanning directory...\n", ts);
        if (find_tables(path, table_paths, MAX_TABLES, &num_tables) != 0 || num_tables == 0) {
            fprintf(stderr, "Error: No rainbow tables found\n");
            free(table_paths);
            return 1;
        }
        printf("         Found %d table(s)\n\n", num_tables);
    } else {
        printf("[%s] Single table mode\n", ts);
        table_paths[0] = strdup(path);
        num_tables = 1;
    }

    get_timestamp(ts, sizeof(ts));
    printf("[%s] Initializing GPU...\n", ts);
    double step_start = get_time_sec();

    gpu_context gpu = {0};
    if (gpu_init(&gpu) != 0) {
        fprintf(stderr, "Error: Failed to initialize GPU\n");
        for (int i = 0; i < num_tables; i++) free(table_paths[i]);
        free(table_paths);
        return 1;
    }
    format_time(get_time_sec() - step_start, time_buf, sizeof(time_buf));
    printf("         %s (%u CUs) - %s\n\n", gpu.device_name, gpu.compute_units, time_buf);

    get_timestamp(ts, sizeof(ts));
    printf("[%s] Loading kernels...\n", ts);
    step_start = get_time_sec();

    if (gpu_load_kernel(&gpu, "kernels/precompute.cl", "precompute") != 0 ||
        gpu_load_false_alarm_kernel(&gpu, "kernels/false_alarm.cl") != 0) {
        fprintf(stderr, "Error: Failed to load kernels\n");
        gpu_cleanup(&gpu);
        for (int i = 0; i < num_tables; i++) free(table_paths[i]);
        free(table_paths);
        return 1;
    }
    format_time(get_time_sec() - step_start, time_buf, sizeof(time_buf));
    printf("         Done - %s\n\n", time_buf);

    uint64_t plaintext_space_total = 1;
    for (int i = 0; i < PLAINTEXT_LEN_MAX; i++)
        plaintext_space_total *= CHARSET_LEN;

    uint32_t num_indices = CHAIN_LEN - 1;
    uint64_t *end_indices = malloc(num_indices * sizeof(uint64_t));
    if (!end_indices) {
        fprintf(stderr, "Error: Failed to allocate memory\n");
        gpu_cleanup(&gpu);
        for (int i = 0; i < num_tables; i++) free(table_paths[i]);
        free(table_paths);
        return 1;
    }

    get_timestamp(ts, sizeof(ts));
    printf("[%s] Precomputing end indices...\n", ts);

    if (load_cache(ct_hex, end_indices, num_indices) == 0) {
        printf("         Loaded from cache\n\n");
    } else {
        step_start = get_time_sec();
        int result = gpu_precompute(&gpu, ciphertext, CHAIN_LEN, REDUCTION_OFFSET,
                                     plaintext_space_total, end_indices);
        if (result < 0) {
            fprintf(stderr, "Error: GPU precomputation failed\n");
            free(end_indices);
            gpu_cleanup(&gpu);
            for (int i = 0; i < num_tables; i++) free(table_paths[i]);
            free(table_paths);
            return 1;
        }
        format_number(result, num_buf, sizeof(num_buf));
        format_time(get_time_sec() - step_start, time_buf, sizeof(time_buf));
        printf("         Computed %s indices - %s\n", num_buf, time_buf);
        save_cache(ct_hex, end_indices, num_indices);
        printf("\n");
    }

    uint64_t *start_indices = malloc(MAX_CANDIDATES * sizeof(uint64_t));
    uint32_t *positions = malloc(MAX_CANDIDATES * sizeof(uint32_t));
    if (!start_indices || !positions) {
        fprintf(stderr, "Error: Failed to allocate candidate buffers\n");
        free(end_indices);
        if (start_indices) free(start_indices);
        if (positions) free(positions);
        gpu_cleanup(&gpu);
        for (int i = 0; i < num_tables; i++) free(table_paths[i]);
        free(table_paths);
        return 1;
    }

    get_timestamp(ts, sizeof(ts));
    printf("[%s] Collecting candidates from %d tables...\n", ts, num_tables);
    step_start = get_time_sec();

    uint32_t total_candidates = 0;
    for (int t = 0; t < num_tables; t++) {
        double search_time;
        collect_candidates(table_paths[t], end_indices, num_indices,
                        start_indices, positions, &total_candidates, MAX_CANDIDATES,
                        &search_time);
        double elapsed = get_time_sec() - step_start;
        double rate = (t + 1) / elapsed;
        double eta = (num_tables - t - 1) / rate;
        char eta_buf[32];
        format_time(eta, eta_buf, sizeof(eta_buf));
        printf("\r         [%d/%d] %u candidates | ETA: %s        ", 
               t + 1, num_tables, total_candidates, eta_buf);
        fflush(stdout);
        
        if (total_candidates >= MAX_CANDIDATES) break;
    }

    format_time(get_time_sec() - step_start, time_buf, sizeof(time_buf));
    format_number(total_candidates, num_buf, sizeof(num_buf));
    printf("\r         Collected %s candidates - %s                    \n\n", num_buf, time_buf);

    int found = 0;
    char found_key[15] = {0};

    if (total_candidates > 0) {
        get_timestamp(ts, sizeof(ts));
        printf("[%s] Checking candidates on GPU...\n", ts);
        step_start = get_time_sec();

        uint8_t key_bytes[7] = {0};
        int result = gpu_check_false_alarms(&gpu, ciphertext, start_indices, positions,
                                             total_candidates, REDUCTION_OFFSET,
                                             plaintext_space_total, key_bytes);
        format_time(get_time_sec() - step_start, time_buf, sizeof(time_buf));

        if (result == 1) {
            found = 1;
            bytes_to_hex(key_bytes, 7, found_key, 15);
            printf("         KEY FOUND - %s\n", time_buf);
        } else {
            printf("         No match - %s\n", time_buf);
        }
    }

    double total_time = get_time_sec() - total_start;
    format_time(total_time, time_buf, sizeof(time_buf));
    get_timestamp(ts, sizeof(ts));

    printf("\n==============================================================\n\n");
    printf("+--------------------------------------------------------------+\n");
    if (found) {
        printf("|                      KEY FOUND!                              |\n");
    } else {
        printf("|                    KEY NOT FOUND                             |\n");
    }
    printf("+--------------------------------------------------------------+\n");
    printf("|  Ciphertext:   %-46s|\n", ct_hex);
    if (found) printf("|  DES Key:      %-46s|\n", found_key);
    printf("|  Candidates:   %-46u|\n", total_candidates);
    printf("|  Tables:       %-46d|\n", num_tables);
    printf("|  Total time:   %-46s|\n", time_buf);
    printf("|  Finished:     %-46s|\n", ts);
    printf("+--------------------------------------------------------------+\n");

    free(end_indices);
    free(start_indices);
    free(positions);
    gpu_cleanup(&gpu);
    for (int i = 0; i < num_tables; i++) free(table_paths[i]);
    free(table_paths);

    return found ? 0 : 1;
}