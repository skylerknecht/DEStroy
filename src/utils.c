#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include "utils.h"

#ifdef _WIN32
#include <windows.h>
#include <direct.h>
#include <io.h>
#define mkdir(path, mode) _mkdir(path)
#else
#include <sys/file.h>
#include <unistd.h>
#endif

// ============ Hex Conversion ============

int hex_to_bytes(const char *hex, uint8_t *bytes, size_t max_bytes) {
    size_t hex_len = strlen(hex);
    size_t byte_len = hex_len / 2;
    if (byte_len > max_bytes) byte_len = max_bytes;
    
    for (size_t i = 0; i < byte_len; i++) {
        unsigned int val;
        if (sscanf(hex + 2*i, "%02x", &val) != 1) return i;
        bytes[i] = (uint8_t)val;
    }
    return byte_len;
}

void bytes_to_hex(const uint8_t *bytes, size_t len, char *hex, size_t hex_size) {
    for (size_t i = 0; i < len && (2*i + 2) < hex_size; i++) {
        sprintf(hex + 2*i, "%02x", bytes[i]);
    }
}

// ============ Utility ============

static void ensure_dir(const char *dir) {
    mkdir(dir, 0755);
}

uint64_t get_plaintext_space(void) {
    uint64_t space = 1;
    for (int i = 0; i < PLAINTEXT_LEN_MAX; i++) space *= CHARSET_LEN;
    return space;
}

void get_table_id(const char *table_path, char *table_id, size_t size) {
    const char *filename = strrchr(table_path, '/');
    if (!filename) filename = strrchr(table_path, '\\');
    if (!filename) filename = table_path;
    else filename++;
    
    strncpy(table_id, filename, size - 1);
    table_id[size - 1] = '\0';
    
    char *dot = strrchr(table_id, '.');
    if (dot) *dot = '\0';
}

// ============ File Locking ============

static void lock_file(FILE *f) {
#ifdef _WIN32
    HANDLE h = (HANDLE)_get_osfhandle(_fileno(f));
    OVERLAPPED ov = {0};
    LockFileEx(h, LOCKFILE_EXCLUSIVE_LOCK, 0, MAXDWORD, MAXDWORD, &ov);
#else
    flock(fileno(f), LOCK_EX);
#endif
}

static void unlock_file(FILE *f) {
#ifdef _WIN32
    HANDLE h = (HANDLE)_get_osfhandle(_fileno(f));
    OVERLAPPED ov = {0};
    UnlockFileEx(h, 0, MAXDWORD, MAXDWORD, &ov);
#else
    flock(fileno(f), LOCK_UN);
#endif
}

// ============ Endpoints ============

int save_endpoints_to(const char *dir, const char *ct_hex, uint64_t *end_indices, uint32_t count) {
    ensure_dir(dir);
    
    char path[512];
    snprintf(path, sizeof(path), "%s/%s.endpoints", dir, ct_hex);
    
    FILE *f = fopen(path, "wb");
    if (!f) return -1;
    
    uint32_t chain_len = CHAIN_LEN;
    fwrite(&chain_len, sizeof(uint32_t), 1, f);
    fwrite(&count, sizeof(uint32_t), 1, f);
    fwrite(end_indices, sizeof(uint64_t), count, f);
    fclose(f);
    
    return 0;
}

int load_endpoints_from(const char *dir, const char *ct_hex, uint64_t *end_indices, uint32_t *count) {
    char path[512];
    snprintf(path, sizeof(path), "%s/%s.endpoints", dir, ct_hex);
    
    FILE *f = fopen(path, "rb");
    if (!f) return -1;
    
    uint32_t stored_chain_len;
    fread(&stored_chain_len, sizeof(uint32_t), 1, f);
    fread(count, sizeof(uint32_t), 1, f);
    
    if (stored_chain_len != CHAIN_LEN) {
        fclose(f);
        return -1;
    }
    
    size_t read = fread(end_indices, sizeof(uint64_t), *count, f);
    fclose(f);
    
    return (read == *count) ? 0 : -1;
}

// ============ Candidates ============

int append_candidates_to(const char *dir, const char *ct_hex,
                         uint64_t *start_indices, uint32_t *positions, uint32_t count) {
    if (count == 0) return 0;
    
    ensure_dir(dir);
    
    char path[512];
    snprintf(path, sizeof(path), "%s/%s.candidates", dir, ct_hex);
    
    FILE *f = fopen(path, "ab");
    if (!f) return -1;
    
    lock_file(f);
    
    // Write each candidate as [start:u64][pos:u32]
    for (uint32_t i = 0; i < count; i++) {
        fwrite(&start_indices[i], sizeof(uint64_t), 1, f);
        fwrite(&positions[i], sizeof(uint32_t), 1, f);
    }
    
    unlock_file(f);
    fclose(f);
    
    return 0;
}

int load_candidates_from(const char *dir, const char *ct_hex,
                         uint64_t *start_indices, uint32_t *positions,
                         uint32_t *total_count, uint32_t max_candidates) {
    char path[512];
    snprintf(path, sizeof(path), "%s/%s.candidates", dir, ct_hex);
    
    FILE *f = fopen(path, "rb");
    if (!f) return -1;
    
    // Get file size
    fseek(f, 0, SEEK_END);
    long file_size = ftell(f);
    fseek(f, 0, SEEK_SET);
    
    // 12 bytes per candidate
    uint32_t file_count = file_size / 12;
    if (file_count > max_candidates) {
        file_count = max_candidates;
    }
    
    // Read all candidates
    for (uint32_t i = 0; i < file_count; i++) {
        if (fread(&start_indices[i], sizeof(uint64_t), 1, f) != 1) break;
        if (fread(&positions[i], sizeof(uint32_t), 1, f) != 1) break;
    }
    
    fclose(f);
    
    *total_count = file_count;
    return 0;
}
