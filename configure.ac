dnl Process this file with autoconf to produce a configure script.

AC_PREREQ(2.69)
AC_INIT(gtoaes, 0.8)
AC_CONFIG_HEADERS(config.h)

AC_CANONICAL_SYSTEM
AM_INIT_AUTOMAKE()

AC_PROG_CXX
AC_PROG_CC
AC_PROG_CPP

CXXFLAGS="-std=c++0x"

AC_CHECK_FUNCS([ftruncate gettimeofday memset mkdir select socket strerror])
AC_CHECK_HEADERS([fcntl.h netdb.h stdlib.h string.h sys/socket.h sys/time.h syslog.h unistd.h])
AC_CHECK_HEADER_STDBOOL
AC_FUNC_FORK
AC_FUNC_MALLOC
AC_FUNC_REALLOC
AC_TYPE_OFF_T
AC_TYPE_PID_T
AC_TYPE_SIZE_T
AC_TYPE_UINT16_T
AC_TYPE_UINT32_T

AC_CONFIG_FILES(Makefile src/Makefile)
AC_OUTPUT
