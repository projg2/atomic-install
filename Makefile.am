ACLOCAL_AMFLAGS = -I m4

bin_PROGRAMS = atomic-install

atomic_install_SOURCES = src/atomic-install.cxx \
	src/copy.cxx src/copy.hxx src/journal.cxx src/journal.hxx \
	src/merge.cxx src/merge.hxx src/exceptions.cxx src/exceptions.hxx
atomic_install_LDADD = $(ATTR_LIBS)

EXTRA_DIST = NEWS
NEWS: configure.ac Makefile.am
	git for-each-ref refs/tags --sort '-*committerdate' \
		--format '# %(tag) (%(*committerdate:short))%0a%(contents:body)' \
		> NEWS
