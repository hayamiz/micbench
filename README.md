 micbench: a benchmarking toolset
==================================

[![wercker status](https://app.wercker.com/status/b1d3b6d0ce4102f7e637c28746eeaa0e/s/master "wercker status")](https://app.wercker.com/project/byKey/b1d3b6d0ce4102f7e637c28746eeaa0e)

"micbench" is a set of programs for measuring basic performance of your machines.

  * micbench-io  : I/O performance
  * micbench-mem : Memory access performance

 Prerequisites
---------------

  * Ruby interpreter (1.8.7 or later)
  * numactl
  * libnuma
  * libaio
  * libtool (for developers)
  * automake (for developers)

Currently it also requires Nehalem or newer x86_64 architectures.

 How to build
--------------

    $ ./configure
    $ make

If you are building in git-cloned repository, you have to generate `configure` script by running:

    $ ./autogen.sh


 Examples of Usage Scenario
----------------------------

In random access mode of `micbench-mem`, each memory access operation
has data dependency on preceding operation, so all the operations are
totally serialized in a pipeline of a processor. It means that
dividing execution time by # of memory access operations is almost
equal to the memory access latency of the processor.

### Check cache layers

```
$ for i in /sys/devices/system/cpu/cpu0/cache/index*; do echo $i $(cat $i/type) $(cat $i/size); done
/sys/devices/system/cpu/cpu0/cache/index0 Data 32K
/sys/devices/system/cpu/cpu0/cache/index1 Instruction 32K
/sys/devices/system/cpu/cpu0/cache/index2 Unified 256K
/sys/devices/system/cpu/cpu0/cache/index3 Unified 20480K
```

### Measuring access latency of L1D cache (32K)

```
$ micbench mem --local -m 1 -t 10 -R -a 0:c0:0 -s 32KB -v
shuffle time: 0.000017
loop end: t=10.000301
access_pattern  random
multiplicity    1
local   false
page_size       4096
size    32768
use_hugepages   false
total_ops       6692798464
total_clk       26987229916
exec_time       10.000301
ops_per_sec     6.692597e+08
clk_per_op      4.032279e+00
total_exec_time 10.001080
```

  * `--local` ... allocate memory region for each thread
  * `-m 1` ... with 1 thread
  * `-t 10` ... the benchmark runs for 10 seconds
  * `-R` ... random access mode
  * `-a 0:c0:0` ... a thread 0 is executed only on cpu0 and the memory region is allocated on node 0
  * `-s 32K` ... the size of memory region accessed is 16KB
  * `-v` ... verbose mode

The result `clk_per_op` shows how many clocks is needed to access the L1 cache of your processor.

In this case, only 32KB of memory is accessed and all memory access
hits the L1 cache. Therefore the results shows the access latency of
the L1 cache is 4.03 clocks.

### Measuring access latency of L2 cache (256K)

```
$ ./micbench mem --local -m 1 -t 10 -R -a 0:c0:0 -s 128KB -v
memset time: 0.000101
shuffle time: 0.000058
loop end: t=10.001106
access_pattern  random
multiplicity    1
local   true
page_size       4096
size    131072
use_hugepages   false
total_ops       2229534720
total_clk       26997431104
exec_time       10.001106
ops_per_sec     2.229288e+08
clk_per_op      1.210900e+01
total_exec_time 10.001918
```

L2D is shared cache for data and instruction, so the size of accessed
region is the half of L2 cache size to avoid marginal behaviour.

This result shows that accesslatency of L2 cache is 12.1 clocks.

### Measuring access latency of DRAM

First, run the command below to mount hugetlb filesystem on `/mnt/hugetlbfile/`:

    sudo ./prepare-hugetlbfile.sh

micbench-mem creates a temporary file for `mmap` with huge pages. In
order to measure access latency of large memory regions precisely, it
is important to eliminate TLB miss latency by using huge pages.

Run the command below to measure access latency to 1GB memory region:

```
$ ./micbench mem --local -m 1 -t 10 -R -a 0:c0:0 -s 1GB --hugetlbfile /mnt/hugetlbfile/file -v
memset time: 1.386253
shuffle time: 2.957960
loop end: t=10.001355
access_pattern  random
multiplicity    1
local   true
page_size       4096
size    1073741824
use_hugepages   true
hugetlbfile     /mnt/hugetlbfile/file
hugepage_size   2097152
total_ops       334233600
total_clk       27000144156
exec_time       10.001355
ops_per_sec     3.341883e+07
clk_per_op      8.078226e+01
total_exec_time 16.226200
```

  * `--hugetlbfile /mnt/hugetlbfile/file` ... creates a temporary file for mmap with huge pages

This result shows that DRAM access latency is 80.8 clocks.


### Measuring access latency of DRAM in remote NUMA node

By changing the affinity settings, access latency to remote NUMA node
can be measured.

```
$ ./micbench mem --local -m 1 -t 10 -R -a 0:c0:1 -s 1GB --hugetlbfile /mnt/hugetlbfile/file -v
memset time: 0.590745
shuffle time: 6.181779
loop end: t=10.020494
access_pattern  random
multiplicity    1
local   true
page_size       4096
size    1073741824
use_hugepages   true
hugetlbfile     /mnt/hugetlbfile/file
hugepage_size   2097152
total_ops       163840000
total_clk       27054213356
exec_time       10.020494
ops_per_sec     1.635049e+07
clk_per_op      1.651258e+02
total_exec_time 25.018897
```

  * `-a 0:c0:1` ... thread 0 runs on core 0 and allocates memory on node 1

This result shows that access latency of DRAM in remote NUMA node is
165 clocks.


### To be written ...

 For Developers
----------------

Latest source code is on GitHub: http://github.com/hayamiz/micbench/

You can build micbench from scratch by executing commands below. In
addition to prerequisites above, you need autotools to build.

    $ ./autogen.sh
    $ ./configure
    $ make

If you want to run tests, you also need GLib and Cutter.

  * GLib   : http://www.gtk.org/
  * Cutter : http://cutter.sf.net/
