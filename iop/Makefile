IOP_BIN = usb_mass.irx
IOP_OBJS = mass_stor.o fat_driver.o scache.o usb_mass.o
IOP_LDFLAGS = -Wl,-s
IOP_CFLAGS = -D_PS2_

all: $(IOP_BIN)

clean:
	rm -f *.elf *.o *.a *.irx

include $(PS2SDK)/samples/Makefile.pref
include $(PS2SDK)/samples/Makefile.iopglobal
