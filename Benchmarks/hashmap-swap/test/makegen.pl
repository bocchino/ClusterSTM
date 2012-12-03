#!/usr/bin/perl

@targets;
@stm_suffixes1 = ("", "-rv", "-wb", "-la");
@lock_suffixes1 = ("");
@stm_suffixes;
@lock_suffixes;
$lib_dir = "../lib";

foreach(@lock_suffixes1) {
    push (@lock_suffixes, $_);
    push (@lock_suffixes, $_."-debug");
}

foreach(@stm_suffixes1) {
    push (@stm_suffixes, $_);
    push (@stm_suffixes, $_."-debug");
}

foreach(@lock_suffixes) {
    push (@targets, "hashmap-locks".$_);
}

foreach(@stm_suffixes) {
    push (@targets, "hashmap-stm-distrib".$_);
}

print "TARGETS-BASE= ";
foreach(@targets) {
   if (!/-rv/ && !/debug/ && !/-wb/ && !/-la/) {
     print "$_ ";
   }
}
print "\n";

print "TARGETS-RV= ";
foreach(@targets) {
   if (/rv/ && !/debug/) {
     print "$_ ";
   }
}
print "\n";

print "TARGETS-WB= ";
foreach(@targets) {
   if (/wb/ && !/debug/) {
     print "$_ ";
   }
}
print "\n";

print "TARGETS-LA= ";
foreach(@targets) {
   if (/la/ && !/debug/) {
     print "$_ ";
   }
}
print "\n";

print "TARGETS= \$(TARGETS-BASE) \$(TARGETS-RV) \$(TARGETS-WB) \$(TARGETS-LA)\n";

print "include \$(ATOMIC_MAK)\n";
print "include \$(STM)/stm.mak\n";
print "CFLAGS += -I$lib_dir\n";

foreach(@stm_suffixes) {
  my $flags;
  my $suffix = $_;
  my $libs = "\$(STM-DISTRIB";
  if (/rv/) {
    $flags .= "-DREAD_VERSIONING ";
    $libs .= "-RV";
    $hs_suffix .= "-rv";
  }
  if (/wb/) {
    $flags .= "-DWRITE_BUFFERING ";
    $libs .= "-WB";
    $hs_suffix .= "-wb";
  }
  if (/la/) {
    $flags .= "-DLATE_ACQUIRE -DWRITE_BUFFERING ";
    $libs .= "-LA";
    $hs_suffix .= "-la";
  }
  if (/debug/) {
    $flags .= "-DSTM_DEBUG ";
    $libs .= "-DEBUG";
  }
  $libs .= "-LIB) \$(GRT_LIB)";
  print "driver-stm$suffix.o: driver.c\n\t\$(CC) \$(CFLAGS) $flags -c \$< -o \$\@\n";
  print "hashmap-stm-distrib$suffix: driver-stm$suffix.o $lib_dir/hashmap-stm-distrib${suffix}.o ${libs}\n\t\$(LINK) \$+ -o \$\@ \$(LD_FLAGS) \$(GASNET_LIBS)\n";
  print "$lib_dir/hashmap-stm-distrib$suffix.o: $lib_dir/hashmap-stm-distrib.c\n\tmake -C $lib_dir hashmap-stm-distrib$suffix.o\n";
}

foreach(@lock_suffixes) {
  my $flags = "-DLOCKS ";
  my $suffix = $_;
  if (/debug/) {
    $flags .= "-DGRT_DEBUG ";
  }
  $libs = "\$(GRT_LIB)";
  print "driver$suffix.o: driver.c\n\t\$(CC) \$(CFLAGS) $flags -c \$< -o \$\@\n";
  print "hashmap-locks$suffix: driver$suffix.o $lib_dir/hashmap-locks${suffix}.o ${libs}\n\t\$(LINK) \$+ -o \$\@ \$(LD_FLAGS) \$(GASNET_LIBS)\n";
  print "$lib_dir/hashmap-locks$suffix.o: $lib_dir/hashmap-locks.c\n\tmake -C $lib_dir hashmap-locks$suffix.o\n";
}
