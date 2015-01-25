#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include "qcio.h"

// Размер блока загрузки
#define dlblock 1017

void main(int argc, char* argv[]) {

int opt;
unsigned int start=0x41700000;
char devname[50]="/dev/ttyUSB0";
FILE* in;
struct stat fstatus;
unsigned int i,partsize,iolen,adr,helloflag=0;

unsigned char iobuf[4096];
unsigned char cmd1[]={0x7E,0x06};
unsigned char cmd2[]={0x7E,0x07};
unsigned char cmddl[2048]={0x7e,0xf};
unsigned char cmdstart[2048]={0x7e,0x5,0,0,0,0};


while ((opt = getopt(argc, argv, "p:a:hi")) != -1) {
  switch (opt) {
   case 'h': 
     printf("\n Утилита предназначена для загрузки программ-прошивальщика (E)NPRG в память модема\n\n\
Допустимы следующие ключи:\n\n\
-p <tty>  - указывает имя устройства последовательного порта, переведенного в download mode\n\
-i        - запускает процедуру HELLO для инициализации загрузчика\n\
-a <adr>  - адрес загрузки, по умолчанию 41700000\n");
    return;
     
   case 'p':
    strcpy(devname,optarg);
    break;

   case 'i':
    helloflag=1;
    break;
    
   case 'a':
     sscanf(optarg,"%x",&start);
     break;
  }     
}

in=fopen(argv[optind],"r");
if (in == 0) {
  printf("\nОшибка открытия входного файла\n");
  return;
}

if (!open_port(devname))  {
   printf("\n - Последовательный порт %s не открывается\n", devname); 
   return; 
}

printf("\n Загрузка файла %s\n Адрес загрузки: %08x",argv[optind],start);
iolen=send_cmd(cmd1,7,iobuf);
if (iolen != 5) {
   printf("\n Модем не находится в режиме загрузки\n");
//   dump(iobuf,iolen,0);
   return;
}   
iolen=send_cmd(cmd2,7,iobuf);
fstat(fileno(in),&fstatus);
printf("\n Размер файла: %i\n",fstatus.st_size);
partsize=dlblock;

// Цикл поблочной загрузки 
for(i=0;i<fstatus.st_size;i+=dlblock) {  
 if ((fstatus.st_size-i) < dlblock) partsize=fstatus.st_size-i;
 fread(cmddl+8,1,partsize,in);          // читаем блок прямо в командный буфер
 adr=start+i;                           // адрес загрузки этого блока
   // Как обычно у убогих китайцев, числа вписываются через жопу - в формате Big Endian
   // вписываем адрес загрузки этого блока
   cmddl[2]=(adr>>24)&0xff;
   cmddl[3]=(adr>>16)&0xff;
   cmddl[4]=(adr>>8)&0xff;
   cmddl[5]=(adr)&0xff;
   // вписываем размер блока 
   cmddl[6]=(partsize>>8)&0xff;
   cmddl[7]=(partsize)&0xff;
 iolen=send_cmd(cmddl,partsize+8,iobuf);
 printf("\r Загружено: %i",i+partsize);
// dump(iobuf,iolen,0);
} 
// вписываем адрес в команду запуска
printf("\n Запуск загрузчика...\n");
cmdstart[2]=(start>>24)&0xff;
cmdstart[3]=(start>>16)&0xff;
cmdstart[4]=(start>>8)&0xff;
cmdstart[5]=(start)&0xff;
iolen=send_cmd(cmdstart,6,iobuf);
usleep(200000);   // ждем инициализации загрузчика
if (helloflag) hello();
printf("\n");

}

