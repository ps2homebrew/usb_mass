/*
 * fat_test.c - USB Mass storage driver for PS2
 *
 * (C) 2004, Marek Olejnik (ole00@post.cz)
 *
 * debug-develop-test program for fat driver
 *
 * See the file LICENSE included with this distribution for licensing terms.
 */

#include <stdio.h>
#include <stdlib.h>
#include <memory.h>

#include <fcntl.h>//for write output
#include <sys/stat.h>	//for write output
#include <unistd.h> //for linux compatibility

#include "fat_driver.h"
#include "fat_write.h"

//#define DEBUG 1


void printHelp() {
	printf("\n");
	printf("fat_test option cluster [p1 [p2 [p3...]]] \n");
	printf("option: -i  partition & boot sector info\n");
	printf("        -d  dump directory p1 \n");
	printf("        -c  copy file p1 to local file p2\n");
	printf("        -p  finds the start cluster of the file p1\n");
	printf("        -s  open file p1, seek at p2 offset and reads 4 bytes\n");
#ifdef WRITE_SUPPORT
	printf("        -u  compute checksum of the p1 - SFN\n");
	printf("        -xc  experimental - create file\n");
	printf("        -xd  experimental - delete file\n");
	printf("        -xw  experimental - write to file\n");
#endif /* WRITE_SUPPORT */
	printf("\n");
}


void testFile(char*  fname) {
	char buf[4096];
	int size;
	int bufSize;
	int readSize;
	int fd;
	int i, max;
	unsigned int csum = 0;
	unsigned int* buf2;
	iop_file_t file;

	buf2 = (unsigned int*) buf;

	bufSize = 4096;

	fd = fs_open(&file, fname, O_RDONLY);

	if (fd >=0) {
		size = fs_lseek(&file, 0, SEEK_END);
		printf("size: %i \n", size);
		fs_lseek(&file, 0, SEEK_SET);

		while (size  > 0) {
			if (size < bufSize) {
				bufSize = size;
			}
			readSize =  fs_read(&file, buf, bufSize);
			max = readSize / 4;
			for (i = 0; i < max; i++) {
				csum = csum ^ buf2[i];
			}
			size -= readSize;
			if (readSize < 1) {
				size = -1;
			}

		}	
		if (size < 0) {
			printf("Error reading file !\n");
		} else {
			printf("File read ok. Control sum=%08X\n", csum);
		}
		fs_close(&file);
	} else {

		printf("open file failed: %s \n", fname);
	}

}

void copyFile(char*  fname, char* oname) {
	char buf[4096];
	int size;
	int bufSize;
	int readSize;
	int fd, fo;
	int i, max;
	unsigned int* buf2;
	iop_file_t file;

	buf2 = (unsigned int*) buf;

	bufSize = 4096;

	fd = fs_open(&file, fname, O_RDONLY);
	fo = open(oname, O_RDWR | O_TRUNC | O_CREAT | O_BINARY, S_IWUSR | S_IRUSR); 
	if (fd >=0 && fo >=0) {
		size = fs_lseek(&file, 0, SEEK_END);
		printf("file: %s size: %i \n", fname, size);
		fs_lseek(&file, 0, SEEK_SET);

		while (size  > 0) {
			if (size < bufSize) {
				bufSize = size;
			}
			readSize =  fs_read(&file, buf, bufSize);
			size -= readSize;
			if (readSize < 1) {
				size = -1;
			} else {
				write(fo, buf, readSize);
			}

		}	
		if (size < 0) {
			printf("Error reading file !\n");
		} 
		fs_close(&file);
	} else {
		if (fo >= 0) {
			printf("open file failed: %s \n", fname);
		} else {
			printf("open file failed: %s \n", oname);
		}
	}

}

void listDirRecord(fat_dir* record) {

	if ((record->attr & 0x10) == 0x10) {
		printf("[*] "); //directory
               	printf("%s \n", record->name);
	} else
	if ((record->attr & 0x08) == 0x08) {
		printf(">-< "); //directory
               	printf("%s \n", record->name);
	} else
	{
		printf("    "); //file
               	printf("%s   size:%d ", record->name, record->size);
		printf(" date:%d/%d/%d time: %d:%d:%d \n", 
			record->date[2] + record->date[3]*256, record->date[1], record->date[0],
			record->time[0], record->time[1], record->time[2]);
	}

}

void dumpDirectory(char * directory) {
	int ret;
	fat_dir record;
	int counter = 0;
	
	printf("DIRECTORY LIST \n");
	printf("-------------- \n");

        /* list the root directory */

	ret = fat_getFirstDirentry(directory, &record);
	while (ret > 0 ) {
		counter++;
		listDirRecord(&record);
		ret = fat_getNextDirentry(&record);
	}
	if (counter == 0) {
                printf("no files in directory ? \n");
	}
}


#ifdef WRITE_SUPPORT

/*
convert short name from condensed format ("Abcd.txt") to direntry format ("ABCD    TXT");
*/


void convertShortName(unsigned char* src, unsigned char* dst) {
	int i,j;
	
	//fill dst with spaces
	for (i = 0; i < 11; i++) dst[i] = 0x20;

	for (i=0; src[i] != '.' && src[i] !=0 && i < 9; i++) {
		dst[i] = toUpperChar(src[i]);
	}
	if (src[i] == '.') {
		i++;
		j=0;	
		for (; src[i] !=0 && j < 3; i++, j++) {
			dst[8+j] = toUpperChar(src[i]);
		}
	}	
}


void computeChecksum(char* fileName) {
	unsigned char result;
	unsigned char sname[12];
	int i;

	sname[11] = 0;
	convertShortName((unsigned char*)fileName, sname);	
	result = 0;
	for (i = 0; i < 11; i++) {
		result = (0x80 * (0x01 & result)) + (result >> 1);  //ROR 1
		result += sname[i];
	}
	printf("checksum of %s ->  '%s'  = %d, 0x%02X \n", fileName, sname, result, result);
}

void createFile(char* param, char* par2) {
	char directory;
	unsigned int sfnSector;
	unsigned int cluster;
	int sfnOffset;

	fat_bpb* bpb = fat_getBpb();
	directory = 0;
	if (par2 != NULL && par2[0] != 0) directory = 1;
	fat_createFile(bpb, param, directory, 0, &cluster,  &sfnSector, &sfnOffset);
}

void deleteFile(char* param, char* par2) {
	char directory;
	int ret;

	fat_bpb* bpb = fat_getBpb();
	directory = 0;
	if (par2 != NULL && par2[0] != 0) directory = 1;
	ret = fat_deleteFile(bpb, param, directory);
	printf("result = %d \n", ret);
}

/*
void writeTest(char* fname, char* extraName) {
	char buf[4096];
	int size;
	int bufSize;
	int readSize;
	int fd, fi;
	int i, max;
	unsigned int* buf2;
	iop_file_t file;

	buf2 = (unsigned int*) buf;

	bufSize = 4096;
	buf[0] = 'A';
	buf[1] = 'b';
	buf[2] = 'c';
	buf[3] = 'd';


	fd = fs_open(&file, fname, O_RDWR | O_TRUNC| O_CREAT);
	printf("open ret=%d \n", fd);
	//fi = open(extraName, O_RDONLY, S_IWUSR | S_IRUSR); 
	if (fd >=0 ) {
		size = fs_write(&file, buf, 4);
		printf("write ret=%d \n", size);
	}

	i = fs_close(&file);
	printf("close ret=%d \n", i);


}
*/

void writeFile(char* fname, char* iname) {
	char buf[4096];
	int size;
	int bufSize;
	int readSize;
	int writeSize;
	int fd, fi;
	int i, max;
	unsigned int* buf2;
	iop_file_t file;

	if (fname == NULL || iname == NULL) {
		printf("E: not enough parameters!\n");
		return;
	}

	buf2 = (unsigned int*) buf;

	bufSize = 4096;

	fd = fs_open(&file, fname, O_RDWR | O_CREAT | O_TRUNC );
	fi = open(iname, O_RDONLY | O_BINARY , S_IWUSR | S_IRUSR); 
	if (fd >=0 && fi >=0) {
		size = lseek(fi, 0, SEEK_END);
		printf("file: %s size: %i \n", iname, size);
		lseek(fi, 0, SEEK_SET);

		while (size  > 0) {
			if (size < bufSize) {
				bufSize = size;
			}
			//readSize =  fs_read(&file, buf, bufSize);
			readSize =  read(fi, buf, bufSize);
			writeSize = fs_write(&file, buf, readSize);

			size -= writeSize;
			if (writeSize < 1) {
				size = -1;
			} else {
				//write(fo, buf, readSize);
			}

		}	
		if (size < 0) {
			printf("Error writing file ! ret=%d\n", writeSize);
		} 
		fs_close(&file);
	} else {
		if (fi >= 0) {
			printf("open file failed: %s \n", fname);
		} else {
			printf("open file failed: %s \n", iname);
		}
	}



}

void testX(char* param1) {

	fat_test();

}

#endif /* WRITE_SUPPORT */

/*****************************************************************/
int main(int argc, char** argv)
{
	unsigned int cluster;
	int size;
	int ret;
	int fd;
	int i;
	unsigned char data[4];
	fat_bpb* bpb;
	iop_file_t file;

	fat_dir  fatDir; //complete directory entry

	if (argc < 2) {
		printHelp();
		return 1;
	}

	ret = fat_initDriver();
	if (ret < 0) {
		printf ("Init driver \n" );
		return 1;
	}
	bpb = fat_getBpb();
	fs_init(NULL);

	cluster = 0;
	if (argc > 1 && argv[1][0] == '-' ) {
		switch (argv[1][1]) {
			case 'i':	//info
						fat_dumpSystemInfo();
						break;
			case 'p':	//path cluster
						cluster = 0;
						ret = fat_getFileStartCluster(bpb, argv[2], &cluster, &fatDir);
						if (ret < 0) {
							printf ("path cluster not found!\n");
						} else {
							printf ("path cluster= %i \n", cluster);
						}
						break;
			case 's':	//seek at file offset, then read 4 bytes
						data[0] = data[1] = data[2] = data[3] = 0;
						fd = fs_open(&file, argv[2], O_RDONLY);
						if (fd < 0) {
							printf ("error opening file! rc=%i\n", fd);
							break;
						} else {
							printf("fs_open ok, fd=%i \n", fd);
						}
						fs_lseek(&file, atoi(argv[3]), SEEK_SET);
						ret = fs_read(&file, (char*)data, 4);
						fs_close(&file);
						printf("seek bytes: %02X %02X %02X %02X \n",  data[0], data[1], data[2], data[3]);
						testFile(argv[2]);
						break;
			case 'd':	//dump directory 
						dumpDirectory(argv[2]);
						break;

			case 'c':	//dump file
						copyFile(argv[2], argv[3]);
						break;
#ifdef WRITE_SUPPORT
			case 'u':	//compute checksum
						computeChecksum(argv[2]);
						break;
			case 'x':	//test functions
						switch(argv[1][2]) {
						case 'c': createFile(argv[2], argv[3]);	break;
						case 'd': deleteFile(argv[2], argv[3]);	break;
						case 'w': writeFile (argv[2], argv[3]);	break;
						case 'x': testX(argv[2]);
						}
						break;
#endif /* WRITE_SUPPORT */
		}
	} 

	fat_closeDriver();
	return 0;
}
