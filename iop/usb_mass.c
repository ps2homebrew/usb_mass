/*
 * usb_mass.c - USB Mass storage driver for PS2
 *
 * (C) 2001, Gustavo Scotti (gustavo@scotti.com)
 * (C) 2002, David Ryan ( oobles@hotmail.com )
 * (C) 2004, Marek Olejnik (ole00@post.cz)
 *
 * IOP file io driver and RPC server
 *
 * See the file LICENSE included with this distribution for licensing terms.
 */

#include <tamtypes.h>
#include <kernel.h>
#include <sifcmd.h>
#include <sifrpc.h>
#include <fileio.h>
#include "mass_stor.h"
#include "fat_driver.h"

//#define DEBUG 1
#include "mass_debug.h"


#define BIND_RPC_ID 0x500C0F1


/* function declaration */
void rpcMainThread(void* param);
void *rpcCommandHandler(u32 command, void *buffer, int size);


static SifRpcDataQueue_t rpc_queue __attribute__((aligned(64)));
static SifRpcServerData_t rpc_server __attribute((aligned(64)));
static int _rpc_buffer[512] __attribute((aligned(64)));
static int threadId;

static struct fileio_driver fs_driver;
static void *fs_functarray[16];


/* init file system driver */
void initFsDriver() {
	int i;

	fs_driver.device = "mass";
	fs_driver.xx1 = 16;
	fs_driver.version = 1;
	fs_driver.description = "Usb mass storage driver";
	fs_driver.function_list = fs_functarray;

	for (i=0;i < 16; i++) {
		fs_functarray[i] = fs_dummy;
	}
	fs_functarray[ FIO_INITIALIZE ] = fs_init;
	fs_functarray[ FIO_OPEN ] = fs_open;
	fs_functarray[ FIO_CLOSE ] = fs_close;
	fs_functarray[ FIO_READ ] = fs_read;
	fs_functarray[ FIO_WRITE ] = fs_write;
	fs_functarray[ FIO_SEEK] = fs_lseek;


	FILEIO_del( "mass");
	FILEIO_add( &fs_driver);
}


int _start( int argc, char **argv)
{
	iop_thread_t param;
	int th;


	FlushDcache();
	UsbInit();

	initFsDriver();

	/*create thread*/
	param.attr         = TH_C;
	param.thread     = rpcMainThread;
	param.priority = 40;
	param.stacksize    = 0x800;
	param.option      = 0;


	th = CreateThread(&param);
	if (th > 0) {
		StartThread(th,0);
		return 0;
	} else  {
		return 1;
	}


}
void rpcMainThread(void* param)
{
	int ret=-1;
	int tid;

	SifInitRpc(0);

	printf("usb_mass: version 0.2\n");

	mass_stor_init();

	tid = GetThreadId();

	SifSetRpcQueue(&rpc_queue, tid);
	SifRegisterRpc(&rpc_server, BIND_RPC_ID, (void *) rpcCommandHandler, (u8 *) &_rpc_buffer, 0, 0, &rpc_queue);

	SifRpcLoop(&rpc_queue);
}

void dumpDiskContent(unsigned int startSector, unsigned int endSector, char* fname) {
	unsigned int i;
	int ret;
	int fd;
	unsigned char* buf;

	printf("--- dump start: start sector=%i end sector=%i fd=%i--- \n", startSector, endSector, fd);
	
	ret = 1;
	for (i = startSector; i < endSector && ret > 0; i++) {
		ret = fat_dumpSector(i);
	}
	printf("-- dump end --- \n" );
	for (i = 0; i < 256; i++) {
		printf("                                           \n");
	}

}

int getFirst(void* buf) {
	fat_dir fatDir;
	int ret;
	ret = fat_getFirstDirentry((char*) buf, &fatDir);
	if (ret > 0) {
		memcpy(buf, &fatDir, sizeof(fat_dir_record)); //copy only important things
	}
	return ret;
}
int getNext(void* buf) {
	fat_dir fatDir;
	int ret;
	
	ret = fat_getNextDirentry(&fatDir);
	if (ret > 0) {
		memcpy(buf, &fatDir, sizeof(fat_dir_record)); //copy only important things
	}
	return ret;
}


void *rpcCommandHandler(u32 command, void *buffer, int size)

{
	int* buf = (int*) buffer;
	int ret = 0;

	switch (command) {
		case 1: //getFirstDirentry
			ret  = getFirst(((char*) buffer) + 4); //reserve 4 bytes for returncode
			break;
		case 2: //getNextDirentry
			ret = getNext(((char*) buffer) + 4);
		 	break;
		case 3: //dumpContent
			dumpDiskContent(buf[0], buf[1], ((char*) buffer) + 8);
			break;
		case 4: //dump system info
			ret = fat_dumpSystemInfo(buf[0], buf[1]);
			break;
	}
	buf[0] = ret; //store return code
	return buffer;
}