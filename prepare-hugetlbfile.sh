#!/bin/bash

if ! [ -d /mnt/hugetlbfile ]; then
    mkdir /mnt/hugetlbfile
    mount none -t hugetlbfs /mnt/hugetlbfile -o mode=0777
fi

echo $((1024*4*4)) > /proc/sys/vm/nr_hugepages
