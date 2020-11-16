dnl Checks for memfd_create() support

AC_DEFUN([CHECK_MEMFD_CREATE], [

  AC_MSG_CHECKING([for memfd_create])

  AC_LINK_IFELSE([
    AC_LANG_PROGRAM([[
      #define _GNU_SOURCE
      #include <sys/mman.h>
    ]], [[
      (void)memfd_create("configure test", MFD_CLOEXEC);
      return 0;
    ]])
  ], [
    AC_MSG_RESULT([yes])
    AC_DEFINE([HAVE_MEMFD_CREATE], [1], [Define if MEMFD_CREATE is supported])
  ], [
    AC_MSG_RESULT([no])
  ])
])
