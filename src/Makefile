TOP=../
MIQ=$(TOP)make-it-quick/

VARIANTS=	agent plugins lib

PRODUCTS=	$(PRODUCTS_$(VARIANT))
PRODUCTS_agent=	spice-streaming-agent.exe
PRODUCTS_plugins=$(HAVE_GST_GST_H:%=gst-plugin.dll)
PRODUCTS_lib=	spice-streaming-agent-utils.lib

PKGCONFIGS=	libjpeg							\
		gstreamer-1.0?						\
		gstreamer-app-1.0?					\
		x11

ifndef SPICE_PROTOCOL
PKGCONFIGS+=	spice-protocol
endif

CONFIG=		<gst/gst.h>						\
		<xf86drm.h>						\
		libpthread						\
		libdl							\
		libX11							\
		libXfixes						\
		libXrandr

SOURCES=	$(SOURCES_$(VARIANT))
SOURCES_agent=	spice-streaming-agent.cpp 				\
		concrete-agent.cpp					\
		cursor-updater.cpp 					\
		frame-log.cpp	 					\
		display-info.cpp					\
		x11-display-info.cpp					\
		mjpeg-fallback.cpp					\
		jpeg.cpp						\
		stream-port.cpp						\
		utils.cpp						\
		hexdump.c

SOURCES_lib=	display-info.cpp					\
		x11-display-info.cpp					\
		hexdump.c						\
		utils.cpp

SOURCES_plugins=							\
		$(HAVE_GST_GST_H:%=gst-plugin.cpp)			\

INCLUDES=	.							\
		../include						\
		$(SPICE_PROTOCOL)

DEFINES=	$(DEFINES_$(VARIANT))					\
		PLUGINSDIR=\"$(PREFIX_LIB)spice-streaming-agent\"	\
		VERSION=\"$(PACKAGE_VERSION)\"

DEFINES_agent=	SPICE_STREAMING_AGENT_PROGRAM

include $(MIQ)rules.mk
$(MIQ)rules.mk:
	cd $(TOP) && make $(TARGET)
