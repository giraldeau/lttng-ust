AM_CPPFLAGS = -I$(top_srcdir)/include -I$(top_builddir)/include -Wsystem-headers

noinst_PROGRAMS = dynseq dynseq-dbg
dynseq_SOURCES = dynseq.c tp.h
dynseq_LDADD = $(top_builddir)/liblttng-ust/liblttng-ust.la
dynseq_CFLAGS = -Werror=old-style-definition

dynseq_dbg_SOURCES = dynseq.c.i.c
dynseq_dbg_LDADD = $(top_builddir)/liblttng-ust/liblttng-ust.la
dynseq_dbg_CFLAGS = -Werror=old-style-definition

.PHONY: preproc
preproc: dynseq.c
	$(CC) $(AM_CPPFLAGS) -I. -c -o $<.i.tmp $< -E
	cat $<.i.tmp | grep -v "# " > $<.i.c

if LTTNG_UST_BUILD_WITH_LIBDL
dynseq_LDADD += -ldl
endif
if LTTNG_UST_BUILD_WITH_LIBC_DL
dynseq_LDADD += -lc
endif
