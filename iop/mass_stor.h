#ifndef _MASS_STOR_H
#define _MASS_STOR_H 1

int mass_stor_init();
int mass_stor_disconnect(int devId);
int mass_stor_connect(int devId);
int mass_stor_probe(int devId);
int mass_stor_readSector1(unsigned int sector, unsigned char* buffer);
int mass_stor_readSector4(unsigned int sector, unsigned char* buffer);
int mass_stor_readSector8(unsigned int sector, unsigned char* buffer);

//void mass_stor_setDisconnectProc(int (*x)(void));
#endif