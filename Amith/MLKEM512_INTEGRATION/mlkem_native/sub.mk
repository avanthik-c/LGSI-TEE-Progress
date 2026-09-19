global-incdirs-y += .
srcs-y += mlkem_native.c
srcs-y += mlkem_native_asm.S
srcs-y += mlkem512_keygen.c
cflags-mlkem_native.c-y += -Wno-unused-parameter
cflags-mlkem_native.c-y += -Wno-sign-conversion
