#!/usr/bin/perl

$LIBDIR= "../lib";

@files = `ls $LIBDIR`;
@targets;

foreach(@files) {
  if (/^(.*)\.c$/) {
    push(@targets, $1);
  }
}

print "TARGETS= ";
foreach(@targets) {
    print "$_ ";
}
print "\n";

print "LIBDIR= $LIBDIR\n";
print "include \$(ATOMIC_MAK)\n";
print "include \$(STM)/stm.mak\n";

print "all : \$(TARGETS)\n";

foreach(@targets) {
    print "\${LIBDIR}/$_.o ::\n\tmake -C \${LIBDIR} $_.o\n";
}

foreach(@targets) {
    my $libs;
    if (/lock/) {
	$libs = "\$(GRT_LIB)";
    } elsif (/stm_base/) {
	$libs = "\$(STM_BASE_LIB) \$(GRT_LIB)";
    } elsif (/stm-distrib/) {
	$libs = "\$(STM-DISTRIB-LIB) \$(GRT_LIB)";
	print "$_.debug: driver.o \${LIBDIR}/$_.o \$(STM-DISTRIB-DEBUG-LIB) \$(GRT_LIB)\n\t\$(LINK) \$+ -o \$\@ \$(LDFLAGS) \$(GASNET_LIBS)\n";
    }
    if ($libs) {
	print "$_: driver.o \${LIBDIR}/$_.o $libs\n\t\$(LINK) \$+ -o \$\@ \$(LDFLAGS) \$(GASNET_LIBS)\n";
    }
}
