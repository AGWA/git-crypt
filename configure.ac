#
#
#

# Autoconf

AC_PREREQ(2.61)
AC_INIT([git-crypt], [0.3])
AC_CONFIG_HEADERS([config.h])

# Automake

AM_INIT_AUTOMAKE([subdir-objects -Wall foreign tar-pax 1.11])
AM_SILENT_RULES([yes])

# Programs

AC_PROG_CXX
AC_PROG_CC

# Packages

PKG_CHECK_MODULES(OPENSSL, [openssl])

# Output

AC_CONFIG_FILES([
	Makefile
	])

AC_OUTPUT
