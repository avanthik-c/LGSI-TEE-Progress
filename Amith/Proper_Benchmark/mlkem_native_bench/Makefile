# Unified Benchmark build for mlkem-native
CC ?= gcc
LDFLAGS := -lm

# 1. Detect architecture from the OS (e.g., x86_64, aarch64, arm64)
ARCH_RAW := $(shell uname -m)

# Mac OS returns "arm64", but the folder in mlkem-native is named "aarch64".
# This normalizes the name so it works seamlessly on Apple Silicon.
ifeq ($(ARCH_RAW),arm64)
    ARCH := aarch64
else
    ARCH := $(ARCH_RAW)
endif

# Base flags shared by both versions
BASE_CFLAGS := -O3 -std=c99 -Wall -I mlkem_native \
          -DMLK_CONFIG_PARAMETER_SET=512 -DMLK_CONFIG_NAMESPACE_PREFIX=mlkem

# 2. Apply specific compiler flags based on architecture
ifeq ($(ARCH),x86_64)
    # Intel/AMD require AVX2 flags to compile the .S files
    NATIVE_FLAGS := -mavx2 -mbmi2 -mpopcnt -DMLK_CONFIG_USE_NATIVE_BACKEND_ARITH -DMLK_CONFIG_USE_NATIVE_BACKEND_FIPS202
else
    # ARM, RISC-V, and POWER64 just need the macros; they don't use -mavx2
    NATIVE_FLAGS := -DMLK_CONFIG_USE_NATIVE_BACKEND_ARITH -DMLK_CONFIG_USE_NATIVE_BACKEND_FIPS202
endif

# Find all C files recursively
MLK_SOURCE := $(shell find mlkem_native/src -name '*.c')

# 3. Dynamic Assembly Discovery!
# This inserts the detected $(ARCH) right into the folder path
ASM_SOURCE := $(shell find mlkem_native/src/native/$(ARCH) -name '*.S' 2>/dev/null) \
              $(shell find mlkem_native/src/fips202/native/$(ARCH) -name '*.S' 2>/dev/null)

APP_SOURCE := main.c

all: bench_mlkem_c bench_mlkem_native

# 1. Build the Pure C Version
bench_mlkem_c: $(MLK_SOURCE) $(APP_SOURCE)
	$(CC) $(BASE_CFLAGS) \
	-DLIB_NAME='"mlkem-native-portable"' \
	-DOUT_FILE='"results_c.csv"' \
	$^ -o $@ $(LDFLAGS)

# 2. Build the Architecture-Specific Version
bench_mlkem_native: $(MLK_SOURCE) $(ASM_SOURCE) $(APP_SOURCE)
	$(CC) $(BASE_CFLAGS) $(NATIVE_FLAGS) \
	-DLIB_NAME='"mlkem-native-$(ARCH)"' \
	-DOUT_FILE='"results_native.csv"' \
	$^ -o $@ $(LDFLAGS)

# Run both benchmarks sequentially
run: all
	./bench_mlkem_c
	./bench_mlkem_native

clean:
	rm -f bench_mlkem_c bench_mlkem_native results_c.csv results_native.csv

.PHONY: all run clean
