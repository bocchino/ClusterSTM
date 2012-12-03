#!/usr/bin/perl

format OUT = 
@<<<<<< @<<<<<<<<<<
$num, $par
.

@files = `ls`;
%runtimes_par;
@nums;

sub process(@) {
    my $num_procs;
    my $runtime;
    my $file = shift @_;
    foreach (@_) {
	if (/(\d+) processors/) { $num_procs = $1; }
	elsif (/execution time=(\d+\.\d+)/) { $runtime = $1; }
    }
    if (!$num_procs) {
	print("WARNING: for file $file, num_procs is empty\n");
    }
    if (!$runtime) {
	print("WARNING: for file $file, runtime is empty\n");
    }
    push @nums, $num_procs;
    my $hash;
    $hash = \%runtimes_par;
    $stored = $hash->{$num_procs};
    if (!$stored || $runtime < $stored) {
	$hash->{$num_procs} = $runtime;
    }
}

sub output(@) {
    (my $filename) = @_;
    open (OUT, ">$filename");
    foreach(sort {$a <=> $b} keys (%runtimes_par)) {
	$num = $_;
        $par = $runtimes_par{$_};
        write OUT;
    }
    close OUT;
}

foreach(@files) {
    my $file = $_;
    chomp($file);
    if (/^.*out/) {
	process($file, `cat $_`);
    }
}

output ("runtimes.dat");



