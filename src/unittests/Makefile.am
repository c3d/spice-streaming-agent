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
	hexdump \
	$(NULL)

TESTS = \
	test-hexdump.sh \
	$(NULL)

noinst_PROGRAMS = \
	$(check_PROGRAMS) \
	$(NULL)

hexdump_SOURCES = \
	hexdump.c \
	$(NULL)

hexdump_LDADD = \
	../libstreaming-utils.a \
	$(NULL)

EXTRA_DIST = \
	test-hexdump.sh \
	hexdump1.in \
	hexdump1.out \
	hexdump2.in \
	hexdump2.out \
	hexdump3.in \
	hexdump3.out
