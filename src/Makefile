TOP=../../
BUILD=$(TOP)build/

PRODUCTS=	spice-streaming-agent.exe

PKGCONFIGS=	libjpeg

CONFIG=		libpthread						\
		libdl							\
		libX11							\
		libXfixes

SOURCES=	spice-streaming-agent.cpp 				\
		concrete-agent.cpp					\
		mjpeg-fallback.cpp					\
		jpeg.cpp						\
		hexdump.c

INCLUDES=	.							\
		../include						\
		$(TOP)protocol						\
		$(INCLUDES_$(OS_NAME))
INCLUDES_macosx=/usr/X11/include

DEFINES=	SPICE_STREAMING_AGENT_PROGRAM				\
		PLUGINSDIR=\"$(PREFIX_LIB)spice-streaming-agent\"	\

LDFLAGS_EXTRA=	$(LDFLAGS_$(OS_NAME))
LDFLAGS_macosx=	-L/usr/X11/lib

include $(BUILD)rules.mk
