#include <stdio.h>
#include <string.h>
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

//*******************************************
//@@@@@@@@@@@@ Головная программа
void main(int argc, char* argv[]) {
  
unsigned char iobuf[14048];
unsigned char cmd_efsh1[16]=  {0x4B, 0x13,0x19, 0};
unsigned char cmd_efsopen[16]=  {0x4B, 0x13,0x16, 0};
unsigned char cmd_efsdata[16]={0x4B, 0x13,0x17,0,0,0,0,0,0,0,0,0};
unsigned char cmd_efsclose[16]=  {0x4B, 0x13,0x18, 0};

unsigned int altflag=0;
unsigned int fixname=0;
int iolen,opt,i;
FILE* out;

char filename[50]="efs.mbn";

#ifndef WIN32
char devname[50]="/dev/ttyUSB0";
#else
char devname[50]="";
#endif
while ((opt = getopt(argc, argv, "po:a")) != -1) {
  switch (opt) {
   case 'h': 
    printf("\n  Утилита предназначена для работы с разделом efs \n\
Допустимы следующие ключи:\n\n\
-p <tty>  - указывает имя устройства диагностического порта модема\n\
-a        - использовать альтернативную EFS\n\
-o <file> - имя файла для сохранения efs\n\
\n");
    return;
    
   case 'o':
     strcpy(filename,optarg);
     fixname=1;
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

// настройка на альтернативную EFS
if (altflag) {
 cmd_efsh1[1]=0x3e;
 cmd_efsopen[1]=0x3e;
 cmd_efsdata[1]=0x3e;
 cmd_efsclose[1]=0x3e;
 if (!fixname) strcpy(filename,"efs_alt.mbn");
}   
   
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
  return;
}
printf("\n");

}

