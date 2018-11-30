#!/bin/bash

mkdir -p .gdbtemp

cat << __EOF__ > .gdbtemp/kernelbreak
file kernel/kernel.elf
set arch i386:x86-64
target remote localhost:1234
break _start
continue
__EOF__

cat << __EOF__ > .gdbtemp/kernel
file kernel/kernel.elf
set arch i386:x86-64
layout reg
target remote localhost:1234
show disassemble-next-line
set disassemble-next-line on
show disassemble-next-line
__EOF__

cat << __EOF__ > .gdbtemp/kernelkill
file kernel/kernel.elf
set arch i386:x86-64
target remote localhost:1234
kill
__EOF__

setsid make simugdb &

(echo disconnect ; echo quit) | gdb -x .gdbtemp/kernelbreak

gdb -x .gdbtemp/kernel -tui
echo quit | gdb -x .gdbtemp/kernelkill



