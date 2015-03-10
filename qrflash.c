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
#include <io.h>
#include "wingetopt.h"
#include "printf.h"
#endif
#include "qcio.h"

//@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@
//* Чтение флеша модема в файл 
//
//


//*************************************
//* Чтение блока данных
//*************************************
void read_block(int block,int sectorsize,FILE* out) {

unsigned char iobuf[14096];  
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
   }  
   fwrite(iobuf,1,sectorsize,out);
  }
 } 
} 

//****************************************************************
//* Чтение блока данных с восстановлением китайского изврата
//****************************************************************
read_block_resequence(int block, FILE* out) {
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
   if (!memread(iobuf,sector_buf,576)) { // выгребаем порцию данных
     printf("\n memread вернул ошибку чтения секторного буфера.");
     printf("\n block = %08x  page=%08x  sector=%08x",block,page,sec);
     printf("\n Повторить операцию? (y,n):");
     if ((getchar() == 'y')||(getchar() == 'Y')) goto retry;
     memset(iobuf,0,512);
   }     
   if (sec != 3) 
     // Для секторов 0-2
     fwrite(iobuf,1,516,out);    // Тело сектора + 4 байта OBB
   else 
     // для сектора 3
     fwrite(iobuf,1,500,out);   // Тело сектора - 12 байт ненужного хвоста
  }
 } 
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
unsigned int i,sec,bcnt,iolen,page,block,filepos,lastpos;
int res;
unsigned char c;
unsigned char* sptr;
unsigned int start=0,len=1,helloflag=0,opt;
unsigned int sectorsize=512;
FILE* out;
FILE* part=0;
int partflag=0;  // 0 - сырой флеш, 1 - таблица разделов из файла, 2 - таблица разделов из флеша
int eccflag=1;  // 1 - отключить ECC,  0 - включить
int partnumber=-1; // номер раздела для чтения, -1 - все разделы
int listmode=0;    // 1- вывод карты разделов
int truncflag=0;  //  1 - отрезать все FF от конца раздела
int chipset9x15=1; // 0 - MSM8200, 1 - MDM9x00

int attr; // арибуты
int npar; // число разедлов в таблице

#ifndef WIN32
char devname[50]="/dev/ttyUSB0";
#else
char devname[50]="";
#endif
unsigned char ptable[1100]; // таблица разделов


while ((opt = getopt(argc, argv, "hp:a:l:o:ixs:ef:mt8k:")) != -1) {
  switch (opt) {
   case 'h': 
    printf("\n  Утилита предназначена для чтения образа флеш через модифицированный загрузчик\n\
Допустимы следующие ключи:\n\n\
-p <tty>  - указывает имя устройства последовательного порта для общения с загрузчиком\n\
-e        - включает коррекцию ECC при чтении (по умолчанию выключаена)\n\
-i        - запускает процедуру HELLO для инициализации загрузчика\n\
-x        - читает полный сектор - данные+obb. Без ключа читает только данные.\n\n\
-k #           - выбор чипсета: 0(MDM9x15, по умолчанию), 1(MDM8200), 2(MSM9x00), 3(MDM9x25)\n\
Для режима неформатированного чтения:\n\
-a <blk>  - начальный номер читаемого блока (по умолчанию 0)\n\
-l <num>  - число читаемых блоков\n\
-o <file> - имя выходного файла (по умолчанию qflash.bin)\n\n\
Для режима чтения разделов\n\
-s <file> - взять карту разделов из указанного файла\n\
-s @      - взять карту разделов из флеша (блок 2 страница 1 сектор 0)\n\
-f n      - читать только раздел под номером n\n\
-t        - отрезать все FF за последним значимым байтом раздела\n\
-m        - вывести на экран полную карту разделов\n");
    return;
    
   case 'k':
     switch(*optarg) {
       case '0':
        nand_cmd=0x1b400000;
	break;

       case '1':
        nand_cmd=0xA0A00000;
	break;

       case '2':
        nand_cmd=0x81200000;
	break;

       case '3':
        nand_cmd=0xf9af0000;
	break;

       default:
	printf("\nНедопустимый номер чипсета\n");
	return;
     }	
    break;
    
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
     sectorsize+=16;
     break;
     
   case 's':
     if (optarg[0] == '@')  partflag=2; // загружаем таблицу разделов из флеша
     else {
       // загружаем таблицу разделов из файла
       partflag=1;
       part=fopen(optarg,"rb");
       if (part == 0) {
         printf("\nОшибка открытия файла таблицы разделов\n");
         return;
       } 
       fread(ptable,1024,1,part); // читаем таблицу разделов из файла
       fclose(part);
     } 
     break;
     
   case 'm':
     listmode=1;
     break;
     
   case 'f':
     sscanf(optarg,"%i",&partnumber);
     break;
     
   case 't':
     truncflag=1;
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

if ((truncflag == 1)&&(sectorsize>512)) {
  printf("\nКлючи -t и -x несовместимы\n");
  return;
}  
if (helloflag) hello();

if (partflag == 2) load_ptable(ptable); // загружаем таблицу разделов

mempoke(nand_cfg1,mempeek(nand_cfg1)&0xfffffffe|eccflag); // ECC on/off
//mempoke(nand_cs,4); // data mover
mempoke(nand_cs,0); // data mover

mempoke(nand_cmd,1); // Сброс всех операций контроллера
mempoke(nand_exec,0x1);
nandwait();
// устанавливаем код команды
mempoke(nand_cmd,0x34); // чтение data+ecc+spare


//###################################################
// Режим чтения сырого флеша
//###################################################
if (partflag == 0) { 
  out=fopen(filename,"wb");
  read_raw(start,len,sectorsize,out);
  return;
}  


//###################################################
// Режим чтения по таблице разделов
//###################################################
/*
if (strncmp(ptable,"\xAA\x73\xEE\x55\xDB\xBD\x5E\xE3",8) != 0) {
   printf("\nТаблица разделов повреждена\n");
   return;
}
*/
npar=*((unsigned int*)&ptable[12]);
printf("\n Версия таблицы разделов: %i",*((unsigned int*)&ptable[8]));
if ((partnumber != -1) && (partnumber>=npar)) {
  printf("\nНедопустимый номер раздела: %i, всего разделов %i\n",partnumber,npar);
  return;
}  
printf("\n #  адрес    размер   атрибуты ------ Имя------\n");     
for(i=0;i<npar;i++) {
    if (chipset9x15) {
      // 9x15
      strncpy(partname,ptable+16+28*i,16);       // имя
      start=*((unsigned int*)&ptable[32+28*i]);   // адрес
      len=*((unsigned int*)&ptable[36+28*i]);     // размер
      attr=*((unsigned int*)&ptable[40+28*i]);    // атрибуты
      if (((start+len) >maxblock)||(len == 0xffffffff)) len=maxblock-start; // если длина - FFFF, или выходит за пределы флешки
    }
    else {
      // 8200
      strncpy(partname,ptable+16+28*i,16);       // имя
      start=*((unsigned int*)&ptable[32+28*i]);   // адрес
      len=*((unsigned int*)&ptable[36+28*i]);     // размер
      attr=*((unsigned int*)&ptable[40+28*i]);    // атрибуты
    if (len == 0xffffffff) len=maxblock-start; // если длина - FFFF, или выходит за пределы флешки
    }
  // Выводим описание раздела - для всех разделов или для конкретного заказанного
    if ((partnumber == -1) || (partnumber==i))  printf("\r%02i %08x  %08x  %08x  %s\n",i,start,len,attr,partname);
  // Читаем раздел - если не указан просто вывод карты. 
    if (listmode == 0) 
      // Все разделы или один конкретный  
      if ((partnumber == -1) || (partnumber==i)) {
        // формируем имя файла
        if (sectorsize == 512) sprintf(filename,"%02i-%s.bin",i,partname); 
        else                   sprintf(filename,"%02i-%s.obb",i,partname);  
        if (filename[4] == ':') filename[4]='-';    // заменяем : на -
        out=fopen(filename,"wb");  // открываем выходной файл
        for(block=start;block<(start+len);block++) {
          printf("\r * R: блок %08x (%i%%)",block-start,(block-start+1)*100/len); fflush(stdout);
          if ((attr != 0x1ff)||(sectorsize>512)) 
	     // сырое чтение или чтение неизварщенных разделов
	     read_block(block,sectorsize,out);
	  else 
	     // чтение извращенных разделов
	     read_block_resequence(block,out);
        }
     // Обрезка всех FF хвоста
      fclose(out);
      if (truncflag) {
       out=fopen(filename,"r+b");  // переоткрываем выходной файл
       fseek(out,0,SEEK_SET);  // перематываем файл на начало
       lastpos=0;
       for(filepos=0;;filepos++) {
	c=fgetc(out);
	if (feof(out)) break;
	if (c != 0xff) lastpos=filepos;  // нашли значимый байт
       }
#ifndef WIN32
       ftruncate(fileno(out),lastpos+1);   // обрезаем файл
#else
       _chsize(_fileno(out),lastpos+1);   // обрезаем файл
#endif
       fclose(out);
      }	
    }	
}
printf("\n"); 
    
} 
