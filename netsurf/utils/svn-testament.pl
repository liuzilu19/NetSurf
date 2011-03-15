#!/usr/bin/perl -w

use strict;
use File::Temp;

=head1

Generate a testament describing the current SVN status. This gets written
out in a C form which can be used to construct the NetSurf SVN testament
file for signon notification.

If there is no SVN in place, the data is invented arbitrarily.

=cut

my $root = shift @ARGV;
my $targetfile = shift @ARGV;

my %svninfo; # The SVN info output

$root .= "/" unless ($root =~ m@/$@);

my $svn_present = 0;
if ( -d ".svn" ) {
   $svn_present = 1;
}

sub gather_output {
   my $cmd = shift;
   my $tmpfile = File::Temp::tmpnam();
   local $/ = undef();
   system("$cmd > $tmpfile");
   open CMDH, "<", $tmpfile;
   my $ret = <CMDH>;
   close CMDH;
   unlink($tmpfile);
   return $ret;
}

if ( $svn_present ) {
   foreach my $line (split(/\n/, gather_output("svn info $root"))) {
      my ($key, $value) = split(/: /, $line, 2);
      $key = lc($key);
      $key =~ s/\s+//g;
      $svninfo{$key} = $value;
   }
} else {
   $svninfo{repositoryroot} = "http://nowhere/";
   $svninfo{url} = "http://nowhere/tarball/";
   $svninfo{revision} = "unknown";
}

my %svnstatus; # The SVN status output

if ( $svn_present ) {
   foreach my $line (split(/\n/, gather_output("svn status $root"))) {
      chomp $line;
      my $op = substr($line, 0, 1);
      if ($op eq ' ' && substr($line, 1, 1) ne ' ') { $op = "p"; }
      my $fn = substr($line, 8);
      $fn = substr($fn, length($root)) if (substr($fn, 0, length($root)) eq $root);
      next unless (care_about_file($fn, $op));
      $svnstatus{$fn} = $op;
   }
}

my %userinfo; # The information about the current user

{
   my @pwent = getpwuid($<);
   $userinfo{USERNAME} = $pwent[0];
   my $gecos = $pwent[6];
   $gecos =~ s/,.+//g;
   $gecos =~ s/"/'/g;
   $userinfo{GECOS} = $gecos;
}

# The current date, in AmigaOS version friendly format (dd.mm.yyyy)

my ($sec,$min,$hour,$mday,$mon,$year,$wday,$yday,$isdst) = localtime();
my $compiledate = sprintf("%02d.%02d.%d",$mday,$mon+1,$year+1900);
chomp $compiledate;

# Spew the testament out

my $testament = "";

$testament .= "#define USERNAME \"$userinfo{USERNAME}\"\n";
$testament .= "#define GECOS \"$userinfo{GECOS}\"\n";

my $qroot = $root;
$qroot =~ s/"/\\"/g;

my $hostname = $ENV{HOSTNAME};

unless ( defined($hostname) && $hostname ne "") {
   # Try hostname command if env-var empty
   $hostname = gather_output("hostname");
   chomp $hostname;
}

$hostname = "unknown-host" unless (defined($hostname) && $hostname ne "");
$hostname =~ s/"/\\"/g;

$testament .= "#define WT_ROOT \"$qroot\"\n";
$testament .= "#define WT_HOSTNAME \"$hostname\"\n";
$testament .= "#define WT_COMPILEDATE \"$compiledate\"\n";

my $url = $svninfo{url};
# This only works on 1.3.x and above
$url = substr($url, length($svninfo{repositoryroot}));
if ( substr($url,0,1) ne '/' ) { $url = "/$url"; }
$testament .= "#define WT_BRANCHPATH \"$url\"\n";
if ($url =~ m@/trunk/@) {
   $testament .= "#define WT_BRANCHISTRUNK 1\n";
}
if ($url =~ m@/tags/@) {
   $testament .= "#define WT_BRANCHISTAG 1\n";
}
if ($url =~ m@/tarball/@) {
   $testament .= "#define WT_NO_SVN 1\n";
}
$testament .= "#define WT_REVID \"$svninfo{revision}\"\n";
$testament .= "#define WT_MODIFIED " . scalar(keys %svnstatus) . "\n";
$testament .= "#define WT_MODIFICATIONS {\\\n";
my $doneone = 0;
foreach my $filename (sort keys %svnstatus) {
   if ($doneone) {
      $testament .= ", \\\n";
   }
   $testament .= "  { \"$filename\", '$svnstatus{$filename}' }";
   $doneone = 1;
}
$testament .= " \\\n}\n";

use Digest::MD5 qw(md5_hex);

my $oldcsum = "";
if ( -e $targetfile ) {
   open OLDVALUES, "<", $targetfile;
   foreach my $line (readline(OLDVALUES)) {
      if ($line =~ /MD5:([0-9a-f]+)/) {
         $oldcsum = $1;
      }
   }
   close OLDVALUES;
}

my $newcsum = md5_hex($testament);

if ($oldcsum ne $newcsum) {
   print "TESTMENT: $targetfile\n";
   open NEWVALUES, ">", $targetfile or die "$!";
   print NEWVALUES "/* ", $targetfile,"\n";
   print NEWVALUES <<'EOS';
 * 
 * Revision testament.
 *
 * *WARNING* this file is automatically generated by svn-testament.pl 
 *
 * Copyright 2011 NetSurf Browser Project
 */

EOS

   print NEWVALUES "#ifndef NETSURF_REVISION_TESTAMENT\n";
   print NEWVALUES "#define NETSURF_REVISION_TESTAMENT \"$newcsum\"\n\n";
   print NEWVALUES "/* Revision testament checksum:\n";
   print NEWVALUES " * MD5:", $newcsum,"\n */\n\n";
   print NEWVALUES "/* Revision testament: */\n";
   print NEWVALUES $testament;
   print NEWVALUES "\n#endif\n";
   close NEWVALUES;
     foreach my $unwanted (@ARGV) {
        next unless(-e $unwanted);
        print "TESTAMENT: Removing $unwanted\n";
        system("rm", "-f", "--", $unwanted);
     }
} else {
   print "TESTMENT: unchanged\n";
}

exit 0;

sub care_about_file {
   my ($fn, $op) = @_;
   return 0 if ($fn =~ /\.d$/); # Don't care for extraneous DEP files
   return 0 if ($fn =~ /\.a$/); # Don't care for extraneous archive files
   return 0 if ($fn =~ /\.md5$/); # Don't care for md5sum files
   return 0 if ($fn =~ /\.map$/); # Don't care for map files
   return 1;
}
