/*
 * fat_driver.c - USB Mass storage driver for PS2
 *
 * (C) 2004, Marek Olejnik (ole00@post.cz)
 *
 * FAT filesystem layer
 *
 * See the file LICENSE included with this distribution for licensing terms.
 */


/*
  This layer should be "independent". Just supply a function that reads sectors
  from media (READ_SECTOR) and use fs_xxxx functions for file access. 
*/

#include <stdio.h>

#ifndef _PS2_
#include <stdlib.h>
#include <memory.h>
#endif

#include "scache.h"
#include "fat_driver.h"

//#define DEBUG 1

#include "debug.h"

#define MEMCPY(a,b,c) memcpy((a),(b),(c))


#define MAX_DIR_CLUSTER 256
#define SECTOR_SIZE 512

#define DISK_INIT(a,b)		scache_init((a), (b))
#define DISK_CLOSE		scache_close
//#define READ_SECTOR(a, b)	scache_readSector((a), (b))
#define READ_SECTOR(a, b)	scache_readSector((a), (void **)&b)

#define MAX_FILES 16
fs_rec  fsRec[MAX_FILES]; //file info record
fat_dir fsDir[MAX_FILES]; //directory entry

int	mounted;	//disk mounted=1 not monuted=0 
fat_part partTable;	//partition master record
fat_bpb  partBpb;	//partition bios parameter block

static unsigned int cbuf[MAX_DIR_CLUSTER]; //cluster index buffer
static unsigned char* sbuf; //sector buffer
static unsigned char tbuf[SECTOR_SIZE + 4]; //temporary buffer

static int workPartition;

unsigned int  direntryCluster; //the directory cluster requested by getFirstDirentry
int direntryIndex; //index of the directory children

unsigned int  lastChainCluster;
int lastChainResult;

int getI32(unsigned char* buf) {
	return (buf[0]  + (buf[1] <<8) + (buf[2] << 16) + (buf[3] << 24));
}
int getI32_2(unsigned char* buf1, unsigned char* buf2) {
	return (buf1[0]  + (buf1[1] <<8) + (buf2[0] << 16) + (buf2[1] << 24));
}

int getI16(unsigned char* buf) {
	return (buf[0] + (buf[1] <<8) );
}

int strEqual(unsigned char *s1, unsigned char* s2) {
    unsigned char u1, u2;
    for (;;) {
		u1 = *s1++;
		u2 = *s2++;
		if (u1 >64 && u1 < 91)  u1+=32;
		if (u2 >64 && u2 < 91)  u2+=32;
        
		if (u1 != u2) {
			return -1;
		}
		if (u1 == '\0') {
		    return 0;
		}
    }
}

unsigned int fat_cluster2sector1216(fat_bpb* bpb, unsigned int cluster) {
	return  bpb->rootDirStart + (bpb->rootSize / 16)  + (bpb->clusterSize * (cluster-2));
}
unsigned int fat_cluster2sector(fat_bpb* bpb, unsigned int cluster) {
/*
	switch(bpb->fatType) {
		case FAT12: return cluster2sector12(bpb, cluster);
		default:    return cluster2sector1632(bpb, cluster);
	}
*/
	return fat_cluster2sector1216(bpb, cluster);

}

void fat_getPartitionRecord(part_raw_record* raw, part_record* rec) {
	rec->sid = raw->sid;
	rec->start = getI32(raw->startLBA);
	rec->count = getI32(raw->size);
}

unsigned int fat_getClusterRecord12(unsigned char* buf, int type) {
	if (type) { //1
		return ((buf[1]<< 4) + (buf[0] >>4));
	} else { // 0
		return (((buf[1] & 0x0F) << 8) + buf[0]);
	}
}
// Get Cluster chain into <buf> buffer
// returns:
// 0    :if buf is full (bufSize entries) and more chain entries exist
// 1-n  :number of filled entries of the buf
// -1   :error

//for fat12 
/* fat12 cluster records can overlap the edge of the sector so we need to detect and maintain
   these cases
*/
int fat_getClusterChain12(fat_bpb* bpb, unsigned int cluster, unsigned int* buf, int bufSize, int start) {
	int ret;
	int i;
	int recordOffset;
	int sectorSpan;
	int fatSector;
	int cont;
	int lastFatSector;
		
	cont = 1;
	lastFatSector = -1;
	i = 0;
	if (start) {
		buf[i] = cluster; //strore first cluster
		i++;
	}
	while(i < bufSize && cont) {
		recordOffset = (cluster * 3) / 2; //offset of the cluster record (in bytes) from the FAT start
		fatSector = recordOffset / bpb->sectorSize;
		sectorSpan = 0;
		if ((recordOffset % bpb->sectorSize) == (bpb->sectorSize - 1)) {
			sectorSpan = 1;
		}
		if (lastFatSector !=  fatSector || sectorSpan) {
				ret = READ_SECTOR(bpb->partStart + bpb->resSectors + fatSector, sbuf); 
				if (ret < 0) {
					printf("FAT driver:Read partition fat12 sector failed! sector=%i! \n", bpb->partStart + bpb->resSectors + fatSector );
					return -1;
				}
				lastFatSector = fatSector;

				if (sectorSpan) {
					tbuf[bpb->sectorSize-2] = sbuf[bpb->sectorSize - 2]; 
					tbuf[bpb->sectorSize-1] = sbuf[bpb->sectorSize - 1]; 
					ret = READ_SECTOR(bpb->partStart + bpb->resSectors + fatSector + 1, sbuf); 
					if (ret < 0) {
						printf("FAT driver:Read partition fat12 sector failed sector=%i! \n", bpb->partStart + bpb->resSectors + fatSector + 1);
						return -1;
					}
					tbuf[bpb->sectorSize ] = sbuf[0]; 
					tbuf[bpb->sectorSize +1] = sbuf[1]; 
				}
		}
		if (sectorSpan) { // use tbuf as source buffer 
			cluster = fat_getClusterRecord12(tbuf + (recordOffset % bpb->sectorSize), cluster % 2);
		} else { // use sector buffer as source buffer
			cluster = fat_getClusterRecord12(sbuf + (recordOffset % bpb->sectorSize), cluster % 2);
		}
		if (cluster >= 0xFF8) {
			cont = 0; //continue = false
		} else {
			buf[i] = cluster;
			i++;
		}
	}
	if (cont) {
		return 0;
	} else {
		return i;
	}
}


//for fat16 
int fat_getClusterChain16(fat_bpb* bpb, unsigned int cluster, unsigned int* buf, int bufSize, int start) {
	int ret;
	int i;
	int indexCount;
	int fatSector;
	int cont;
	int lastFatSector;
		
	cont = 1;
	indexCount = bpb->sectorSize / 2; //FAT16->2, FAT32->4
	lastFatSector = -1;
	i = 0;
	if (start) {
		buf[i] = cluster; //strore first cluster
		i++;
	}
	while(i < bufSize && cont) {
		fatSector = cluster / indexCount;
		if (lastFatSector !=  fatSector) {
				ret = READ_SECTOR(bpb->partStart + bpb->resSectors + fatSector,  sbuf); 
				if (ret < 0) {
					printf("FAT driver:Read partition fat16 sector failed! sector=%i! \n", bpb->partStart + bpb->resSectors + fatSector );
					return -1;
				}

				lastFatSector = fatSector;
		}
		cluster = getI16(sbuf + ((cluster % indexCount) * 2));
		if (cluster >= 0xFFF8) {
			cont = 0; //continue = false
		} else {
			buf[i] = cluster;
			i++;
		}
	}
	if (cont) {
		return 0;
	} else {
		return i;
	}
}

//for fat32
int fat_getClusterChain32(fat_bpb* bpb, unsigned int cluster, unsigned int* buf, int bufSize, int start) {
	int ret;
	int i;
	int indexCount;
	int fatSector;
	int cont;
	int lastFatSector;
		
	cont = 1;
	indexCount = bpb->sectorSize / 4; //FAT16->2, FAT32->4
	lastFatSector = -1;
	i = 0;
	if (start) {
		buf[i] = cluster; //strore first cluster
		i++;
	}
	while(i < bufSize && cont) {
		fatSector = cluster / indexCount;
		if (lastFatSector !=  fatSector) {
				ret = READ_SECTOR(bpb->partStart + bpb->resSectors + fatSector,  sbuf); 
				if (ret < 0) {
					printf("FAT driver: Read partition fat32 sector failed sector=%i! \n", bpb->partStart + bpb->resSectors + fatSector );
					return -1;
				}

				lastFatSector = fatSector;
		}
		cluster = getI32(sbuf + ((cluster % indexCount) * 4));
		if (cluster >= 0xFFFFFFF8) {
			cont = 0; //continue = false
		} else {
			buf[i] = cluster;
			i++;
		}
	}
	if (cont) {
		return 0;
	} else {
		return i;
	}
}


int fat_getClusterChain(fat_bpb* bpb, unsigned int cluster, unsigned int* buf, int bufSize, int start) {

	if (cluster == lastChainCluster) {
		return lastChainResult;
	}

	switch (bpb->fatType) {
		case FAT12: lastChainResult = fat_getClusterChain12(bpb, cluster, buf, bufSize, start); break;
		case FAT16: lastChainResult = fat_getClusterChain16(bpb, cluster, buf, bufSize, start); break;
		case FAT32: lastChainResult = fat_getClusterChain32(bpb, cluster, buf, bufSize, start); break;
	}
	lastChainCluster = cluster;
	return lastChainResult;
}


void fat_getPartitionTable(fat_part* part) {
	part_raw_record* part_raw;
	int i;
	int ret;
	workPartition = 0;

	ret = READ_SECTOR(0 , sbuf); //read sector 0 - Disk MBR

	if (ret < 0) {
		printf("FAT driver: Read sector 0 failed ! \n");
		return;
	}

	/* read 4 partition records */
	for (i= 0; i < 4; i++) {
		part_raw = (part_raw_record*) (sbuf + 0x01BE + (i* 16));
		fat_getPartitionRecord(part_raw, &part->record[i]);
		if (part->record[i].sid == 6 || part->record[i].sid == 4) {
			workPartition = i;			
		}
	}
}

void fat_determineFatType(fat_bpb* bpb) {
	int sector;
	int clusterCount;

	//get sector of cluster 0
	sector = fat_cluster2sector(bpb, 0);
	//remove partition start sector to get BR+FAT+ROOT_DIR sector count
	sector -= bpb->partStart;
	sector = bpb->sectorCount - sector;
	clusterCount = sector / bpb->clusterSize;
	//printf("Data cluster count = %i \n", clusterCount);

	if (clusterCount < 4085) {
		bpb->fatType = FAT12;
	} else
	if (clusterCount < 65525) {
		bpb->fatType = FAT16;
	} else {
		bpb->fatType = FAT32;
	}
}

void fat_getPartitionBootSector(part_record* part_rec, fat_bpb* bpb) {
	fat_raw_bpb* bpb_raw;
	int ret;

	ret = READ_SECTOR(part_rec->start, sbuf); //read partition boot sector (first sector on partition)

	if (ret < 0) {
		printf("FAT driver: Read partition boot sector failed sector=%i! \n", part_rec->start);
		return;
	}

	bpb_raw = (fat_raw_bpb*) sbuf;
	
	bpb->sectorSize	= getI16(bpb_raw->sectorSize);
	bpb->clusterSize = bpb_raw->clusterSize;
	bpb->resSectors = getI16(bpb_raw->resSectors);
	bpb->fatCount = bpb_raw->fatCount;
	bpb->rootSize = getI16(bpb_raw->rootSize);
	bpb->fatSize = getI16(bpb_raw->fatSize);
	bpb->trackSize = getI16(bpb_raw->trackSize);
	bpb->headCount = getI16(bpb_raw->headCount);
	bpb->hiddenCount = getI32(bpb_raw->hiddenCountL);
	bpb->sectorCount = getI16(bpb_raw->sectorCountO);
	if (bpb->sectorCount == 0) {
		bpb->sectorCount = getI32(bpb_raw->sectorCount); // large partition
	}
	bpb->partStart = part_rec->start;
	bpb->rootDirStart = part_rec->start + (bpb->fatCount * bpb->fatSize) + bpb->resSectors;
	for (ret = 0; ret < 8; ret++) {
		bpb->fatId[ret] = bpb_raw->fatId[ret];
	}
	bpb->fatId[ret] = 0;

	fat_determineFatType(bpb);

}


int fat_getDirentry(fat_direntry_sfn* dsfn, fat_direntry_lfn* dlfn, fat_direntry* dir ) {
	int i, j;
	int offset;
	int cont;

	//detect last entry - all zeros
	if (dsfn->name[0] == 0 && dsfn->name[1] == 0) {
		return 0;
	}
	//detect deleted entry - it will be ignored
	if (dsfn->name[0] == 0xE5) {
		return 3;
	}

	//detect long filename
	if (dlfn->rshv == 0x0F && dlfn->reserved1 == 0x00 && dlfn->reserved2[0] == 0x00) {
		//long filename - almost whole direntry is unicode string - extract it
		offset = dlfn->entrySeq & 0x3f;
		offset--;
		offset = offset * 13;
		//name - 1st part
		cont = 1;
		for (i = 0; i < 10 && cont; i+=2) {
			if (dlfn->name1[i]==0 && dlfn->name1[i+1] == 0) {
				dir->name[offset] = 0; //terminate
				cont = 0; //stop 
			} else {
				dir->name[offset] = dlfn->name1[i];
				offset++;
			}
		}
		//name - 2nd part
		for (i = 0; i < 12 && cont; i+=2) {
			if (dlfn->name2[i]==0 && dlfn->name2[i+1] == 0) {
				dir->name[offset] = 0; //terminate
				cont = 0; //stop 
			} else {
				dir->name[offset] = dlfn->name2[i];
				offset++;
			}
		}
		//name - 3rd part
		for (i = 0; i < 4 && cont; i+=2) {
			if (dlfn->name3[i]==0 && dlfn->name3[i+1] == 0) {
				dir->name[offset] = 0; //terminate
				cont = 0; //stop 
			} else {
				dir->name[offset] = dlfn->name3[i];
				offset++;
			}
		}
		if ((dlfn->entrySeq & 0x40)) { //terminate string flag
			dir->name[offset] = 0;
		}
		return 2;
	} else {
		//short filename
		//copy name
		for (i = 0; i < 8 && dsfn->name[i]!= 32; i++) {
			dir->sname[i] = dsfn->name[i];
		}
		for (j=0; j < 3 && dsfn->ext[j] != 32; j++) {
			if (j == 0) {
				dir->sname[i] = '.';
				i++;
			}
			dir->sname[i+j] = dsfn->ext[j];
		}
		dir->sname[i+j] = 0; //terminate
		dir->attr = dsfn->attr;
		dir->size = getI32(dsfn->size);
		dir->cluster = getI32_2(dsfn->clusterL, dsfn->clusterH);
		return 1;
	}

}


//Set chain info (cluster/offset) cache
void fat_setFatDirChain(fat_bpb* bpb, fat_dir* fatDir) {
	int i,j;
	int index;
	int chainSize;
	int nextChain; 
	int clusterChainStart ;
	unsigned int fileCluster;
	int fileSize;
	int blockSize;


	XPRINTF("FAT driver: reading cluster chain  \n");
	fileCluster = fatDir->chain[0].cluster;

	if (fileCluster < 2) {
		XPRINTF("early exit... \n");
		return;
	}

	fileSize = fatDir->size;
	blockSize = fileSize / DIR_CHAIN_SIZE;

	nextChain = 1;
	clusterChainStart = 1;
	j = 1;
	fileSize = 0;
	index = 0;

	while (nextChain) {
		chainSize = fat_getClusterChain(bpb, fileCluster, cbuf, MAX_DIR_CLUSTER, clusterChainStart);
		clusterChainStart = 0;
		if (chainSize == 0) { //the chain is full, but more chain parts exist
			chainSize = MAX_DIR_CLUSTER;
			fileCluster = cbuf[MAX_DIR_CLUSTER - 1];
		}else { //chain fits in the chain buffer completely - no next chain exist
			nextChain = 0;
		}
#ifdef DEBUG
		fat_dumpClusterChain(cbuf, chainSize);
#endif

		//process the cluster chain (cbuf)
		for (i = 0; i < chainSize; i++) {
			fileSize += (bpb->clusterSize * bpb->sectorSize);
			while (fileSize > (j * blockSize) && j < DIR_CHAIN_SIZE) {
				fatDir->chain[j].cluster = cbuf[i];
				fatDir->chain[j].index = index;
				j++;
			}
			index++;
		}
	}

#ifdef DEBUG
	//debug
	printf("SEEK CLUSTER CHAIN CACHE fileSize=%i blockSize=%i \n", fatDir->size, blockSize);
	for (i = 0; i < DIR_CHAIN_SIZE; i++) {
		printf("index=%i cluster=%i offset= %i - %i start=%i \n", 
			fatDir->chain[i].index, fatDir->chain[i].cluster,
			fatDir->chain[i].index * bpb->clusterSize * bpb->sectorSize,
			(fatDir->chain[i].index+1) * bpb->clusterSize * bpb->sectorSize,
			i * blockSize);
	}
#endif /* debug */
	XPRINTF("FAT driver: read cluster chain  done!\n");


}


/* Set base attributes of direntry */
void fat_setFatDir(fat_bpb* bpb, fat_dir* fatDir, fat_direntry_sfn* dsfn, fat_direntry* dir, int getClusterInfo ) {
	int i;
	unsigned char* srcName;

	XPRINTF("setting fat dir...\n");
	srcName = dir->sname; 
	if (dir->name[0] != 0) { //long filename not empty
		srcName = dir->name;
	}
	//copy name
	for (i = 0; srcName[i] != 0; i++) fatDir->name[i] = srcName[i];
	fatDir->name[i] = 0; //terminate

	fatDir->attr = dsfn->attr;
	fatDir->size = dir->size;

	//Date: Day, Month, Year-low, Year-high
	fatDir->date[0] = (dsfn->dateWrite[0] & 0x1F);
	fatDir->date[1] = (dsfn->dateWrite[0] >> 5) + ((dsfn->dateWrite[1] & 0x01) << 3 );
	i = 1980 + (dsfn->dateWrite[1] >> 1);
	fatDir->date[2] = (i & 0xFF);
	fatDir->date[3] = ((i & 0xFF00)>> 8);

	//Time: Hours, Minutes, Seconds
	fatDir->time[0] = ((dsfn->timeWrite[1] & 0xF8) >> 3);
	fatDir->time[1] = ((dsfn->timeWrite[1] & 0x07) << 3) + ((dsfn->timeWrite[0] & 0xE0) >> 5);
	fatDir->time[2] = ((dsfn->timeWrite[0] & 0x1F) < 1);

	fatDir->chain[0].cluster = dir->cluster;
	fatDir->chain[0].index  = 0;
	if (getClusterInfo) {
		fat_setFatDirChain(bpb, fatDir);
	}
}

int fat_getDirentryStartCluster(fat_bpb* bpb, unsigned char* dirName, unsigned int* startCluster, fat_dir* fatDir) {
	fat_direntry_sfn* dsfn;
	fat_direntry_lfn* dlfn;
	fat_direntry dir;
	int i, j;
	int dirSector;
	int startSector;
	int cont;
	int ret;
	int dirPos;
	int clusterMod;
	int chainSize;

	cont = 1;
	XPRINTF("\n");
	XPRINTF("getting cluster for dir entry: %s \n", dirName);
	clusterMod = bpb->clusterSize - 1;
	//clear name strings
	dir.sname[0] = 0;
	dir.name[0] = 0;

	if (*startCluster == 0 && bpb->fatType < FAT32) { //Root directory
		startSector = bpb->rootDirStart;
		dirSector =  bpb->rootSize / (bpb->sectorSize / 32);
	} else { //other directory
		startSector = fat_cluster2sector(bpb, *startCluster);
		chainSize = fat_getClusterChain(bpb, *startCluster, cbuf, MAX_DIR_CLUSTER, 1);
		if (chainSize > 0) {
			dirSector = chainSize * bpb->clusterSize;
		} else {
			printf("FAT driver: Error getting cluster chain! startCluster=%i \n", *startCluster);
			return -1;
		}
#ifdef DEBUG
		fat_dumpClusterChain(cbuf, chainSize);
#endif /*debug*/
	}
	XPRINTF("dirCluster=%i startSector=%i (%i) dirSector=%i \n", *startCluster, startSector, startSector * 512, dirSector);

	//go through first directory sector till the max number of directory sectors
	//or stop when no more direntries detected
	for (i = 0; i < dirSector && cont; i++) {
		ret = READ_SECTOR(startSector + i, sbuf); 
		if (ret < 0) {
			printf("FAT driver: read directory sector failed ! sector=%i\n", startSector + i);
			return -1;
		}
		XPRINTF("read sector ok, scanning sector for direntries...\n");
		
		//get correct sector from cluster chain buffer
		if ((*startCluster != 0) && (i % bpb->clusterSize == clusterMod)) {
			startSector = fat_cluster2sector(bpb, cbuf[(i / bpb->clusterSize) +  1]);
			startSector -= (i+1);
		}
		dirPos = 0;

		// go through start of the sector till the end of sector
		while (cont &&  dirPos < bpb->sectorSize) {
			dsfn = (fat_direntry_sfn*) (sbuf + dirPos);
			dlfn = (fat_direntry_lfn*) (sbuf + dirPos);
			cont = fat_getDirentry(dsfn, dlfn, &dir); //get single directory entry from sector buffer
			if (cont == 1) { //when short file name entry detected
				if (!(dir.attr & 0x08)) { //not volume label
					if ((strEqual(dir.sname, dirName) == 0) || 
						(strEqual(dir.name, dirName) == 0) ) {
							XPRINTF("found! %s\n", dir.name);
							if (fatDir != NULL) { //fill the directory properties
								fat_setFatDir(bpb, fatDir, dsfn, &dir, 1);
							}
							*startCluster = dir.cluster;
							XPRINTF("direntry %s found at cluster: %i \n", dirName, dir.cluster);
							return dir.attr; //returns file or directory attr
						}
				}
				//clear name strings
				dir.sname[0] = 0;
				dir.name[0] = 0;
			}
			dirPos += 32; //directory entry of size 32 bytes
		}
		
	}
	XPRINTF("direntry %s not found! \n", dirName);
	return -1;
}

// start cluster should be 0 - if we want to search from root directory
// otherwise the start cluster should be correct cluster of directory
int fat_getFileStartCluster(fat_bpb* bpb, char* fname, unsigned int* startCluster, fat_dir* fatDir) {
	unsigned char tmpName[257];
	int i;
	int offset;
	int cont;
	int ret;

	cont = 1;
	offset = 0;

/*
	//find the : character or directory separator
	for (i = 0; fname[i] != 0 && cont; i++) {
		if (fname[i] == ':' || fname[i]== '/' || fname[i]=='\\') { //invalid directory name - typically the disk identifier "disk0:/"
				cont = 0;
		}
	}
*/
	i=0;
	if (fname[i] == '/' || fname[i] == '\\' ) {
		i++;
	}

	for ( ; fname[i] !=0; i++) {
		if (fname[i] == '/' || fname[i] == '\\') { //directory separator
			tmpName[offset] = 0; //terminate string
			ret = fat_getDirentryStartCluster(bpb, tmpName, startCluster, NULL);
			if (ret < 0) {
				return ret;
			}
			offset = 0;
		} else{
			tmpName[offset] = fname[i];
			offset++;
		}
	}
	//and the final file
	tmpName[offset] = 0; //terminate string
	ret = fat_getDirentryStartCluster(bpb, tmpName, startCluster, fatDir);
	if (ret < 0) {
		return ret;
	}
	XPRINTF("file's startCluster found. Name=%s, cluster=%i \n", fname, *startCluster);
	return 1;
}

void fat_getClusterAtFilePos(fat_bpb* bpb, fat_dir* fatDir, unsigned int filePos, unsigned int* cluster, unsigned int* clusterPos) {
	int i;
	int blockSize;
	int j = (DIR_CHAIN_SIZE-1);

	blockSize = bpb->clusterSize * bpb->sectorSize;

	for (i = 0; i < (DIR_CHAIN_SIZE-1); i++) {
		if (fatDir->chain[i].index   * blockSize <= filePos &&
			fatDir->chain[i+1].index * blockSize >  filePos) {
				j = i;
				break;
			}
	}
	*cluster    = fatDir->chain[j].cluster;
	*clusterPos = (fatDir->chain[j].index * blockSize);
}

int fat_readFile(fat_bpb* bpb, fat_dir* fatDir, unsigned int filePos, unsigned char* buffer, int size) {
	int ret;
	int i,j;
	int chainSize;
	int nextChain; 
	int startSector;
	int bufSize;
	int sectorSkip;
	int clusterSkip;
	int dataSkip;

	unsigned int bufferPos;
	unsigned int fileCluster;
	unsigned int clusterPos;

	int clusterChainStart;

	fat_getClusterAtFilePos(bpb, fatDir, filePos, &fileCluster, &clusterPos);
	sectorSkip = (filePos - clusterPos) / bpb->sectorSize;
	clusterSkip = sectorSkip / bpb->clusterSize;
	sectorSkip %= bpb->clusterSize;
	dataSkip  = filePos  % bpb->sectorSize;
	bufferPos = 0;

	XPRINTF("fileCluster = %i,  clusterPos= %i clusterSkip=%i, sectorSkip=%i dataSkip=%i \n",
		fileCluster, clusterPos, clusterSkip, sectorSkip, dataSkip);

	if (fileCluster < 2) {
		return 0;
	}

	bufSize = SECTOR_SIZE;
	nextChain = 1;
	clusterChainStart = 1;

	while (nextChain && size > 0 ) {
		chainSize = fat_getClusterChain(bpb, fileCluster, cbuf, MAX_DIR_CLUSTER, clusterChainStart);
		clusterChainStart = 0;
		if (chainSize == 0) { //the chain is full, but more chain parts exist
			chainSize = MAX_DIR_CLUSTER;
			fileCluster = cbuf[MAX_DIR_CLUSTER - 1];
		}else { //chain fits in the chain buffer completely - no next chain needed
			nextChain = 0;
		}
		while (clusterSkip >= MAX_DIR_CLUSTER) {
			chainSize = fat_getClusterChain(bpb, fileCluster, cbuf, MAX_DIR_CLUSTER, clusterChainStart);
			clusterChainStart = 0;
			if (chainSize == 0) { //the chain is full, but more chain parts exist
				chainSize = MAX_DIR_CLUSTER;
				fileCluster = cbuf[MAX_DIR_CLUSTER - 1];
			}else { //chain fits in the chain buffer completely - no next chain needed
				nextChain = 0;
			}
			clusterSkip -= MAX_DIR_CLUSTER;
		}

#ifdef DEBUG
		fat_dumpClusterChain(cbuf, chainSize, clusterSkip);
		printf("fileCluster = %i,  clusterPos= %i clusterSkip=%i, sectorSkip=%i dataSkip=%i filePos=%i \n",
		fileCluster, clusterPos, clusterSkip, sectorSkip, dataSkip, filePos);
#endif

		//process the cluster chain (cbuf) and skip leading clusters if needed
		for (i = 0 + clusterSkip; i < chainSize && size > 0; i++) {
			//read cluster and save cluster content
			startSector = fat_cluster2sector(bpb, cbuf[i]);
			//process all sectors of the cluster (and skip leading sectors if needed)
			for (j = 0 + sectorSkip; j < bpb->clusterSize && size > 0; j++) {
				ret = READ_SECTOR(startSector + j, sbuf); 
				if (ret < 0) {
					printf("Read sector failed ! sector=%i\n", startSector + j);
					return bufferPos;
				}

				//compute exact size of transfered bytes
				if (size < bufSize) {
					bufSize = size + dataSkip;
				}
				if (bufSize > SECTOR_SIZE) {
					bufSize = SECTOR_SIZE;
				}
				XPRINTF("memcopy dst=%i, src=%i, size=%i  bufSize=%i \n", bufferPos, dataSkip, bufSize-dataSkip, bufSize);
				MEMCPY(buffer+bufferPos, sbuf + dataSkip, bufSize - dataSkip);
				size-= (bufSize - dataSkip);
				bufferPos +=  (bufSize - dataSkip);
				dataSkip = 0;
				bufSize = SECTOR_SIZE;
			}
			sectorSkip = 0;
		}
		clusterSkip = 0;
	}
	return bufferPos;
}

int fat_mountCheck() {
	int mediaStatus;
	int ret;

#ifdef _PS2_
	mediaStatus = mass_stor_getStatus(); 
	if (mediaStatus < 0) {
		mounted = 0;
		return mediaStatus;
	}
	if ((mediaStatus & 0x03) == 3) { /* media is ready for operation */
		/* in the meantime the media was reconnected and maybe changed - force unmount*/
		if ((mediaStatus & 0x04) == 4) { 
			mounted = 0;
		}
	    if (mounted) { /* and is mounted */
        	return 1;
        }
        ret = fat_initDriver();
		return ret;
	}
	if (mounted) { /* fs mounted but media is not ready - force unmount */
		mounted = 0;
	}
	return -10;
#else
	return 1;
#endif
}

int fat_getNextDirentry(fat_dir* fatDir) {
	fat_bpb* bpb;
	fat_direntry_sfn* dsfn;
	fat_direntry_lfn* dlfn;
	fat_direntry dir;
	int i, j;
	int dirSector;
	int startSector;
	int cont = 1;
	int ret;
	int dirPos;
	int clusterMod;
	int index;
	int dirCluster;

	//the getFirst function was not called
	if (direntryCluster == 0xFFFFFFFF || fatDir == NULL) {
		return -2;
	}
	bpb = &partBpb;

	dirCluster = direntryCluster;
	index  = 0;

	clusterMod = bpb->clusterSize - 1;

	//clear name strings
	dir.sname[0] = 0;
	dir.name[0] = 0;
	if (dirCluster == 0 && bpb->fatType < FAT32) { //Root directory
		startSector = bpb->rootDirStart;
		dirSector =  bpb->rootSize / (bpb->sectorSize / 32);
	} else { //other directory
		startSector = fat_cluster2sector(bpb, dirCluster);
		dirSector = fat_getClusterChain(bpb, dirCluster, cbuf, MAX_DIR_CLUSTER, 1);
		if (dirSector > 0) {
#ifdef DEBUG
			fat_dumpClusterChain(cbuf, dirSector);
#endif
			dirSector *=  bpb->clusterSize;
		} else {
			printf("FAT driver: Error getting cluster chain! startCluster=%i \n", dirCluster);
			return -3;
		}
	}
	XPRINTF("dirCluster=%i startSector=%i (%i) dirSector=%i \n", dirCluster, startSector, startSector * 512, dirSector);

	//go through first directory sector till the max number of directory sectors
	//or stop when no more direntries detected
	for (i = 0; i < dirSector && cont; i++) {
		ret = READ_SECTOR(startSector + i, sbuf); 
		if (ret < 0) {
			printf("Read directory  sector failed ! sector=%i\n", startSector + i);
			return -3;
		}
		//get correct sector from cluster chain buffer
		if ((dirCluster != 0) && (i % bpb->clusterSize == clusterMod)) {
			startSector = fat_cluster2sector(bpb, cbuf[(i / bpb->clusterSize) +  1]);
			startSector -= (i+1);
		}
		dirPos = 0;

		// go through start of the sector till the end of sector
		while (cont &&  dirPos < bpb->sectorSize) {
			dsfn = (fat_direntry_sfn*) (sbuf + dirPos);
			dlfn = (fat_direntry_lfn*) (sbuf + dirPos);
			cont = fat_getDirentry(dsfn, dlfn, &dir); //get single directory entry from sector buffer
			if (cont == 1) { //when short file name entry detected
				index++;
				if ((index-1) == direntryIndex) {
						direntryIndex++;
						fat_setFatDir(bpb, fatDir, dsfn, &dir, 0);
						return 1;
				}
				//clear name strings
				dir.sname[0] = 0;
				dir.name[0] = 0;
			}
			dirPos += 32; //directory entry of size 32 bytes
		}
		
	}
	// when we get this far - reset the direntry cluster
	direntryCluster = 0xFFFFFFFF; //no more files
	return -1; //indicate that no direntry is avalable
}

int fat_getFirstDirentry(char * dirName, fat_dir* fatDir) {
	int ret; 
	unsigned int startCluster = 0;

	ret = fat_mountCheck();
	if (ret < 0) {
		return ret;
	}
	if ( ((dirName[0] == '/' || dirName[0]=='\\') && dirName[1] == 0) || // the root directory
		dirName[0] == 0 || dirName == NULL) {
			direntryCluster = 0;
	} else {
		ret = fat_getFileStartCluster(&partBpb, dirName, &startCluster, fatDir);
		if (ret < 0) { //dir name not found
			return -4;
		}
		//check that direntry is directory 
		if ((fatDir->attr & 0x10) == 0) {
			return -3; //it's a file - exit
		}
		direntryCluster = startCluster;
	}
	direntryIndex = 0;
	return fat_getNextDirentry(fatDir);
}


int fat_initDriver() {
	int ret;

	lastChainCluster = 0xFFFFFFFF;
	lastChainResult = -1;

	direntryCluster = 0xFFFFFFFF;
	direntryIndex = 0;

	ret = DISK_INIT("disk1.bin", 512);
	if (ret < 0) {
		printf ("fat_driver: disk init failed \n" );
		return ret;
	}
	fat_getPartitionTable(&partTable);	
	fat_getPartitionBootSector(&partTable.record[workPartition],  &partBpb);
#ifdef DEBUG
	fat_dumpPartitionTable();
	fat_dumpPartitionBootSector();
#endif
	fs_init(NULL);
	mounted = 1;

}


void fat_closeDriver() {
	DISK_CLOSE();
}

fat_bpb*  fat_getBpb() {
 	return &partBpb;
}



/*************************************************************************************/
/*    File IO functions                                                              */
/*************************************************************************************/

int fs_findFileSlot(int fd) {
	int i;
	int result = -1;
	for (i = 0; i < MAX_FILES; i++) {
		if (fsRec[i].fd == fd) {
			result = i;
			break;
		}
	}
	return result;
}

int fs_init(struct fileio_driver *driver) {
	int i;
	mounted = 0;
	for (i = 0; i < MAX_FILES; i++) {
		fsRec[i].fd = -1;
	}
	return 1;
}

int fs_open(int fd, char *name, int mode) {
	int index;
	int ret;
	unsigned int cluster;


	XPRINTF("fs_open called: %s \n", name) ;
        //check if media mounted
        ret = fat_mountCheck();
        if (ret < 0) {
        	return ret;
        }
		
	//read only support
	if (mode != O_RDONLY) return -2;

	//check if the slot is free
	index = fs_findFileSlot(-1);
	if (index  < 0) return -3;

	cluster = 0; //allways start from root
	//find the file
	ret = fat_getFileStartCluster(&partBpb, name, &cluster, &fsDir[index]);
	if (ret < 0) {
		return -1;
	}
	//store fd to file slot
	fsRec[index].fd = fd;
	fsRec[index].filePos = 0;

	return fd;
}

int fs_close(int fd) {
	int index;
	
	index = fs_findFileSlot(fd);
	if (index < 0) {
		return -1;
	}
	fsRec[index].fd = -1;
	return 0;
}

int fs_lseek(int fd, int offset, int whence) {
	int index;
	
	index = fs_findFileSlot(fd);
	if (index < 0) {
		return -1;
	}
	switch(whence) {
		case SEEK_SET:
			fsRec[index].filePos = offset;
			break;
		case SEEK_CUR:
			fsRec[index].filePos += offset;
			break;
		case SEEK_END:
			fsRec[index].filePos = fsDir[index].size + offset;
			break;
		default:
			return -1;
	}
	if (fsRec[index].filePos < 0) {
		fsRec[index].filePos = 0;
	}
	if (fsRec[index].filePos > fsDir[index].size) {
		fsRec[index].filePos = fsDir[index].size;
	}
	return fsRec[index].filePos;
}

int fs_write(int fd, char * buffer, int size ) {
	return -1; //no write support
}

int fs_read(int fd, char * buffer, int size ) {
	int index;
	int result;
	
	index = fs_findFileSlot(fd);
	if (index < 0) {
		return -1;
	}

	if ((fsRec[index].filePos+size) > fsDir[index].size) {
		size = fsDir[index].size - fsRec[index].filePos;
	}

	if (size<=0) {
		return 0;
	}

	result = fat_readFile(&partBpb, &fsDir[index], fsRec[index].filePos, (unsigned char*) buffer, size);
	if (result > 0) { //read succesful 
		fsRec[index].filePos += result;
	}
	return result;
}

int fs_dummy(void) {
	return -5;
}

/*************************************************************************************/
/*    Test / debug functions                                                         */
/*************************************************************************************/

void fat_dumpClusterChain(unsigned int* buf, int maxBuf, int clusterSkip) {
	int i;
	printf("cluster chain:");
	for (i=0 + clusterSkip; i < maxBuf; i++) {
		printf(" %i,", buf[i]);
	}
	printf(" end\n");
}
void fat_dumpPartitionTable() {
	fat_part* part;
	int i;

	part = &partTable;

	printf ("\nPartition info\n" );
	printf ("--------------\n" );
	for (i = 0 ; i < 4 ; i++) {
        	if (part->record[i].sid != 0) {
        		printf ("Partition nr: %i \n", i);
		        printf ("id=%02X \n", 		part->record[i].sid);
        		printf ("start sector=%i\n", 	part->record[i].start);
	        	printf ("sector count=%i\n",	part->record[i].count);
	        }
	}

}

void fat_dumpPartitionBootSector() {
	fat_bpb* bpb;
	bpb = &partBpb;

	printf("\n");
	printf("Partition boot sector info\n" );
	printf("--------------------------\n");
	printf("sector size        = %i \n", bpb->sectorSize);
	printf("cluster size       = %i \n", bpb->clusterSize);
	printf("reserved sectors   = %i \n", bpb->resSectors);
	printf("fat count          = %i \n", bpb->fatCount);
	printf("root entry count   = %i sectors=%i \n", bpb->rootSize, bpb->rootSize / 16);
	printf("fat size (sect)    = %i \n", bpb->fatSize);
	printf("track size (sect)  = %i \n", bpb->trackSize);
	printf("head count         = %i \n", bpb->headCount);
	printf("hidden count(sect) = %i \n", bpb->hiddenCount);
	printf("sector count       = %i \n", bpb->sectorCount);
	printf("\n");
	printf("root start sector  = %i \n", bpb->rootDirStart);
	printf("fat type           = %i \n", bpb->fatType);
	printf("fat id             = %s \n", bpb->fatId);
}

void fat_dumpSectorHex(unsigned char* buf, int bufSize) {
	int i;
	char c;
	char line[17];
	line[16] = 0;
	for (i = 0; i < bufSize; i++) {
		c = buf[i];
		if (c < 32) {
			c = 32;
		}
		line[i % 16] = c;

		if (i % 16 == 0)  {
			printf ("%08X   ", i);
		}

		printf("%02X ", buf[i]);

		if (i % 16 == 15)  {
			printf ("  %s\n", line);
		}
	}
	if (i % 16 != 0) {
		line[i%16] = 0;
	        printf ("  %s\n", line);
	}

}

int fat_dumpFatSector(int fatSectorIndex) {
	int ret;
	unsigned int sector;
	fat_bpb* bpb;

	bpb = &partBpb;
	sector = bpb->partStart + bpb->resSectors + fatSectorIndex;
	ret = READ_SECTOR(sector, sbuf); 
	if (ret < 0) {
		printf("FAT driver: read fat sector failed! sector=%i! \n", sector );
		return -1;
	}
	printf("\n");
	printf("Fat sector, index=%i\n", fatSectorIndex);
	fat_dumpSectorHex(sbuf, SECTOR_SIZE);
}

int fat_dumpRootDirSector(int sectorIndex, int sectorCount) {
	int ret;
	int i;
	unsigned int sector;
	fat_bpb* bpb;

	bpb = &partBpb;
	printf("\n");

	sector = bpb->rootDirStart + sectorIndex;
	for (i = 0; i < sectorCount; i++) {
		ret = READ_SECTOR(sector + i , sbuf); 
		if (ret < 0) {
			printf("FAT driver: read directory sector failed! sector=%i! \n", sector  + i);
			return -1;
		}
		printf("Directory sector=%i, index=%i\n", sector, i);
		fat_dumpSectorHex(sbuf, SECTOR_SIZE);
	}
}

/* dumps the fat system information */
int fat_dumpSystemInfo() {
	int ret;
	
	ret = fat_mountCheck() ;
	if (ret < 0) {
		return ret;
	}

	fat_dumpPartitionTable();
	fat_dumpPartitionBootSector();

	fat_dumpFatSector(0);
	fat_dumpRootDirSector(0, 10);
}



/*
these functions helps the developper to get the flash disk sector image 
encoded as hex byes. That image can be useful when debugging 
the fat filesystem functions.
*/

void dumpReadData(unsigned char * buf, int bufSize) {
	int i;
	char c;
	int size = bufSize / 16;
	int offs;

	printf("\n");
	for (i = 0; i < size; i++) {
		offs = i * 16;
		printf("##%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X\n",
			buf[offs],    buf[offs+1],  buf[offs+2],  buf[offs+3],
			buf[offs+4],  buf[offs+5],  buf[offs+6],  buf[offs+7],
			buf[offs+8],  buf[offs+9],  buf[offs+10], buf[offs+11],
			buf[offs+12], buf[offs+13], buf[offs+14], buf[offs+15]
                );
	}
}

int fat_dumpSector(unsigned int sector) {
	int ret;

	ret = READ_SECTOR(sector, sbuf); 
	if (ret < 0) {
		printf("Read sector failed ! sector=%i\n", sector);
		return -1;
	}
	dumpReadData(sbuf, SECTOR_SIZE);
	return 1;
}
