
EE_BIN = mass_example.elf
EE_OBJS = mass_example.o mass_rpc.o

EE_LIBS = -lpad


all: $(EE_BIN)

run: $(EE_BIN)
	ps2exec -p2 $(EE_BIN)

include ../../../Makefile.pref
include ../../../Makefile.eeglobal
