/*
 * scache.c - USB Mass storage driver for PS2
 *
 * (C) 2004, Marek Olejnik (ole00@post.cz)
 *
 * Sector cache 
 *
 * See the file LICENSE included with this distribution for licensing terms.
 */

#ifdef _PS2_
#include <kernel.h>
#define malloc(a)	AllocSysMemory(0,(a), NULL)
#define free(a)		FreeSysMemory((a))
#define MEMCPY(a,b,c) memcpy((a),(b),(c))

#include "mass_stor.h"

#define DISK_INIT(a,b)		dummy_init((a),(b))
#define DISK_CLOSE		
//allways read 8 sectors instead of 1 (the rest 7 sectors are precached)
#define READ_SECTOR(a, b)	mass_stor_readSector8((a), (b)) 

#else
#include <stdio.h>
#include <stdlib.h>
#include <memory.h>
#define MEMCPY(a,b,c) memcpy((a),(b),(c))

#include "vdisk.h"

#define DISK_INIT(a,b)		vdisk_init((a), (b))
#define DISK_CLOSE		vdisk_close
#define READ_SECTOR(a, b)	vdisk_readSector8((a), (b))

#endif

//#define DEBUG 1
#include "debug.h"




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

void ps2_memcpy(void* dst, void* src, int size) {
	unsigned char* d = (unsigned char*) dst;
	unsigned char* s = (unsigned char*) src;
	XPRINTF("ps2_memcpy dst=%p src=%p size=%d \n", dst, src, size);
	while (size--) {
		*d = *s;
		d++;
		s++;
	}
	XPRINTF("memcpy done.\n");
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

	for (i = 0; i < CACHE_SIZE; i++) {
		if (sector >= rec[i].sector && sector < (rec[i].sector + 8)) {
			if (rec[i].tax < 0) rec[i].tax = 0;
			rec[i].tax +=2;
			index = i;
		} 
		rec[i].tax--;     //apply tax penalty
	}
	if (index < 0) 
		return index;
	else 
		return ((index<<3) + (sector - rec[index].sector));
}

/* select the best record where to store new sector */
int getIndexWrite(unsigned int sector) {
	int i;
	int minTax = 0x0FFFFFFF;
	int index = 0;

	for (i = 0; i < CACHE_SIZE; i++) {
		if (rec[i].tax < minTax) {
			index = i;
			minTax = rec[i].tax;
		} 
	}
	rec[index].tax ++;
	rec[index].sector = sector;
	return index << 3;
}


int scache_readSector(unsigned int sector, void** buf) {
	int index; //index is given in single sectors not octal sectors
	int ret;

	XPRINTF("cache: readSector = %i \n", sector);
	cacheAccess ++;
	index = getIndexRead(sector);
	XPRINTF("cache: indexRead=%i \n", index);
	if (index > 0) { //sector found in cache
		cacheHits ++;
		//MEMCPY(buf, sbuf + (index * sectorSize), sectorSize);
		*buf = sbuf + (index * sectorSize);
		XPRINTF("cache: hit and done reading sector \n");
		return sectorSize;
	}

	index = getIndexWrite(sector);
	XPRINTF("cache: indexWrite=%i \n", index);
	ret = READ_SECTOR(sector, sbuf + (index * sectorSize));
	if (ret < 0) {
		return ret;
	}
	//MEMCPY(buf, sbuf + (index * sectorSize), sectorSize);
	*buf = sbuf + (index * sectorSize);
	XPRINTF("cache: done reading physical sector \n");
	return sectorSize;
}


int scache_init(char * param, int sectSize) {
	sectorSize = sectSize;

	if (sbuf == NULL) {
		XPRINTF("scache init! \n");
		sbuf = (unsigned char*) malloc(sectorSize * CACHE_SIZE * 8 ); //allocate octal sectors
		if (sbuf == NULL) {
			XPRINTF("Sector cache: can't alloate memory of size:%d \n", sectorSize * CACHE_SIZE * 8);		
			return -1;
		}
		XPRINTF("Sector cache: alocated memory at:%p of size:%d \n", sbuf,  sectorSize * CACHE_SIZE * 8);
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
	}
	//DISK_CLOSE();
}


