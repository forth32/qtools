#include <sys/types.h>
#include <sys/stat.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifndef WIN32
#include <unistd.h>
#else
#include <windows.h>
#include "wingetopt.h"
#include "printf.h"
#endif
#include "qcio.h"

// Размер блока загрузки
#define dlblock 1017

//****************************************
//* Загрузка через сахару
//****************************************
void dload_sahara(FILE* in) {

unsigned char sendbuf[10240];
unsigned char replybuf[10240];
unsigned int iolen,offset,len;
unsigned char helloreply[60]={
 02, 00, 00, 00, 30, 00, 00, 00, 02, 00, 00, 00, 01, 00, 00, 00,
 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00,
 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00
}; 


port_timeout(50); // запроса HELLO будем ждать 5 секунд
iolen=read(siofd,replybuf,48);  // читаем запрос HELLO
if ((iolen != 48)||(replybuf[1] != 1)) {
  printf("\n Пакет HELLO не получен\n");
  dump(replybuf,iolen,0);
  exit(1);
}

// Получили запрос HELLO, 
tcflush(siofd);  // очищаем буфер приема
port_timeout(1); // теперь обмен пакетами пойдет быстро - таймаут 0.1с
write(siofd,helloreply,48);   // отвечаем подтверждением hello
iolen=read(siofd,replybuf,20);      // ответный пакет
  if (iolen == 0) {
    printf("\n Нет ответа от модема\n");
    exit(1);
  }  
// в replybuf - запрос первого блока загрузчика

// Основной цикл передачи тела загрузчика
printf("\nПоблочная передача загрузчика\n");
while(replybuf[1] != 4) {
 if (replybuf[1] != 3) {
    printf("\n Запрос с недопустимым кодом, прерываем загрузку");
    dump(replybuf,iolen,0);
    exit(1);
 }
  // выделяем параметры фоагмента файла
  offset=*((unsigned int*)&replybuf[13]);
  len=*((unsigned int*)&replybuf[17]);
  printf("\r* адрес=%08x длина=%08x");
  fseek(in,offset,SEEK_SET);
  fread(sendbuf,1,len,in);
  // отправляем блок данных сахаре
  write(siofd,sendbuf,len);
  // получаем ответ
  iolen=read(siofd,replybuf,20);      // ответный пакет
  if (iolen == 0) {
    printf("\n Нет ответа от модема\n");
    exit(1);
  }  
}
// конец загрузки
}

//@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@2
void main(int argc, char* argv[]) {

int opt;
unsigned int start=0x41700000;
#ifndef WIN32
char devname[50]="/dev/ttyUSB0";
#else
char devname[50]="";
#endif
FILE* in;
struct stat fstatus;
unsigned int i,partsize,iolen,adr,helloflag=0;
unsigned int sahara_flag=0;

unsigned char iobuf[4096];
unsigned char cmd1[]={0x06};
unsigned char cmd2[]={0x07};
unsigned char cmddl[2048]={0xf};
unsigned char cmdstart[2048]={0x5,0,0,0,0};


while ((opt = getopt(argc, argv, "p:a:his")) != -1) {
  switch (opt) {
   case 'h': 
     printf("\n Утилита предназначена для загрузки программ-прошивальщика (E)NPRG в память модема\n\n\
Допустимы следующие ключи:\n\n\
-p <tty>  - указывает имя устройства последовательного порта, переведенного в download mode\n\
-i        - запускает процедуру HELLO для инициализации загрузчика\n\
-s        - использовать протокол SAHARA\n\
-k #           - выбор чипсета: 0(MDM9x15, по умолчанию), 1(MDM8200), 2(MSM9x00), 3(MDM9x25)\n\
-a <adr>  - адрес загрузки, по умолчанию 41700000\n");
    return;
     
   case 'p':
    strcpy(devname,optarg);
    break;

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

   case 'i':
    helloflag=1;
    break;
    
   case 's':
    sahara_flag=1;
    break;
    
   case 'a':
     sscanf(optarg,"%x",&start);
     break;
  }     
}

#ifdef WIN32
if (*devname == '\0')
{
   printf("\n - Последовательный порт не задан\n"); 
   return; 
}
#endif

in=fopen(argv[optind],"rb");
if (in == 0) {
  printf("\nОшибка открытия входного файла\n");
  return;
}

if (!open_port(devname))  {
#ifndef WIN32
   printf("\n - Последовательный порт %s не открывается\n", devname); 
#else
   printf("\n - Последовательный порт COM%s не открывается\n", devname); 
#endif
   return; 
}

//----- Вариант загрузки через сахару -------
if (sahara_flag) {
  dload_sahara(in);
  if (helloflag) hello();
  printf("\n");
  return;
}

//------- Вариант загрузки через запись загрузчика в память ----------

printf("\n Загрузка файла %s\n Адрес загрузки: %08x",argv[optind],start);
iolen=send_cmd_base(cmd1,1,iobuf,1);
if (iolen != 5) {
   printf("\n Модем не находится в режиме загрузки\n");
//   dump(iobuf,iolen,0);
   return;
}   
iolen=send_cmd_base(cmd2,1,iobuf,1);
#ifndef WIN32
fstat(fileno(in),&fstatus);
#else
fstat(_fileno(in),&fstatus);
#endif
printf("\n Размер файла: %i\n",fstatus.st_size);
partsize=dlblock;

// Цикл поблочной загрузки 
for(i=0;i<fstatus.st_size;i+=dlblock) {  
 if ((fstatus.st_size-i) < dlblock) partsize=fstatus.st_size-i;
 fread(cmddl+7,1,partsize,in);          // читаем блок прямо в командный буфер
 adr=start+i;                           // адрес загрузки этого блока
   // Как обычно у убогих китайцев, числа вписываются через жопу - в формате Big Endian
   // вписываем адрес загрузки этого блока
   cmddl[1]=(adr>>24)&0xff;
   cmddl[2]=(adr>>16)&0xff;
   cmddl[3]=(adr>>8)&0xff;
   cmddl[4]=(adr)&0xff;
   // вписываем размер блока 
   cmddl[5]=(partsize>>8)&0xff;
   cmddl[6]=(partsize)&0xff;
 iolen=send_cmd_base(cmddl,partsize+7,iobuf,1);
 printf("\r Загружено: %i",i+partsize);
// dump(iobuf,iolen,0);
} 
// вписываем адрес в команду запуска
printf("\n Запуск загрузчика...");
cmdstart[1]=(start>>24)&0xff;
cmdstart[2]=(start>>16)&0xff;
cmdstart[3]=(start>>8)&0xff;
cmdstart[4]=(start)&0xff;
iolen=send_cmd_base(cmdstart,5,iobuf,1);
#ifndef WIN32
usleep(200000);   // ждем инициализации загрузчика
#else
Sleep(200);   // ждем инициализации загрузчика
#endif
printf("ok\n");
if (helloflag) hello();
printf("\n");

}

