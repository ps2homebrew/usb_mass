CC=C:/sdk/gcc/bin/g++

CFLAGS= -s

ALL: fat_test.exe 

clean:
	rm *.o

fat_test.exe: vdisk.o  scache.o fat_driver.o fat_test.o
	$(CC) $(CFLAGS) -ofat_test.exe fat_test.o fat_driver.o scache.o vdisk.o

fat_test.o: fat_test.c
	$(CC) $(CFLAGS) -c fat_test.c -o fat_test.o

fat_driver.o: fat_driver.c
	$(CC) $(CFLAGS) -c fat_driver.c -o fat_driver.o

vdisk.o: vdisk.c
	$(CC) $(CFLAGS) -c vdisk.c -o vdisk.o

scache.o: scache.c
	$(CC) $(CFLAGS) -c scache.c -o scache.o
