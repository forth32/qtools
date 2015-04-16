#include <stdio.h>
#include <stdlib.h>
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
         int sysitem=-1;     // номер раздела nvram (-1 - все разделы)
char filename[50];        // имя выходного файла


//*******************************************
//* Проверка границ номера раздела
//*******************************************
void verify_item(int item) {

if (item>0xffff) {
  printf("\n Неправильно указан номер раздела - %x\n",item);
  exit(1);
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
  
char buf[134];
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
unsigned int start=0;
unsigned int end=0x10000;

// Создаем каталог nv для сбора файлов
if (mkdir("nv",0777) != 0) 
  if (errno != EEXIST) {
    printf("\n Невозможно создать каталог nv/");
    return;
  }  

// установка границ читаемых записей
if (sysitem != -1) {
  start=sysitem;
  end=start+1;
}  
  
printf("\n");  
for(nv=start;nv<end;nv++) {
  printf("\r NV %04x:  ok",nv); fflush(stdout);
  if (!get_nvitem(nv,buf)) continue;
  if (zeroflag && (test_zero(buf,128) == 0)) continue;
  sprintf(filename,"nv/%04x-%04x.bin",nv,*((unsigned short*)&buf[128]));
  out=fopen(filename,"w");
  fwrite(buf,1,130,out);
  fclose(out);
}  
}  

//*******************************************
//* Запись раздела nvram из буфера
//*******************************************
void write_item(int item, char*buf) {
  
unsigned char cmdwrite[200]={0x27,0,0};
unsigned char iobuf[200];
int iolen;

*((unsigned short*)&cmdwrite[1])=item;
memcpy(cmdwrite+3,buf,130);
iolen=send_cmd_base(cmdwrite,133,iobuf,0);
if ((iolen != 136) || (iobuf[0] != 0x27)) {
  printf("\n Ошибка записи раздела\n");
  dump(iobuf,iolen,0);
}  
}

//*******************************************
//*  Запись раздела nvram из файла
//*******************************************
void write_nvram() {
  
FILE* in;
char buf[135];

in=fopen(filename,"r");
if (in == 0) {
  printf("\n Ошибка открытия файла %s\n",filename);
  exit(1);
}
if (fread(buf,1,130,in) != 130) {
  printf("\n Файл %s слишком мал\n",filename);
  exit(1);
}
fclose (in);
write_item(sysitem,buf);
}


//@@@@@@@@@@@@ Головная программа
void main(int argc, char* argv[]) {

unsigned int opt,i;
  
enum{
  MODE_BACK_NVRAM,
  MODE_READ_NVRAM,
  MODE_SHOW_NVRAM,
  MODE_WRITE_NVRAM,
}; 

int mode=-1;

#ifndef WIN32
char devname[50]="/dev/ttyUSB0";
#else
char devname[50]="";
#endif

while ((opt = getopt(argc, argv, "hp:o:ab:r:w:")) != -1) {
  switch (opt) {
   case 'h': 
    printf("\n  Утилита предназначена для работы с разделом efs \n\
%s [ключи] [параметр или имя файла]\n\
Допустимы следующие ключи:\n\n\
Ключи, определяюще выполняемую операцию:\n\
-bn           - дамп nvram\n\
-ri[z] [item] - чтение всех или только указанной записей nvram в отдельные файлы (z-пропускать пустые записи)\n\
-rd[z]        - дамп указанного раздела nvram (z-отрезать хвостовые нули)\n\
-wi item file - запись раздела item из файла file\n\
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
//----------------------------------------------	 
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

//----------------------------------------------	 
   //  === группа ключей read ==
   case 'r':
     if (mode != -1) {
       printf("\n В командной строке задано более 1 ключа режима работы");
       return;
     }  
     switch(*optarg) {
       case 'i':
	 // rn - чтение в файл одного или всех разделов
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
//----------------------------------------------	 
    //  === группа ключей write ==
    case 'w':
     if (mode != -1) {
       printf("\n В командной строке задано более 1 ключа режима работы");
       return;
     }  
     switch(*optarg) {
       case 'i':
         mode=MODE_WRITE_NVRAM;
	 break;

       default:
	 printf("\n Неправильно задано значение ключа -w\n");
	 return;
      }
      break;
	 
//----------------------------------------------	 
      
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
    if (optind<argc) {
      // указан номер раздела - выделяем его
      sscanf(argv[optind],"%x",&sysitem);
      verify_item(sysitem);
    }  
    read_all_nvram_items();
    break;

  case MODE_SHOW_NVRAM:
    if (optind>=argc) {
      printf("\n Не указан номер раздела nvram");
      break;
    }
    sscanf(argv[optind],"%x",&sysitem);
    verify_item(sysitem);
    nvdump(sysitem);
    break;

  case MODE_WRITE_NVRAM:
    if (optind != (argc-2)) {
       printf("\n Неверное число параметров в командной строке\n");
       exit(1);
    }   
    sscanf(argv[optind],"%x",&sysitem);
    verify_item(sysitem);
    strcpy(filename,argv[optind+1]);
    write_nvram();
    break;
    
  default:
    printf("\n Не указан ключ выполняемой операции\n");
    return;

}    
printf("\n");

}

