#!/bin/bash

NUM_CPU=$(ls -d /sys/devices/system/cpu/*/cpufreq|wc -l)

for i in $(seq $((NUM_CPU-1))); do
    echo cpufreq-set -c $i -g performance
    cpufreq-set -c $i -g performance
done