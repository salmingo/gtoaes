dnl Process this file with autoconf to produce a configure script.

AC_PREREQ(2.59)
AC_INIT(gtoaes, 0.6)
AC_CONFIG_HEADERS(config.h)

AC_CANONICAL_SYSTEM
AM_INIT_AUTOMAKE()

AC_PROG_CXX
AC_PROG_CC

AC_CHECK_FUNCS([mkdir ftruncate gettimeofday memset select socket strerror])
AC_CHECK_HEADERS([unistd.h fcntl.h netdb.h sys/socket.h syslog.h limits.h])
AC_CHECK_HEADER_STDBOOL
AC_FUNC_FORK
AC_TYPE_OFF_T
AC_TYPE_PID_T
AC_TYPE_SIZE_T
AC_TYPE_UINT32_T

AC_CONFIG_FILES(Makefile src/Makefile)
AC_OUTPUT
