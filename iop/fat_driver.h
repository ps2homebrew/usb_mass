#ifndef _FAT_DRIVER_H
#define _FAT_DRIVER_H 1

#ifdef _PS2_
#include <io_common.h>
#include <ioman.h>
#else

#define FIO_SO_IFREG		0x0010		// Regular file
#define FIO_SO_IFDIR		0x0020		// Directory
/* fake struct for non ps2 systems */
typedef struct _iop_file {
	void	*privdata;
} iop_file_t;
typedef void iop_device_t;

typedef struct {
	unsigned int mode;
	unsigned int attr;
	unsigned int size;
	unsigned char ctime[8];
	unsigned char atime[8];
	unsigned char mtime[8];
	unsigned int hisize;
} fio_stat_t;

typedef struct {
	fio_stat_t stat;
	char name[256];
	unsigned int unknown;
} fio_dirent_t;


#endif /* _PS2_ */

#include "fat.h"

typedef struct _fs_rec {
	int fd;
	unsigned int filePos;
} fs_rec;

#if !defined O_RDONLY
#define O_RDONLY 0
#endif

int mass_stor_getStatus();

/*
int fs_open( int fd, char *name, int mode);
int fs_lseek(int fd, int offset, int whence);
int fs_read( int fd, char * buffer, int size );
int fs_write( int fd, char * buffer, int size );
int fs_close( int fd);
int fs_dummy(void);
*/

int fs_init   (iop_device_t *driver); 
int fs_open   (iop_file_t* , const char *name, int mode, ...);
int fs_lseek  (iop_file_t* , unsigned long offset, int whence);
int fs_read   (iop_file_t* , void * buffer, int size );
int fs_write  (iop_file_t* , void * buffer, int size );
int fs_close  (iop_file_t* );
int fs_dummy  (void);

int fs_deinit (iop_device_t *);
int fs_format (iop_file_t *, ...);
int fs_ioctl  (iop_file_t *, unsigned long, void *);
int fs_remove (iop_file_t *, const char *);
int fs_mkdir  (iop_file_t *, const char *);
int fs_rmdir  (iop_file_t *, const char *);
int fs_dopen  (iop_file_t *, const char *);
int fs_dclose (iop_file_t *);
int fs_dread  (iop_file_t *, void *);
int fs_getstat(iop_file_t *, const char *, void *);

int fs_chstat (iop_file_t *, const char *, void *, unsigned int);



int      fat_initDriver(void);
void     fat_closeDriver(void);
fat_bpb* fat_getBpb(void);
int      fat_getFileStartCluster(fat_bpb* bpb, const char* fname, unsigned int* startCluster, fat_dir* fatDir);


//void fat_dumpFile(fat_bpb* bpb, int fileCluster, int size, char* fname);
void fat_dumpDirectory(fat_bpb* bpb, int dirCluster);
void fat_dumpPartitionBootSector();
void fat_dumpPartitionTable();
void fat_dumpClusterChain(unsigned int* buf, int maxBuf, int clusterSkip);
int  fat_dumpSystemInfo();

int fat_getFirstDirentry(char * dirName, fat_dir* fatDir);
int fat_getNextDirentry(fat_dir* fatDir);
#endif

