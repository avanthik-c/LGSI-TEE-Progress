# ML-KEM-512 Benchmark: mlkem-native vs PQClean

Real, runnable benchmark comparing the two candidate ML-KEM-512 implementations
for OP-TEE integration. Built directly against the official upstream sources
(not the earlier informal repos), using one identical harness design for both
so the numbers are directly comparable.

## What's in here

```
mlkem-benchmark/
├── mlkem_native_bench/     # benchmark against official pq-code-package/mlkem-native
│   ├── main.c              # benchmark harness (edit NUM_ITERS/WARMUP_ITERS here)
│   ├── Makefile
├── pqclean_bench/          # benchmark against official PQClean/PQClean, ml-kem-512/clean
│   ├── main.c              # mirrors the mlkem-native harness exactly
│   ├── Makefile
│   └── *.c/*.h             # vendored PQClean ml-kem-512 clean sources + fips202 + randombytes
├── plot_results.py         # generates the 3 charts from results/combined.csv
└── results/
    ├── combined.csv                 # raw per-iteration timing samples, both libraries
    ├── timing_comparison.png        # headline grouped bar chart (median + p99 whiskers)
    ├── codesize_comparison.png      # .text size comparison
    └── distribution_boxplots.png    # per-operation timing distributions
```

## How to reproduce

```bash
# mlkem-native
cd mlkem_native_bench
make
./bench_mlkem_native          # prints stats, writes results_mlkem_native.csv
./bench_mlkem_c		      # prints stats, writes results_mlkem_c.csv

# PQClean
cd ../pqclean_bench
make
./bench_pqclean                # prints stats, writes results_pqclean.csv

# combine + plot
cd ..
cat mlkem_native_bench/results_native.csv > results/combined.csv
tail -n +2 pqclean_bench/results_pqclean.csv >> results/combined.csv
tail -n +2 mlkem_native_bench/results_c.csv >> results/combined.csv
pip install matplotlib pandas
python3 plot_results.py
```

## Methodology

- **Both harnesses are structurally identical**: same iteration count (50,000, +1000
  discarded warm-up), same timer (`clock_gettime(CLOCK_MONOTONIC)`, nanosecond
  resolution), same statistics computed (mean/median/stddev/min/max/p99), same
  CSV schema.
- Built with identical flags on both sides: `gcc -O3 -std=c99`.
- **This run was done on the host x86_64 machine, not inside QEMU/OP-TEE
  secure world.
