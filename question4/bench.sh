#!/bin/bash
# Runs logscan with 1, 2, 4, 8 threads, 3 runs each, prints average time and speedup.
# Also checks the total against grep -c.
PATTERN="ERROR"
FILES=(logs/*.log)

cat "${FILES[@]}" > /dev/null      # warm the page cache so every run starts equal

expected=$(grep -cF "$PATTERN" "${FILES[@]}" | awk -F: '{s+=$NF} END{print s}')
echo "grep -c total: $expected"

t1=0
for t in 1 2 4 8; do
    times=""
    for i in 1 2 3; do
        out=$(./logscan "$PATTERN" report_$t.txt $t "${FILES[@]}")
        total=$(echo "$out" | grep '^Total' | awk '{print $NF}')
        el=$(echo "$out" | grep '^Elapsed' | awk '{print $2}')
        [ "$total" = "$expected" ] || echo "MISMATCH: threads=$t total=$total expected=$expected"
        times="$times $el"
    done
    avg=$(echo $times | awk '{printf "%.6f", ($1+$2+$3)/3}')
    [ "$t" = "1" ] && t1=$avg
    speedup=$(awk -v a=$t1 -v b=$avg 'BEGIN{printf "%.2f", a/b}')
    echo "threads=$t runs=[$times ] average=$avg s speedup=$speedup"
done