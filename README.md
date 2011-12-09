 micbench: a benchmarking toolset
==================================

"micbench" is a set of programs for measuring basic performance of your machines.

  * micbench-io  : I/O performance
  * micbench-mem : Memory access performance

 Prerequisites
---------------

  * Ruby interpreter (1.8.7 or later)
  * numactl
  * libnuma

Currently it also requires Nehalem or newer x86_64 architectures.

 How to build
--------------

    $ ./configure
    $ make

 Examples of Usage Scenario
----------------------------

### Measuring access latency L1

    $ micbench mem -m 1 -t 10 -R -a 0:c0 -s 16K -v
    access_pattern  random
    multiplicity    1
    local   true
    page_size       4096
    size    16384
    use_hugepages   false
    total_ops       5602803712
    total_clk       22561968928
    exec_time       10.000461
    ops_per_sec     5.602545e+08
    clk_per_op      4.026907e+00
    total_exec_time 11.518878

  * `-m 1` ... with 1 thread
  * `-t 10` ... the benchmark runs for 10 seconds
  * `-R` ... random access mode
  * `-a 0:c0` ... a thread 0 is executed only on cpu0
  * `-s 16K` ... the size of memory region accessed is 16KB
  * `-v` ... verbose mode

The result `clk_per_op` shows how many clocks is needed to access the L1 cache of your processor.

In random access mode of micbench-mem, each memory access operation
has data dependency on preceding operation, so all the operations are
totally serialized in a pipeline of a processor. It means that
dividing execution time by # of memory access operations is almost
equal to the memory access latency of the processor. In this case,
only 16KB of memory is accessed and all memory access hits the L1
cache. Therefore the results shows the access latency of the L1 cache.

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
