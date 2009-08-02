CC=gcc
CFLAGS=-g3 -I../iop

#uncomment to enable write support
WRITE_SUPPORT=1

OBJECTS=fat_test.o fat_driver.o scache.o vdisk.o

ifdef WRITE_SUPPORT
CFLAGS += -DWRITE_SUPPORT
OBJECTS += fat_write.o
endif

all: fat_test.exe

clean:
	rm -f *.o

fat_test: $(OBJECTS)
   $(CC) $(CFLAGS) -o $@ $^


%.o: %.c
   $(CC) $(CFLAGS) -c $^ -o $@

fat_driver.c: ../iop/fat_driver.c
   ln -s $^ $@

scache.c: ../iop/scache.c
   ln -s $^ $@

fat_write.c: ../iop/fat_write.c
   ln -s $^ $@
