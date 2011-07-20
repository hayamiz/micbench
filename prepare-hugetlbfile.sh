mount none -t hugetlbfs /mnt/hugetlbfile -o mode=0777
echo $((1024*12*8)) > /proc/sys/vm/nr_hugepages 
