```bash
avc@DESKTOP-JKFMRP4:~/optee-qemu/optee_os/lib/libmbedtls$ ls
core  include  mbedtls  mlkem  sub.mk
avc@DESKTOP-JKFMRP4:~/optee-qemu/optee_os/lib/libmbedtls$ tree mlkem
mlkem
└── sample.c

1 directory, 1 file
avc@DESKTOP-JKFMRP4:~/optee-qemu/optee_os/lib/libmbedtls$ cat mlkem/sample.c
#include <mlkem/sample.h>
int simple_add(int a,int b){
        return a+b;
}
avc@DESKTOP-JKFMRP4:~/optee-qemu/optee_os/lib/libmbedtls$
```
add line 
`srcs-y += mlkem/sample.c #change made` in sub.mk
```bash
avc@DESKTOP-JKFMRP4:~/optee-qemu/optee_os/lib/libmbedtls/include$ ls
aes_alt.h  mbedtls_config_kernel.h  mbedtls_config_uta.h  mlkem
avc@DESKTOP-JKFMRP4:~/optee-qemu/optee_os/lib/libmbedtls/include$ tree mlkem
mlkem
└── sample.h

1 directory, 1 file
avc@DESKTOP-JKFMRP4:~/optee-qemu/optee_os/lib/libmbedtls/include$ cat mlkem/sample.h
#ifndef SAMPLE_H
#define SAMPLE_H

int simple_add(int a,int b);
#endif
avc@DESKTOP-JKFMRP4:~/optee-qemu/optee_os/lib/libmbedtls/include$
```
similarly add the header file under mlkem folder

and make modifications in the ta
add header `#include<mlkem/sample.h>`
and can call function call like code directly
```c
IMSG("Calling sample.c from mbedtls dir");
int add_res=simple_add(20,30);
```
what we have doen here
when optee os is booted these cryptolib are compiled for the os to use/like for os to perform the functions and give result

but here these same libraries (if given headers in ta) will be complied along with the ta too when its executed
so ta can execute crypto function without accessing kernel or triggering any system call

other method is using the TEE API call TEE_AllocateOperation to call specific crypto functions


