#include <tamtypes.h>
#include <sifrpc.h>
#include <kernel.h>
#include <loadfile.h>
#include "libpad.h"
#include "debug.h"

#include "mass_rpc.h"

#define ROM_PADMAN

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
		scr_printf("[*] "); //directory
               	scr_printf("%s \n", record->name);

		printf("[*] "); //directory
               	printf("%s \n", record->name);
	} else
	if ((record->attr & 0x08) == 0x08) {
		scr_printf(">-<"); //volume label
               	scr_printf("%s \n", record->name);

		printf(">-< "); //directory
               	printf("%s \n", record->name);
	} else
	{
		scr_printf("    "); //file
               	scr_printf("%s   size:%d \n", record->name, record->size);
		printf("    "); //file
               	printf("%s   size:%d \n", record->name, record->size);

	}

}

void listDirectory() {
	int ret;
	fat_dir_record record;
	int counter = 0;
	
	scr_printf("DIRECTORY LIST \n");
	scr_printf("-------------- \n");
	printf("DIRECTORY LIST \n");
	printf("-------------- \n");

        /* list the root directory */

	ret = usb_mass_getFirstDirentry("/", &record);
	while (ret > 0 ) {
		counter++;
		listDirRecord(&record);
		ret = usb_mass_getNextDirentry(&record);
	}
	if (counter == 0) {
		scr_printf("no files in root directory ? \n");
                printf("no files in root directory ? \n");
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
        scr_printf("sifLoadModule sio failed: %d\n", ret);
        SleepThread();
    }    

#ifdef ROM_PADMAN
    ret = SifLoadModule("rom0:PADMAN", 0, NULL);
#else
    ret = SifLoadModule("rom0:XPADMAN", 0, NULL);
#endif 
    if (ret < 0) {
        scr_printf("sifLoadModule pad failed: %d\n", ret);
        SleepThread();
    }
}


void loadModules() {
	int ret;

	loadPadModules();

	ret = SifLoadModule("host0:usbd.irx", 0, NULL);
	if (ret < 0) {
		printf("sifLoadModule %s failed: %d\n", "usbd", ret);
		while(1);
	} else printf("usbd.irx ok  ret=%i\n", ret );

        ret = SifLoadModule("host0:usb_mass.irx", 0, NULL);    
	if (ret < 0) {
		printf("sifLoadModule %s failed: %d\n", "usb_mass", ret);
		while(1);
	} else {
		printf("usbtest.irx ok. ret=%i \n", ret);
        }

	delay(1);

	ret = usb_mass_bindRpc();
	if (ret < 0 ) {
	        printf("\nSifBindRpc failed: %d !!!!\n", ret);
	}else {
		printf("ok\n");
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
        scr_printf("USB MASS driver example\n" );

	SifInitRpc(0);        


	loadModules();
	scr_printf("modules loaded ok.\n");

	padInit(0);

	port = 0; // 0 -> Connector 1, 1 -> Connector 2
	slot = 0; // Always zero if not using multitap

	if((ret = padPortOpen(port, slot, padBuf)) == 0) {
        	scr_printf("padOpenPort failed: %d\n", ret);
		SleepThread();
	}

	if(!initializePad(port, slot)) {
		scr_printf("pad initalization failed!\n");
		SleepThread();
	}

	scr_printf("press (X) to list directory, (O) for system info \n");

	end = 0;
	while (!end) { 
	        ret=padGetState(port, slot);
	        while((ret != PAD_STATE_STABLE) && (ret != PAD_STATE_FINDCTP1)) {
        		if(ret==PAD_STATE_DISCONN) {
				scr_printf("Pad(%d, %d) is disconnected\n", port, slot);
			}
			ret=padGetState(port, slot);
		}
            
	        ret = padRead(port, slot, &buttons); // port, slot, buttons
            
        	if (ret != 0) {
			paddata = 0xffff ^ ((buttons.btns[0] << 8) | 
                                buttons.btns[1]);
                
			new_pad = paddata & ~old_pad;
			old_pad = paddata;
                
			if(new_pad & PAD_CROSS) {
                       		listDirectory();
			}
			if(new_pad & PAD_CIRCLE) {
                       		usb_mass_dumpSystemInfo();
			}
			if(new_pad & PAD_START) {
				end = 1;
			}
		}


	}
	scr_printf("END!");
}
