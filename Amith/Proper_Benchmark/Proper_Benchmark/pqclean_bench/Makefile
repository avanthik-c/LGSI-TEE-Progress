# Benchmark build for PQClean ML-KEM-512 (clean reference implementation)
CC ?= gcc
CFLAGS := -O3 -std=c99 -Wall -I.
LDFLAGS := -lm

# Grab local .c files, but filter out default randombytes.c since we put getrandom() in main.c
SOURCES := $(filter-out randombytes.c, $(wildcard *.c))
BIN := bench_pqclean

all: $(BIN)

$(BIN): $(SOURCES)
	$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS)

run: $(BIN)
	./$(BIN)

clean:
	rm -f $(BIN) results_pqclean.csv

.PHONY: all run clean
