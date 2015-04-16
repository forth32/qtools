#include <stdio.h>
#include <string.h>
#include <stdlib.h>
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
  
return;  
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


//*******************************************
//* Отсылка загрузчику таблицы разделов
//*******************************************

void send_ptable(char* ptraw, unsigned int len, unsigned int mode) {
  
unsigned char iobuf[600];
unsigned char cmdbuf[8192]={0x19,0};
int iolen;
  
memcpy(cmdbuf+2,ptraw,len);
// режим прошивки: 0 - обноление, 1 - полная перепрошивка
cmdbuf[1]=mode;
//printf("\n");
//dump(cmdbuf,len+2,0);

printf("\n Отсылаем таблицу разделов...");
iolen=send_cmd(cmdbuf,len+2,iobuf);

if (iobuf[1] != 0x1a) {
  printf("\n ошибка!");
  show_errpacket("send_ptable()",iobuf,iolen);
  if (iolen == 0) {
    printf("\n Требуется загрузчик в режиме тихого запуска\n");
  }
  exit(1);
}  
if (iobuf[2] == 0) {
  printf("ok");
  return;
}  
printf("\n Таблицы разделов не совпадают - требуется полная перепрошивка модема (ключ -f)\n");
exit(1);
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
int ptflag=0;
int listmode=0;
int wcount=0; 

char filename[50][120]; // массив имен файлов для записи разделов
char partname[50][120]; // массив имен разделов

char* fptr, *nptr;
#ifndef WIN32
char devname[50]="/dev/ttyUSB0";
#else
char devname[50]="";
#endif
unsigned int i,opt,iolen,j;
unsigned int renameflag=0;
unsigned int adr,len;
unsigned int fsize;
unsigned int forceflag=0;

// очищаем массив имен файлов и разделов
for(i=0;i<50;i++) {
  filename[i][0]=0;
  partname[i][0]=0;
}  


while ((opt = getopt(argc, argv, "hp:s:w:mrk:f")) != -1) {
  switch (opt) {
   case 'h': 
    printf("\n  Утилита предназначена для записи разделов (по таблице) на флеш модема\n\
Допустимы следующие ключи:\n\n\
-p <tty>  - указывает имя устройства последовательного порта для общения с загрузчиком\n\
-k #      - код чипсета (-kl - получить список кодов)\n\
-s <file> - взять карту разделов из указанного файла\n\
-s -      - взять карту разделов из файла ptable/current-r.bin\n\
-f        - полная перепрошивка модема с изменением карты разделов\n\
-r        - переименовать разделы PAD в PADnn (дбавляется номер раздела\n\
-w #:file[:part] - записать раздел с номером # из файла file, можно указать имя раздела part\n\
-m        - вывести на экран полную карту разделов\n");
    return;
    
   case 'k':
    define_chipset(optarg);
    break;
    
   case 'p':
    strcpy(devname,optarg);
    break;
    
   case 'r': 
     renameflag=1;
     break;
     
   case 'w':
     // определение имени файла для раздела
     strcpy(iobuf,optarg);
     
     // выделяем имя файла
     fptr=strchr(iobuf,':');
     if (fptr == 0) {
       printf("\nОшибка в параметрах ключа -w - не указано имя файла: %s\n",optarg);
       return;
     }
     *fptr=0; // ограничитель номера раздела
     sscanf(iobuf,"%i",&i);  // номер раздела
     if (i>50) {
       printf("\nСлишком большой номер раздела - %i\n",i);
       return;
     }
     fptr++;
     // выделяем имя раздела
     nptr=strchr(fptr,':');
     if (nptr != 0) {
       *nptr=0;
       strcpy(partname[i],nptr+1);
     }  
     // копируем имя файла
     strcpy(filename[i],fptr); 
//     printf("\n-- write # %i : %s - %s",i,filename[i],partname[i]);
     break;
     
   case 's':
       // загружаем таблицу разделов из файла
       if (optarg[0] == '-')   part=fopen("ptable/current-w.bin","rb");
       else part=fopen(optarg,"rb");
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

   case 'f':
     forceflag=1;
     break;
     
   case '?':
   case ':':  
     return;
  }
}  

if (!ptflag) {
  printf("\n Не указана таблица разделов!\n");
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
//hello(0);
// сохраняем конфигурацию контроллера
cfg0=mempeek(nand_cfg0);
cfg1=mempeek(nand_cfg1);


// Загрузка и разбор таблицы разделов

npart=*((unsigned int*)&ptabraw[12]);
for(i=0;i<npart;i++) {
    strncpy(ptable[i].name,ptabraw+16+28*i,16);       // имя
    // заменяем имя раздела, если заказывали
    if (partname[i][0] != 0) strcpy(ptable[i].name,partname[i]);
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
  printf("\n #  размер1  размер2  атрибуты ------ Имя------  ------- Файл -----\n");     
  for(i=0;i<npart;i++) {
    printf("\r%02i %08x  %08x  %08x  %-15.15s   %s\n",i,ptable[i].start,ptable[i].len,ptable[i].attr,ptable[i].name,filename[i]);
  }
//  restore_reg();
  return;
}  
printf("\n secure mode...");
if (!secure_mode()) {
  printf("\n Ошибка входа в режим Secure mode\n");
//  restore_reg();
  return;
}
printf("ok");
qclose(0);  //####################################################
#ifndef WIN32
  usleep(50000);
#else
  Sleep(50);
#endif
// отсылаем таблицу разделов
send_ptable(ptabraw,16+28*npart,forceflag);
// главный цикл записи - по разделам:
port_timeout(1000);
for(i=0;i<npart;i++) {
  if (filename[i][0] == 0) continue; // этот раздел записывать не надо
  part=fopen(filename[i],"rb");
  if (part == 0) {
    printf("\n Раздел %i: ошибка открытия файла %s\n",i,filename[i]);
//    restore_reg();
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
 //   restore_reg();
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
 //     restore_reg();
      return;
    }
    if (feof(part)) break; // конец раздела и конец файла
  }
  // Раздел передан полностью
  if (!qclose(1)) {
    printf("\n Ошибка закрытия потока даных\n");
//    restore_reg();
    return;
  }  
  printf(" ... запись завершена");
#ifndef WIN32
  usleep(500000);
#else
  Sleep(500);
#endif
}
printf("\n");
//restore_reg();
}


