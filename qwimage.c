#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <getopt.h>
#include "qcio.h"

// Размер блока записи
#define wbsize 1024

unsigned int cfg0,cfg1; // сохранение конфигурации контроллера

//*****************************************************
//*  Восстановление конфигурации контроллера 
//*****************************************************
void restore_reg() {
  
mempoke(nand_cfg0,cfg0);  
mempoke(nand_cfg1,cfg1);  
}


//*****************************************************
//*  Открытие потока записи с полным стиранием флешки
//*****************************************************
void open() {
  
char cmdbuf[10]={0x13,0x04};
char iobuf[1024];
int iolen;

//if (send_cmd
}
//*******************************************
//@@@@@@@@@@@@ Головная программа
void main(int argc, char* argv[]) {
  
unsigned char iobuf[16384];
unsigned char scmd[2048]={0x30,0,0,0};
unsigned char databuf[16384];
int res;
FILE* in;
int helloflag=0;
char* sptr;
char devname[]="/dev/ttyUSB0";
unsigned int i,opt,iolen,j;
unsigned int adr,badr=0,len;
unsigned int fsize;


while ((opt = getopt(argc, argv, "hp:ika:")) != -1) {
  switch (opt) {
   case 'h': 
    printf("\n  Утилита предназначена для записи сырого образа flash\n\
Допустимы следующие ключи:\n\n\
-p <tty>  - указывает имя устройства последовательного порта для общения с загрузчиком\n\
-i        - запускает процедуру HELLO для инициализации загрузчика\n\
-k #      - выбор чипсета: 0(MDM9x15, по умолчанию), 1(MDM8200), 2(MSM9x00), 3(MDM9x25)\n\
-a #      - начальный адрес (в байтах) для записи \n\
\n");
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
    
   case 'i':
     helloflag=1;
     break;
     
   case 'a':
     sscanf(optarg,"%x",&badr);
     break;
     
   case '?':
   case ':':  
     return;
  }
}  

if (!open_port(devname))  {
   printf("\n - Последовательный порт %s не открывается\n", devname); 
   return; 
}
if (helloflag) hello();
// сохраняем конфигурацию контроллера
cfg0=mempeek(nand_cfg0);
cfg1=mempeek(nand_cfg1);

in=fopen(argv[optind],"rb");
if (in == 0) {
  printf("\nОшибка открытия входного файла\n");
  return;
}

printf("\n Запись из файла %s, стартовый адрес %08x\n",argv[optind],badr);
port_timeout(1000);
for(adr=badr;;adr+=wbsize) {
  len=fread(scmd+12,1,wbsize,in);
  if (len == 0) break;
  *((unsigned int*)&scmd[4])=adr;
  *((unsigned int*)&scmd[8])=len;
  printf("\r W: %08x",adr);
  
  iolen=send_cmd(scmd,12+len,iobuf);  
  if ((iobuf[1] != 0x31)||(iolen == 0)) {
    show_errpacket("unstrem_write() ",iobuf,iolen);
    printf("\n Команда unstream write завершилась с ошибкой");
    return;
  }  
  if (feof(in)) break;  
}
qclose(1);
printf("\n");
restore_reg();
}

