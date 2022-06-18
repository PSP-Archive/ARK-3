TARGET = ARK3
C_OBJS = main.o imports.o
OBJS = $(C_OBJS)
all: $(TARGET).prx
CFLAGS = -Os -G0 -Wall

LIBS = -lpspdebug -lpspdisplay -lpspge -lpspctrl -lpspsdk -lc -lpspnet -lpspnet_inet -lpspnet_apctl -lpspnet_resolver -lpsputility -lpspuser -lpspkernel

CXXFLAGS = $(CFLAGS)
ASFLAGS = $(CFLAGS)
LDFLAGS = -L sysmem
BUILD_PRX = 1

PSP_EBOOT_TITLE += ARK-3

EXTRA_TARGETS = EBOOT.PBP

PSPSDK=$(shell psp-config --pspsdk-path)
include $(PSPSDK)/lib/build.mak
