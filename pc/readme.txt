This is the test program that can be used on your pc to develop/debug 
the fat driver. Some parts of the program are shared with the ps2 irx file.

Shared:
 - fat_driver.c   //the fat driver
 - fat_driver.h
 - scache.c       //sector cache
 - scache.h
 - fat.h          //fat data structures
 - debug.h

PS2 specific:
 - mass_stor.c  //usb acces to device
 - mass_stor.h
 - usb_mass.c   //irx driver main


PC specific:
 - vdisk.c       //virtual disk (virtual device - reads from file)
 - vdisk.h
 - fat_test.c    // test program main



The test_program can be compiled by gcc, just redefine the path in makefile.
The virtual disk is an ordinary file that contains raw data of the sectors
of the flash disk device. That sector data can be dumped from within ps2 
by calling rpc function nr.3 (note that the dump function doesn't check that
flash disk is initialized so you have to call for example dumpSystemInfo 
function to properly initialize device). The dump function takes 3 parameters,
the start sector, the number of sectors to be dumped, filename of the
disk image (works only if your ps2link client supports writting to host:/
device).




