#!/usr/bin/perl
use strict;
use warnings;
use Cwd;
use File::Path;
use File::Copy;
use Term::ANSIColor;
use Digest::MD5 qw(md5);

print "\n"."========= GIT-CRYPT TEST ========="."\n";

my $cdir=Cwd::cwd();
my $dir=$cdir."/test";

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
my $out=`git init `; !($?) or die;
print $out;

## put some files
my @files_hpp=glob "$cdir/*.hpp";
my @files_cpp=glob "$cdir/*.cpp";

foreach my $f (@files_hpp) { copy "$f", "." or die; }
print "add: *.hpp files"."\n";
foreach my $f (@files_cpp) { copy "$f", "." or die; } 
print "add: *.cpp files"."\n"; 

my $all_hpp = join ' ', @files_hpp;
`tar -zcf "hpp.tgz" $all_hpp 2>&1`; !($?) or die;
print "add: hpp.tgz file"."\n";
my $all_cpp = join ' ', @files_cpp;
`tar -zcf "cpp.tgz" $all_cpp 2>&1`; !($?) or die;
print "add: cpp.tgz file"."\n";

open (FILE, '>>.gitattributes');
print FILE "*.hpp filter=git-crypt diff=git-crypt"."\n";
print FILE "*.cpp filter=git-crypt diff=git-crypt"."\n";
print FILE "*.tgz filter=git-crypt diff=git-crypt"."\n";
close (FILE);
print "set .gitattributes filter for: *.hpp *.cpp *.tgz"."\n";

print "Initialized git-crypt repository with key: $dir/$key"."\n";
`git-crypt init $dir/$key`; !($?) or die;

print "git add & commit files"."\n";
`git add --all`; !($?) or die;
`git commit -m "test git-crypt"`; !($?) or die;

chdir($dir);


print "\n"."======= CLONE GIT ENCRYPTED REPO ======="."\n";

my $clonerepo="clonerepo";

`git clone --quiet file://'$dir/$repo' $clonerepo`; !($?) or die;
print "\n"."Clone Encrypted Git repository in "."$dir/$clonerepo/"."\n";



### TEST

### Test#1
## isencrypted clone repo ?

chdir("$dir/$clonerepo");

print "\n"."======= TEST 1: isencrypted clone repo? ======="."\n";
print "\n";

my @all_files=glob("*.hpp *.cpp *.tgz");
foreach my $file (@all_files) {
	print "\"$file\" encrypted?: ";
	if ( isencrypted($file) ){
		print colored("OK", "green")."\n";
	} else{
		print colored("FAIL", "red")."\n";
	}	
}

print "\n";


### Test#2
## decrypt clone repo

print "\n"."======= TEST 2: decrypt clone repo ======="."\n";
print "\n";

print "Initialized git-crypt repository with key: $dir/$key"."\n";
`git-crypt init $dir/$key`; !($?) or die;

print "\n";
foreach my $file (@all_files) {
	print "\"$file\" decrypted?: ";
	if ( isdecrypted($file) ){
		print colored("OK", "green")."\n";
	} else{
		print colored("FAIL", "red")."\n";
	}	
}

print "\n";


sub isencrypted {
	my $crypthead="\x00GITCRYPT\x00";
	my $file = "$dir/$clonerepo/".shift;
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


=begin comment
print "\n";
if ( isencrypted($file1) ) {
	print "\"$file1\" encrypted?: ".colored("OK", "green")."\n";
} else {
	print "\"$file1\" encrypted?: ".colored("FAIL", "red")."\n";
}











### Clone git repository

chdir($dir);





if ( isencrypted($file2) ) {
	print "\"$file2\" encrypted?: ".colored("OK", "green")."\n";
} else {
	print "\"$file2\" encrypted?: ".colored("FAIL", "red")."\n";
}




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






=end comment
=cut

exit 0