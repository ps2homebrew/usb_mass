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
#include <thbase.h>
#include <sifcmd.h>
#include <sifrpc.h>
#include <ioman.h>
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

static iop_device_t fs_driver;
static iop_device_ops_t fs_functarray;


/* init file system driver */
void initFsDriver() {
	int i;

	fs_driver.name = "mass";
	fs_driver.type = 16;
	fs_driver.version = 1;
	fs_driver.desc = "Usb mass storage driver";
	fs_driver.ops = &fs_functarray;

	fs_functarray.init    = fs_init;
	fs_functarray.deinit  = fs_deinit;
	fs_functarray.format  = fs_format;
	fs_functarray.open    = fs_open;
	fs_functarray.close   = fs_close;
	fs_functarray.read    = fs_read;
	fs_functarray.write   = fs_write;
	fs_functarray.lseek   = fs_lseek;
	fs_functarray.ioctl   = fs_ioctl;
	fs_functarray.remove  = fs_remove;
	fs_functarray.mkdir   = fs_mkdir;
	fs_functarray.rmdir   = fs_rmdir;
	fs_functarray.dopen   = fs_dopen;
	fs_functarray.dclose  = fs_dclose;
	fs_functarray.dread   = fs_dread;
	fs_functarray.getstat = fs_getstat;
	fs_functarray.chstat  = fs_chstat;

	DelDrv("mass");
	AddDrv(&fs_driver);

}


int _start( int argc, char **argv)
{
	iop_thread_t param;
	int th;


	FlushDcache();
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

	printf("usb_mass: version 0.21\n");

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