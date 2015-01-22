#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include "qcio.h"

//@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@
//* Чтение участка флеша модема в файл 
//
// qflash <адрес> <длина>
//
// -p - порт командного tty (ttyUSB0)
// -a - начальный номер блока (0)
// -l - число читаемых блоков (1)
// -o - выходной файл (qflash.bin)
// -i - запуск hello
// -x - вывод сектора+obb (без -x только сектор)
//
//

#define nand_cmd 0x1b400000
#define nand_addr0 0x1b400004
#define nand_addr1 0x1b400008
#define nand_cs    0x1b40000c
#define nand_exec  0x1b400010
#define nand_status 0x1b400014
#define nand_cfg1  0x1b400024
#define sector_buf 0x1b400100

// число страниц в 1 блоке
#define ppb 64

// ожидание завершения операции контроллером
inline nandwait() { while ((mempeek(nand_status)&0xf) != 0); }


//@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@
void main(int argc, char* argv[]) {
  
unsigned char iobuf[2048];
unsigned char filename[300]="qflash.bin";
int i,sec,bcnt,iolen,page,block;
unsigned char* sptr;
unsigned int start=0,len=1,helloflag=0,opt,adr;
unsigned int blocksize=512;
FILE* out;


char hellocmd[]="\x01QCOM fast download protocol host\x03###";
char devname[]="/dev/ttyUSB0";

while ((opt = getopt(argc, argv, "p:a:l:o:ix")) != -1) {
  switch (opt) {
   case 'p':
    strcpy(devname,optarg);
    break;
    
   case 'o':
    strcpy(filename,optarg);
    break;
    
   case 'a':
     sscanf(optarg,"%x",&start);
     break;

   case 'l':
     sscanf(optarg,"%x",&len);
     break;

   case 'i':
     helloflag=1;
     break;
     
   case 'x':
     blocksize+=64;
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
printf("\n Чтение области %08x - %08x\n",start,start+len);



mempoke(nand_cfg1,0x6745d); // ECC off
mempoke(nand_cs,4);

mempoke(nand_cmd,1); // Сброс всех операций контроллера
mempoke(nand_exec,0x1);
nandwait();
// устанавливаем код команды
mempoke(nand_cmd,0x34); // чтение data+ecc+spare
printf("\n");
// главыный цикл
// по блокам
for (block=start;block<(start+len);block++) {
 // по страницам
 for(page=0;page<ppb;page++)  {

  printf("\r %08x",block); fflush(stdout);
  adr=block*ppb+page;
  mempoke(nand_addr0,adr<<16);
  mempoke(nand_addr1,(adr>>16)&0xff);
  // по секторам  
  for(sec=0;sec<4;sec++) {

   mempoke(nand_exec,0x1); 
//   nandwait();

   if (!memread(iobuf,sector_buf, blocksize)) { // выгребаем порцию данных
     printf("\n memread вернул ошибку, завершаем чтение.\n");
     return;
   }  
   fwrite(iobuf,1,blocksize,out);
  }
 } 
} 
printf("\n"); 
} 
