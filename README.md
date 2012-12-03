ClusterSTM
==========

This is the code base for Cluster-STM, a software transactional memory
(STM) implementation designed for large-scale "shared nothing"
clusters.

License
-------

This software is free for general use, provided that attribution is
maintained (citing this repository is sufficient).

This software incorporates the `umalloc` library, which is released
under GPL2.  For further information, see the file `COPYING.LIB` in the
directory `ClusterSTM/Implementation/grt/umalloc`.

This software is provided with absolutely no warranty of any kind.

Dependencies
------------

Cluster-STM depends on the following third-party software, which is
not provided in this repository.  

1. The GASNet networking library (http://gasnet.cs.berkeley.edu).

2. The libmba library of generic C modules
(http://www.ioplex.com/~miallen/libmba/).

To use Cluster-STM, you must be able to install this software.

Setup
-----

To set up Cluster-STM on your computer, do the following:

1. Download and install GASNet and libmba.

2. Create and setup the file `init-local.csh` as instructed in the
   file `init.csh` contained in this directory.

3. In the top-level Cluster-STM directory, issue the command `make`.

This should build the runtime and benchmarks.  Then you can run the
benchmarks located in `${CLUSTER_STM}/Benchmarks`.

References
----------

For further information on Cluster-STM and how to use it, see the
paper R. Bocchino, V. Adve, and B. Chamberlain, *Software
Transactional Memory for Large-Scale Clusters*, PPoPP 2008.