ACLOCAL_AMFLAGS = -I m4

bin_PROGRAMS = atomic-install

atomic_install_SOURCES = src/atomic-install.c \
	src/copy.c src/copy.h src/journal.c src/journal.h \
	src/merge.c src/merge.h
atomic_install_LDADD = $(ATTR_LIBS)

EXTRA_DIST = NEWS
NEWS: configure.ac Makefile.am
	git for-each-ref refs/tags --sort '-*committerdate' \
		--format '# %(tag) (%(*committerdate:short))%0a%(contents:body)' \
		> NEWS
