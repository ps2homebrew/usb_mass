/*
 * scache.c - USB Mass storage driver for PS2
 *
 * (C) 2004, Marek Olejnik (ole00@post.cz)
 * (C) 2004  Hermes (support for sector sizes from 512 to 4096 bytes)
 *
 * Sector cache 
 *
 * See the file LICENSE included with this distribution for licensing terms.
 */



#ifdef _PS2_
#include <tamtypes.h>
#define malloc(a)	AllocSysMemory(0,(a), NULL)
#define free(a)		FreeSysMemory((a))

#include "mass_stor.h"

#define DISK_INIT(a,b)		dummy_init((a),(b))
#define DISK_CLOSE		
// Modified Hermes
//always read 4096 bytes from sector (the rest bytes is stored in the cache)  
#define READ_SECTOR_4096(a, b)	mass_stor_readSector4096((a), (b)) 
#define READ_SECTOR(a, b)	mass_stor_readSector1((a), (b)) 

#else
#include <stdio.h>
#include <stdlib.h>
#include <memory.h>

#include "vdisk.h"

#define DISK_INIT(a,b)		vdisk_init((a), (b))
#define DISK_CLOSE		vdisk_close
#define READ_SECTOR(a, b)	vdisk_readSector8((a), (b))

#endif

//#define DEBUG 1
#include "mass_debug.h"


//size in quadsectors (ie in 2048 bytes)
#define CACHE_SIZE 6

typedef struct _cache_record {
	unsigned int sector;
	int tax;
} cache_record;


int sectorSize;
unsigned char* sbuf = NULL;		//sector content - the cache buffer
cache_record rec[CACHE_SIZE];	//cache info record

//statistical infos
unsigned int cacheAccess;
unsigned int cacheHits;


int dummy_init(unsigned char * p1, int i) {
	return 1;

}

void initRecords() {
	int i;
	for (i = 0; i < CACHE_SIZE; i++) {
		rec[i].sector = 0xFFFFFFF0;
		rec[i].tax = 0;
	}
}


/* search cache records for the sector number stored in cache */
int getIndexRead(unsigned int sector) {
	int i;
	int index =-1;
        int indexLimit=4096/sectorSize;  //added Hermes

	for (i = 0; i < CACHE_SIZE; i++) {
		if (sector >= rec[i].sector && sector < (rec[i].sector + indexLimit)) {
			if (rec[i].tax < 0) rec[i].tax = 0;
			rec[i].tax +=2;
			index = i;
		} 
		rec[i].tax--;     //apply tax penalty
	}
	if (index < 0) 
		return index;
	else 
		return ((index * indexLimit) + (sector - rec[index].sector));
}

/* select the best record where to store new sector */
int getIndexWrite(unsigned int sector) {
	int i;
	int minTax = 0x0FFFFFFF;
	int index = 0;
        int indexLimit=4096/sectorSize;  //added Hermes

	for (i = 0; i < CACHE_SIZE; i++) {
		if (rec[i].tax < minTax) {
			index = i;
			minTax = rec[i].tax;
		} 
	}
	rec[index].tax ++;
	rec[index].sector = sector;
	return index * indexLimit;
}


int scache_readSector(unsigned int sector, void** buf) {
	int index; //index is given in single sectors not octal sectors
	int ret;

	XPRINTF("cache: readSector = %i \n", sector);
	cacheAccess ++;
	index = getIndexRead(sector);
	XPRINTF("cache: indexRead=%i \n", index);
	if (index >= 0) { //sector found in cache
		cacheHits ++;
		*buf = sbuf + (index * sectorSize);
		XPRINTF("cache: hit and done reading sector \n");
		return sectorSize;
	}

	index = getIndexWrite(sector);
	XPRINTF("cache: indexWrite=%i \n", index);
	ret = READ_SECTOR_4096(sector, sbuf + (index * sectorSize));
	if (ret < 0) {
		return ret;
	}
	*buf = sbuf + (index * sectorSize);
	XPRINTF("cache: done reading physical sector \n");
	return sectorSize;
}


int scache_init(char * param, int sectSize) {
	//added by Hermes
	sectorSize = sectSize;

	if (sbuf == NULL) {
		XPRINTF("scache init! \n");
		XPRINTF("sectorSize: 0x%x\n",sectorSize);
		sbuf = (unsigned char*) malloc(4096 * CACHE_SIZE ); //allocate 4096 bytes per 1 cache record
		if (sbuf == NULL) {
			XPRINTF("Sector cache: can't alloate memory of size:%d \n", 4096 * CACHE_SIZE);		
			return -1;
		}
		XPRINTF("Sector cache: alocated memory at:%p of size:%d \n", sbuf, 4096 * CACHE_SIZE);
	} else {
		XPRINTF("scache flush! \n");
	}
	cacheAccess = 0;
	cacheHits = 0;
	initRecords();
	return DISK_INIT(param, sectSize);
}

void scache_getStat(unsigned int* access, unsigned int* hits) {
	*access = cacheAccess;
        *hits = cacheHits;
}

void scache_dumpRecords() {
	int i;

	printf("CACHE RECORDS\n");
	printf("-------------\n");
	for (i = 0; i < CACHE_SIZE; i++) {
		printf("%02i) sector=%08i  tax=%08i \n", i, rec[i].sector, rec[i].tax);
	}
	printf("access=%i  hits=%i \n", cacheAccess, cacheHits);
}

void scache_close() {

	if (sbuf != NULL) {
		free(sbuf);
		sbuf = NULL;
	}
	//DISK_CLOSE();
}


