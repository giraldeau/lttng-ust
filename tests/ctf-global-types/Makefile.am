AM_CPPFLAGS = -I$(top_srcdir)/include -I$(top_builddir)/include -Wsystem-headers

noinst_PROGRAMS = ctf-global-types
ctf_global_types_SOURCES = ctf-global-types.c tp.c ust_tests_ctf_global_types.h
ctf_global_types_LDADD = $(top_builddir)/liblttng-ust/liblttng-ust.la
ctf_global_types_CFLAGS = -Werror=old-style-definition

if LTTNG_UST_BUILD_WITH_LIBDL
ctf_global_types_LDADD += -ldl
endif
if LTTNG_UST_BUILD_WITH_LIBC_DL
ctf_global_types_LDADD += -lc
endif
