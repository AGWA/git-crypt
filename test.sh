#!/usr/bin/perl
use strict;
use warnings;
use Cwd;
use File::Path;
use Term::ANSIColor;
use Digest::MD5 qw(md5);

print "\n"."========= GIT-CRYPT TEST ========="."\n";


my $dir=Cwd::cwd()."/test";

### Set test folder
if ( -d $dir ) {
	rmtree($dir) or die "Cannot remove '$dir' : $!";
}
mkpath($dir) or die "Cannot create '$dir' : $!";
chdir($dir);

### Generate git-crypt key
my $key="test.key";

print "\n";
`git-crypt keygen $key`; !($?) or die;

### Create git repository

print "\n"."========= CREATE GIT ORIGIN REPO ========="."\n";

my $repo="repo";
mkpath($repo) or die "Cannot create '$repo' : $!";
chdir($repo);

print "\n";
my $out=`git init`; !($?) or die;
print $out;

## put some files
my $file1="crypto.hpp";
my $file2="crypto.tgz";

`curl -sS "https://raw.github.com/AGWA/git-crypt/master/crypto.hpp" > $file1`; !($?) or die;
`tar -zcf $file2 $file1`; !($?) or die;

print "add file: ".$file1."\n";
print "add file: ".$file2."\n";

open (FILE, '>>.gitattributes');
print FILE "*.hpp filter=git-crypt diff=git-crypt"."\n";
print FILE "*.tgz filter=git-crypt diff=git-crypt"."\n";
close (FILE);
print "set .gitattributes filter for: *.hpp *.tgz"."\n";

print "Initialized git-crypt repository"."\n";
`git-crypt init ../$key`; !($?) or die;

print "git add & commit files"."\n";
`git add --all`; !($?) or die;
`git commit -m "test git-crypt"`; !($?) or die;


### Clone git repository

chdir($dir);

print "\n"."======= CLONE GIT ENCRYPTED REPO ======="."\n";

my $clonerepo="clonerepo";

`git clone --quiet file://'$dir/$repo' $clonerepo`; !($?) or die;
print "\n"."Clone Git repository in "."$dir/$clonerepo/"."\n";


### TEST
chdir("$dir/$clonerepo");

### Test#1
## isencrypted clone repo ?

print "\n"."======= TEST 1: isencrypted clone repo? ======="."\n";

print "\n";
if ( isencrypted($file1) ) {
	print "\"$file1\" encrypted?: ".colored("OK", "green")."\n";
} else {
	print "\"$file1\" encrypted?: ".colored("FAIL", "red")."\n";
}

if ( isencrypted($file2) ) {
	print "\"$file2\" encrypted?: ".colored("OK", "green")."\n";
} else {
	print "\"$file2\" encrypted?: ".colored("FAIL", "red")."\n";
}


### Test#2
## decrypt clone repo

print "\n"."======= TEST 2: decrypt clone repo ======="."\n";

`git-crypt init ../$key`; !($?) or die;

print "\n";
if ( isdecrypted($file1) ) {
	print "\"$file1\" decrypted?: ".colored("OK", "green")."\n";
} else {
	print "\"$file1\" decrypted?: ".colored("FAIL", "red")."\n";
}

if ( isdecrypted($file2) ) {
	print "\"$file2\" decrypted?: ".colored("OK", "green")."\n";
} else {
	print "\"$file2\" decrypted?: ".colored("FAIL", "red")."\n";
}
print "\n";



sub isencrypted {
	my $crypthead="\x00GITCRYPT\x00";
	my $file = shift;
	open (FILE, $file) or die "Can't open '$file' : $!";
	binmode(FILE) or die "Can't binmode '$file' : $!";
	my $filehead;
	read (FILE, $filehead, 10);
	close (FILE);

	if ( "$crypthead" eq "$filehead" ) {
		return 1;
	}
	return 0;
}

sub isdecrypted {
	my $file = shift;
	if ( getmd5("$dir/$repo/$file") eq getmd5("$dir/$clonerepo/$file") ) {
		return 1;
	}
	return 0;
}

sub getmd5 {
	my $file = shift;
	open (FILE, $file) or die "Can't open '$file' : $!";
	binmode(FILE) or die "Can't binmode '$file' : $!";
	my $data = <FILE>;
	close(FILE);
	return md5($data);
}

exit 0