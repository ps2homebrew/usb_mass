-----------------------------------------------------------------------------
              USB_MASS - the usb mass storage driver for ps2
               		          ver. 0.2

  --- by Ole --- with the help of ps2dev scene --- for the ps2dev scene ---
-----------------------------------------------------------------------------

Introduction:
As I was struggling with the development on ps2, one of the most disappointing
thing was inability to perform the code on non-development ps2. When the 
independence exploit was introduced thing gets much better, but some
restrictions (mostly the media capacity) remained. Existing ps2 file devices
are imho quite limiting:
- memory cards: are fine, quiet, but have small capacity and files can be
  accesed from ps2 only.
- cdroms: good capcity but noisy and restricted access (toc can't be read
  without the modchip - personaly I don't want modchip in my machine)
- harddisks: good capacity and fast but noisy and requires network adapter
- networked fs: requires connected computer 
- usb memory device: good capacity, quiet, files can be managed on pc,
  can be easily plugged in to another ps2, BUT - no device driver on ps2.

And that last missing option forced me to make an attempt to create it.
The basic idea was to plug the usb bar flash disk into ps2 usb port and
to be able access stored files from within the ps2 program.
As it may sounds as easy task to programm such driver the reality was
different (as allways in such cases :-). Writting of the driver threw
me into realm of usb devices, scsi command set, fat file system, iop
programming and lot of specifications that normal person is not able to read
(or even understand :-). 

Supported hardware:
  - bulk only devices with scsi command set (I hope that most of the recent
    flash disks will work)
    Tested with:
    * Seitec Usb Bar (1022_128M sticker) - working
    * SWEEX (FX300010) 6 In 1 multi pannel - first CF slot working
    * ViPowER usb to ide cable (VP-9208 + VP-9054V enclosure) - working

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


Credits:
oobles, mrbrown, TyRaNiD, Gustvo Scotti, Jonathan Fox (FAT system guide)
and the ps2dev scene.

Thanks:
to all folks who helped me with testing.

License:
See the file LICENSE included with this distribution for licensing terms.
