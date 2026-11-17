#!/bin/bash
# Runs each program 3 times per buffer size, prints the average time,
# verifies the copy with cmp, and saves one strace -c summary per case.
# sync + rm before every run so one run's unwritten data does not slow the next.

[ -f big.bin ] || dd if=/dev/urandom of=big.bin bs=1M count=100 status=none

for prog in copy_sys copy_stdio; do
    for bs in 512 4096 65536; do
        times=""
        for i in 1 2 3; do
            rm -f out.bin
            sync
            t=$(./$prog big.bin out.bin $bs | awk '{print $2}')
            cmp -s big.bin out.bin || { echo "COPY MISMATCH: $prog $bs"; exit 1; }
            times="$times $t"
        done
        avg=$(echo $times | awk '{printf "%.6f", ($1+$2+$3)/3}')
        echo "$prog buffer=$bs runs=[$times ] average=$avg s"

        rm -f out.bin
        sync
        strace -c -o strace_${prog}_${bs}.txt ./$prog big.bin out.bin $bs > /dev/null
    done
done