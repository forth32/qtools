//---------------------------------------------------
// Утилита для работы с дефектными блоками flash
//---------------------------------------------------

#include "include.h"

//*********************************
//* Построение списка бедблоков
//*********************************
void defect_list(int start, int len) {
  
FILE* out; 
int blk,pg=0;
int badcount=0;
int vptable=0; // флаг валидности таблицы разделов
int i,npar;
int pstart,plen;
unsigned char ptable[1100]; // таблица разделов

out=fopen("badblock.lst","w");
fprintf(out,"Список дефектных блоков");
if (out == 0) {
  printf("\n Невозможно создать файл badblock.lst\n");
  return;
}

// загружаем таблицу разделов
if (load_ptable(ptable)) 
 if (strncmp(ptable,"\xAA\x73\xEE\x55\xDB\xBD\x5E\xE3",8) == 0) {
   vptable=1;
   npar=*((unsigned int*)&ptable[12]);
 }

printf("\nПостроение списка дефектных блоков в интервале %08x - %08x\n",start,start+len);
for(blk=start;blk<(start+len);blk++) {
 printf("\r Проверка блока %08x",blk); fflush(stdout);
 if (check_block(blk)) {
     printf(" - badblock");
     fprintf(out,"\n%08x",blk);
     if (vptable) 
       // поиск раздела, в котором лежит этот блок
       for(i=0;i<npar;i++) {
         pstart=*((unsigned int*)&ptable[32+28*i]);   // адрес блока начала раздела
         plen=*((unsigned int*)&ptable[36+28*i]);     // размер раздела
	 if ((blk>pstart) && (blk<(pstart+plen))) {
	   printf(" (%s+%x)",ptable+16+28*i+2,blk-pstart);
	   fprintf(out," (%s+%x)",ptable+16+28*i,blk-pstart);
	   break;
	 }  
    }
     badcount++;
     printf("\n");
    }
}     // blk
fprintf(out,"\n");
fclose (out);
printf("\r * Всего дефектных блоков: %i\n",badcount);
}
 


//@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@
void main(int argc, char* argv[]) {

unsigned int start=0,len=0,opt;
int mflag=0, uflag=0, sflag=0;
int dflag=0;
int cfg1;

#ifndef WIN32
char devname[50]="/dev/ttyUSB0";
#else
char devname[50]="";
#endif

while ((opt = getopt(argc, argv, "hp:b:l:dm:u:s:")) != -1) {
  switch (opt) {
   case 'h': 
     printf("\n Утилита для работы с дефектными блоками flash-накопителя\n\
 Допустимы следующие ключи:\n\n\
-p <tty> - последовательный порт для общения с загрузчиком\n\
-b <blk> - начальный номер читаемого блока (по умолчанию 0)\n\
-l <num> - число читаемых блоков, 0 - до конца флешки\n\n\
-d - вывести список имеющихся дефектных блоков\n\
-m blk - пометить блок blk как дефектный\n\
-u blk - снять с блока blk признак дефектности\n\
-s ### - перманентно установить позицию маркера на байт ###\n\
");
    return;

   case 'p':
    strcpy(devname,optarg);
    break;

   case 'b':
     sscanf(optarg,"%x",&start);
     break;

   case 'l':
     sscanf(optarg,"%x",&len);
     break;

   case 'm':
     sscanf(optarg,"%x",&mflag);
     break;

   case 'u':
     sscanf(optarg,"%x",&uflag);
     break;

   case 's':
     sscanf(optarg,"%x",&sflag);
     break;

   case 'd':
     dflag=1;
     break;
     
   case '?':
   case ':':  
     return;
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

hello(0);

// Сброс всех операций контроллера
mempoke(nand_cmd,1); 
mempoke(nand_exec,0x1);
nandwait();

// установка позиции маркера бедблока
if (sflag) {
 badposition=sflag;
 hardware_bad_on();
}

//###################################################
// Режим списка дефектных блоков:
//###################################################

if (dflag) {
  if (len == 0) len=maxblock-start; //  до конца флешки
  defect_list(start,len);
  return;
}  

//###################################################
//# Пометка блока как дефектного
//###################################################
if (mflag) {
  mark_bad(mflag);
  return;
}

//###################################################
//# Снятие с блока дефектного маркера
//###################################################
if (uflag) {
  unmark_bad(mflag);
  return;
}
}
