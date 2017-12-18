NULL =
SUBDIRS = \
	include/spice-streaming-agent \
	src \
	$(NULL)

# this will start the program for each user session
xdgautostartdir = $(sysconfdir)/xdg/autostart
xdgautostart_DATA = $(srcdir)/data/spice-streaming.desktop

# this will start the program for the login session
gdmautostartdir = $(datadir)/gdm/greeter/autostart
gdmautostart_DATA = $(srcdir)/data/spice-streaming.desktop

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = spice-streaming-agent.pc

EXTRA_DIST = \
	spice-streaming-agent.spec \
	spice-streaming-agent.pc \
	data/spice-streaming.desktop \
	$(NULL)
