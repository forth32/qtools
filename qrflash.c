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
void read_block(int block,int sectorsize,FILE* out) {

unsigned char iobuf[4096];  
int page,sec;
 // цикл по страницам
for(page=0;page<ppb;page++)  {

  setaddr(block,page);
  // по секторам  
  for(sec=0;sec<4;sec++) {
   mempoke(nand_exec,0x1); 
   nandwait();
 retry:  
   if (!memread(iobuf,sector_buf, sectorsize)) { // выгребаем порцию данных
     printf("\n memread вернул ошибку чтения секторного буфера.");
     printf("\n block = %08x  page=%08x  sector=%08x",block,page,sec);
     printf("\n Повторить операцию? (y,n):");
     if ((getchar() == 'y')||(getchar() == 'Y')) goto retry;
     memset(iobuf,0,sectorsize);
     exit(0);
   }  
   fwrite(iobuf,1,sectorsize,out);
  }
 } 
} 

//*************************************
//* чтение таблицы разделв из flash
//*************************************
int load_ptable(char* ptable) {
  
memset(ptable,0,512); // обнуляем таблицу
flash_read(2, 1, 0);  // блок 2 страница 1 - здесь лежит таблица разделов  
return memread(ptable,sector_buf, 512);
}

//*****************************
//* чтение сырого флеша
//*****************************
void read_raw(int start,int len,int sectorsize,FILE* out) {
  
int block;  

printf("\n Чтение области %08x - %08x\n",start,start+len);
printf("\n");
// главыный цикл
// по блокам
for (block=start;block<(start+len);block++) {
  printf("\r %08x",block); fflush(stdout);
  read_block(block,sectorsize,out);
} 
printf("\n"); 
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
unsigned int sectorsize=512;
FILE* out;
FILE* part=0;
int partflag=0;  // 0 - сырой флеш, 1 - таблица разделов из файла, 2 - таблица разделов из флеша
int eccflag=1;  // 1 - отключить ECC,  0 - включить

int attr; // арибуты
int npar; // число разедлов в таблице

char hellocmd[]="\x01QCOM fast download protocol host\x03###";
char devname[]="/dev/ttyUSB0";
unsigned char ptable[520]; // таблица разделов


while ((opt = getopt(argc, argv, "hp:a:l:o:ixs:e")) != -1) {
  switch (opt) {
   case 'h': 
    printf("\n  Утилита предназначена для чтения образа флеш через модифицированный загрузчик\n\
Допустимы следующие ключи:\n\n\
-p <tty>  - указывает имя устройства последовательного порта для общения с загрузчиком\n\
-e        - включает коррекцию ECC при чтении (по умолчанию выключаена)\n\
-i        - запускает процедуру HELLO для инициализации загрузчика\n\
-x        - читает полный сектор - данные+obb. Без ключа читает только данные.\n\n\
Для режима неформатированного чтения:\n\
-a <blk>  - начальный номер читаемого блока (по умолчанию 0)\n\
-l <num>  - число читаемых блоков\n\
-o <file> - имя выходного файла (по умолчанию qflash.bin)\n\n\
Для режима чтения разделов\n\
-s <file> - взять карту разделов из указанного файла\n\
-s @      - взять карту разделов из флеша (блок 2 страница 1 сектор 0)\n\n");
    return;
    
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
     sectorsize+=64;
     break;
     
   case 's':
     if (optarg[0] == '@')  partflag=2; // загружаем таблицу разделов из флеша
     else {
       // загружаем таблицу разделов из файла
       partflag=1;
       part=fopen(optarg,"r");
       if (part == 0) {
         printf("\nОшибка открытия файла таблицы разделов\n");
         return;
       } 
       fread(ptable,512,1,part); // читаем таблицу разделов из файла
       fclose(part);
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

if (partflag == 2) load_ptable(ptable); // загружаем таблицу разделов

mempoke(nand_cfg1,0x6745c|eccflag); // ECC on/off
mempoke(nand_cs,4); // data mover

mempoke(nand_cmd,1); // Сброс всех операций контроллера
mempoke(nand_exec,0x1);
nandwait();
// устанавливаем код команды
mempoke(nand_cmd,0x34); // чтение data+ecc+spare

//###################################################
// Режим чтения сырого флеша
//###################################################
if (partflag == 0) { 
  out=fopen(filename,"w");
  read_raw(start,len,sectorsize,out);
  return;
}  


//###################################################
// Режим чтения по таблице разделов
//###################################################

if (strncmp(ptable,"\xAA\x73\xEE\x55\xDB\xBD\x5E\xE3",8) != 0) {
   printf("\nТаблица разделов повреждена\n");
   return;
}
npar=*((unsigned int*)&ptable[12]);
printf("\n Число разделов: %i",npar);
printf("\n  адрес    размер   атрибуты ------ Имя------\n");     
for(i=0;i<npar;i++) {
    strncpy(partname,ptable+16+28*i,16);       // имя
    start=*((unsigned int*)&ptable[32+28*i]);   // адрес
    len=*((unsigned int*)&ptable[36+28*i]);     // размер
    attr=*((unsigned int*)&ptable[40+28*i]);    // атрибуты
    if (((start+len) >maxblock)||(len == 0xffffffff)) len=maxblock-start; // если длина - FFFF, или выходит за пределы флешки
    sprintf(iobuf,"%02i-%s.bin",i,partname); // формируем имя файла
    out=fopen(iobuf,"w");  // открываем выходной файл
    printf("\r%08x  %08x  %08x  %s\n",start,len,attr,partname);
    for(block=start;block<(start+len);block++) {
       printf("\r * %08x",block); fflush(stdout);
       read_block(block,sectorsize,out);
    }
    fclose(out);
}
printf("\n"); 
    
} 
