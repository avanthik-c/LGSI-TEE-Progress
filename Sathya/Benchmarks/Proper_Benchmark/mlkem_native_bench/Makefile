# Unified Benchmark build for mlkem-native
# main.c records TIME and STACK USAGE, both per iteration, into two CSVs.
# Stack usage uses a runtime poison/scan watermark technique, so
# -fno-stack-protector is required (stack canaries would otherwise
# write into the poisoned region and corrupt the measurement).
CC ?= gcc
LDFLAGS := -lm

# 1. Detect architecture from the OS (e.g., x86_64, aarch64, arm64)
ARCH_RAW := $(shell uname -m)

# Mac OS returns "arm64", but the folder in mlkem-native is named "aarch64".
ifeq ($(ARCH_RAW),arm64)
    ARCH := aarch64
else
    ARCH := $(ARCH_RAW)
endif

# Base flags shared by both versions
BASE_CFLAGS := -O3 -std=c99 -Wall -fno-stack-protector -I mlkem_native \
          -DMLK_CONFIG_PARAMETER_SET=512 -DMLK_CONFIG_NAMESPACE_PREFIX=mlkem

# 2. Apply specific compiler flags based on architecture
ifeq ($(ARCH),x86_64)
    NATIVE_FLAGS := -mavx2 -mbmi2 -mpopcnt -DMLK_CONFIG_USE_NATIVE_BACKEND_ARITH -DMLK_CONFIG_USE_NATIVE_BACKEND_FIPS202
else
    NATIVE_FLAGS := -DMLK_CONFIG_USE_NATIVE_BACKEND_ARITH -DMLK_CONFIG_USE_NATIVE_BACKEND_FIPS202
endif

# Find all C files recursively
MLK_SOURCE := $(shell find mlkem_native/src -name '*.c')

# 3. Dynamic Assembly Discovery
ASM_SOURCE := $(shell find mlkem_native/src/native/$(ARCH) -name '*.S' 2>/dev/null) \
              $(shell find mlkem_native/src/fips202/native/$(ARCH) -name '*.S' 2>/dev/null)

APP_SOURCE := main.c

all: bench_mlkem_c bench_mlkem_native

# 1. Build the Pure C Version
bench_mlkem_c: $(MLK_SOURCE) $(APP_SOURCE)
	$(CC) $(BASE_CFLAGS) \
	-DLIB_NAME='"mlkem-native-portable"' \
	-DOUT_FILE='"results_c_time.csv"' \
	-DSTACK_FILE='"results_c_stack.csv"' \
	$^ -o $@ $(LDFLAGS)

# 2. Build the Architecture-Specific Version
bench_mlkem_native: $(MLK_SOURCE) $(ASM_SOURCE) $(APP_SOURCE)
	$(CC) $(BASE_CFLAGS) $(NATIVE_FLAGS) \
	-DLIB_NAME='"mlkem-native-$(ARCH)"' \
	-DOUT_FILE='"results_native_time.csv"' \
	-DSTACK_FILE='"results_native_stack.csv"' \
	$^ -o $@ $(LDFLAGS)

# Run both benchmarks sequentially (writes time AND stack CSVs)
run: all
	./bench_mlkem_c
	./bench_mlkem_native

clean:
	rm -f bench_mlkem_c bench_mlkem_native bench_mlkem_verify_c bench_mlkem_verify_native \
	      results_c_time.csv results_c_stack.csv \
	      results_native_time.csv results_native_stack.csv \
	      keypair_verify_c.txt keypair_verify_native.txt \
	      expected_pk_c.bin expected_sk_c.bin \
	      expected_pk_native.bin expected_sk_native.bin

.PHONY: all run clean verify verify-c verify-native