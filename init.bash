#
# Initialization script for ClusterSTM.  To use this file:
#
# 1. Copy it to something else, like init-local.csh, so this file can
#    live unmodified in the git repository.
#
# 2. In your .cshrc file, set the environment variable CLUSTER_STM to
#    point to the head of the Cluster STM tree (it should be the
#    directory where this file resides).
#
# 3. Configure this file as appropriate for your system and/or how you
#    intend to use GASNet.
#
# 4. In your .cshrc file, add the line "source
#    ${CLUSTER_STM}/init-local.csh"
#

# Set LIBMBA and GASNET as appropriate for your system
export LIBMBA=/path/to/your/libmba/installation
export GASNET=/path/to/your/gasnet/installation

# Add scripts to path
export PATH=${CLUSTER_STM}/Benchmarks/scripts:${PATH}

export ATOMIC_ROOT=${CLUSTER_STM}/Implementation
export ATOMIC_MAK=${ATOMIC_ROOT}/atomic.mak
export ATOMIC_CC=cc
export ATOMIC_CXX=c++
export GRT=${ATOMIC_ROOT}/grt

# Where to run GASNet threads
export GASNET_SPAWNFN=L
export SSH_SERVERS='localhost

# To use the UDP conduit with ssh
export GASNET_CONDUIT=udp-conduit
#export GASNET_MAKE_INCL=udp-seq.mak
#export GASNET_DEFS='-DGASNET_SEQ
export GASNET_MAKE_INCL=udp-par.mak
export GASNET_DEFS='-DGASNET_PAR

# To use the mpi conduit with mpirun
#export GASNET_CONDUIT=mpi-conduit
#export GASNET_MAKE_INCL=mpi-par.mak
#export GASNET_DEFS='-DGASNET_PAR

