AC_PREREQ([2.57])

AC_INIT(spice-streaming-agent, 0.1,
        [spice-devel@lists.freedesktop.org])

AM_CONFIG_HEADER([config.h])
AC_CONFIG_MACRO_DIR([m4])
AC_CONFIG_AUX_DIR([build-aux])

AM_INIT_AUTOMAKE([dist-xz no-dist-gzip subdir-objects])

m4_ifdef([AM_SILENT_RULES],[AM_SILENT_RULES([yes])])

AC_PROG_CC
AC_PROG_CC_C99
if test x"$ac_cv_prog_cc_c99" = xno; then
    AC_MSG_ERROR([C99 compiler is required.])
fi
AC_PROG_CXX
AC_LANG(C++)
AX_CXX_COMPILE_STDCXX_11
AC_PROG_INSTALL
AC_CANONICAL_HOST
LT_INIT([disable-static])
AM_PROG_CC_C_O
AC_C_BIGENDIAN
PKG_PROG_PKG_CONFIG

dnl =========================================================================
dnl Check deps

SPICE_PROTOCOL_MIN_VER=0.12.14
PKG_CHECK_MODULES([SPICE_PROTOCOL], [spice-protocol >= $SPICE_PROTOCOL_MIN_VER])
AC_SUBST([SPICE_PROTOCOL_MIN_VER])

PKG_CHECK_MODULES(X11, x11)
PKG_CHECK_MODULES(XFIXES, xfixes)

PKG_CHECK_MODULES(JPEG, libjpeg)

AC_CHECK_HEADER([catch/catch.hpp],,[AC_MSG_ERROR([Could not find Catch dependency header (catch/catch.hpp)])])

dnl ===========================================================================
dnl check compiler flags

SPICE_COMPILE_WARNINGS
LIBVIRT_LINKER_RELRO
LIBVIRT_LINKER_NO_INDIRECT

AC_SUBST(WARN_CFLAGS)
AC_SUBST(WARN_CXXFLAGS)

dnl =========================================================================
dnl -fvisibility stuff

have_gcc4=no
AC_MSG_CHECKING(for -fvisibility)
AC_COMPILE_IFELSE([AC_LANG_SOURCE([
#if defined(__GNUC__) && (__GNUC__ >= 4)
#else
error Need GCC 4.0 for visibility
#endif
int main () { return 0; }
])], have_gcc4=yes)

if test "x$have_gcc4" = "xyes" && test ! "$os_win32" = "yes" ; then
   VISIBILITY_HIDDEN_CFLAGS="-fvisibility=hidden"
fi
AC_MSG_RESULT($have_gcc4)
AC_SUBST(VISIBILITY_HIDDEN_CFLAGS)

AC_DEFINE_DIR([BINDIR], [bindir], [Where data are placed to.])

AC_OUTPUT([
spice-streaming-agent.spec
data/spice-streaming.desktop
Makefile
src/Makefile
src/unittests/Makefile
include/spice-streaming-agent/Makefile
spice-streaming-agent.pc
])

dnl ==========================================================================
AC_MSG_NOTICE([

        SPICE streaming agent $VERSION
        ==============================

        prefix:                   ${prefix}
        C compiler:               ${CC}
        C++ compiler:             ${CXX}

        Now type 'make' to build $PACKAGE
])
