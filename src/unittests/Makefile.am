NULL =

AM_CPPFLAGS = \
	-DRH_TOP_SRCDIR=\"$(abs_top_srcdir)\" \
	-I$(top_srcdir)/src \
	-I$(top_srcdir)/src/unittests \
	$(SPICE_PROTOCOL_CFLAGS) \
	$(NULL)

AM_CFLAGS = \
	$(VISIBILITY_HIDDEN_CFLAGS) \
	$(WARN_CFLAGS) \
	$(NULL)

check_PROGRAMS = \
	test-hexdump \
	$(NULL)

TESTS = \
	hexdump.sh \
	$(NULL)

noinst_PROGRAMS = \
	$(check_PROGRAMS) \
	$(NULL)

test_hexdump_SOURCES = \
	test-hexdump.c \
	$(NULL)

test_hexdump_LDADD = \
	../libstreaming-utils.a \
	$(NULL)

EXTRA_DIST = hexdump.sh hexdump*.in hexdump*.out
