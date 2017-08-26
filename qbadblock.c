//---------------------------------------------------
// Утилита для работы с дефектными блоками flash
//---------------------------------------------------

#include "include.h"

//*********************************
//* Построение списка бедблоков
//*********************************
void defect_list(int start, int len) {
  
FILE* out; 
int blk;
int badcount=0;
int pn;

out=fopen("badblock.lst","w");
if (out == 0) {
  printf("\n Невозможно создать файл badblock.lst\n");
  return;
}
fprintf(out,"Список дефектных блоков");

// загружаем таблицу разделов с флешки
load_ptable("@");

printf("\nПостроение списка дефектных блоков в интервале %08x - %08x\n",start,start+len);

// главный цикл по блокам
for(blk=start;blk<(start+len);blk++) {
 printf("\r Проверка блока %08x",blk); fflush(stdout);
 if (check_block(blk)) {
     printf(" - badblock");
     fprintf(out,"\n%08x",blk);
     // увеличиваем счетчик бедблоков
     badcount++;
     // выводим имя раздела с этим блоком
     if (validpart) {
       // поиск раздела, в котором лежит этот блок
       pn=block_to_part(blk);
       if (pn != -1) {
         printf(" (%s+%x)",part_name(pn),blk-part_start(pn));
         fprintf(out," (%s+%x)",part_name(pn),blk-part_start(pn));
       }       	 
    }
    printf("\n");
  }
}     // blk
fprintf(out,"\n");
fclose (out);
printf("\r * Всего дефектных блоков: %i\n",badcount);
}
 

//********************************************************8 
//* Сканирование на ошибки ECC
//* flag=0 - все ошибки, 1 - только корректируемые
//********************************************************8 
void ecc_scan(start,len,flag) {
  
int blk;
int errcount=0;
int pg,sec;
int stat;
FILE* out; 

printf("\nПостроение списка ошибок ЕСС в интервале %08x - %08x\n",start,start+len);

out=fopen("eccerrors.lst","w");
fprintf(out,"Список ошибок ЕСС");
if (out == 0) {
  printf("\n Невозможно создать файл eccerrors.lst\n");
  return;
}

// включаем ЕСС
mempoke(nand_ecc_cfg,mempeek(nand_ecc_cfg)&0xfffffffe); 
mempoke(nand_cfg1,mempeek(nand_cfg1)&0xfffffffe); 

for(blk=start;blk<(start+len);blk++) {
 printf("\r Проверка блока %08x",blk); fflush(stdout);
 if (check_block(blk)) {
     printf(" - badblock\n");
     continue;
 }
 bch_reset(); 
 for (pg=0;pg<ppb;pg++) {
   setaddr(blk,pg);
   mempoke(nand_cmd,0x34); // чтение data
   for (sec=0;sec<spp;sec++) {
    mempoke(nand_exec,0x1);
    nandwait();
    stat=check_ecc_status();
    if (stat == 0) continue;
    if ((stat == -1) && (flag == 1)) continue;
    if (stat == -1) { 
      printf("\r!  Блок %x  Страница %d  сектор %d: некорректируемая ошибка чтения\n",blk,pg,sec);
      fprintf(out,"\r!  Блок %x  Страница %d  сектор %d: некорректируемая ошибка чтения\n",blk,pg,sec);
    }  
    else {
      printf("\r!  Блок %x  Страница %d  сектор %d: скорректировано бит: %d\n",blk,pg,sec,stat);
      fprintf(out,"\r!  Блок %x  Страница %d  сектор %d: скорректировано бит: %d\n",blk,pg,sec,stat);
    }  
    errcount++;
   }
 }
} 
printf("\r * Всего ошибок: %i\n",errcount);
}
 

//@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@
void main(int argc, char* argv[]) {

unsigned int start=0,len=0,opt;
int mflag=0, uflag=0, sflag=0;
int dflag=0;
int badloc=0;
int eflag=-1;

#ifndef WIN32
char devname[50]="/dev/ttyUSB0";
#else
char devname[50]="";
#endif

while ((opt = getopt(argc, argv, "hp:b:l:dm:k:u:s:e:")) != -1) {
  switch (opt) {
   case 'h': 
     printf("\n Утилита для работы с дефектными блоками flash-накопителя\n\
 Допустимы следующие ключи:\n\n\
-p <tty> - последовательный порт для общения с загрузчиком\n\
-k # - код чипсета (-kl - получить список кодов)\n\
-b <blk> - начальный номер читаемого блока (по умолчанию 0)\n\
-l <num> - число читаемых блоков, 0 - до конца флешки\n\n\
-d - вывести список имеющихся дефектных блоков\n\
-e# - вывести список ошибок ЕСС, #=0 - все ошибки, 1 - только корректируемые\n\
-m blk - пометить блок blk как дефектный\n\
-u blk - снять с блока blk признак дефектности\n\
-s L### - перманентно установить позицию маркера на байт ###, L=U(user) или S(spare)\n\
");
    return;

   case 'k':
    define_chipset(optarg);
    break;

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
     parse_badblock_arg(optarg, &sflag, &badloc);
     break;

   case 'd':
     dflag=1;
     break;

   case 'e':
     sscanf(optarg,"%i",&eflag);
     if ((eflag != 0) && (eflag != 1)) {
       printf("\n Неправильное значение ключа -e\n");
       return;
     }  
     break;
     
   case '?':
   case ':':  
     return;
  }
}  
if ((eflag != -1) && (dflag != 0)) {
  printf("\n Ключи -e и -d несовместимы\n");
  return;
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
if (sflag) set_badmark_pos (sflag, badloc);

//###################################################
// Режим списка дефектных блоков:
//###################################################

if (dflag) {
  if (len == 0) len=maxblock-start; //  до конца флешки
  defect_list(start,len);
  return;
}  

//###################################################
//#  Режим построения списка ошибок ЕСС
//###################################################
if (eflag != -1) {
  if (len == 0) len=maxblock-start; //  до конца флешки
  ecc_scan(start,len,eflag);
  return;
}  

//###################################################
//# Пометка блока как дефектного
//###################################################
if (mflag) {
  if (mark_bad(mflag)) {
   printf("\n Блок %x отмечен как дефектный\n",mflag);
  }
  else printf("\n Блок %x уже является дефектным\n",mflag);	
  return;
}

//###################################################
//# Снятие с блока дефектного маркера
//###################################################
if (uflag) {
  if (unmark_bad(uflag)) {
     printf("\n Маркер блока %x удален\n",uflag);
  }
  else printf("\n Блок %x не является дефектным\n",uflag);
  return;
}
}
