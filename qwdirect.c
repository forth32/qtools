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
  

// ARM-программа для переноса данных в секторный буфер
unsigned char datacmd[8192]={0x11,0x00,0x01,0xf1,0x1f,0x01,0x05,0x4a,
                             0x4f,0xf0,0x00,0x03,0x51,0xf8,0x23,0x00,
			     0x42,0xf8,0x23,0x00,0x01,0x33,0x8f,0x2b,
			     0xf8,0xd3,0x70,0x47,0x00,0x00,0x00,0x01,
			     0xaf,0xf9}; // байты 32-33 - старшие байты адреса контроллера
unsigned char iobuf[2048]; // буфер ответа команд
unsigned char srcbuf[8192]; // буфер страницы 
unsigned char membuf[1024]; // буфер верификации
unsigned char databuf[8192], oobuf[8192]; // буферы сектора данных и ООВ
unsigned char vbuf[2048];
unsigned int fsize;
int res;
FILE* in;
int vflag=0;
int cflag=0;
unsigned int flen=0;
char* sptr;
#ifndef WIN32
char devname[]="/dev/ttyUSB0";
#else
char devname[20]="";
#endif
unsigned int i,opt,iolen,j;
unsigned int block,page,sector,len;
unsigned int startblock=0;
int wmode=0; // режим записи

#define w_standart 0
#define w_linux    1
#define w_yaffs    2
#define w_image    3

while ((opt = getopt(argc, argv, "hp:k:b:f:vcz:l:")) != -1) {
  switch (opt) {
   case 'h': 
    printf("\n  Утилита предназначена для записи сырого образа flash через регистры контроллера\n\
Допустимы следующие ключи:\n\n\
-p <tty>  - указывает имя устройства последовательного порта для общения с загрузчиком\n\
-k #      - выбор чипсета: 0(MDM9x15, по умолчанию), 1(MDM8200), 2(MSM9x00), 3(MDM9x25)\n\
-b #      - начальный номер блока для записи \n\
-f <x>    - выбор формата записи:\n\
        -fs (по умолчанию) - запись только секторов данных\n\
        -fl - запись только секторов данных в линуксовом формате\n\
        -fy - запись yaffs2-образов\n\
        -fi - запись сырого образа данные+OOB, как есть, без пересчета ЕСС\n\
-z #      - размер OOB на одну страницу, в байтах (перекрывает автоопределенный размер)\n\
-l #      - число записываемых блоков, по умолчанию - до конца входного файла\n\
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
    
   case 'c':
     cflag=1;
     break;
     
   case 'b':
     sscanf(optarg,"%x",&startblock);
     break;
     
   case 'z':
     sscanf(optarg,"%i",&oobsize);
     break;
     
   case 'l':
     sscanf(optarg,"%i",&flen);
     break;
     
   case 'v':
     vflag=1;
     break;
     
   case 'f':
     switch(*optarg) {
       case 's':
        wmode=w_standart;
	break;
	
       case 'l':
        wmode=w_linux;
	break;
	
       case 'y':
        wmode=w_yaffs;
	break;
	
       case 'i':
        wmode=w_image;
	break;
	
       default:
	printf("\n Неправильное значение ключа -f\n");
	return;
     }
     break;
     
   case '?':
   case ':':  
     return;
  }
}  


// вписываем адрес контроллера в образ команды копирования
*((unsigned short*)&datacmd[32])=nand_cmd>>16;


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

hello();
if ((wmode == w_standart)||(wmode == w_linux)) oobsize=0; // для входных файлов без OOB
oobsize/=spp;   // теперь oobsize - это размер OOB на один сектор

// Сброс контроллера nand
nand_reset();

// режим стирания
if (cflag) {
  block_erase(block);
  return;
}

//ECC on-off
if (wmode != w_image) {
  mempoke(nand_ecc_cfg,mempeek(nand_ecc_cfg)&0xfffffffe); 
  mempoke(nand_cfg1,mempeek(nand_cfg1)&0xfffffffe); 
}
else {
  mempoke(nand_ecc_cfg,mempeek(nand_ecc_cfg)|1); 
  mempoke(nand_cfg1,mempeek(nand_cfg1)|1); 
}
  
// Определяем размер файла
fseek(in,0,SEEK_END);
fsize=ftell(in)/(pagesize*ppb); // размер в блоках
rewind(in);

if (flen == 0) flen=fsize;
else if (flen>fsize) {
  printf("\n Указанная длина превосходит размер файла\n");
  return;
}  
  
printf("\n Запись из файла %s, стартовый блок %03x, размер %03x\n Режим записи: ",argv[optind],block,flen);
switch (wmode) {
  case w_standart:
    printf("только данные, стандартный формат\n");
    break;
    
  case w_linux: 
    printf("только данные, линуксовый формат\n");
    break;
    
  case w_image: 
    printf("сырой образ без рассчета ЕСС\n");
    break;
    
  case w_yaffs: 
    printf("образ yaffs2\n");
    break;
}   
    
port_timeout(1000);

// цикл по блокам

for(block=startblock;block<(startblock+flen);block++) {
  // стираем блок
  block_erase(block);
  // цикл по страницам
  for(page=0;page<ppb;page++) {
    
    // читаем весь дамп страницы
    fread(srcbuf,1,pagesize+(spp*oobsize),in);  // образ страницы - page+oob
    // разбираем дамп по буферам
    for (i=0;i<spp;i++) {
      memcpy(databuf+sectorsize*i,srcbuf+(sectorsize+oobsize)*i,sectorsize);
      if (oobsize != 0) memcpy(oobuf+oobsize*i,srcbuf+(sectorsize+oobsize)*i+sectorsize,oobsize);
    }  
    
    // устанавливаем адрес флешки
    printf("\r block: %04x   page:%02x",block,page); fflush(stdout);
    setaddr(block,page);

    // устанавливаем код команды записи
    switch (wmode) {
      case w_standart:
	mempoke(nand_cmd,0x36); // page program
	break;

      case w_linux:
      case w_yaffs:
        mempoke(nand_cmd,0x39); // запись data+spare
	break;
	 
      case w_image:
        mempoke(nand_cmd,0x37); // запись data+spare+ecc
	break;
    }
    // цикл по секторам
    for(sector=0;sector<spp;sector++) {
      memset(datacmd+34,0xff,sectorsize+64); // заполняем секторный буфер FF - значения по умолчанию
      
      // заполняем секторный буфер данными
      switch (wmode) {
        case w_linux:
	// линуксовый (китайский извратный) вариант раскладки данных, запись без OOB
          if (sector < (spp-1))  
	 //первые n секторов
             memcpy(datacmd+34,databuf+sector*(sectorsize+4),sectorsize+4); 
          else 
	 // последний сектор
             memcpy(datacmd+34,databuf+(spp-1)*(sectorsize+4),sectorsize-4*(spp-1)); // данные последнего сектора - укорачиваем
	  break;
	  
	case w_standart:
	 // стандартный формат - только сектора по 512 байт, без ООВ
          memcpy(datacmd+34,databuf+sector*sectorsize,sectorsize); 
	  break;
	  
	case w_image:
	 // сырой образ - data+oob, ECC не вычисляется
          memcpy(datacmd+34,databuf+sector*sectorsize,sectorsize);       // data
          memcpy(datacmd+34+sectorsize,oobuf+sector*oobsize,oobsize);    // oob
          break;

	case w_yaffs:
	 // образ yaffs - записываем только данные и yaffs-тег в область spare
	 // входной файл имеет формат 512+oob, но при этом тег лежит с позиции 0 OOB 
          if (sector < (spp-1))  
	 //первые n секторов
             memcpy(datacmd+34,databuf+sector*(sectorsize+4),sectorsize+4); 
          else  {
	 // последний сектор
             memcpy(datacmd+34,databuf+(spp-1)*(sectorsize+4),sectorsize-4*(spp-1)); // данные последнего сектора - укорачиваем
             memcpy(datacmd+34+sectorsize,oobuf,16 );    // oob - тег yaffs
	  }
	  break;
      }
      // пересылаем сектор в секторный буфер
      iolen=send_cmd(datacmd,34+sectorsize+oobsize,iobuf); 
      // выполняем команду записи и ждем ее завершения
      mempoke(nand_exec,0x1);
      nandwait();
     }  // конец цикла записи по секторам
     if (!vflag) continue;   // верификация не требуется
    // верификация данных
     printf("\r");
     setaddr(block,page);
     mempoke(nand_cmd,0x34); // чтение data+ecc+spare
     
     // цикл верификации по секторам
     for(sector=0;sector<spp;sector++) {
      // читаем очередной сектор 
      mempoke(nand_exec,0x1);
      nandwait();
      
      // читаем секторный буфер
      memread(membuf,sector_buf,sectorsize+oobsize);
      switch (wmode) {
        case w_linux:
	case w_yaffs:  
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
	  break; 
	  
	 case w_standart:
	 case w_image:  
          // верификация в стандартном формате
	  for (i=0;i<sectorsize;i++) 
	      if (membuf[i] != databuf[sector*sectorsize+i])
                 printf("! block: %04x  page:%02x  sector:%i  byte: %03x  %02x != %02x\n",
			block,page,sector,i,membuf[i],databuf[sector*sectorsize+i]); 
	  break;   
      }  // switch(wmode)
    }  // конец секторного цикла верификации
  }  // конец цикла по страницам 
} // конец цикла по блокам  

printf("\n");
}

