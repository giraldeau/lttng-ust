AM_CPPFLAGS = -I$(top_srcdir)/include -I$(top_builddir)/include -Wsystem-headers

noinst_PROGRAMS = dynseq
dynseq_SOURCES = dynseq.c tp.h
dynseq_LDADD = $(top_builddir)/liblttng-ust/liblttng-ust.la
dynseq_CFLAGS = -Werror=old-style-definition

if LTTNG_UST_BUILD_WITH_LIBDL
dynseq_LDADD += -ldl
endif
if LTTNG_UST_BUILD_WITH_LIBC_DL
dynseq_LDADD += -lc
endif
