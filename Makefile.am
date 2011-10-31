ACLOCAL_AMFLAGS = -I m4

aiincludedir = ${includedir}/atomic-install

lib_LTLIBRARIES = lib/libai-copy.la lib/libai-journal.la
aiinclude_HEADERS = lib/copy.h lib/journal.h

lib_libai_copy_la_SOURCES = lib/copy.c lib/copy.h
lib_libai_journal_la_SOURCES = lib/journal.c lib/journal.h

noinst_PROGRAMS = util/ai-cp util/ai-journal-create

util_ai_cp_SOURCES = util/ai-cp.c
util_ai_cp_LDADD = lib/libai-copy.la

util_ai_journal_create_SOURCES = util/ai-journal-create.c
util_ai_journal_create_LDADD = lib/libai-journal.la

EXTRA_DIST = NEWS
NEWS: configure.ac Makefile.am
	git for-each-ref refs/tags --sort '-*committerdate' \
		--format '# %(tag) (%(*committerdate:short))%0a%(contents:body)' \
		> NEWS
