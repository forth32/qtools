#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <getopt.h>
#include "qcio.h"

// Размер блока записи
#define wbsize 1024
  
// хранилище таблицы разделов
struct  {
  char name[17];
  unsigned int start;
  unsigned int len;
  unsigned int attr;
} ptable[30];

int npart=0;    // число разделов в таблице

//***********************************8
//* Установка secure mode
//***********************************8
int secure_mode() {
  
unsigned char iobuf[600];
unsigned char cmdbuf[]={0x17,1};
int iolen;

iolen=send_cmd(cmdbuf,2,iobuf);
//printf("\n--secure--\n");
//dump(iobuf,iolen,0);
if (iobuf[1] == 0x18) return 1;
return 0; // была ошибка

}  

//**********************************
//* Закрытие потока данных раздела
//**********************************
void qclose() {
unsigned char iobuf[600];
unsigned char cmdbuf[]={0x15};
int iolen;

iolen=send_cmd(cmdbuf,1,iobuf);
//printf("\n--close--\n");
//dump(iobuf,iolen,0);

}  

//*******************************************
//* Отсылка загрузчику таблицы разделов
//*******************************************

int send_ptable(char* ptraw, unsigned int len) {
  
unsigned char iobuf[600];
unsigned char cmdbuf[8192]={0x19,0};
int iolen;
  
memcpy(cmdbuf+2,ptraw,len);
//dump(cmdbuf,len+2,0);

iolen=send_cmd(cmdbuf,len+2,iobuf);

if (iobuf[1] == 0x1a) return 1;
printf("\n--ptable--\n");
dump(iobuf,iolen,0);
return 0; // была ошибка
}

//*******************************************
//* Отсылка загрузчику заголовка раздела
//*******************************************
int send_head(char* name) {

unsigned char iobuf[600];
unsigned char cmdbuf[32]={0x1b,0x1e};
int iolen;
  
strcpy(cmdbuf+2,name);
iolen=send_cmd(cmdbuf,strlen(cmdbuf)+1,iobuf);
if (iobuf[1] == 0x1c) return 1;
printf("\n--head--\n");
dump(iobuf,iolen,0);
return 0; // была ошибка
}


//*******************************************
//@@@@@@@@@@@@ Головная программа
void main(int argc, char* argv[]) {
  
unsigned char iobuf[2048];
unsigned char scmd[3068]={0x7,0,0,0};
int res;
char ptabraw[2048];
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

// очищаем массив имен файлов
for(i=0;i<50;i++)  wname[i][0]=0;

while ((opt = getopt(argc, argv, "hp:s:w:imr8")) != -1) {
  switch (opt) {
   case 'h': 
    printf("\n  Утилита предназначена для чтения образа флеш через модифицированный загрузчик\n\
Допустимы следующие ключи:\n\n\
-p <tty>  - указывает имя устройства последовательного порта для общения с загрузчиком\n\
-i        - запускает процедуру HELLO для инициализации загрузчика\n\
-8        - работа с чипсетом MSM8200 вместо MDM9x12\n\
-s <file> - взять карту разделов из указанного файла\n\
-r        - переименовать разделы PAD в PADnn (дбавляется номер раздела\n\
-w #:file - записать раздел с номером # из файла file\n\
-m        - вывести на экран полную карту разделов\n");
    return;
    
   case '8':
    nand_cmd=0xA0A00000;
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
     printf("\n-- write # %i : %s",i,wname[i]);
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
if (strncmp(ptabraw,"\xAA\x73\xEE\x55\xDB\xBD\x5E\xE3",8) != 0) {
   printf("\nТаблица разделов повреждена\n");
   return;
}
*/
npart=*((unsigned int*)&ptabraw[12]);
for(i=0;i<npart;i++) {
    strncpy(ptable[i].name,ptabraw+16+28*i,16);       // имя
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
  return;
}  
printf("\n secure mode...");
if (!secure_mode()) {
  printf("\n Ошибка входа в режим Secure mode\n");
  return;
}
qclose();
usleep(50000);
printf("\nотсылаем таблицу разделов");
// отсылаем таблицу разделов
if (!send_ptable(ptabraw,16+28*npart)) { 
  printf("\n Ошибка отсылки таблицы разделов\n");
  return;
}  

// главный цикл записи - по разделам:
port_timeout(1000);
for(i=0;i<npart;i++) {
  if (wname[i][0] == 0) continue; // этот раздел записывать не надо
  part=fopen(wname[i],"r");
  if (part == 0) {
    printf("\n Раздел %i: ошибка открытия файла %s\n",i,wname[i]);
    return;
  }
  // отсылаем заголовок
  if (!send_head(ptable[i].name)) {
    printf("\n Модем отверг заголовок раздела\n");
    return;
  }  
  // цикл записи кусков раздела по 1К за команду
  printf("\n");
  for(adr=0;;adr+=wbsize) {  
    // адрес
    scmd[0]=7;
    scmd[1]=(adr>>16)&0xff;
    scmd[2]=(adr>>8)&0xff;
    scmd[3]=(adr)&0xff;
    memset(scmd+3,0xff,wbsize);   // заполняем буфер данных FF
    len=fread(scmd+4,1,wbsize,part);
    printf("\r Запись раздела %i (%s): адрес:%06x",i,ptable[i].name,adr); fflush(stdout);
    iolen=send_cmd_base(scmd,len+4,iobuf,1);
    if ((iolen == 0) || (iobuf[1] != 8)) {
      printf("\n Ошибка записи раздела %i (%s): адрес:%06x\n",i,ptable[i].name,adr);
      return;
    }
    if (feof(part)) break; // конец раздела и конец файла
  }
  // Раздел передан полностью
  qclose();
  printf(" ... запись завершена\n");
  usleep(50000);
}
}


