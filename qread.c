#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#ifndef WIN32
#include <unistd.h>
#else
#include <windows.h>
#include "wingetopt.h"
#include "printf.h"
#endif
#include "qcio.h"

//@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@
//* Чтение участка памяти модема в файл qmem.bin
//
// qread <адрес> <длина>
// Чтение идет блоками по 512 байт. Все чила - в hex
//

void main(int argc, char* argv[]) {
  
unsigned char cmdbuf[2048],iobuf[2048];
unsigned char filename[300]="qmem.bin";
int i,bcnt,iolen;
unsigned char* sptr;
unsigned int adr=0,len=0x200,endadr,blklen=512,helloflag=0,opt;
FILE* out;


#ifndef WIN32
char devname[20]="/dev/ttyUSB0";
#else
char devname[20]="";
#endif

while ((opt = getopt(argc, argv, "p:a:l:o:ih")) != -1) {
  switch (opt) {
   case 'h': 
     printf("\n Утилита предназначена для чтения адресного пространства модема\n\n\
Допустимы следующие ключи:\n\n\
-p <tty>  - указывает имя устройства последовательного порта для общения с загрузчиком\n\
-o <file> - имя выходного файла (по умолчанию qmem.bin)\n\n\
-i        - запускает процедуру HELLO для инициализации загрузчика\n\
-a <adr>  - начальный адрес\n\
-l <num>  - размер читаемого участка\n");
    return;
     
   case 'p':
    strcpy(devname,optarg);
    break;
    
   case 'o':
    strcpy(filename,optarg);
    break;
    
   case 'a':
     sscanf(optarg,"%x",&adr);
     break;

   case 'l':
     sscanf(optarg,"%x",&len);
     break;

   case 'i':
     helloflag=1;
     break;
  }
}  

#ifdef WIN32
if (*devname == '\0')
{
   printf("\n - Последовательный порт не задан\n"); 
   return; 
}
#endif

if (len == 0) {
  printf("\n Неправильная длина");
  return;
}  

if (!open_port(devname))  {
 #ifndef WIN32
   printf("\n - Последовательный порт %s не открывается\n", devname); 
#else
   printf("\n - Последовательный порт COM%s не открывается\n", devname); 
#endif
  return; 
}

out=fopen(filename,"wb");

if (helloflag) hello();

endadr=adr+len;
printf("\n Чтение области %08x - %08x\n",adr,endadr);
cmdbuf[0]=3; // команда чтения
*((unsigned short*)&cmdbuf[5])=blklen; // длина блока

for(i=adr;i<endadr;i+=512)  {
  
 printf("\r %08x",i); 
 *((unsigned int*)&cmdbuf[1])=i;
 if ((i+512) > endadr) {
   blklen=endadr-adr;
   *((unsigned short*)&cmdbuf[5])=blklen;  
 }  
 iolen=send_cmd(cmdbuf,7,iobuf);
 if (iolen == 0) {
   printf("\n Ошибка в процессе обработки команды\n");
   return;
 }  
 fwrite(iobuf+6,1,blklen,out);
} 
printf("\n"); 
} 
