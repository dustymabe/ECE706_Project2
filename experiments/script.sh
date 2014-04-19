#!/bin/bash

TAB="_tab"

INTERVALS=(
#   0       # No migration
#   10000   # Migrate every 10000   accesses
    100000  # Migrate every 100000  accesses
#   1000000 # Migrate every 1000000 accesses
#   1
)


TRACES=(
#   CG_TRACE.t.100M
#   CG_TRACE.t
#   FT_TRACE.t
#   BT_TRACE.t
#   SEQ_NAMD.t
#   SEQ_ZEUS.t
    SEQ_LBM.t
)

OVERLAPS=(
    0      #  No partition sharing (fully private caches)
    1000   #  Partition sharing for 1000 accesses after migration
    10000  # 
    100000 # 
    1000000 # 
)

for trace in ${TRACES[@]}; do
    file=~/Desktop/traces/$trace
    for interval in ${INTERVALS[@]}; do
        for overlap in ${OVERLAPS[@]}; do
            outfile="./${trace}_int${interval}_overlap${overlap}${TAB}.txt"
            cmd="../sim $interval $overlap $file $TAB"
            echo "$cmd > $outfile"
            $cmd > $outfile || rm $outfile
        done
    done
done 
