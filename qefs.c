#include <stdio.h>
#include <string.h>
#include <errno.h>
#ifndef WIN32
#include <unistd.h>
#include <getopt.h>
#else
#include <windows.h>
//#include <io.h>
#include "wingetopt.h"
#include "printf.h"
#endif
#include "qcio.h"

//%%%%%%%%%  Общие переменные %%%%%%%%%%%%%%%%55

unsigned int altflag=0;   // флаг альтернативной EFS
unsigned int fixname=0;   // индикатор явного уазания имени файла
unsigned int zeroflag=0;  // флаг пропуска пустых записей nvram
char filename[50];        // имя выходного файла


//****************************************************
//* Проверка массива на ненулевые значения
//*
//*  0 - в массиве одни нули
//*  1 - в массиве есть ненули
//****************************************************
int test_zero(unsigned char* buf, int len) {
  
int i;
for (i=0;i<len;i++)
  if (buf[i] != 0) return 1;
return 0;
}


//****************************************************
//* Чтение дампа EFS (efs.mbn)
//****************************************************

void back_efs() {

unsigned char iobuf[14048];
unsigned char cmd_efsh1[16]=  {0x4B, 0x13,0x19, 0};
unsigned char cmd_efsopen[16]=  {0x4B, 0x13,0x16, 0};
unsigned char cmd_efsdata[16]={0x4B, 0x13,0x17,0,0,0,0,0,0,0,0,0};
unsigned char cmd_efsclose[16]=  {0x4B, 0x13,0x18, 0};

int iolen,i;
FILE* out;

// настройка на альтернативную EFS
if (altflag) {
 cmd_efsh1[1]=0x3e;
 cmd_efsopen[1]=0x3e;
 cmd_efsdata[1]=0x3e;
 cmd_efsclose[1]=0x3e;
 if (!fixname) strcpy(filename,"efs_alt.mbn");
}
// настройка на стандартную EFS
else strcpy(filename,"efs.mbn");
   
out=fopen(filename,"w");
  
iolen=send_cmd_base(cmd_efsh1, 4, iobuf, 0);
if ((iolen != 11) || test_zero(iobuf+3,5)) {
  printf("\n Неправильный ответ на команду 19\n");
  dump(iobuf,iolen,0);
  return;
}

iolen=send_cmd_base(cmd_efsopen, 4, iobuf, 0);
if ((iolen != 11) || test_zero(iobuf+3,5)) {
  printf("\n Неправильный ответ на команду открытия (16)\n");
  dump(iobuf,iolen,0);
  return;
}
printf("\n");

// главный цикл получения efs.mbn
while(1) {
  iolen=send_cmd_base(cmd_efsdata, 12, iobuf, 0);
  if (iolen != 532) {
    printf("\n Неправильный ответ на команду 4b 13 17\n");
    dump(iobuf,iolen,0);
    return;
  }
  printf("\r Читаем раздел: %i   блок: %08x",*((unsigned int*)&iobuf[8]),*((unsigned int*)&iobuf[12]));
  fflush(stdout);
  if (iobuf[8] == 0) break;
  fwrite(iobuf+16,512,1,out);
  memcpy(cmd_efsdata+4,iobuf+8,8);
}
// закрываем EFS
iolen=send_cmd_base(cmd_efsclose,4,iobuf,0);
if ((iolen != 11) || test_zero(iobuf+3,5)) {
  printf("\n Неправильный ответ на команду закрытия (18)\n");
  dump(iobuf,iolen,0);
}
}

//*******************************************
//*   Получение раздела nvram из модема
//*
//*  0 -не прочитался
//*  1 -прочитался
//*******************************************
int get_nvitem(int item, char* buf) {

unsigned char iobuf[140];
unsigned char cmd_rd[16]=  {0x26, 0,0};
unsigned int iolen;

*((unsigned short*)&cmd_rd[1])=item;
iolen=send_cmd_base(cmd_rd,3,iobuf,0);
if (iolen != 136) return 0;
memcpy(buf,iobuf+3,130);
return 1;
}

//*******************************************
//*  Дамп раздела nvram
//*******************************************
void nvdump(int item) {
  
char buf[130];
int len;

if (!get_nvitem(item,buf)) {
  printf("\n! Раздел %04x не читается\n",item);
  return;
}
if (zeroflag && (test_zero(buf,128) == 0)) {
  printf("\n! Раздел %04x пуст\n",item);
  return;
}  
printf("\n *** NVRAM: Раздел %04x  атрибут %04x\n--------------------------------------------------\n",
       item,*((unsigned short*)&buf[128]));

// отрезаем хвостовые нули 
if (zeroflag) {
 for(len=127;len>=0;len--)
   if (buf[len]!=0) break;
 len++;  
}
else len=128;

dump(buf,len,0);
}

//*******************************************
//*  Чтение всех заисей NVRAM
//*******************************************
void read_all_nvram_items() {
  
char buf[130];
unsigned int nv;
char filename[50];
FILE* out;

// Создаем каталог nv для сбора файлов
if (mkdir("nv",0777) != 0) 
  if (errno != EEXIST) {
    printf("\n Невозможно создать каталог nv/");
    return;
  }  

printf("\n");  
for(nv=0;nv<0x10000;nv++) {
  printf("\r NV %04x",nv); fflush(stdout);
  if (!get_nvitem(nv,buf)) continue;
  if (zeroflag && (test_zero(buf,128) == 0)) continue;
  sprintf(filename,"nv/%04x-%04x",nv,*((unsigned short*)&buf[128]));
  out=fopen(filename,"w");
  fwrite(buf,1,128,out);
  fclose(out);
}  
}  
  
//@@@@@@@@@@@@ Головная программа
void main(int argc, char* argv[]) {

unsigned int opt,i;
  
enum{
  MODE_BACK_EFS,
  MODE_BACK_NVRAM,
  MODE_READ_NVRAM,
  MODE_SHOW_NVRAM
}; 

int mode=-1;

#ifndef WIN32
char devname[50]="/dev/ttyUSB0";
#else
char devname[50]="";
#endif

while ((opt = getopt(argc, argv, "hpo:ab:r:")) != -1) {
  switch (opt) {
   case 'h': 
    printf("\n  Утилита предназначена для работы с разделом efs \n\
%s [ключи] [параметр или имя файла]\n\
Допустимы следующие ключи:\n\n\
Ключи, определяюще выполняемую операцию:\n\
-be       - дамп efs\n\
-bn       - дамп nvram\n\
-rn[z]    - чтение всех записей nvram в отдельные файлы (z-пропускать пустые записи)\n\
-rd[z]    - дамп указанного раздела nvram (z-отрезать хвостовые нули)\n\
\n\
Ключи-модификаторы:\n\
-p <tty>  - указывает имя устройства диагностического порта модема\n\
-a        - использовать альтернативную EFS\n\
-o <file> - имя файла для сохранения efs\n\
\n",argv[0]);
    return;
    
   case 'o':
     strcpy(filename,optarg);
     fixname=1;
     break;
   //  === группа ключей backup ==
   case 'b':
     if (mode != -1) {
       printf("\n В командной строке задано более 1 ключа режима работы");
       return;
     }  
     switch(*optarg) {
       case 'e':
         mode=MODE_BACK_EFS;
         break;
     
       case 'n':
         mode=MODE_BACK_NVRAM;
         break;
	 
       default:
	 printf("\n Неправильно задано значение ключа -b\n");
	 return;
      }
      break;

   //  === группа ключей read ==
   case 'r':
     if (mode != -1) {
       printf("\n В командной строке задано более 1 ключа режима работы");
       return;
     }  
     switch(*optarg) {
       case 'n':
         mode=MODE_READ_NVRAM;
	 if (optarg[1] == 'z') zeroflag=1;
         break;

       case 'd':
         mode=MODE_SHOW_NVRAM;
	 if (optarg[1] == 'z') zeroflag=1;
         break;
	 
	 
       default:
	 printf("\n Неправильно задано значение ключа -r\n");
	 return;
      }
      break;
      
   case 'p':
    strcpy(devname,optarg);
    break;

   case 'a':
     altflag=1;
     break;
     
   case '?':
   case ':':  
     return;
  }
}  

if (mode == -1) {
  printf("\n Не указан ключ выполняемой операции\n");
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


//////////////////

switch (mode) {
  case MODE_BACK_EFS:
    back_efs();
    break;
    
  case MODE_READ_NVRAM:
    read_all_nvram_items();
    break;

  case MODE_SHOW_NVRAM:
    if (optind>=argc) {
      printf("\n Не указан номер раздела nvram");
      break;
    }
    sscanf(argv[optind],"%x",&i);
    nvdump(i);
    break;
    
  default:
    printf("\n Не указан ключ выполняемой операции\n");
    return;

}    
printf("\n");

}

