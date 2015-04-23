#!/bin/sh
arm-linux-androideabi-as -o $1.o -a=$1.lst $1.asm 
arm-linux-androideabi-objcopy -O binary $1.o $1.bin
hexdump -v -s2 -e '/1 "0x%02x,"' $1.bin
