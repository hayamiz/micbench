 MICBENCH: a benchmarking toolset
==================================

MICBENCH is a set of programs for measuring basic performances of your machines.

  * micbench-io  : I/O performance
  * micbench-mem : Memory access performance

 Prerequisites
---------------

  * Ruby interpreter (1.8.7 or later)
  * numactl
  * libnuma

 How to build
--------------

    $ ./configure
    $ make

 For Developers
----------------

You can build micbench from scratch by executing commands below. In
addition to prerequisites above, you need autotools to build.

    $ ./autogen.sh
    $ ./configure
    $ make

If you want to run tests, you also need GLib and Cutter.

  * GLib   : http://www.gtk.org/
  * Cutter : http://cutter.sf.net/
