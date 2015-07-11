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
#include "getopt.h"
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
void read_block(int block,int cwsize,FILE* out) {

unsigned char iobuf[14096];  
int page,sec;
 // цикл по страницам
for(page=0;page<ppb;page++)  {

  setaddr(block,page);
  // по секторам  
  for(sec=0;sec<spp;sec++) {
   mempoke(nand_exec,0x1); 
   nandwait();
   // выгребаем порцию данных
   memread(iobuf,sector_buf, cwsize);
   fwrite(iobuf,1,cwsize,out);
  }
 } 
} 

//****************************************************************
//* Чтение блока данных с восстановлением китайского изврата
//****************************************************************
void read_block_resequence(int block, FILE* out) {
unsigned char iobuf[4096];  
int page,sec;
 // цикл по страницам
for(page=0;page<ppb;page++)  {

  setaddr(block,page);
  // по секторам  
  for(sec=0;sec<spp;sec++) {
   mempoke(nand_exec,0x1); 
   nandwait();
   // выгребаем порцию данных
   memread(iobuf,sector_buf,sectorsize+4);
   if (sec != (spp-1)) 
     // Для непоследних секторов
     fwrite(iobuf,1,sectorsize+4,out);    // Тело сектора + 4 байта OBB
   else 
     // для последнего сектора
     fwrite(iobuf,1,sectorsize-4*(spp-1),out);   // Тело сектора - хвост oob
  }
 } 
} 



//*****************************
//* чтение сырого флеша
//*****************************
void read_raw(int start,int len,int cwsize,FILE* out, unsigned int rflag) {
  
int block;  

printf("\n Чтение блоков %08x - %08x",start,start+len-1);
printf("\n Формат данных: %i+%i\n",sectorsize,cwsize-sectorsize);
// главыный цикл
// по блокам
for (block=start;block<(start+len);block++) {
  printf("\r Блок: %08x",block); fflush(stdout);
  if (rflag != 2) read_block(block,cwsize,out);
  else            read_block_resequence(block,out); 
} 
printf("\n"); 
}


//@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@
void main(int argc, char* argv[]) {
  
unsigned char partname[17]={0}; // имя раздела
unsigned char filename[300]="qflash.bin";
unsigned int i,block,filepos,lastpos;
unsigned char c;
unsigned int start=0,len=1,opt;
unsigned int cwsize;
unsigned int partlist[60]; // список разделов, разрешенных для чтения

FILE* out;
FILE* part=0;
int partflag=0;  // 0 - сырой флеш, 1 - таблица разделов из файла, 2 - таблица разделов из флеша
int eccflag=1;  // 1 - отключить ECC,  0 - включить
int partnumber=-1; // флаг выбра раздела для чтения, -1 - все разделы, 1 - по списку
int rflag=0;     // формат разделов: 0 - авто, 1 - стандартный, 2 - линуксокитайский
int listmode=0;    // 1- вывод карты разделов
int truncflag=0;  //  1 - отрезать все FF от конца раздела
int xflag=0;

int attr; // арибуты
int npar; // число разедлов в таблице


#ifndef WIN32
char devname[50]="/dev/ttyUSB0";
#else
char devname[50]="";
#endif
unsigned char ptable[1100]; // таблица разделов

// параметры флешки
oobsize=0;      // оов на 1 страницу
memset(partlist,0,sizeof(partlist)); // очищаем список разрешенных к чтению разделов

while ((opt = getopt(argc, argv, "hp:b:l:o:xs:ef:mtk:r:z:")) != -1) {
  switch (opt) {
   case 'h': 
     
printf("\n Утилита предназначена для чтения образа флеш через модифицированный загрузчик\n\
 Допустимы следующие ключи:\n\n\
-p <tty> - последовательный порт для общения с загрузчиком\n\
-e - включить коррекцию ECC при чтении\n\
-x - читать полный сектор - данные+oob (без ключа - только данные)\n\n\
-k # - код чипсета (-kl - получить список кодов)\n\
-z # - размер OOB на страницу, в байтах (перекрывает автоопределенный размер)\n\
 Для режима неформатированного чтения:\n\
-b <blk> - начальный номер читаемого блока (по умолчанию 0)\n\
-l <num> - число читаемых блоков, 0 - до конца флешки\n\
-o <file> - имя выходного файла (по умолчанию qflash.bin)\n\n\
 Для режима чтения разделов:\n\
-s <file> - взять карту разделов из файла\n\
-s @ - взять карту разделов из флеша\n\
-s - - взять карту разделов из файла ptable/current-r.bin\n\
-f n - читать только раздел c номером n (может быть указан несколько раз для формирования списка разделов)\n\
-t - отрезать все FF за последним значимым байтом раздела\n\
-r <x> - формат данных:\n\
    -rs - стандартный формат (512-байтные блоки)\n\
    -rl - линуксовый формат (516-байтные блоки, кроме последнего на странице)\n\
    -ra - (по умолчанию и только для режима разделов) автоопределение формата по атрибуту раздела\n\
-m - вывести полную карту разделов\n");
    return;
    
   case 'k':
    define_chipset(optarg);
    break;
    
   case 'p':
    strcpy(devname,optarg);
    break;
    
   case 'o':
    strcpy(filename,optarg);
    break;
    
   case 'b':
     sscanf(optarg,"%x",&start);
     break;

   case 'l':
     sscanf(optarg,"%x",&len);
     break;

   case 'z':
     sscanf(optarg,"%i",&oobsize);
     break;
     
   case 'r':
     switch(*optarg) {
       case 'a':
	 rflag=0;   // авто
	 break;     
       case 's':
	 rflag=1;   // стандартный
	 break;
       case 'l':
	 rflag=2;   // линуксовый
	 break;
       default:
	 printf("\n Недопустимое значение ключа r\n");
	 return;
     } 
     break;
     
   case 'x':
     xflag=1;
     break;
     
   case 's':
     if (optarg[0] == '@')  partflag=2; // загружаем таблицу разделов из флеша
     else {
       // загружаем таблицу разделов из файла
       partflag=1;
       if (optarg[0] == '-')   part=fopen("ptable/current-r.bin","rb");
       else part=fopen(optarg,"rb");
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
     partnumber=1;
     sscanf(optarg,"%i",&i);
     partlist[i]=1;
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


if (!open_port(devname))  {
#ifndef WIN32
   printf("\n - Последовательный порт %s не открывается\n", devname); 
#else
   printf("\n - Последовательный порт COM%s не открывается\n", devname); 
#endif
   return; 
}

if ((truncflag == 1)&&(cwsize>sectorsize)) {
  printf("\nКлючи -t и -x несовместимы\n");
  return;
}  
hello(0);
cwsize=sectorsize;
if (xflag) cwsize+=oobsize/spp; // наращиваем размер codeword на размер порции OOB на каждый сектор

if (partflag == 2) load_ptable(ptable); // загружаем таблицу разделов

mempoke(nand_cfg1,mempeek(nand_cfg1)&0xfffffffe|eccflag); // ECC on/off
//mempoke(nand_cs,4); // data mover
//mempoke(nand_cs,0); // data mover

mempoke(nand_cmd,1); // Сброс всех операций контроллера
mempoke(nand_exec,0x1);
nandwait();
// устанавливаем код команды
//if (cwsize == sectorsize) mempoke(nand_cmd,0x32); // чтение только данных
//else  

// В принципе, проще всегда читать data+oob, а уже программа сама разберется, что из этого реально нужно.
mempoke(nand_cmd,0x34); // чтение data+ecc+spare

// чистим секторный буфер
for(i=0;i<cwsize;i+=4) mempoke(sector_buf+i,0xffffffff);

//###################################################
// Режим чтения сырого флеша
//###################################################

if (len == 0) len=maxblock-start; //  до конца флешки

if (partflag == 0) { 
  out=fopen(filename,"wb");
  read_raw(start,len,cwsize,out,rflag);
  fclose(out);
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
printf("\n Версия таблицы разделов: %i",*((unsigned int*)&ptable[8]));
if ((partnumber != -1) && (partnumber>=npar)) {
  printf("\nНедопустимый номер раздела: %i, всего разделов %i\n",partnumber,npar);
  return;
}  
printf("\n #  адрес    размер   атрибуты ------ Имя------\n");     
for(i=0;i<npar;i++) {
   // разбираем элементы таблицы разделов
      strncpy(partname,ptable+16+28*i,16);       // имя
      start=*((unsigned int*)&ptable[32+28*i]);   // адрес
      len=*((unsigned int*)&ptable[36+28*i]);     // размер
      attr=*((unsigned int*)&ptable[40+28*i]);    // атрибуты
      if (((start+len) >maxblock)||(len == 0xffffffff)) len=maxblock-start; // если длина - FFFF, или выходит за пределы флешки
  // Выводим описание раздела - для всех разделов или для конкретного заказанного
    if ((partnumber == -1) || (partlist[i]==1))  printf("\r%02i %08x  %08x  %08x  %s\n",i,start,len,attr,partname);
  // Читаем раздел - если не указан просто вывод карты. 
    if (listmode == 0) 
      // Все разделы или один конкретный  
      if ((partnumber == -1) || (partlist[i]==1)) {
        // формируем имя файла
        if (cwsize == sectorsize) sprintf(filename,"%02i-%s.bin",i,partname); 
        else                   sprintf(filename,"%02i-%s.oob",i,partname);  
        if (filename[4] == ':') filename[4]='-';    // заменяем : на -
        out=fopen(filename,"wb");  // открываем выходной файл
        for(block=start;block<(start+len);block++) {
          printf("\r * R: блок %08x (%i%%)",block-start,(block-start+1)*100/len); fflush(stdout);
	  
    //	  Собственно чтение блока
	  switch (rflag) {
	    case 0: // автовыбор формата
               if ((attr != 0x1ff)||(cwsize>sectorsize)) 
	       // сырое чтение или чтение неизварщенных разделов
	           read_block(block,cwsize,out);
	       else 
	       // чтение китайсколинуксовых разделов
	           read_block_resequence(block,out);
	       break;
	       
	    case 1: // стандартный формат  
	      read_block(block,cwsize,out);
	      break;
	      
	    case 2: // китайсколинуксовый формат  
               read_block_resequence(block,out);
	      break;
	 }  

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
