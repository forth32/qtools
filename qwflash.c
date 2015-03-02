#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <getopt.h>
#include "qcio.h"

// Размер блока записи
#define wbsize 1024
//#define wbsize 1538
  
// хранилище таблицы разделов
struct  {
  char name[17];
  unsigned int start;
  unsigned int len;
  unsigned int attr;
} ptable[30];

int npart=0;    // число разделов в таблице

unsigned int cfg0,cfg1; // сохранение конфигурации контроллера

//*****************************************************
//*  Восстановление конфигурации контроллера 
//*****************************************************
void restore_reg() {
  
mempoke(nand_cfg0,cfg0);  
mempoke(nand_cfg1,cfg1);  
}


//***********************************8
//* Установка secure mode
//***********************************8
int secure_mode() {
  
unsigned char iobuf[600];
unsigned char cmdbuf[]={0x17,1};
int iolen;

iolen=send_cmd(cmdbuf,2,iobuf);
if (iobuf[1] == 0x18) return 1;
show_errpacket("secure_mode()",iobuf,iolen);
return 0; // была ошибка

}  

//**********************************
//* Закрытие потока данных раздела
//**********************************
int qclose(int errmode) {
unsigned char iobuf[600];
unsigned char cmdbuf[]={0x15};
int iolen;

iolen=send_cmd(cmdbuf,1,iobuf);
if (!errmode) return 1;
if (iobuf[1] == 0x16) return 1;
show_errpacket("close()",iobuf,iolen);
return 0;

}  

//*******************************************
//* Отсылка загрузчику таблицы разделов
//*******************************************

int send_ptable(char* ptraw, unsigned int len) {
  
unsigned char iobuf[600];
unsigned char cmdbuf[8192]={0x19,0};
int iolen;
  
memcpy(cmdbuf+2,ptraw,len);
//printf("\n");
//dump(cmdbuf,len+2,0);

iolen=send_cmd(cmdbuf,len+2,iobuf);

if (iobuf[1] == 0x1a) return 1;
show_errpacket("send_ptable()",iobuf,iolen);
return 0; // была ошибка
}

//*******************************************
//* Отсылка загрузчику заголовка раздела
//*******************************************
int send_head(char* name) {

unsigned char iobuf[600];
unsigned char cmdbuf[32]={0x1b,0x0e};
int iolen;
  
strcpy(cmdbuf+2,name);
iolen=send_cmd(cmdbuf,strlen(cmdbuf)+1,iobuf);
if (iobuf[1] == 0x1c) return 1;
show_errpacket("send_head()",iobuf,iolen);
return 0; // была ошибка
}


//*******************************************
//@@@@@@@@@@@@ Головная программа
void main(int argc, char* argv[]) {
  
unsigned char iobuf[14048];
unsigned char scmd[13068]={0x7,0,0,0};
int res;
char ptabraw[4048];
FILE* part;
int helloflag=0;
int ptflag=0;
int listmode=0;
int wcount=0; 
char wname[50][120]; // массив имен файлов для записи разделов
char* sptr;
char devname[]="/dev/ttyUSB0";
unsigned int i,opt,iolen,j;
unsigned int renameflag=0;
unsigned int adr,len;
unsigned int fsize;


// очищаем массив имен файлов
for(i=0;i<50;i++)  wname[i][0]=0;


while ((opt = getopt(argc, argv, "hp:s:w:imrk:")) != -1) {
  switch (opt) {
   case 'h': 
    printf("\n  Утилита предназначена для записи разделов (по таблице) на флеш модема\n\
Допустимы следующие ключи:\n\n\
-p <tty>  - указывает имя устройства последовательного порта для общения с загрузчиком\n\
-i        - запускает процедуру HELLO для инициализации загрузчика\n\
-k #           - выбор чипсета: 0(MDM9x15, по умолчанию), 1(MDM8200), 2(MSM9x00)\n\
-s <file> - взять карту разделов из указанного файла\n\
-r        - переименовать разделы PAD в PADnn (дбавляется номер раздела\n\
-w #:file - записать раздел с номером # из файла file\n\
-m        - вывести на экран полную карту разделов\n");
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
     
   case 'r': 
     renameflag=1;
     break;
     
   case 'w':
     // определение имени файла для раздела
     strcpy(iobuf,optarg);
     sptr=strchr(iobuf,':');
     if (sptr == 0) {
       printf("\nОшибка в параметрах ключа -w\n");
       return;
     }
     *sptr=0;
     sscanf(iobuf,"%i",&i);  // номер раздела
     if (i>50) {
       printf("\nСлишком большой номер раздела - %i\n",i);
       return;
     }
     strcpy(wname[i],sptr+1); // копируем имя файла
//     printf("\n-- write # %i : %s",i,wname[i]);
     break;
     
   case 's':
       // загружаем таблицу разделов из файла
       part=fopen(optarg,"r");
       if (part == 0) {
         printf("\nОшибка открытия файла таблицы разделов\n");
         return;
       }	 
       iolen=fread(ptabraw,1024,1,part); // читаем таблицу разделов из файла
       fclose(part);       
       ptflag=1; 
     break;
     
   case 'm':
     listmode=1;
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


// Загрузка и разбор таблицы разделов

//if (!ptflag) load_ptable(ptabraw);  // Загрузка таблицы из модема
if (!ptflag) {
 memset(ptabraw,0,1024); // обнуляем таблицу
 flash_read(2, 2, 0);  // блок 2 страница 1 - здесь лежит таблица разделов  
 memread(ptabraw,sector_buf, 512);
 flash_read(2, 2, 1);  // продолжение таблицы разделов
 memread(ptabraw+512,sector_buf, 512);
}
/*
if (strncmp(ptabraw,"\x9A\x1b\x7d\xaa\xbc\x48\x7d\x1f",8) != 0) {
   printf("\nТаблица разделов повреждена\n");
   return;
}
*/
npart=*((unsigned int*)&ptabraw[12]);
for(i=0;i<npart;i++) {
    strncpy(ptable[i].name,ptabraw+16+28*i,16);       // имя
    // заменяем MIBIB на SBL1
    if (strcmp(ptable[i].name,"0:MIBIB") == 0) strcpy(ptable[i].name,"0:SBL1"); 
    // переименование разделов PAD
    if (renameflag && (strcmp(ptable[i].name,"0:PAD") == 0)) {
      sprintf(iobuf,"%02x",i);
      strcat(ptable[i].name,iobuf);
    }  
    ptable[i].start=*((unsigned int*)&ptabraw[32+28*i]);   // адрес
    ptable[i].len=*((unsigned int*)&ptabraw[36+28*i]);     // размер
    ptable[i].attr=*((unsigned int*)&ptabraw[40+28*i]);    // атрибуты
    if (ptable[i].len == 0xffffffff) ptable[i].len=maxblock-ptable[i].start; // если длина - FFFF, или выходит за пределы флешки
}


// режим вывода таблицы разделов
if (listmode) {
  printf("\n #  адрес    размер   атрибуты ------ Имя------\n");     
  for(i=0;i<npart;i++) {
    printf("\r%02i %08x  %08x  %08x  %s\n",i,ptable[i].start,ptable[i].len,ptable[i].attr,ptable[i].name);
  }
  restore_reg();
  return;
}  
printf("\n secure mode...");
if (!secure_mode()) {
  printf("\n Ошибка входа в режим Secure mode\n");
  restore_reg();
  return;
}
qclose(0);  //####################################################
usleep(50000);
printf("\n Отсылаем таблицу разделов...");
// отсылаем таблицу разделов
if (!send_ptable(ptabraw,16+28*npart)) { 
  printf("\n Ошибка отсылки таблицы разделов\n");
  restore_reg();
  return;
}  
// главный цикл записи - по разделам:
port_timeout(1000);
for(i=0;i<npart;i++) {
  if (wname[i][0] == 0) continue; // этот раздел записывать не надо
  part=fopen(wname[i],"r");
  if (part == 0) {
    printf("\n Раздел %i: ошибка открытия файла %s\n",i,wname[i]);
    restore_reg();
    return;
  }
  
  // получаем размер раздела
  fseek(part,0,SEEK_END);
  fsize=ftell(part);
  rewind(part);

  printf("\n Запись раздела %i (%s)",i,ptable[i].name); fflush(stdout);
  // отсылаем заголовок
  if (!send_head(ptable[i].name)) {
    printf("\n! Модем отверг заголовок раздела\n");
    restore_reg();
    return;
  }  
  // цикл записи кусков раздела по 1К за команду
  for(adr=0;;adr+=wbsize) {  
    // адрес
    scmd[0]=7;
    scmd[1]=(adr)&0xff;
    scmd[2]=(adr>>8)&0xff;
    scmd[3]=(adr>>16)&0xff;
    scmd[4]=(adr>>24)&0xff;
    memset(scmd+5,0xff,wbsize+1);   // заполняем буфер данных FF
    len=fread(scmd+5,1,wbsize,part);
    printf("\r Запись раздела %i (%s): байт %i из %i (%i%%) ",i,ptable[i].name,adr,fsize,(adr+1)*100/fsize); fflush(stdout);
//    dump(scmd,len+4,0);
//    return;
    iolen=send_cmd_base(scmd,len+5,iobuf,0);
    if ((iolen == 0) || (iobuf[1] != 8)) {
      show_errpacket("Пакет данных ",iobuf,iolen);
      printf("\n Ошибка записи раздела %i (%s): адрес:%06x\n",i,ptable[i].name,adr);
      restore_reg();
      return;
    }
    if (feof(part)) break; // конец раздела и конец файла
  }
  // Раздел передан полностью
  if (!qclose(1)) {
    printf("\n Ошибка закрытия потока даных\n");
    restore_reg();
    return;
  }  
  printf(" ... запись завершена");
  usleep(500000);
}
printf("\n");
restore_reg();
}


