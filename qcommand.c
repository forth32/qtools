#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <readline/readline.h>
#include <readline/history.h>
#include <unistd.h>
#include "qcio.h"

//@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@
//* Интерактивная оболочка для ввода команд в загрузчик
//@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@


//*******************************************************************
//* Разбор введенной командной последовательности и ее запуск
//*******************************************************************
void ascii_cmd(char* line) {

char* sptr;
unsigned char cmdbuf[2048];
int bcnt=0;
int i;
unsigned char iobuf[8192];
unsigned int iolen;

sptr=strtok(line," "); // разбор командных байтов, разделенных пробелами
if (sptr == 0) return;
 
do {
  sscanf(sptr,"%x",&i);
  cmdbuf[bcnt++]=i;
  sptr=strtok(0," ");
} while (sptr != 0);
iolen=send_cmd(cmdbuf,bcnt,iobuf);
printf("\n ---- ответ --- \n");
dump(iobuf,iolen,0);
printf("\n");
}


//**********************************************8
//* Процедура активации загрузчика hello
//**********************************************8
void hello() {

int i;  
char rbuf[1024];
char hellocmd[]="\x01QCOM fast download protocol host\x03### ";

printf("Отсылка hello...");
i=send_cmd(hellocmd,strlen(hellocmd),rbuf);
if (rbuf[1] != 2) {
   printf(" hello возвратил ошибку!\n");
   dump(rbuf,i,0);
   return;
}  
i=rbuf[0x2c];
rbuf[0x2d+i]=0;
printf("ok\nFlash: %s\n",rbuf+0x2d);
}


//*********************************************8
//  Передача загрузчика в модем
//*********************************************8
void send_loader(char* filename) {
  
int adr;
FILE* loader;
char cmd06[]={6,0,0,0};
char cmd07[]={7,0,0,0};
//char cmdsend[1024]={
}

//**********************************************8
//* обработка команд
//**********************************************8

void process_command(char* cmdline) {

int adr,len=128,data;
char* sptr;
char membuf[4096];

switch (cmdline[0]) {
  
  // help
  case 'h':
    printf("\n Доступны следующие команды:\n\n\
c nn nn nn nn.... - формирование и запуск командного пакета из перечисленных байтов\n\
d adr [len]       - прсмотр дампа адресного пространства системы\n\
m adr word ...    - записать слова по указанному адресу\n\
r adr0 adr        - чтение блока флешки в секторный буфер \n\
s                 - прсмотр дампа сектороного буфера NAND-контроллера\n\
n                 - просмотр содежримого регистров NAND-контроллера\n\
x                 - выход из программы\n\
i                 - запуск процедуры HELLO\n");
    break;
  // обработка командного пакета
  case 'c':  
   ascii_cmd(cmdline+1);
   break;
 
  // активация загрузчика 
  case 'i':
    hello();
    break;
    
  // дамп памяти
  case 'd':  
   sptr=strtok(cmdline+1," "); // адрес
   if (sptr == 0) {printf("\n Не указан адрес"); return;}
   sscanf(sptr,"%x",&adr);
   sptr=strtok(0," "); // длина
   if (sptr != 0) sscanf(sptr,"%x",&len);
   memread(membuf,adr,len);
   dump(membuf,len,adr); 
   break;

  case 'm':
   sptr=strtok(cmdline+1," "); // адрес
   if (sptr == 0) {printf("\n Не указан адрес"); return;}
   sscanf(sptr,"%x",&adr);
   while((sptr=strtok(0," ")) != 0) { // данные
     sscanf(sptr,"%x",&data);
     if (!mempoke(adr,data)) printf("\nКоманда возвратила ошибку, adr=%08x  data=%08x\n",adr,data);
     adr+=4;
   }
   break;

  case 'r':
   sptr=strtok(cmdline+1," "); // адрес0
   if (sptr == 0) {printf("\n Не указан адрес 0"); return;}
   sscanf(sptr,"%x",&adr);
   sptr=strtok(0," ");        // адрес1
   if (sptr == 0) {printf("\n Не указан адрес 1"); return;}
   sscanf(sptr,"%x",&data);
   flash_read(adr,data);
//   break;
   
   
  case 's': 
   memread(membuf,0x1b400100,0x23c);
   dump(membuf,0x23c,0); 
   break;

  case 'n': 
   memread(membuf,0x1b400000,0x100);
   printf("\n* 000 NAND_FLASH_CMD         = %08x",*((unsigned int*)&membuf[0]));
   printf("\n* 004 NAND_ADDR0             = %08x",*((unsigned int*)&membuf[4]));
   printf("\n* 008 NAND_ADDR1             = %08x",*((unsigned int*)&membuf[8]));
   printf("\n* 00c NAND_CHIP_SELECT       = %08x",*((unsigned int*)&membuf[0xc]));
   printf("\n* 010 NANDC_EXEC_CMD         = %08x",*((unsigned int*)&membuf[0x10]));
   printf("\n* 014 NAND_FLASH_STATUS      = %08x",*((unsigned int*)&membuf[0x14]));
   printf("\n* 018 NANDC_BUFFER_STATUS    = %08x",*((unsigned int*)&membuf[0x18]));
   printf("\n* 020 NAND_DEV0_CFG0         = %08x",*((unsigned int*)&membuf[0x20]));
   printf("\n* 024 NAND_DEV0_CFG1         = %08x",*((unsigned int*)&membuf[0x24]));
   printf("\n* 028 NAND_DEV0_ECC_CFG      = %08x",*((unsigned int*)&membuf[0x28]));
   printf("\n* 040 NAND_FLASH_READ_ID     = %08x",*((unsigned int*)&membuf[0x40]));
   printf("\n* 044 NAND_FLASH_READ_STATUS = %08x",*((unsigned int*)&membuf[0x44]));
   printf("\n* 064 FLASH_MACRO1_REG       = %08x",*((unsigned int*)&membuf[0x64]));
   printf("\n* 070 FLASH_XFR_STEP1        = %08x",*((unsigned int*)&membuf[0x70]));
   printf("\n* 074 FLASH_XFR_STEP2        = %08x",*((unsigned int*)&membuf[0x74]));
   printf("\n* 078 FLASH_XFR_STEP3        = %08x",*((unsigned int*)&membuf[0x78]));
   printf("\n* 07c FLASH_XFR_STEP4        = %08x",*((unsigned int*)&membuf[0x7c]));
   printf("\n* 080 FLASH_XFR_STEP5        = %08x",*((unsigned int*)&membuf[0x80]));
   printf("\n* 084 FLASH_XFR_STEP6        = %08x",*((unsigned int*)&membuf[0x84]));
   printf("\n* 088 FLASH_XFR_STEP7        = %08x",*((unsigned int*)&membuf[0x88]));
   printf("\n* 0a0 FLASH_DEV_CMD0         = %08x",*((unsigned int*)&membuf[0xa0]));
   printf("\n* 0a4 FLASH_DEV_CMD1         = %08x",*((unsigned int*)&membuf[0xa4]));
   printf("\n* 0a8 FLASH_DEV_CMD2         = %08x",*((unsigned int*)&membuf[0xa8]));
   printf("\n* 0ac FLASH_DEV_CMD_VLD      = %08x",*((unsigned int*)&membuf[0xac]));
   printf("\n* 0d0 FLASH_DEV_CMD3         = %08x",*((unsigned int*)&membuf[0xd0]));
   printf("\n* 0d4 FLASH_DEV_CMD4         = %08x",*((unsigned int*)&membuf[0xd4]));
   printf("\n* 0d8 FLASH_DEV_CMD5         = %08x",*((unsigned int*)&membuf[0xd8]));
   printf("\n* 0dc FLASH_DEV_CMD6         = %08x",*((unsigned int*)&membuf[0xdc]));
   printf("\n* 0e8 NAND_ERASED_CW_DET_CFG = %08x",*((unsigned int*)&membuf[0xe8]));
   printf("\n* 0ec NAND_ERASED_CW_DET_ST  = %08x",*((unsigned int*)&membuf[0xec]));
   printf("\n* 0f0 EBI2_ECC_BUF_CFG       = %08x\n",*((unsigned int*)&membuf[0xf0]));
   break;
  case 'x': 
   exit(0);
   break;

  default:
    printf("\nНеопределенная команда\n");
    break;
}   
}    
    
//@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@

void main(int argc,char* argv[]) {
  
unsigned char* line;
unsigned char scmdline[200]={0};
int i,iolen;
char devname[50]="/dev/ttyUSB0";
int opt,helloflag=0;

while ((opt = getopt(argc, argv, "p:ic:")) != -1) {
  switch (opt) {
   case 'p':
    strcpy(devname,optarg);
    break;
    
   case 'i':
     helloflag=1;
     break;
     
   case 'c':
     strcpy(scmdline,optarg);
     break;
  }
}  

if (!open_port(devname))  {
   printf("\n - Последовательный порт %s не открывается\n", devname); 
   return; 
}
if (helloflag) hello();
printf("\n");

// запуск команды из ключа -C, если есть
if (strlen(scmdline) != 0) {
  process_command(scmdline);
  return;
}

// Основной цикл обработки команд
for(;;)  {
 line=readline(">"); 
 if (line == 0) {
    printf("\n");
    return;
 }   
 if (strlen(line) <1) continue; // слишком короткая команда
 add_history(line); // в буфер ее для истории
 process_command(line);
 free(line);
} 
}
