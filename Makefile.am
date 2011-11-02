ACLOCAL_AMFLAGS = -I m4
DISTCHECK_CONFIGURE_FLAGS = --enable-gtk-doc

SUBDIRS = doc

aiincludedir = ${includedir}/atomic-install

lib_LTLIBRARIES = lib/libai-copy.la lib/libai-journal.la lib/libai-merge.la
aiinclude_HEADERS = lib/copy.h lib/journal.h lib/merge.h

lib_libai_copy_la_SOURCES = lib/copy.c lib/copy.h
lib_libai_journal_la_SOURCES = lib/journal.c lib/journal.h

lib_libai_merge_la_SOURCES = lib/merge.c lib/merge.h
lib_libai_merge_la_LIBADD = lib/libai-copy.la lib/libai-journal.la

noinst_PROGRAMS = util/ai-cp util/ai-journal
bin_PROGRAMS = util/atomic-install

util_ai_cp_SOURCES = util/ai-cp.c
util_ai_cp_LDADD = lib/libai-copy.la

util_ai_journal_SOURCES = util/ai-journal.c
util_ai_journal_LDADD = lib/libai-journal.la

util_atomic_install_SOURCES = util/atomic-install.c
util_atomic_install_LDADD = lib/libai-merge.la

EXTRA_DIST = NEWS
NEWS: configure.ac Makefile.am
	git for-each-ref refs/tags --sort '-*committerdate' \
		--format '# %(tag) (%(*committerdate:short))%0a%(contents:body)' \
		> NEWS
