/*
 * fat_test.c - USB Mass storage driver for PS2
 *
 * (C) 2004, Marek Olejnik (ole00@post.cz)
 *
 *  virtual disk - reads the sectors from file
 *
 * See the file LICENSE included with this distribution for licensing terms.
 */


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h> //for linux compatibility

int fi;		//input file 
int fileSize;	//virtual disk size (given in bytes)
unsigned g_MaxLBA; /* Size of disk */ 

unsigned Size_Sector=512; // 


/*
initialze virtual disk - the param is the filename
*/

int vdisk_init(char * param, int sectorSize) {
	Size_Sector = sectorSize;
	printf ("Accessing file : %s \n", param);
#if defined (__CYGWIN__) || defined (__MINGW32__)
	fi = open(param, O_RDWR | O_BINARY , S_IWUSR);
#else
	fi = open(param, O_RDWR, S_IWUSR);
#endif

	if (fi >=0) {
		fileSize = lseek(fi, 0, SEEK_END);
		lseek(fi,0,SEEK_SET);
	} else {
		fileSize = 0;
	}
    g_MaxLBA = fileSize / Size_Sector; 
	return fi;
}

void vdisk_close() {
	
	if (fi >=0) {
		close(fi);
	} 
}


int vdisk_readSector(unsigned int sector,  void* buf) {
	int offset;
	int size;
        int count = 1;

	if (fi < 0) {
		return -1;
	}
	//printf("read sector:%i \n", sector);
	offset = (Size_Sector * sector);
	size = (Size_Sector * count);

	if ((offset + size) > fileSize) {
		size = fileSize - offset;
	}

	if (size <= 0) {
		return -2;
	}
		
	lseek(fi, offset, SEEK_SET);
	return read(fi, buf, size);
}

/* reads buffer of 4096 bytes */
int vdisk_readSector4096(unsigned int sector,  void* buf) {
	int offset;
	int size;

	if (fi < 0) {
		return -1;
	}
	//printf("read sector:%i \n", sector);
	offset = (Size_Sector * sector);
	size = 4096;

	if ((offset + size) > fileSize) {
		size = fileSize - offset;
	}

	if (size <= 0) {
		return -2;
	}
	
	lseek(fi, offset, SEEK_SET);
	return read(fi, buf, size);
}


/* writes buffer of 4096 bytes */
int vdisk_writeSector4096(unsigned int sector,  void* buf) {
	int offset;
	int size;

	if (fi < 0) {
		return -1;
	}
	//printf("write sector:%i \n", sector);
	offset = (Size_Sector * sector);
	size = 4096;

	if ((offset + size) > fileSize) {
		size = fileSize - offset;
	}

	if (size <= 0) {
		return -2;
	}
	
	lseek(fi, offset, SEEK_SET);
	return write(fi, buf, size);
}
