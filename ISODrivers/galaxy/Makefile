TARGET = galaxy
C_OBJS = main.o
OBJS = $(C_OBJS) imports.o $(PROVITA)/common/functions.o
all: $(TARGET).prx
INCDIR = $(PSPSDK)/include $(PROVITA)/common/include $(PROVITA)/kxploit/
CFLAGS = -Os -G0 -Wall
LDFLAGS = -mno-crt0 -nostartfiles -L. -L$(PROVITA)/libs -L$(PROVITA)/libs/colodebugger

ifdef DEBUG
CFLAGS += -DDEBUG=$(DEBUG)
endif

PSP_FW_VERSION = 660

BUILD_PRX=1
USE_KERNEL_LIBS=1
USE_KERNEL_LIBC=1

PRX_EXPORTS = export.exp
LIBS += -lpsid -lpspsystemctrl_kernel

include $(PROVITA)/common/make/global.mak
PSPSDK = $(shell psp-config --pspsdk-path)
include $(PSPSDK)/lib/build.mak
include $(PROVITA)/common/make/beauty.mak
