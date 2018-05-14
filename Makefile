MIQ=make-it-quick/

SUBDIRS=src

AUTOSTART=	spice-streaming.deskop
SRC_AUTOSTART=	src/$(AUTOSTART)
XDG_AUTOSTART=	$(SYSCONFIG)xdg/autostart/$(AUTOSTART)
GDM_AUTOSTART=	$(PREFIX_SHARE)gdm/greeter/autostart/$(AUTOSTART)

include $(MIQ)rules.mk
$(MIQ)rules.mk:
	git clone http://github.com/c3d/make-it-quick

install: $(XDG_AUTOSTART) $(GDM_AUTOSTART)
$(XDG_AUTOSTART) $(GDM_AUTOSTART): $(SRC_AUTOSTART)
	$(PRINT_INSTALL) $< $@
