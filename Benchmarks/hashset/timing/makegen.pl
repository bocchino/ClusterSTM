#!/usr/bin/perl

$LIBDIR= "../lib";

@files = `ls $LIBDIR`;
@targets;

foreach(@files) {
    if (/^(.*)\.c$/) {
	my $target = $1;
	push(@targets, $target);
	if (/stm.*\.c$/) {
	    push(@targets, "$target-rv");
	    push(@targets, "$target-wb");
	    push(@targets, "$target-la");
	}
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
    print "\${LIBDIR}/$_.o :\n\tmake -C \${LIBDIR} $_.o\n";
}

foreach(@targets) {
    my $libs, $timing, $suffix;
    $suffix = ".o";
    if (/lock/) {
	$libs = "\$(GRT_LIB)";
    } elsif (/stm_base/) {
	$libs = "\$(STM_BASE_LIB) \$(GRT_LIB)";
    } elsif (/stm-distrib-rv/) {
	$libs = "\$(STM-DISTRIB-RV-LIB) \$(GRT_LIB)";
	$suffix = "-rv.o";
    } elsif (/stm-distrib-wb/) {
	$libs = "\$(STM-DISTRIB-WB-LIB) \$(GRT_LIB)";
	$suffix = "-rv.o";
    } elsif (/stm-distrib-la/) {
	$libs = "\$(STM-DISTRIB-LA-LIB) \$(GRT_LIB)";
	$suffix = "-rv.o";
    } elsif (/stm-distrib/) {
	$libs = "\$(STM-DISTRIB-LIB) \$(GRT_LIB)";
    }
    if ($libs) {
	print "$_: timing${suffix} \${LIBDIR}/$_.o\n\t\$(LINK) \$+ $libs -o \$\@ \$(LDFLAGS) \$(GASNET_LIBS)\n";
    }
}
