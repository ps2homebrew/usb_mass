#ifndef _VDISK_H
#define _VDISK_H 1

int  vdisk_init(char * param, int sectorSize);
void vdisk_close();
int  vdisk_readSector(unsigned int sector, void* buf) ;
int  vdisk_readSector4096(unsigned int sector, void* buf) ;

#endif 
