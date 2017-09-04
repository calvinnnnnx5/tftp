#!/usr/bin/perl
#

use strict;
use warnings;
use IPC::Open2;

my($IN,$OUT,$in,$out);

my $sid = open2($OUT,$IN,"./server");
my $cid = open2($out, $in,"./client -r test.txt");

while (<$OUT>) {

    print;
}
