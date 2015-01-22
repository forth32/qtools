#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
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


char hellocmd[]="\x01QCOM fast download protocol host\x03###";
char devname[]="/dev/ttyUSB0";

while ((opt = getopt(argc, argv, "p:a:l:o:i")) != -1) {
  switch (opt) {
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

if (len == 0) {
  printf("\n Неправильная длина");
  return;
}  

if (!open_port(devname))  {
   printf("\n - Последовательный порт %s не открывается\n", devname); 
   return; 
}

out=fopen(filename,"w");

if (helloflag) {
  printf("\n Отсылка hello...");
  iolen=send_cmd(hellocmd,strlen(hellocmd),iobuf);
  if (iobuf[1] != 2) {
    printf("\nhello возвратил ошибку!\n");
    dump(iobuf,iolen,0);
    return;
  }  
  i=iobuf[0x2c];
  iobuf[0x2d+i]=0;
  printf("\n Flash: %s",iobuf+0x2d);
}
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
