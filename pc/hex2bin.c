/****************************************************************************
 * hex2bin - convert heax ascii code to binary data
 * (C) 2004, Marek Olejnik (ole00@post.cz)
 * See the file LICENSE included with this distribution for licensing terms.
****************************************************************************/


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <sys/stat.h>

#define INIT_READ 1
#define INIT_WRITE 2

//#define DEBUG 1

#ifdef DEBUG
#define xprintf printf
#else
#define xprintf //
#endif


void printHelp(void){

   printf ("\nhex2bin  0.1 by Ole");
   printf ("\nUsage : src_hex_file dst_bin_file");
   printf ("\n");

}

/****************************************************************
 Initialize input/output file
*****************************************************************/

int initFile(char* ifn, int mode)
{
  int f = 1;

  printf ("Accessing file : %s \n",ifn);
  switch (mode) {
	case INIT_READ:
	    f = open(ifn, O_RDONLY | O_BINARY , S_IWUSR); 
	    break;
	case INIT_WRITE:
	    f = open(ifn, O_RDWR | O_TRUNC | O_CREAT | O_BINARY, S_IWUSR | S_IRUSR); 
	    break;
  }
  return f;
}




/* reads a single line from input text file */

int readLine(int f, char* buf) {
	char	 c;
	int      i = 0;
	int	 ret = 1;
	int 	 endLine = 0;

	while (ret > 0 && endLine==0) {
		ret = read(f, buf+ i, 1);
		if (buf[i] == 0x0A) {
			endLine = 1;
		}
		i++;
	}
	if (ret > 0) {
		i--;
		buf[i]=0;
		if ( buf[i-1]==0x0D ) {
			buf[i-1] = 0;	
		}
	}		

	return ret;
}

unsigned char hex2byte(char* line) {
	
	unsigned char a,b;

	a = line[0] - 48;   // 48 -> '0'
	b = line[1] - 48;

	if (a > 9)  {
		a = line[0] - 65 + 10;
	}
	if (b > 10) {
		b = line[1] - 65 + 10;
	}

	return (unsigned char) ((a<<4) + b);

}


void storeLine(int fo, char* line) {
	int i;
	int offset = 0;
	unsigned char buf[16];
	int size = strlen(line) / 2;

	if (size > 16) {
		size = 16;
	}

	for (i = 0; i < size; i++) {
		offset = i*2;
		buf[i] = hex2byte(line + offset);		
	}
	write(fo, buf, size);

}





/*
Parse the text lines of the input text file
*/

void convertData(int fi, int fo) {
	char buf[512];
	int ret;
	int i;
	int glyphIndex;
	int glyphHeight;

	ret = 1;
	while (ret > 0) {
		ret = readLine(fi, buf);
		if (ret < 0) {
			break;
		}
		if (buf[0] == '#') {
		    switch(buf[1]) {
		    	case '#':
			    storeLine(fo, buf + 2);
			    break;
		    }
		    
		}
	}
}





/*****************************************************************/
/*****************************************************************/
/*****************************************************************/
int main(int argc, char** argv)
{
 int fin;   /* input file handle */
 int fout;  /* output file handle */

  /* too few parameters passed */
 if (argc <= 2) 
 {
  printHelp();
  return 1;
 }

 fin = initFile(argv[1], INIT_READ);
 if (fin < 0) {
   printf("\n !!! Error opening file %s! \n", argv[1]);
   return 1;
 }

 fout = initFile(argv[2], INIT_WRITE);
 if (fout < 0) {
   printf("\n !!! Error opening file %s! \n", argv[2]);
   return 1;
 }


 convertData(fin, fout);  	 

 close(fout);
 close(fin);
 return 0;
}
