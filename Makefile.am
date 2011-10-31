ACLOCAL_AMFLAGS = -I m4

aiincludedir = ${includedir}/atomic-install

lib_LTLIBRARIES = lib/libai-copy.la
aiinclude_HEADERS = lib/copy.h

lib_libai_copy_la_SOURCES = lib/copy.c lib/copy.h

noinst_PROGRAMS = util/ai-cp

util_ai_cp_SOURCES = util/ai-cp.c
util_ai_cp_LDADD = lib/libai-copy.la

EXTRA_DIST = NEWS
NEWS: configure.ac Makefile.am
	git for-each-ref refs/tags --sort '-*committerdate' \
		--format '# %(tag) (%(*committerdate:short))%0a%(contents:body)' \
		> NEWS
