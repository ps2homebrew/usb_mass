CC=d:/gcc/bin/g++
CFLAGS= -s

#uncomment to enable write support
WRITE_SUPPORT=1


OBJECTS1=fat_test.o fat_driver.o scache.o vdisk.o
OBJECTS2=obj/fat_test.o obj/fat_driver.o obj/scache.o obj/vdisk.o 

ifdef WRITE_SUPPORT
CFLAGS += -DWRITE_SUPPORT
OBJECTS1 += fat_write.o
OBJECTS2 += obj/fat_write.o 
endif


ALL: fat_test.exe 

clean:
	rm obj/*.o

fat_test.exe: $(OBJECTS1)
	$(CC) $(CFLAGS) -ofat_test.exe $(OBJECTS2)


.c.o:
	$(CC) $(CFLAGS) -c $(<) -o obj/$*.o

