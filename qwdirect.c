#include <stdio.h>
#include <string.h>
#ifndef WIN32
#include <unistd.h>
#include <getopt.h>
#else
#include <windows.h>
#include "wingetopt.h"
#include "printf.h"
#endif
#include "qcio.h"



//*******************************************
//@@@@@@@@@@@@ Головная программа
void main(int argc, char* argv[]) {
  
unsigned char iobuf[2048];

// ARM-программа для переноса данных в секторный буфер
unsigned char datacmd[8192]={0x11,0x00,0x01,0xf1,0x1f,0x01,0x05,0x4a,
                             0x4f,0xf0,0x00,0x03,0x51,0xf8,0x23,0x00,
			     0x42,0xf8,0x23,0x00,0x01,0x33,0x8f,0x2b,
			     0xf8,0xd3,0x70,0x47,0x00,0x00,0x00,0x01,
			     0xaf,0xf9}; // байты 32-33 - старшие байты адреса контроллера
unsigned char databuf[8192];
unsigned char* oobuf=databuf+4096; // указатель на OOB. Пока жестко задаем 4096/160
unsigned char membuf[1024];
int res;
FILE* in;
int mflag=0;
int oflag=0;
int vflag=0;
int cflag=0;
int eflag=1;
char* sptr;
#ifndef WIN32
char devname[]="/dev/ttyUSB0";
#else
char devname[20]="";
#endif
unsigned int i,opt,iolen,j;
unsigned int block=0,page,sector,len;
unsigned int fsize;
unsigned int oobsize=64;


// параметры флешки
oobsize=16;      // оов на 1 блок
pagesize=2048;   // размер страницы в байтах 


while ((opt = getopt(argc, argv, "hp:k:b:movcez:")) != -1) {
  switch (opt) {
   case 'h': 
    printf("\n  Утилита предназначена для записи сырого образа flash через регистры контроллера\n\
Допустимы следующие ключи:\n\n\
-p <tty>  - указывает имя устройства последовательного порта для общения с загрузчиком\n\
-k #      - выбор чипсета: 0(MDM9x15, по умолчанию), 1(MDM8200), 2(MSM9x00), 3(MDM9x25)\n\
-b #      - начальный номер блока для записи \n\
-m        - устанавливает линуксовый вариант раскладки данных на flash\n\
-o        - запись с ООB, без ключа - входной фалй содержит только данные\n\
-z #      - размер OOB на одну страницу, в байтах, по умолчанию - 64\n\
-e        - включить ЕСС при записи\n\
-v        - проверка записанных данных после записи\n\
-c        - только стереть заданный блок\n\
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
    
   case 'm':
     mflag=1;
     break;
     
   case 'c':
     cflag=1;
     break;
     
   case 'o':
     oflag=1;
     break;
     
   case 'b':
     sscanf(optarg,"%x",&block);
     break;
     
   case 'z':
     sscanf(optarg,"%i",&oobsize);
     break;
     
   case 'v':
     vflag=1;
     break;
     
   case 'e':
     eflag=0;
     break;
     
   case '?':
   case ':':  
     return;
  }
}  

if (oflag && (!mflag)) {
  printf("\n Ключ -o без ключа -m недопустим\n");
  return;
}  

// вписываем адрес контроллера в образ команды копирования
*((unsigned short*)&datacmd[32])=nand_cmd>>16;

if (!oflag) oobsize=0; // для записи без OOB

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

if (!cflag) { 
 in=fopen(argv[optind],"rb");
 if (in == 0) {
   printf("\nОшибка открытия входного файла\n");
   return;
 }
}
else if (optind < argc) {// в режиме стирания входной файл не нужен
  printf("\n С ключом -с недопустим входной файл\n");
  return;
}


get_flash_config(); // читаем параметры флешки

// Сброс контроллера nand
nand_reset();

// режим стирания
if (cflag) {
  block_erase(block);
  return;
}

//ECC on-off
mempoke(nand_ecc_cfg,mempeek(nand_ecc_cfg)&0xfffffffe|eflag); 
mempoke(nand_cfg1,mempeek(nand_cfg1)&0xfffffffe|eflag); 

printf("\n Запись из файла %s, стартовый блок %i\n Режим записи: ",argv[optind],block);
if (mflag) printf("данные+oob\n");
else       printf("только данные\n");
port_timeout(1000);

// цикл по блокам
for(;;block++) {
  // стираем блок
  block_erase(block);
  // цикл по страницам
  for(page=0;page<ppb;page++) {
    len=fread(databuf,1,pagesize+(spp*oobsize),in);  // образ страницы - page+oob
    if (len < (pagesize+(spp*oobsize))) goto wdone; // неполный хвост файла игнорируем
    printf("\r block: %04x   page:%02x",block,page); fflush(stdout);
    setaddr(block,page);
    // цикл по секторам
    if (mflag) mempoke(nand_cmd,0x39); // запись data+oob
               mempoke(nand_cmd,0x36); // page program
    for(sector=0;sector<spp;sector++) {
      memset(datacmd+34,0xff,sectorsize+28); // заполнитель секторного буфера

      if (mflag) {
	// линуксовый (китайский извратный) вариант раскладки данных
       if (sector < (spp-1)) { //первые n секторов
         memcpy(datacmd+34,databuf+sector*(sectorsize+4),sectorsize+4); // данные сектора
         //  для первых секторов oob не копируем
       } 
       else { // последний сектор
         memcpy(datacmd+34,databuf+(spp-1)*(sectorsize+4),sectorsize-4*(spp-1)); // данные последнего сектора
         if (oflag) memcpy(datacmd+34+sectorsize-4*(spp-1),databuf+pagesize,16); // тэг yaffs, остальная часть OOB игнорируется
       }        
       iolen=send_cmd(datacmd,34+sectorsize+oobsize,iobuf);  // пересылаем сектор в секторный буфер
       mempoke(nand_exec,0x1);
       nandwait();
      }
      else {
	// запись только блоков данных

       memcpy(datacmd+34,databuf+sector*sectorsize,sectorsize); // данные сектора
       iolen=send_cmd(datacmd,34+sectorsize,iobuf);  // пересылаем сектор в секторный буфер
       mempoke(nand_exec,0x1);
       nandwait();
     }
     // конец цикла записи по секторам
    }
    // верификация данных, если надо
    if (vflag) {
      printf("\r");
      setaddr(block,page);
      mempoke(nand_cmd,0x34); // чтение data+ecc+spare
      for(sector=0;sector<spp;sector++) {
       mempoke(nand_exec,0x1);
       nandwait();
       memread(membuf,sector_buf,sectorsize+oobsize);
       if (mflag) {
	// верификация в линуксовом формате
	  if (sector != (spp-1)) {
	    // все сектора кроме последнего
	    for (i=0;i<sectorsize+4;i++) 
	      if (membuf[i] != databuf[sector*(sectorsize+4)+i])
                 printf("! block: %04x  page:%02x  sector:%i  byte: %03x  %02x != %02x\n",
			block,page,sector,i,membuf[i],databuf[sector*(sectorsize+4)+i]); 
	  }  
	  else {
	      // последний сектор
	    for (i=0;i<sectorsize-4*(spp-1);i++) 
	      if (membuf[i] != databuf[(spp-1)*(sectorsize+4)+i])
                 printf("! block: %04x  page:%02x  sector:%i  byte: %03x  %02x != %02x\n",
			block,page,sector,i,membuf[i],databuf[(spp-1)*(sectorsize+4)+i]); 
	    
	 }    
       }   	//mflag
       //----------------------------------------------------------
       else {
	 // верификация в стандартном формате
	    for (i=0;i<sectorsize;i++) 
	      if (membuf[i] != databuf[sector*sectorsize+i])
                 printf("! block: %04x  page:%02x  sector:%i  byte: %03x  %02x != %02x\n",
			block,page,sector,i,membuf[i],databuf[sector*sectorsize+i]); 
	    
      }
    }  // конец секторного цикла верификации
//       printf("\n");
    } // vflag 
  // конец цикла по страницам 
  }
// конец цикла по блокам  
} 

wdone:

printf("\n");
}

