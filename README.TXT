-----------------------------------------------------------------------------
              USB_MASS - the usb mass storage driver for ps2
               		          ver. 0.23
-----------------------------------------------------------------------------

This is the driver for ps2 that supports usb flash drives and other usb
mass storage devices plugged in the 'black box'.
The basic idea is to plug the usb bar flash disk into ps2 usb port and
be able to access stored files from within the homebrew ps2 programs.

Supported hardware:
  - bulk only devices with scsi command set (I hope that most of the recent
    flash disks will work)
    Tested with:
    * Seitec Usb Bar (1022_128M sticker) - working
    * SWEEX (FX300010) 6 In 1 multi pannel - first CF slot working
    * ViPowER usb to ide cable (VP-9208 + VP-9054V enclosure) - working
    * Ne Way MP3 player 256MB

Supported features:
  - fat12, fat16, fat32. First suitable partition is used. 
  - basic support for long filenames (128 chars in the filename)
  - read only access
  - directory listing


Missing features:
  - multi partition acces
  - write access
  - real unicode decoding and conversion for long filenames
  - only one mass device connected is currently supported
  - file system error detection - if you plug in disk with corrupted
    filesystem then the resluts are unspecified (but as long as the driver
    can only read data it will not modify the disk content).
  - and much more :-)  Driver is still in development so you may
    encounter many bugs.


How to use:
The usb_mass driver requires that usbd.irx is loaded. You can use either
usbd.irx that can be found on some games and demo disks or you can use 
npm_usbd.irx that is part of the naplink ps2 server side. Then load
the mass_usb.irx and you are ready to acces the "mass:" device by calling
fioOpen("mass:xxx/yyy.txt", O_RDONLY), fioRead etc. When you plug in
the flash disk (the supported one) you should see the message on the
console ("usb_mass: connect ok!") which means that device is recognised
and ready to use. There is an example in the ee directory that lists
directory content of the disk's root directory.

Performance:
My tests shows that file transfer speed is about 500kbytes per second. 
If large non fragmented files are accesed (several megabytes) then the
speed jumps at 550kbytes/s. It's not much but when compared to maximum
usb 1.1 tranfer speed it's not that bad (max is about 700kb/s).

Compilation:
Copy the content to ps2sdk/samples/usb_mass directory and run make 
in ee and/or iop subdirectory to get example elf and driver irx.


Change log:

ver 0.23
--------
- dopen dclose dread getstat implementation by raipsu

ver 0.22
--------
Hermes Notes:
-Added support for different sizes of sector (512 to 4096)
-Modified scache control and reader to read blocks of 4096 bytes. It use the same memory and run well.
-Some changes necessary in fat_driver
-Tested in a 128MB Pendrive with FAT16 and 512bytes per sector (4 sector per cluster)
-Tested in a Ne Way MP3 player 256MB with FAT16 & FAT32, 2048 bytes per sector (2 sector per cluster)

ver 0.21
--------
- ps2sdk compatible

ver 0.2
-------
- support for the fat32 filesystem

ver 0.1 
-------
-initial version


Credits: (alphabetical order :)
Jonathan Fox (FAT system guide), Hermes, mrbrown, ole, oobles, 
Gustvo Scotti, raipsu, TyRaNiD, and the ps2dev scene.

Thanks:
to all folks who helped me with testing.

License:
See the file LICENSE included with this distribution for licensing terms.
