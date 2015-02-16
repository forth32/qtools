#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#ifndef WIN32
#include <readline/readline.h>
#include <readline/history.h>
#include <unistd.h>
#else
#include <windows.h>
#include "wingetopt.h"
#include "printf.h"
#endif
#include "qcio.h"

// флаг посылки префикса 7E
int prefixflag=1;


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
iolen=send_cmd_base(cmdbuf,bcnt,iobuf,prefixflag);
printf("\n ---- ответ --- \n");
dump(iobuf,iolen,0);
printf("\n");
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
int block,page,sect;


switch (cmdline[0]) {
  
  // help
  case 'h':
    printf("\n Доступны следующие команды:\n\n\
c nn nn nn nn.... - формирование и запуск командного пакета из перечисленных байтов\n\
d adr [len]       - прсмотр дампа адресного пространства системы\n\
m adr word ...    - записать слова по указанному адресу\n\
r block page sect - чтение блока флешки в секторный буфер \n\
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
   sptr=strtok(cmdline+1," "); // блок
   if (sptr == 0) {printf("\n Не указан # блока"); return;}
   sscanf(sptr,"%x",&block);

   sptr=strtok(0," ");        // страница
   if (sptr == 0) {printf("\n Не указан # страницы"); return;}
   sscanf(sptr,"%x",&page);
   if (page>63)  {printf("\n Слишком большой # страницы"); return;}
   
   sptr=strtok(0," ");        // сектор
   if (sptr == 0) {printf("\n Не указан # сектора"); return;}
   sscanf(sptr,"%x",&sect);
   if (sect>3)  {printf("\n Слишком большой # сектора"); return;}
   
   flash_read(block,page,sect);
//   break;
   
   
  case 's': 
   memread(membuf,sector_buf,0x23c);
   dump(membuf,0x23c,0); 
   break;

  case 'n': 
   memread(membuf,nand_cmd,0x100);
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
  
#ifndef WIN32
char* line;
#else
char line[200];
#endif
char scmdline[200]={0};
int i,iolen;
#ifndef WIN32
char devname[50]="/dev/ttyUSB0";
#else
char devname[50]="";
#endif
int opt,helloflag=0;

while ((opt = getopt(argc, argv, "p:ic:hek")) != -1) {
  switch (opt) {
   case 'h': 
     printf("\nИнтерактивная оболочка для ввода команд в загрузчик\n\n\
Допустимы следующие ключи:\n\n\
-p <tty>       - указывает имя устройства последовательного порта для общения с загрузчиком\n\
-i             - запускает процедуру HELLO для инициализации загрузчика\n\
-e             - запрещает передачу префикса 7E перед командой\n\
-k #           - выбор чипсета: 0(MDM9x15, по умолчанию), 1(MDM8200), 2(MSM9x00)\n\
-c \"<команда>\" - запускает указанную команду и завершает работу\n");
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

       default:
	printf("\nНедопустимый номер чипсета\n");
	return;
     }	
    break;
    
   case 'i':
     helloflag=1;
     break;
     
   case 'e':
     prefixflag=0;
     break;
     
   case 'c':
     strcpy(scmdline,optarg);
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

if (!open_port(devname))  {
#ifndef WIN32
   printf("\n - Последовательный порт %s не открывается\n", devname); 
#else
   printf("\n - Последовательный порт COM%s не открывается\n", devname); 
#endif
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
#ifndef WIN32
 line=readline(">");
#else
 printf(">");
 gets(line);
#endif
 if (line == 0) {
    printf("\n");
    return;
 }   
 if (strlen(line) <1) continue; // слишком короткая команда
#ifndef WIN32
 add_history(line); // в буфер ее для истории
#endif
 process_command(line);
#ifndef WIN32
 free(line);
#endif
} 
}
