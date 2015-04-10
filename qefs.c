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

//*******************************************
//@@@@@@@@@@@@ Головная программа
void main(int argc, char* argv[]) {
  
unsigned char iobuf[14048];
unsigned char cmd_efsh1[16]=  {0x4B, 0x13,0x19, 0};
unsigned char cmd_efsh2[16]=  {0x4B, 0x13,0x16, 0};
unsigned char cmd_efsdata[16]={0x4B, 0x13,0x17,0,0,0,0,0,0,0,0,0};

int iolen,opt;
FILE* out;

char filename[50]="efs.mbn";

#ifndef WIN32
char devname[50]="/dev/ttyUSB0";
#else
char devname[50]="";
#endif
while ((opt = getopt(argc, argv, "p")) != -1) {
  switch (opt) {
   case 'h': 
    printf("\n  Утилита предназначена для работы с разделом efs \n\
Допустимы следующие ключи:\n\n\
-p <tty>  - указывает имя устройства диагностического порта модема\n\
-o <file> - имя файла для сохранения efs\n\
\n");
    return;
    
   case 'o':
     strcpy(filename,optarg);
     break;
    
   case 'p':
    strcpy(devname,optarg);
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

out=fopen(filename,"w");
  
iolen=send_cmd_base(cmd_efsh1, 4, iobuf, 0);
if (iolen != 11) {
  printf("\n Неправильный ответ на команду 4b 13 19\n");
  dump(iobuf,iolen,0);
  return;
}

iolen=send_cmd_base(cmd_efsh2, 4, iobuf, 0);
if (iolen != 11) {
  printf("\n Неправильный ответ на команду 4b 13 16\n");
  dump(iobuf,iolen,0);
  return;
}

// главный цикл получения efs.mbn
while(1) {
  iolen=send_cmd_base(cmd_efsdata, 12, iobuf, 0);
  if (iolen != 532) {
    printf("\n Неправильный ответ на команду 4b 13 17\n");
    dump(iobuf,iolen,0);
    return;
  }
  printf("\n block: %02x %02x %02x %02x",iobuf[8],iobuf[9],iobuf[12],iobuf[13]);
  if (iobuf[8] == 0) break;
  fwrite(iobuf+16,512,1,out);
  memcpy(cmd_efsdata+4,iobuf+8,8);
}

}

