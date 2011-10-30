ACLOCAL_AMFLAGS = -I m4

noinst_LTLIBRARIES = lib/libcommon.la

lib_libcommon_la_SOURCES = lib/io.c lib/io.h
lib_libcommon_la_LDFLAGS = -static

EXTRA_DIST = NEWS
NEWS: configure.ac Makefile.am
	git for-each-ref refs/tags --sort '-*committerdate' \
		--format '# %(tag) (%(*committerdate:short))%0a%(contents:body)' \
		> NEWS
