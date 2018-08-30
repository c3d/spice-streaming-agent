MIQ=make-it-quick/

SUBDIRS=src

AUTOSTART=	spice-streaming.desktop
SRC_AUTOSTART=	data/$(AUTOSTART)
XDG_AUTOSTART=	$(SYSCONFIG)xdg/autostart/$(AUTOSTART)
GDM_AUTOSTART=	$(PREFIX_SHARE)gdm/greeter/autostart/$(AUTOSTART)

HEADERS=	$(wildcard include/spice-streaming-agent/*.hpp)
PREFIX_HDR=	$(PREFIX)include/spice-streaming-agent/

include $(MIQ)rules.mk
$(MIQ)rules.mk:
	git clone http://github.com/c3d/make-it-quick

install: $(XDG_AUTOSTART) $(GDM_AUTOSTART)
$(XDG_AUTOSTART) $(GDM_AUTOSTART): $(SRC_AUTOSTART)
	$(PRINT_INSTALL) $< $@
$(SRC_AUTOSTART): $(SRC_AUTOSTART).in
	$(PRINT_GENERATE) sed -e 's|@BINDIR@|$(PREFIX_BIN)|g' < $< > $@
