ACLOCAL_AMFLAGS = -I m4

bin_PROGRAMS = util/atomic-install

util_atomic_install_SOURCES = util/atomic-install.c \
	lib/copy.c lib/copy.h lib/journal.c lib/journal.h \
	lib/merge.c lib/merge.h
util_atomic_install_LDADD = $(ATTR_LIBS)

EXTRA_DIST = NEWS
NEWS: configure.ac Makefile.am
	git for-each-ref refs/tags --sort '-*committerdate' \
		--format '# %(tag) (%(*committerdate:short))%0a%(contents:body)' \
		> NEWS
