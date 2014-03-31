AC_INIT(
  [wet],
  [1.5.4],
  [sforbes41@gmail.com],
  [wet],
  [https://github.com/natediddy/wet],
)

AC_MSG_NOTICE([Configuring $PACKAGE_NAME $PACKAGE_VERSION])

AC_CANONICAL_SYSTEM
AC_CONFIG_SRCDIR([wet.c])
AM_INIT_AUTOMAKE([-Wall no-define foreign])
AC_CONFIG_HEADERS([config.h])
AC_CONFIG_FILES([Makefile])

AC_PROG_CC

AS_IF(
  [test x$GCC != x],
  [CFLAGS="$CFLAGS -Wall"],
  []
)

AC_HEADER_STDBOOL
AC_CHECK_HEADERS([unistd.h sys/ioctl.h windows.h])

AC_TYPE_LONG_LONG_INT
AC_TYPE_SIZE_T
AC_TYPE_SSIZE_T

AC_ARG_ENABLE(
  [debug],
  [AS_HELP_STRING([--enable-debug], [Enable debugging])
AS_HELP_STRING([--disable-debug], [Disable debugging (DEFAULT)])]
)

AS_IF(
  [test x$enable_debug = xyes],
  [AC_DEFINE([WET_DEBUG], [1], [Define if debugging is enabled])],
  []
)

AC_OUTPUT
