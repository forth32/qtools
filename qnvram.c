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

