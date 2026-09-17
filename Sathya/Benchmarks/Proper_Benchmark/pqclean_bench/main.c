/*
 * Benchmark harness for PQClean ML-KEM-512 (Keygen Only) - Secure RNG
 */
#define _POSIX_C_SOURCE 199309L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <math.h>
#include <stdint.h>
#include <sys/random.h>
#include <errno.h>
#include "api.h"

#ifndef NUM_ITERS
#define NUM_ITERS 50000
#endif

#ifndef WARMUP_ITERS
#define WARMUP_ITERS 100
#endif

#ifndef LIB_NAME
#define LIB_NAME "PQClean"
#endif

#ifndef OUT_FILE
#define OUT_FILE "results_pqclean_time.csv"
#endif

#ifndef STACK_FILE
#define STACK_FILE "results_pqclean_stack.csv"
#endif

#ifndef POISON_REGION_BYTES
#define POISON_REGION_BYTES (16 * 1024)
#endif

#define POISON_BYTE 0xAA
#define GUARD_BYTES 256

static double now_us(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec * 1e6 + (double)ts.tv_nsec / 1e3;
}

static int cmp_double(const void *a, const void *b) {
    double da = *(const double *)a, db = *(const double *)b;
    return (da > db) - (da < db);
}

static int cmp_long(const void *a, const void *b) {
    long la = *(const long *)a, lb = *(const long *)b;
    return (la > lb) - (la < lb);
}

#if defined(__x86_64__) || defined(__i386__)
static inline uintptr_t get_sp(void) {
    uintptr_t sp;
    __asm__ __volatile__("mov %%rsp, %0" : "=r"(sp));
    return sp;
}
#elif defined(__aarch64__) || defined(__arm__)
static inline uintptr_t get_sp(void) {
    uintptr_t sp;
    __asm__ __volatile__("mov %0, sp" : "=r"(sp));
    return sp;
}
#else
static inline uintptr_t get_sp(void) {
    volatile int dummy;
    return (uintptr_t)&dummy;
}
#endif

static void poison_stack_region(volatile uint8_t *base, size_t len) {
    for (size_t i = 0; i < len; i++) {
        base[i] = POISON_BYTE;
    }
}

static size_t scan_stack_depth(volatile uint8_t *base, size_t len) {
    size_t i;
    for (i = 0; i < len; i++) {
        if (base[i] != POISON_BYTE) {
            return len - i;
        }
    }
    return 0;
}

static size_t benchmark_keypair(uint8_t *pk, uint8_t *sk, int *call_result, double *elapsed_us) {
    uintptr_t sp = get_sp();
    volatile uint8_t *poison_top = (volatile uint8_t *)(sp - GUARD_BYTES);
    volatile uint8_t *poison_base = poison_top - POISON_REGION_BYTES;
    poison_stack_region(poison_base, POISON_REGION_BYTES);
    __asm__ __volatile__("" ::: "memory");
    double t0 = now_us();
    *call_result = PQCLEAN_MLKEM512_CLEAN_crypto_kem_keypair(pk, sk);
    double t1 = now_us();
    *elapsed_us = t1 - t0;
    __asm__ __volatile__("" ::: "memory");
    return scan_stack_depth(poison_base, POISON_REGION_BYTES);
}

int PQCLEAN_randombytes(uint8_t *out, size_t outlen) {
#ifdef __APPLE__
    size_t total_read = 0;
    while (total_read < outlen) {
        size_t chunk = outlen - total_read;
        if (chunk > 256) chunk = 256;
        if (getentropy(out + total_read, chunk) != 0) {
            if (errno == EINTR) continue;
            return -1;
        }
        total_read += chunk;
    }
    return 0;
#else
    size_t total_read = 0;
    while (total_read < outlen) {
        ssize_t res = getrandom(out + total_read, outlen - total_read, 0);
        if (res < 0) {
            if (errno == EINTR) continue;
            return -1;
        }
        total_read += res;
    }
    return 0;
#endif
}

static void report_time(const char *label, double *samples, int n, FILE *csv) {
    double sum = 0.0, min = samples[0], max = samples[0];
    int i;
    double *sorted = malloc((size_t)n * sizeof(double));
    if (!sorted) {
        fprintf(stderr, "malloc failed in report_time()\n");
        return;
    }
    memcpy(sorted, samples, (size_t)n * sizeof(double));
    qsort(sorted, (size_t)n, sizeof(double), cmp_double);
    for (i = 0; i < n; i++) {
        sum += samples[i];
        if (samples[i] < min) min = samples[i];
        if (samples[i] > max) max = samples[i];
    }
    double mean = sum / n;
    double sq = 0.0;
    for (i = 0; i < n; i++) sq += (samples[i] - mean) * (samples[i] - mean);
    double stddev = sqrt(sq / n);
    double median = sorted[n / 2];
    double p99 = sorted[(int)(0.99 * n)];
    printf("%-10s mean=%.3f us median=%.3f us stddev=%.3f min=%.3f max=%.3f p99=%.3f\n", label, mean, median, stddev, min, max, p99);
    for (i = 0; i < n; i++) fprintf(csv, "%s,%s,%d,%.4f\n", LIB_NAME, label, i, samples[i]);
    free(sorted);
}

static void report_stack(const char *label, long *samples, int n, FILE *csv) {
    long sum = 0, min = samples[0], max = samples[0];
    int i;
    long *sorted = malloc((size_t)n * sizeof(long));
    if (!sorted) {
        fprintf(stderr, "malloc failed in report_stack()\n");
        return;
    }
    memcpy(sorted, samples, (size_t)n * sizeof(long));
    qsort(sorted, (size_t)n, sizeof(long), cmp_long);
    for (i = 0; i < n; i++) {
        sum += samples[i];
        if (samples[i] < min) min = samples[i];
        if (samples[i] > max) max = samples[i];
    }
    double mean = (double)sum / n;
    long median = sorted[n / 2];
    long p99 = sorted[(int)(0.99 * n)];
    printf("%-10s stack: mean=%.1f B (%.2f KB) median=%ld B min=%ld max=%ld p99=%ld\n", label, mean, mean / 1024.0, median, min, max, p99);
    if (max >= (long)POISON_REGION_BYTES) fprintf(stderr, "WARNING: measured stack usage reached the edge of the %d-byte poison region -- POISON_REGION_BYTES should be increased, the max value below is not trustworthy.\n", POISON_REGION_BYTES);
    for (i = 0; i < n; i++) fprintf(csv, "%s,%s,%d,%ld\n", LIB_NAME, label, i, samples[i]);
    free(sorted);
}

int main(void) {
    uint8_t pk[PQCLEAN_MLKEM512_CLEAN_CRYPTO_PUBLICKEYBYTES];
    uint8_t sk[PQCLEAN_MLKEM512_CLEAN_CRYPTO_SECRETKEYBYTES];
    int i;
    double *keygen_t = malloc(NUM_ITERS * sizeof(double));
    long *keygen_stack = calloc(NUM_ITERS , sizeof(long));
    if (!keygen_t || !keygen_stack) {
        fprintf(stderr, "malloc failed\n");
        free(keygen_t);
        free(keygen_stack);
        return 1;
    }

    FILE *time_csv = fopen(OUT_FILE, "w");
    fprintf(time_csv, "library,operation,iter,microseconds\n");

    FILE *stack_csv = fopen(STACK_FILE, "w");
    fprintf(stack_csv, "library,operation,iter,stack_bytes\n");

    int call_result;
    double warmup_start = now_us();

    while (now_us() - warmup_start < 100000.0) {
        double warmup_time;
        (void)benchmark_keypair(pk, sk, &call_result, &warmup_time);
    }

    for (i = 0; i < NUM_ITERS; i++) {
        keygen_stack[i] = (long)benchmark_keypair(pk, sk, &call_result, &keygen_t[i]);
    }

    printf("=== PQClean ML-KEM-512 (Keygen Only) (N=%d, warmup=%d) ===\n", NUM_ITERS, WARMUP_ITERS);

    report_time("keygen", keygen_t, NUM_ITERS, time_csv);
    report_stack("keygen", keygen_stack, NUM_ITERS, stack_csv);

    fclose(time_csv);
    fclose(stack_csv);
    free(keygen_t);
    free(keygen_stack);

    return 0;
}