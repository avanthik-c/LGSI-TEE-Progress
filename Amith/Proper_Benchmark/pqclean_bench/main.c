/*
 * Benchmark harness for PQClean ML-KEM-512 (Keygen Only) - Secure RNG
 */
#define _POSIX_C_SOURCE 199309L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <math.h>
#include <sys/random.h>
#include <errno.h>
#include "api.h"
#ifndef NUM_ITERS
#define NUM_ITERS 50000
#endif
#ifndef WARMUP_ITERS
#define WARMUP_ITERS 100
#endif
static double now_us(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec * 1e6 + (double)ts.tv_nsec / 1e3;
}
static int cmp_double(const void *a, const void *b) {
    double da = *(const double *)a, db = *(const double *)b;
    return (da > db) - (da < db);
}
static void report(const char *label, double *samples, int n, FILE *csv) {
    double sum = 0.0, min = samples[0], max = samples[0];
    int i;
    double *sorted = malloc((size_t)n * sizeof(double));
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
    printf("%-10s  mean=%.3f us  median=%.3f us  stddev=%.3f  min=%.3f  max=%.3f  p99=%.3f\n",
           label, mean, median, stddev, min, max, p99);
    for (i = 0; i < n; i++) {
        fprintf(csv, "PQClean,%s,%d,%.4f\n", label, i, samples[i]);
    }
    free(sorted);
}
/* Secure OS-level RNG */
int PQCLEAN_randombytes(uint8_t *out, size_t outlen) {
#ifdef __APPLE__
    size_t total_read = 0;
    while (total_read < outlen) {
        size_t chunk = outlen - total_read;
        if (chunk > 256) chunk = 256;  /* getentropy() max per call on macOS */
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
            if (errno == EINTR) {
                continue;
            }
            return -1; 
        }
        total_read += res;
    }
    return 0;
#endif
}
int main(void) {
    uint8_t pk[PQCLEAN_MLKEM512_CLEAN_CRYPTO_PUBLICKEYBYTES];
    uint8_t sk[PQCLEAN_MLKEM512_CLEAN_CRYPTO_SECRETKEYBYTES];
    int i;
    double *keygen_t = malloc(NUM_ITERS * sizeof(double));
    FILE *csv = fopen("results_pqclean.csv", "w");
    fprintf(csv, "library,operation,iter,microseconds\n");
    /* Warm-up (not recorded) */
    for (i = 0; i < WARMUP_ITERS; i++) {
        PQCLEAN_MLKEM512_CLEAN_crypto_kem_keypair(pk, sk);
    }
    /* Keygen */
    for (i = 0; i < NUM_ITERS; i++) {
        double t0 = now_us();
        PQCLEAN_MLKEM512_CLEAN_crypto_kem_keypair(pk, sk);
        keygen_t[i] = now_us() - t0;
    }
    printf("=== PQClean ML-KEM-512 (Keygen Only) (N=%d, warmup=%d) ===\n",
           NUM_ITERS, WARMUP_ITERS);
    report("keygen", keygen_t, NUM_ITERS, csv);
    fclose(csv);
    free(keygen_t);
    return 0;
}
