#ifndef _FAT_DRIVER_H
#define _FAT_DRIVER_H 1

#include "fat.h"


typedef struct _fs_rec {
	int fd;
	unsigned int filePos;
} fs_rec;

#if !defined O_RDONLY
#define O_RDONLY 0
#endif

int mass_stor_getStatus();

int fs_init( struct fileio_driver *driver);
int fs_open( int fd, char *name, int mode);
int fs_lseek(int fd, int offset, int whence);
int fs_read( int fd, char * buffer, int size );
int fs_write( int fd, char * buffer, int size );
int fs_close( int fd);
int fs_dummy(void);

int      fat_initDriver(void);
void     fat_closeDriver(void);
fat_bpb* fat_getBpb(void);
int      fat_getFileStartCluster(fat_bpb* bpb, char* fname, unsigned int* startCluster, fat_dir* fatDir);


//void fat_dumpFile(fat_bpb* bpb, int fileCluster, int size, char* fname);
void fat_dumpDirectory(fat_bpb* bpb, int dirCluster);
void fat_dumpPartitionBootSector();
void fat_dumpPartitionTable();
void fat_dumpClusterChain(unsigned int* buf, int maxBuf, int clusterSkip);
int  fat_dumpSystemInfo();

int fat_getFirstDirentry(char * dirName, fat_dir* fatDir);
int fat_getNextDirentry(fat_dir* fatDir);
#endif

