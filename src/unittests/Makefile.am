NULL =

AM_CPPFLAGS = \
	-DRH_TOP_SRCDIR=\"$(abs_top_srcdir)\" \
	-I$(top_srcdir)/include \
	-I$(top_srcdir)/src \
	-I$(top_srcdir)/src/unittests \
	$(DRM_CFLAGS) \
	$(SPICE_PROTOCOL_CFLAGS) \
	$(NULL)

AM_CFLAGS = \
	$(VISIBILITY_HIDDEN_CFLAGS) \
	$(WARN_CFLAGS) \
	$(NULL)

AM_CXXFLAGS = \
	$(WARN_CXXFLAGS) \
	$(NULL)

check_PROGRAMS = \
	hexdump \
	test-mjpeg-fallback \
	test-stream-port \
	$(NULL)

TESTS = \
	test-hexdump.sh \
	test-mjpeg-fallback \
	test-stream-port \
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

test_mjpeg_fallback_SOURCES = \
	test-mjpeg-fallback.cpp \
	../display-info.cpp \
	../jpeg.cpp \
	../mjpeg-fallback.cpp \
	../utils.cpp \
	../x11-display-info.cpp \
	spice-catch.hpp \
	$(NULL)

test_mjpeg_fallback_LDADD = \
	$(DRM_LIBS) \
	$(X11_LIBS) \
	$(JPEG_LIBS) \
	$(XRANDR_LIBS) \
	$(NULL)

test_stream_port_SOURCES = \
	test-stream-port.cpp \
	../stream-port.cpp \
	spice-catch.hpp \
	$(NULL)

EXTRA_DIST = \
	test-hexdump.sh \
	hexdump1.in \
	hexdump1.out \
	hexdump2.in \
	hexdump2.out \
	hexdump3.in \
	hexdump3.out
