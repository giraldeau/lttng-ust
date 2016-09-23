AM_CPPFLAGS = -I$(top_srcdir)/include -I$(top_builddir)/include
AM_CFLAGS = -fno-strict-aliasing

lib_LTLIBRARIES = \
	liblttng-ust-sampling.la

liblttng_ust_sampling_la_SOURCES = \
	sampling.c \
	sampling_tp.h

liblttng_ust_sampling_la_LIBADD = \
	-L$(top_builddir)/liblttng-ust/.libs \
	-llttng-ust \
	-lunwind

if LTTNG_UST_BUILD_WITH_LIBDL
liblttng_ust_sampling_la_LIBADD += -ldl
endif
if LTTNG_UST_BUILD_WITH_LIBC_DL
liblttng_ust_sampling_la_LIBADD += -lc
endif

noinst_SCRIPTS = run
EXTRA_DIST = run
