#!/bin/bash
# Creates 12 log files of different sizes (1 MB to 50 MB) in logs/.
# Sizes are deliberately not in order, so file sizes are uneven.
mkdir -p logs
i=1
for mb in 20 1 50 5 12 3 30 8 2 40 15 25; do
    name=$(printf "logs/log%02d.log" $i)
    awk -v mb=$mb -v seed=$i 'BEGIN {
        srand(seed)
        split("INFO INFO INFO DEBUG WARN ERROR", lv, " ")
        bytes = 0
        while (bytes < mb * 1048576) {
            line = sprintf("2026-11-11 10:%02d:%02d [%s] service-%d request id=%d took %dms",
                           int(rand()*60), int(rand()*60), lv[int(rand()*6)+1],
                           int(rand()*20), int(rand()*1000000), int(rand()*900))
            print line
            bytes += length(line) + 1
        }
    }' > "$name"
    i=$((i+1))
done
ls -lh logs/