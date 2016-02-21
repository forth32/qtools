#include "include.h"

// Размер блока записи
#define wbsize 1024
//#define wbsize 1538
  
// хранилище таблицы разделов
struct  {
  char filename[50];
  char partname[16];
} ptable[30];

unsigned int npart=0;    // число разделов в таблице

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
  printf(" ошибка!");
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
char ptabraw[4048];
FILE* part;
int ptflag=0;
int listmode=0;

char* fptr;
#ifndef WIN32
char devname[50]="/dev/ttyUSB0";
#else
char devname[50]="";
#endif
unsigned int i,opt,iolen;
unsigned int adr,len;
unsigned int fsize;
unsigned int forceflag=0;

while ((opt = getopt(argc, argv, "hp:s:w:mk:f")) != -1) {
  switch (opt) {
   case 'h': 
    printf("\n  Утилита предназначена для записи разделов (по таблице) на флеш модема\n\
Допустимы следующие ключи:\n\n\
-p <tty>  - указывает имя устройства последовательного порта для общения с загрузчиком\n\
-k #      - код чипсета (-kl - получить список кодов)\n\
-s <file> - взять карту разделов из указанного файла\n\
-s -      - взять карту разделов из файла ptable/current-w.bin\n\
-f        - полная перепрошивка модема с изменением карты разделов\n\
-w file:part - записать раздел с именем part из файла file, имя раздела part указывается без 0:\n\
-m        - только просмотр карты прошивки без реальной записи\n");
    return;
    
   case 'k':
    define_chipset(optarg);
    break;
    
   case 'p':
    strcpy(devname,optarg);
    break;
    
   case 'w':
     // определение разделов для записи
     strcpy(iobuf,optarg);
     
     // выделяем имя файла
     fptr=strchr(iobuf,':');
     if (fptr == 0) {
       printf("\nОшибка в параметрах ключа -w - не указано имя раздела: %s\n",optarg);
       return;
     }
     *fptr=0; // ограничитель имени файла
     strcpy(ptable[npart].filename,iobuf); // копируем имя файла
     fptr++;
     // копируем имя раздела
     strcpy(ptable[npart].partname,"0:");
     strcat(ptable[npart].partname,fptr); 
     npart++;
     if (npart>19) {
       printf("\nСлишком много разделов\n");
       return;
     }
     break;
     
   case 's':
       // загружаем таблицу разделов из файла
       if (optarg[0] == '-')   part=fopen("ptable/current-w.bin","rb");
       else part=fopen(optarg,"rb");
       if (part == 0) {
         printf("\nОшибка открытия файла таблицы разделов\n");
         return;
       }	 
       fread(ptabraw,1024,1,part); // читаем таблицу разделов из файла
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
hello(2);
// сохраняем конфигурацию контроллера
//cfg0=mempeek(nand_cfg0);
//cfg1=mempeek(nand_cfg1);



// вывод таблицы прошивки

printf("\n #  --Раздел--  ------- Файл -----");     
for(i=0;i<npart;i++) {
    printf("\n%02u  %-14.14s  %s",i,ptable[i].partname,ptable[i].filename);
}
printf("\n");
if (listmode)  return; // ключ -m - на этом все.
  
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
if (ptflag) send_ptable(ptabraw,16+28*npart,forceflag);

// -- главный цикл записи - по разделам: --
port_timeout(1000);
for(i=0;i<npart;i++) {
  part=fopen(ptable[i].filename,"rb");
  if (part == 0) {
    printf("\n Раздел %u: ошибка открытия файла %s\n",i,ptable[i].filename);
    return;
  }
  
  // получаем размер раздела
  fseek(part,0,SEEK_END);
  fsize=ftell(part);
  rewind(part);

  printf("\n Запись раздела %u (%s)",i,ptable[i].partname); fflush(stdout);
  // отсылаем заголовок
  if (!send_head(ptable[i].partname)) {
    printf("\n! Модем отверг заголовок раздела\n");
    fclose(part);
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
    printf("\r Запись раздела %u (%s): байт %u из %u (%i%%) ",i,ptable[i].partname,adr,fsize,(adr+1)*100/fsize); fflush(stdout);
    iolen=send_cmd_base(scmd,len+5,iobuf,0);
    if ((iolen == 0) || (iobuf[1] != 8)) {
      show_errpacket("Пакет данных ",iobuf,iolen);
      printf("\n Ошибка записи раздела %u (%s): адрес:%06x\n",i,ptable[i].partname,adr);
      fclose(part);
      return;
    }
    if (feof(part)) break; // конец раздела и конец файла
  }
  // Раздел передан полностью
  if (!qclose(1)) {
    printf("\n Ошибка закрытия потока даных\n");
    fclose(part);
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
fclose(part);
}


