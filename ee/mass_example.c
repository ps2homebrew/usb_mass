#ifndef __PS2SDK_1_1__
#include <stdio.h>
#endif
#include <tamtypes.h>
#include <sifrpc.h>
#include <kernel.h>
#include <loadfile.h>
#include <fileio.h>
#include <string.h>
#include <errno.h>

#include "libpad.h"
#include "debug.h"

#include "mass_rpc.h"

#define ROM_PADMAN

#define TWIN_PRINTF(format, args...)	\
	printf(format, ## args) ; \
	scr_printf(format, ## args)



static char padBuf[256] __attribute__((aligned(64)));

void delay(int count) {
	int i;
	int ret;
	for (i  = 0; i < count; i++) {
	        ret = 0x01000000;
		while(ret--) asm("nop\nnop\nnop\nnop");
	}
}



void listDirRecord(fat_dir_record* record) {
	

	if ((record->attr & 0x10) == 0x10) {
		TWIN_PRINTF("[*] "); //directory
               	TWIN_PRINTF("%s \n", record->name);
	} else
	if ((record->attr & 0x08) == 0x08) {
		TWIN_PRINTF(">-<"); //volume label
               	TWIN_PRINTF("%s \n", record->name);
	} else
	{
		TWIN_PRINTF("    "); //file
               	TWIN_PRINTF("%s   size:%d \n", record->name, record->size);
	}
}

void writeTest() {
	char buf[4096];
	int size;
	int bufSize;
	int readSize;
	int writeSize;
	int fi, fo;
	s32 ret;
	char iname[256];
	char oname[256];

	ret = fioMkdir("mass:/usb_mass test");
	printf("create directory  result=%d\n", ret);

	if (ret < 0 && ret != -EEXIST) {
		return;
	}

	iname[0] = 0;
	strcat(iname,"host:mass_example.elf" );

	oname[0] = 0;
	strcat(oname,"mass:/usb_mass test/mass_example.elf"); 

	bufSize = 4096;

	fi = fioOpen(iname, O_RDONLY);
	fo = fioOpen(oname, O_WRONLY | O_TRUNC | O_CREAT ); 

	if (fi >=0 && fo >=0) {
		size = fioLseek(fi, 0, SEEK_END);
		TWIN_PRINTF("file: %s size: %i \n", iname, size);
		fioLseek(fi, 0, SEEK_SET);

		while (size  > 0) {
			if (size < bufSize) {
				bufSize = size;
			}
			readSize =  fioRead(fi, buf, bufSize);
			writeSize = fioWrite(fo, buf, readSize);
			size -= writeSize;
			if (writeSize < 1) {
				size = -1;
			}
		}	
		if (size < 0) {
			TWIN_PRINTF("Error reading file !\n");
		} 
	} else {
		if (fo < 0) {
			TWIN_PRINTF("open file failed: %s \n", oname);
		}
		if (fi < 0) {
			TWIN_PRINTF("open file failed: %s \n", iname);
		}
	}
	ret = fioClose(fo);
	TWIN_PRINTF("close file %s result=%d \n", oname, ret);
	ret = fioClose(fi);
	TWIN_PRINTF("close file %s result=%d \n", iname, ret);

}

void deleteTest() {
	int ret;
	ret = fioRemove("mass:/usb_mass test/mass_example.elf");
	TWIN_PRINTF("remove file ret = %d\n", ret);

	ret = fioMkdir("mass:/usb_mass test/newDir_1");
	TWIN_PRINTF("make directory ret = %d\n", ret);

	ret = fioMkdir("mass:/usb_mass test/newDir_2");
	TWIN_PRINTF("make directory ret = %d\n", ret);

//!NOTE! mass0 is intentional! if you use only 'mass:/' the rmdir won't work.
	ret = fioRmdir("mass0:/usb_mass test/newDir_2");
	TWIN_PRINTF("remove directory ret = %d\n", ret);
}

void listDirectory(char* path) {
	int ret;
	fat_dir_record record;
	int counter = 0;
	
	TWIN_PRINTF("DIRECTORY LIST path=%s \n", path);
	TWIN_PRINTF("------------------------------------ \n");
        /* list the root directory */

	ret = usb_mass_getFirstDirentry(path, &record);
	while (ret > 0 ) {
		counter++;
		listDirRecord(&record);
		ret = usb_mass_getNextDirentry(&record);
	}
	if (counter == 0) {
                TWIN_PRINTF("no files in root directory ? \n");
	}
}




void loadPadModules(void)
{
    int ret;

    
#ifdef ROM_PADMAN
    ret = SifLoadModule("rom0:SIO2MAN", 0, NULL);
#else
    ret = SifLoadModule("rom0:XSIO2MAN", 0, NULL);
#endif
    if (ret < 0) {
        TWIN_PRINTF("sifLoadModule sio failed: %d\n", ret);
        SleepThread();
    }    

#ifdef ROM_PADMAN
    ret = SifLoadModule("rom0:PADMAN", 0, NULL);
#else
    ret = SifLoadModule("rom0:XPADMAN", 0, NULL);
#endif 
    if (ret < 0) {
        TWIN_PRINTF("sifLoadModule pad failed: %d\n", ret);
        SleepThread();
    }
}


#define USBD_IRX "host:usbd.irx"
#define USB_MASS_IRX "host:usb_mass.irx"


void loadModules() {
	int ret;

	loadPadModules();

	ret = SifLoadModule(USBD_IRX, 0, NULL);
	if (ret < 0) {
		TWIN_PRINTF("sifLoadModule %s failed: %d\n", "usbd", ret);
		while(1);
	} else TWIN_PRINTF("usbd.irx ok  ret=%i\n", ret );

        ret = SifLoadModule(USB_MASS_IRX, 0, NULL);    
	if (ret < 0) {
		TWIN_PRINTF("sifLoadModule %s failed: %d\n", "usb_mass", ret);
		while(1);
	} else {
		TWIN_PRINTF("usb_mass.irx ok. ret=%i \n", ret);
        }

	delay(1);

	ret = usb_mass_bindRpc();
	if (ret < 0 ) {
	        TWIN_PRINTF("\nSifBindRpc failed: %d !!!!\n", ret);
	}else {
		TWIN_PRINTF("ok\n");
	}

}

int waitPadReady(int port, int slot) {

    int state;
    int lastState;
    char stateString[16];

    state = padGetState(port, slot);
    lastState = -1;
    while((state != PAD_STATE_STABLE) && (state != PAD_STATE_FINDCTP1)) {
        if (state != lastState) {
            padStateInt2String(state, stateString);
//            scr_printf("Please wait, pad(%d,%d) is in state %s\n", 
//                       port, slot, stateString);
        }
        lastState = state;
        state=padGetState(port, slot);
    }
    // Were the pad ever 'out of sync'?
    if (lastState != -1) {
//        scr_printf("Pad OK!\n");
    }
}

int initializePad(int port, int slot)
{
    waitPadReady(port, slot);

    padSetMainMode(port, slot, PAD_MMODE_DUALSHOCK, PAD_MMODE_LOCK);

    waitPadReady(port, slot);

    return 1;
}



int main()
{
	int port, slot;
	int ret;
	int end;
	struct padButtonStatus buttons;
	u32 paddata;
	u32 old_pad = 0;
	u32 new_pad;

        init_scr();
        TWIN_PRINTF("USB MASS driver example\n" );

	SifInitRpc(0);        


	loadModules();
	TWIN_PRINTF("modules loaded ok.\n");

	padInit(0);

	port = 0; // 0 -> Connector 1, 1 -> Connector 2
	slot = 0; // Always zero if not using multitap

	if((ret = padPortOpen(port, slot, padBuf)) == 0) {
        	TWIN_PRINTF("padOpenPort failed: %d\n", ret);
		SleepThread();
	}

	if(!initializePad(port, slot)) {
		TWIN_PRINTF("pad initalization failed!\n");
		SleepThread();
	}

	TWIN_PRINTF("press:  X   to list directory\n");
	TWIN_PRINTF("        O   for system info\n");
	TWIN_PRINTF("        []  to create directory and write a file \n");
	TWIN_PRINTF("        /\\  to delete file and directory\n");


	end = 0;
	while (!end) { 
	        ret=padGetState(port, slot);
	        while((ret != PAD_STATE_STABLE) && (ret != PAD_STATE_FINDCTP1)) {
        		if(ret==PAD_STATE_DISCONN) {
				TWIN_PRINTF("Pad(%d, %d) is disconnected\n", port, slot);
			}
			ret=padGetState(port, slot);
		}
            
	        ret = padRead(port, slot, &buttons); // port, slot, buttons
            
        	if (ret != 0) {
#ifdef __PS2SDK_1_1__
			paddata = 0xffff ^ ((buttons.btns[0] << 8) | 
                                buttons.btns[1]);
#else
			paddata = 0xffff ^ buttons.btns;
#endif

                
			new_pad = paddata & ~old_pad;
			old_pad = paddata;
                
			if(new_pad & PAD_CROSS) {
                       		listDirectory("/");
			} else
			if(new_pad & PAD_CIRCLE) {
                       		usb_mass_dumpSystemInfo();
			} else
			if (new_pad & PAD_TRIANGLE) {
				deleteTest();
			} else
			if (new_pad & PAD_SQUARE) {
				writeTest();
			} else
			if (new_pad & PAD_SELECT) {
				//usb_mass_dumpDiskContent(0, 255456+32, "host:dump.bin");
				usb_mass_dumpDiskContent(0, 8192, "host:part.bin");
			} else
	
			if(new_pad & PAD_START) {
				end = 1;
			}
		}


	}
	TWIN_PRINTF("END!");
}
