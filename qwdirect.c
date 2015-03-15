#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <getopt.h>
#include "qcio.h"


unsigned int cfg0,cfg1; // сохранение конфигурации контроллера

//*****************************************************
//*  Восстановление конфигурации контроллера 
//*****************************************************
void restore_reg() {
  
mempoke(nand_cfg0,cfg0);  
mempoke(nand_cfg1,cfg1);  
}


//*******************************************
//@@@@@@@@@@@@ Головная программа
void main(int argc, char* argv[]) {
  
unsigned char iobuf[2048];
unsigned char datacmd[8192]={0x11,0x00,0x01,0xf1,0x1f,0x01,0x05,0x4a,
                             0x4f,0xf0,0x00,0x03,0x51,0xf8,0x23,0x00,
			     0x42,0xf8,0x23,0x00,0x01,0x33,0x8f,0x2b,
			     0xf8,0xd3,0x70,0x47,0x00,0x00,0x00,0x01,
			     0xaf,0xf9};
unsigned char databuf[8192];
unsigned char* oobuf=databuf+4096; // указатель на OOB. Пока жестко задаем 4096/160
int res;
FILE* in;
int helloflag=0;
char* sptr;
char devname[]="/dev/ttyUSB0";
unsigned int i,opt,iolen,j;
unsigned int block=0,page,sector,len;
unsigned int fsize;


while ((opt = getopt(argc, argv, "hp:ikb:")) != -1) {
  switch (opt) {
   case 'h': 
    printf("\n  Утилита предназначена для записи сырого образа flash через регистры контроллера\n\
Допустимы следующие ключи:\n\n\
-p <tty>  - указывает имя устройства последовательного порта для общения с загрузчиком\n\
-i        - запускает процедуру HELLO для инициализации загрузчика\n\
-k #      - выбор чипсета: 0(MDM9x15, по умолчанию), 1(MDM8200), 2(MSM9x00), 3(MDM9x25)\n\
-b #      - начальный номер блока для записи \n\
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
     sscanf(optarg,"%x",&block);
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

printf("\n Запись из файла %s, стартовый блок %i\n",argv[optind],block);
port_timeout(1000);

// цикл по блокам
for(;;block++) {
  // цикл по страницам
  for(page=0;page<64;page++) {
    len=fread(databuf,1,4096+160,in);  // образ страницы - 4096+160
    if (len < (4096+160)) break; // неполный хвост файла игнорируем
    printf("\r block: %04x   page:%02x",block,page);
    setaddr(block,page);
    // цикл по секторам
    for(sector=0;sector<8;sector++) {
      memcpy(datacmd+34,databuf+sector*512,512); // данные сектора
      memcpy(datacmd+34+512,databuf+8*512+sector*20,20); // oob сектора
      iolen=send_cmd(datacmd,34+512+20,iobuf);  // пересылаем сектор в секторный буфер
      mempoke(nand_cmd,0x39); // запись data+oob
      mempoke(nand_exec,0x1);
      nandwait();
    }
  }
}  
printf("\n");
restore_reg();
}

