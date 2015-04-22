#!/bin/sh
arm-linux-androideabi-as -o iowmem.o -a=iowmem.lst iowmem.asm 
arm-linux-androideabi-objcopy -O binary iowmem.o iowmem.bin
hexdump -v -s2 -e '/1 "0x%02x,"' iowmem.bin