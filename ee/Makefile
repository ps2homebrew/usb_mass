
EE_BIN = mass_example.elf
EE_OBJS = mass_example.o mass_rpc.o
EE_LIBS = -lc -ldebug -lpad

all: $(EE_BIN)

clean:
	rm -f *.elf *.o *.a

include $(PS2SDK)/samples/Makefile.pref
include $(PS2SDK)/samples/Makefile.eeglobal
