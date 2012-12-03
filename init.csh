#
# Initialization script for ClusterSTM.  To use this file with tcsh:
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
# 4. In your .cshrc or file, add the line "source
#    ${CLUSTER_STM}/init-local.csh"
#
# If you use bash, carry out the same steps, but adjust the syntax
# accordingly.
#

# Set LIBMBA and GASNET as appropriate for your system
setenv LIBMBA /path/to/your/libmba/installation
setenv GASNET /path/to/your/gasnet/installation

# Add scripts to path
setenv PATH ${CLUSTER_STM}/Benchmarks/scripts:${PATH}

setenv ATOMIC_ROOT ${CLUSTER_STM}/Implementation
setenv ATOMIC_MAK ${ATOMIC_ROOT}/atomic.mak
setenv ATOMIC_CC cc
setenv ATOMIC_CXX c++
setenv GRT ${ATOMIC_ROOT}/grt

# Where to run GASNet threads
setenv GASNET_SPAWNFN L
setenv SSH_SERVERS 'localhost localhost'

# To use the UDP conduit with ssh
setenv GASNET_CONDUIT udp-conduit
#setenv GASNET_MAKE_INCL udp-seq.mak
#setenv GASNET_DEFS '-DGASNET_SEQ -DGASNET_NDEBUG'
setenv GASNET_MAKE_INCL udp-par.mak
setenv GASNET_DEFS '-DGASNET_PAR -DGASNET_DEBUG'

# To use the mpi conduit with mpirun
#setenv GASNET_CONDUIT mpi-conduit
#setenv GASNET_MAKE_INCL mpi-par.mak
#setenv GASNET_DEFS '-DGASNET_PAR -DGASNET_NDEBUG'

