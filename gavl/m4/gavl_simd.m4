#
# Automatic simd compiler feature check by Burkhard Plaum (2006-05-16)
#
dnl GAVL_CHECK_SIMD (cpumodel)
dnl Check for specific compiler simd features. These include inline
dnl assembly as well as intrinsics. If a feature is detected, have
dnl variable HAVE_feature is set to "true". The following features are
dnl Supported:
dnl MMX: Compiler can compile inline MMX assembly
dnl SSE: Compiler can compile inline SSE assembly
AC_DEFUN([GAVL_CHECK_SIMD],[
AC_MSG_CHECKING([Architecture])
case $1 in
  i[[3-7]]86)
    AC_MSG_RESULT(IA32)
    ARCH_X86=true
    ;;
  x86_64*)
    AC_MSG_RESULT(x86_64)
    ARCH_X86=true
    ARCH_X86_64=true
    ;;
  powerpc | powerpc64)
    AC_MSG_RESULT(PowerPC)
    ARCH_PPC=true
    ;;
  *)
    AC_MSG_RESULT(unknown)
    ;;
esac

OLD_CFLAGS=$CFLAGS
CFLAGS=$2

if test x$ARCH_X86 = xtrue; then

dnl
dnl Check for MMX assembly
dnl

  AC_MSG_CHECKING([if C compiler accepts MMX assembly])
  AC_TRY_COMPILE([],[ __asm__ __volatile__ ("movq" " %" "mm0" ", %" "mm1");],
                 HAVE_MMX=true)
  if test $HAVE_MMX = true; then
    AC_MSG_RESULT(yes)
  else
    AC_MSG_RESULT(no)
  fi

dnl
dnl Check for 3Dnow assembly
dnl

  AC_MSG_CHECKING([if C compiler accepts 3Dnow assembly])
  AC_TRY_COMPILE([],[ __asm__ __volatile__ ("pmulhrw" " %" "mm0" ", %" "mm1");],
                 HAVE_3DNOW=true)
  if test "$HAVE_3DNOW" = true; then
    AC_MSG_RESULT(yes)
  else
    AC_MSG_RESULT(no)
  fi

dnl
dnl Check for 3Dnowext assembly
dnl

  AC_MSG_CHECKING([if C compiler accepts 3Dnowext assembly])
  AC_TRY_COMPILE([],[ __asm__ __volatile__ ("pswapd" " %" "mm0" ", %" "mm0");],
                 HAVE_3DNOWEXT=true)
  if test "$HAVE_3DNOWEXT" = true; then
    AC_MSG_RESULT(yes)
  else
    AC_MSG_RESULT(no)
  fi

dnl
dnl Check for SSE assembly
dnl

  AC_MSG_CHECKING([if C compiler accepts SSE assembly])
  AC_TRY_COMPILE([],[ __asm__ __volatile__ ("movaps" " %" "xmm0" ", %" "xmm1");],
                 HAVE_SSE=true)
  if test $HAVE_SSE = true; then
    AC_MSG_RESULT(yes)
  else
    AC_MSG_RESULT(no)
  fi

dnl
dnl Check for SSE2 assembly
dnl

  AC_MSG_CHECKING([if C compiler accepts SSE2 assembly])
  AC_TRY_COMPILE([],[ __asm__ __volatile__ ("movdqa" " %" "xmm0" ", %" "xmm1");],
                 HAVE_SSE2=true)
  if test $HAVE_SSE2 = true; then
    AC_MSG_RESULT(yes)
  else
    AC_MSG_RESULT(no)
  fi

dnl
dnl Check for MMX intrinsics
dnl

  AC_MSG_CHECKING([if C compiler accepts MMX intrinsics])
  AC_TRY_LINK([#include <mmintrin.h>],[__m64 m1, m2; _m_paddb(m1, m2)],HAVE_MMX_INT=true)

  if test "$HAVE_MMX_INT" = true; then
    AC_MSG_RESULT(yes)
  else
    AC_MSG_RESULT(no)
  fi


dnl
dnl Check for 3Dnow intrinsics
dnl

  AC_MSG_CHECKING([if C compiler accepts 3Dnow intrinsics])
  AC_TRY_LINK([#include <mm3dnow.h>],[_m_femms();],HAVE_3DNOW_INT=true)

  if test "$HAVE_3DNOW_INT" = true; then
    AC_MSG_RESULT(yes)
  else
    AC_MSG_RESULT(no)
  fi

dnl
dnl Check for 3Dnowext intrinsics
dnl

  AC_MSG_CHECKING([if C compiler accepts 3Dnowext intrinsics])
  AC_TRY_LINK([#include <mm3dnow.h>],[__m64 b1;b1 = _m_pswapd(b1);_m_femms();],HAVE_3DNOWEXT_INT=true)

  if test "$HAVE_3DNOWEXT_INT" = true; then
    AC_MSG_RESULT(yes)
  else
    AC_MSG_RESULT(no)
  fi

dnl
dnl Check for SSE intrinsics
dnl

  AC_MSG_CHECKING([if C compiler accepts SSE intrinsics])
  AC_TRY_LINK([#include <xmmintrin.h>],[__m128 m1, m2; _mm_add_ss(m1, m2)],HAVE_SSE_INT=true)

  if test "$HAVE_SSE_INT" = true; then
    AC_MSG_RESULT(yes)
  else
    AC_MSG_RESULT(no)
  fi

dnl
dnl Check for SSE2 intrinsics
dnl

  AC_MSG_CHECKING([if C compiler accepts SSE2 intrinsics])
  AC_TRY_LINK([#include <emmintrin.h>],[__m128d m1; m1 = _mm_set1_pd(1.0)],HAVE_SSE2_INT=true)
  if test "$HAVE_SSE2_INT" = true; then
    AC_MSG_RESULT(yes)
  else
    AC_MSG_RESULT(no)
  fi




fi

CFLAGS=$OLD_CFLAGS

])