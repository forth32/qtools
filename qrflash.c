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
// -s <файл> - чтение разделов, описанных в файле partition.mbn
//
//


//*************************************
//* Чтение блока данных
//*************************************
void read_block(int block,int blocksize,FILE* out) {

unsigned char iobuf[4096];  
int page,sec;
 // цикл по страницам
for(page=0;page<ppb;page++)  {

  setaddr(block,page);
  // по секторам  
  for(sec=0;sec<4;sec++) {
   mempoke(nand_exec,0x1); 
//   nandwait();
   if (!memread(iobuf,sector_buf, blocksize)) { // выгребаем порцию данных
     printf("\n memread вернул ошибку, завершаем чтение.\n");
     exit(0);
   }  
   fwrite(iobuf,1,blocksize,out);
  }
 } 
} 


//@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@
void main(int argc, char* argv[]) {
  
unsigned char iobuf[2048];
unsigned char partname[17]={0}; // имя раздела
unsigned char filename[300]="qflash.bin";
unsigned int i,sec,bcnt,iolen,page,block;
int res;
unsigned char* sptr;
unsigned int start=0,len=1,helloflag=0,opt;
unsigned int blocksize=512;
FILE* out;
FILE* part=0;

int attr; // арибуты
int npar; // число разедлов в таблице

char hellocmd[]="\x01QCOM fast download protocol host\x03###";
char devname[]="/dev/ttyUSB0";

while ((opt = getopt(argc, argv, "p:a:l:o:ixs:")) != -1) {
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
     
   case 's':
     part=fopen(optarg,"r");
     if (part == 0) {
       printf("\nОшибка открытия файла таблицы разделов\n");
       return;
     }
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

out=fopen(filename,"w");

mempoke(nand_cfg1,0x6745d); // ECC off
mempoke(nand_cs,4); // data mover

mempoke(nand_cmd,1); // Сброс всех операций контроллера
mempoke(nand_exec,0x1);
nandwait();
// устанавливаем код команды
mempoke(nand_cmd,0x34); // чтение data+ecc+spare

//###################################################333
// Режим чтения по таблице разделов
//###################################################333
if (part != 0) {
 res=fread(iobuf,1,8,part);
 if((res != 8) || (strncmp(iobuf,"\xAA\x73\xEE\x55\xDB\xBD\x5E\xE3",8) != 0)) {
   printf("\nФайл не является таблицей разделов\n");
   return;
 }
 fseek(part,4,SEEK_CUR); // пропускаем таблицу разделов
 res=fread(&npar,1,4,part); // Число разделов
 printf("\n Число разделов: %i",npar);
 printf("\n  адрес    размер   атрибуты ------ Имя------\n");     
 for(i=0;i<npar;i++) {
    res=fread(partname,1,16,part); // имя
    res=fread(&start,1,4,part); // адрес
    res=fread(&len,1,4,part);  // размер
    res=fread(&attr,1,4,part);  // атрибуты
    if (((start+len) >maxblock)||(len == 0xffffffff)) len=maxblock-start; // если длина - FFFF, или выходит за пределы флешки
    sprintf(iobuf,"%02i-%s.bin",i,partname); // фрмируем имя файла
    out=fopen(iobuf,"w");  // открываем выхдной файл
    printf("\r%08x  %08x  %08x  %s\n",start,len,attr,partname);
    for(block=start;block<(start+len);block++) {
       printf("\r * %08x",block); fflush(stdout);
       read_block(block,blocksize,out);
    }
    fclose(out);
 }
 return; // все разделы прочитаны
    
}
//###################################################333
// Режим чтения сырого флеша
//###################################################333
printf("\n Чтение области %08x - %08x\n",start,start+len);



printf("\n");
// главыный цикл
// по блокам
for (block=start;block<(start+len);block++) {
  printf("\r %08x",block); fflush(stdout);
  read_block(block,blocksize,out);
} 
printf("\n"); 
} 
