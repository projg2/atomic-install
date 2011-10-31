ACLOCAL_AMFLAGS = -I m4

aiincludedir = ${includedir}/atomic-install

lib_LTLIBRARIES = lib/libai-copy.la
aiinclude_HEADERS = lib/copy.h

lib_libai_copy_la_SOURCES = lib/copy.c lib/copy.h

EXTRA_DIST = NEWS
NEWS: configure.ac Makefile.am
	git for-each-ref refs/tags --sort '-*committerdate' \
		--format '# %(tag) (%(*committerdate:short))%0a%(contents:body)' \
		> NEWS
