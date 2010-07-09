#!/usr/bin/perl -w

use strict;

die "Usage: copyclock.pl source.input dest.input " if (@ARGV!=2);


my $source = shift @ARGV;
my $dest = shift @ARGV;

open(SOURCE, $source) || die "Could not open $source: $!\n";

my $telescope = undef;
my $ntel = undef;
my $line;

my %delays = ();
while (<SOURCE>) {
  if (/TELESCOPE NAME (\d+):\s+(\S+)/) {
    die "Unmatched: $line\n" if (defined $telescope);
    $ntel = $1;
    $telescope = $2;
    $line = $_;
  } elsif (/CLOCK DELAY \(us\) (\d+):\s+(\S+)/) {
    die "Error: $_ does not match $line\n" if ($1 != $ntel);
    die "Error: $_ does not match\n" if (! defined $telescope);
    $delays{$telescope} = $2;
    $telescope = undef;
  } elsif (/DATA STREAM/) {
    last;
  } else {
    #print;
  }
}
close(SOURCE) || die "Could not close $source: $!\n";

open(DEST, $dest) || die "Could not open $dest: $!\n";
open(OUTPUT, '>', "$dest.$$") || die "Error opening $dest.$$ for writing\n";

while (<DEST>) {
  if (/TELESCOPE NAME (\d+):\s+(\S+)/) {
    die "Unmatched: $line\n" if (defined $telescope);
    $ntel = $1;
    $telescope = $2;
    $line = $_;
    print OUTPUT;
  } elsif (/CLOCK DELAY \(us\) (\d+):\s+(\S+)/) {
    die "Error: $_ does not match\n" if (! defined $telescope);
    die "Error: $_ does not match $line\n" if ($1 != $ntel);
    if (exists $delays{$telescope}) {
      print OUTPUT "CLOCK DELAY (us) $ntel: $delays{$telescope}\n";
      delete $delays{$telescope}; # This does not work
    } else {
      warn "No match for $telescope\n";
      print OUTPUT;
    }

    $delays{$telescope} = $2;
    $telescope = undef;
  } else {
    print OUTPUT;
  }
}

close(OUTPUT) || die "Error closing $dest.$$: $!\n";
close(DEST) || die "Error closing $dest: $!\n";

# This turned off as delete above seems not to work
#foreach (keys %delays) {
#  warn "$_ not in $dest\n";
#}

my $n = 1;

while (-e "$dest.$n") {
  $n++;
}

rename $dest, "$dest.$n" || die "Could not rename $dest $dest.$n: $!\n";
rename "$dest.$$", $dest || die "Could not rename $dest.$$ $dest: $!\n";

