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
│   ├── mlkem_native/       # vendored mlkem-native source (portable C, no native backend)
│   └── test_only_rng/      # TEST-ONLY randombytes (replace before any real deployment!)
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

# PQClean
cd ../pqclean_bench
make
./bench_pqclean                # prints stats, writes results_pqclean.csv

# combine + plot
cd ..
cat mlkem_native_bench/results_mlkem_native.csv > results/combined.csv
tail -n +2 pqclean_bench/results_pqclean.csv >> results/combined.csv
pip install matplotlib pandas
python3 plot_results.py
```

## Methodology

- **Both harnesses are structurally identical**: same iteration count (5,000, +100
  discarded warm-up), same timer (`clock_gettime(CLOCK_MONOTONIC)`, nanosecond
  resolution), same statistics computed (mean/median/stddev/min/max/p99), same
  CSV schema — this is what makes the two result sets comparable, unlike the
  original single-shot benchmarks that started this investigation.
- Each operation (keygen / encaps / decaps) is timed **individually**, in its
  own loop, rather than only timing a full round trip.
- Built with identical flags on both sides: `gcc -O3 -std=c99`.
- **This run was done on the host x86_64 machine, not inside QEMU/OP-TEE
  secure world.** It validates the harness and gives a first-order comparison,
  but before finalizing a decision you should re-run this same code inside an
  OP-TEE TA on your actual QEMU target (or hardware) — cache behavior and
  instruction mix can differ in secure world. The harness code doesn't need to
  change, just the build target and how results are retrieved (e.g. print over
  the secure UART or copy out via shared memory).

## Important caveats to mention in your presentation

- `test_only_rng/` and the PQClean `randombytes.c` here read from a
  non-cryptographic/test source — **never use these in a real deployment**.
  In OP-TEE, randomness must come from `TEE_GenerateRandom()`.
- Both builds use only the **portable C** backend (no assembly/native backend
  enabled for mlkem-native) so the comparison isolates algorithmic/implementation
  differences from architecture-specific optimization. If you want to show the
  extra speedup mlkem-native's native AArch64 backend provides, that's a
  natural "bonus" slide — rebuild with `MLK_CONFIG_USE_NATIVE_BACKEND_ARITH`
  defined and restore `mlkem_native/src/native/`.
- Code size numbers here are `.text` from the **benchmark binary**, which
  includes the test harness itself — for a cleaner apples-to-apples size
  comparison, measure the compiled `.o` files for just the library sources.
