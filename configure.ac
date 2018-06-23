AC_PREREQ([2.69])

AC_INIT([bin-utils], [1.0], [tharindubathigama@gmail.com])

AC_CONFIG_HEADERS([config.h])

AC_CONFIG_AUX_DIR([build-aux])

AC_CONFIG_MACRO_DIR([m4])

AC_SUBST(AM_CFLAGS)
AC_SUBST(AM_LDFLAGS)

# Checks for programs.
AC_PROG_CC

# Checks for libraries.
# AC_PROG_RANLIB

# Checks for header files.

# Checks for typedefs, structures, and compiler characteristics.

# Checks for library functions.
AX_PTHREAD
if test "x$ax_pthread_ok" = xno; then
    AC_MSG_ERROR([requires pthreads])
fi
test -z "$PTHREAD_LIBS" && PTHREAD_LIBS="-lpthread"

CFLAGS="$CFLAGS -std=gnu99 $PTHREAD_CLAGS"
LIBS="$LIBS $PTHREAD_LIBS"

AC_CONFIG_FILES([Makefile\
		 src/Makefile\
		 test/Makefile])

AM_INIT_AUTOMAKE([1.11 foreign subdir-objects -Wall -Werror])

AC_OUTPUT
