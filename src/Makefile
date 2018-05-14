TOP=../
MIQ=$(TOP)make-it-quick/

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
		stream-port.cpp						\
		error.cpp						\
		hexdump.c

INCLUDES=	.							\
		../include						\
		$(TOP)protocol						\
		$(INCLUDES_$(OS_NAME))
INCLUDES_macosx=/usr/X11/include

DEFINES=	SPICE_STREAMING_AGENT_PROGRAM				\
		PLUGINSDIR=\"$(PREFIX_LIB)spice-streaming-agent\"	\

LDFLAGS +=	$(LDFLAGS_$(OS_NAME))
LDFLAGS_macosx=	-L/usr/X11/lib

include $(MIQ)rules.mk
$(MIQ)rules.mk:
	cd $(TOP) && make $(TARGET)
