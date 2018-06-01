TOP=../
MIQ=$(TOP)make-it-quick/

PRODUCTS=	spice-streaming-agent.exe

PKGCONFIGS=	libjpeg							\
		x11

ifndef SPICE_PROTOCOL
PKGCONFIGS+=	spice-protocol
endif

CONFIG=		libpthread						\
		libdl							\
		libX11							\
		libXfixes						\
		sigaction						\
		<regex.h>						\
		<sys/mman.h>


SOURCES=	spice-streaming-agent.cpp 				\
		concrete-agent.cpp					\
		cursor-updater.cpp 					\
		frame-log.cpp	 					\
		mjpeg-fallback.cpp					\
		jpeg.cpp						\
		stream-port.cpp						\
		error.cpp						\
		hexdump.c						\
		recorder/recorder.c					\
		recorder/recorder_ring.c

INCLUDES=	.							\
		../include						\
		$(SPICE_PROTOCOL)

DEFINES=	SPICE_STREAMING_AGENT_PROGRAM				\
		PLUGINSDIR=\"$(PREFIX_LIB)spice-streaming-agent\"	\
		VERSION=\"$(PACKAGE_VERSION)\"

include $(MIQ)rules.mk
$(MIQ)rules.mk:
	cd $(TOP) && make $(TARGET)

recorder/recorder.c recorder/recoder_ring.c:
	git submodule update --init
