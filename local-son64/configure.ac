AC_PREREQ(2.59)
LT_PREREQ(2.4.6)
AC_INIT([local-son64],[1.0.3],[dshuman@usf.edu])
AM_INIT_AUTOMAKE()
AC_CONFIG_SRCDIR([son64.cpp])
AC_CONFIG_HEADER([config.h])

AC_GNU_SOURCE

# Checks for programs.
AC_PROG_CC
AC_PROG_CXX
AM_PROG_CC_C_O
LT_INIT([disable-static])
AC_CONFIG_MACRO_DIRS([m4])

CPPFLAGS=""
CXXFLAGS=""
CFLAGS=""

AC_CHECK_PROGS([MXE_QMAKE],[x86_64-w64-mingw32.static-gcc])
if test -z "$MXE_QMAKE"; then
   AC_MSG_WARN([The MXE cross development environment is required to build the MS Windows version of the son64 library (not fatal).  Consult the HOWTO_BUILD_FOR_WIN document included in this package.])
else
   AC_MSG_NOTICE([The MXE cross development environment is available.])
fi


AC_CONFIG_FILES([Makefile])
AC_OUTPUT
