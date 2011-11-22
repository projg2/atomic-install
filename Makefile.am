ACLOCAL_AMFLAGS = -I m4
DISTCHECK_CONFIGURE_FLAGS = --enable-gtk-doc

SUBDIRS = doc

aiincludedir = ${includedir}/atomic-install

lib_LTLIBRARIES = lib/libai-copy.la lib/libai-journal.la lib/libai-merge.la
aiinclude_HEADERS = lib/copy.h lib/journal.h lib/merge.h

lib_libai_copy_la_SOURCES = lib/copy.c lib/copy.h
lib_libai_copy_la_LIBADD = $(ATTR_LIBS)

lib_libai_journal_la_SOURCES = lib/journal.c lib/journal.h

lib_libai_merge_la_SOURCES = lib/merge.c lib/merge.h
lib_libai_merge_la_LIBADD = lib/libai-copy.la lib/libai-journal.la

bin_PROGRAMS = util/atomic-install

util_atomic_install_SOURCES = util/atomic-install.c
util_atomic_install_LDADD = lib/libai-merge.la

check_PROGRAMS = tests/copy/cp

TESTS = reg empty symlink inval-symlink pipe-named blk-dev chr-dev
.PHONY: $(TESTS)
TESTS_ENVIRONMENT = tests/copy/cp

TEST_INPUT_FILE = tests/copy/input.tmp
TEST_OUTPUT_FILE = tests/copy/output.tmp
TEST_ADD_FILE = tests/copy/additional-tmpfile

tests_copy_cp_SOURCES = tests/copy/cp.c
tests_copy_cp_CPPFLAGS = -I$(top_srcdir)/lib \
	-DINPUT_FILE=\"$(TEST_INPUT_FILE)\" -DOUTPUT_FILE=\"$(TEST_OUTPUT_FILE)\" \
	-DADDITIONAL_TMPFILE=\"additional-tmpfile\"
tests_copy_cp_LDADD = lib/libai-copy.la

symlink: $(TEST_ADD_FILE)
$(TEST_ADD_FILE):
	touch $@

CLEANFILES = $(TEST_INPUT_FILE) $(TEST_OUTPUT_FILE) $(TEST_ADD_FILE)

EXTRA_DIST = NEWS
NEWS: configure.ac Makefile.am
	git for-each-ref refs/tags --sort '-*committerdate' \
		--format '# %(tag) (%(*committerdate:short))%0a%(contents:body)' \
		> NEWS
