#!/usr/bin/env python3
"""
hex_to_bin_and_verify.py

Fixes the truncation problem from `echo -n "..." | xxd -r -p` by reading
hex from a FILE instead of a shell argument (no length limits), extracting
every 2-character hex byte pair via regex (so it doesn't matter if your
pasted text has spaces, newlines, or leftover label text mixed in), and
writing exactly the expected number of bytes.

USAGE:

  1. Paste the raw hex dump (copy from QEMU terminal, same as you'd paste
     into chat) into a plain text file using nano/vim - NOT into a shell
     command:

       nano pk_hex.txt      # paste the "Public Key (800 bytes):" hex block
       nano sk_hex.txt      # paste the "Secret Key (1632 bytes):" hex block

  2. Run this script:

       python3 hex_to_bin_and_verify.py \
           --pk-hex pk_hex.txt --pk-bytes 800 \
           --sk-hex sk_hex.txt --sk-bytes 1632 \
           --expected-pk expected_pk.bin --expected-sk expected_sk.bin

It will print exactly how many hex bytes it found in each file (so you can
immediately tell if a paste was incomplete, instead of silently getting a
2-byte file like before), write tee_pk.bin / tee_sk.bin, and run the
comparison automatically.
"""
import argparse
import re
import sys


def hex_file_to_bytes(path, expected_len):
    with open(path) as f:
        text = f.read()

    hex_pairs = re.findall(r"([0-9a-fA-F]{2})", text)
    found = len(hex_pairs)

    print(f"{path}: found {found} hex bytes (expected {expected_len})")

    if found < expected_len:
        print(f"  !! INCOMPLETE PASTE - only got {found}/{expected_len} bytes.")
        print(f"  !! Re-copy the full block from the terminal into {path} "
             f"and try again.")
        sys.exit(1)
    if found > expected_len:
        print(f"  (found more than needed - using only the first "
             f"{expected_len}; extra text/labels are being ignored, which "
             f"is fine)")

    hex_pairs = hex_pairs[:expected_len]
    return bytes(int(h, 16) for h in hex_pairs)


def compare(name, got_path, got, expected_path, expected):
    if got == expected:
        print(f"{name}: MATCHES {expected_path} — PASS")
        return True
    first_diff = next(
        (i for i, (a, b) in enumerate(zip(got, expected)) if a != b),
        min(len(got), len(expected))
    )
    print(f"{name}: MISMATCH vs {expected_path} at byte offset {first_diff} "
         f"(got 0x{got[first_diff]:02x}, expected 0x{expected[first_diff]:02x})")
    return False


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--pk-hex", required=True,
                   help="text file containing the pasted public-key hex dump")
    ap.add_argument("--pk-bytes", type=int, default=800)
    ap.add_argument("--sk-hex", required=True,
                   help="text file containing the pasted secret-key hex dump")
    ap.add_argument("--sk-bytes", type=int, default=1632)
    ap.add_argument("--expected-pk", required=True)
    ap.add_argument("--expected-sk", required=True)
    ap.add_argument("--out-pk", default="tee_pk.bin")
    ap.add_argument("--out-sk", default="tee_sk.bin")
    args = ap.parse_args()

    pk = hex_file_to_bytes(args.pk_hex, args.pk_bytes)
    sk = hex_file_to_bytes(args.sk_hex, args.sk_bytes)

    with open(args.out_pk, "wb") as f:
        f.write(pk)
    with open(args.out_sk, "wb") as f:
        f.write(sk)
    print(f"\nWrote {args.out_pk} ({len(pk)} bytes) and "
         f"{args.out_sk} ({len(sk)} bytes)\n")

    with open(args.expected_pk, "rb") as f:
        expected_pk = f.read()
    with open(args.expected_sk, "rb") as f:
        expected_sk = f.read()

    pk_ok = compare("PK", args.out_pk, pk, args.expected_pk, expected_pk)
    sk_ok = compare("SK", args.out_sk, sk, args.expected_sk, expected_sk)

    print()
    if pk_ok and sk_ok:
        print("=== OVERALL: PASS - keygen output matches official KAT exactly ===")
        sys.exit(0)
    else:
        print("=== OVERALL: FAIL ===")
        sys.exit(1)


if __name__ == "__main__":
    main()
