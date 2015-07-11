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
#include <io.h>
#include <windows.h>
#include "getopt.h"
#include "printf.h"
#endif
#include "qcio.h"

// флаг посылки префикса 7E
int prefixflag=1;
// флаг HDLC-режима
int hdlcflag=1; 


//@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@
//* Интерактивная оболочка для ввода команд в загрузчик
//@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@


//*******************************************************************
//* Отправка командного буфера в модем и размор результата
//*******************************************************************
void iocmd(char* cmdbuf, int cmdlen) {

unsigned char iobuf[2048];
unsigned int iolen;
  
if (hdlcflag) {
  // Команда HDLC-режима
  iolen=send_cmd_base(cmdbuf,cmdlen,iobuf,prefixflag);
  if (iobuf[1] == 0x0e) {
    show_errpacket ("[ERR] ", iobuf, iolen);
    return;
  }  
}
else  {
  write(siofd,cmdbuf,cmdlen);
  iolen=read(siofd,iobuf,1024);
}  
  
if (iolen != 0) {
  printf("\n ---- ответ --- \n");
  dump(iobuf,iolen,0);
}  
printf("\n");
}

//*******************************************************************
//* Разбор введенной командной последовательности и ее запуск
//*******************************************************************
void ascii_cmd(char* line) {

char* sptr;
unsigned char cmdbuf[2048];
int bcnt=0;
int i;

sptr=strtok(line," "); // разбор командных байтов, разделенных пробелами
if (sptr == 0) return;
 
do {
  sscanf(sptr,"%x",&i);
  cmdbuf[bcnt++]=i;
  sptr=strtok(0," ");
} while (sptr != 0);
iocmd(cmdbuf,bcnt);
}

//*******************************************************************
//*  Отправка в модем содержимого файла в качестве команды 
//*******************************************************************
void binary_cmd(char* line) {
  
unsigned char cmdbuf[2048];
FILE* fcmd;
unsigned int i;

char* sptr;
sptr=strtok(line," "); // выделяем имя файла
if (sptr == 0) {
  printf(" Не указано имя файла\n");
  return;
}  
fcmd=fopen(sptr,"r");
if (fcmd == 0) {
  printf(" Ошибка открытия файла %s\n",sptr);
  return;
}
fseek(fcmd,0,SEEK_END);
i=ftell(fcmd);
if (i>1024) {
  printf(" Слишком большой файл - %u байт\n",i);
  fclose(fcmd);
  return;
}
rewind(fcmd);
fread(cmdbuf,i,1,fcmd);
fclose(fcmd);
iocmd(cmdbuf,i);
}


//*******************************************************************
//*   Переключение hdlc-режима
//*******************************************************************
void hdlcswitch(char* line) {

char* sptr;
unsigned int mode;
sptr=strtok(line," "); // выделяем параметр

if (sptr != 0) {  // режим указан - устанавливаем его
  sscanf(sptr,"%u",&mode);
  hdlcflag=mode?1:0;
}
printf(" HDLC %s\n",hdlcflag?"On":"Off");
}

//*********************************************8
//  Передача загрузчика в модем
//*********************************************8
void send_loader(char* filename) {
  
char cmd06[]={6,0,0,0};
char cmd07[]={7,0,0,0};
//char cmdsend[1024]={
}

//**********************************************
//*  Разбор содержимого регистра CFG0
//**********************************************
void decode_cfg0() {
  
unsigned int cfg0=mempeek(nand_cfg0);
printf("\n **** Конфигурационный регистр 0 *****");
printf("\n * NUM_ADDR_CYCLES              = %x",(cfg0>>27)&7);
printf("\n * SPARE_SIZE_BYTES             = %x",(cfg0>>23)&0xf);
printf("\n * ECC_PARITY_SIZE_BYTES        = %x",(cfg0>>19)&0xf);
printf("\n * UD_SIZE_BYTES                = %x",(cfg0>>9)&0x3ff);
printf("\n * CW_PER_PAGE                  = %x",((cfg0>>6)&7) | ((cfg0>>2)&8));
printf("\n * DISABLE_STATUS_AFTER_WRITE   = %x",(cfg0>>4)&1);
printf("\n * BUSY_TIMEOUT_ERROR_SELECT    = %x",(cfg0)&7);
}

//**********************************************
//*  Разбор содержимого регистра CFG1
//**********************************************
void decode_cfg1() {
  
unsigned int cfg1=mempeek(nand_cfg1);
printf("\n **** Конфигурационный регистр 1 *****");
printf("\n * ECC_MODE                      = %x",(cfg1>>28)&3);
printf("\n * ENABLE_BCH_ECC                = %x",(cfg1>>27)&1);
printf("\n * DISABLE_ECC_RESET_AFTER_OPDONE= %x",(cfg1>>25)&1);
printf("\n * ECC_DECODER_CGC_EN            = %x",(cfg1>>24)&1);
printf("\n * ECC_ENCODER_CGC_EN            = %x",(cfg1>>23)&1);
printf("\n * WR_RD_BSY_GAP                 = %x",(cfg1>>17)&0x3f);
printf("\n * BAD_BLOCK_IN_SPARE_AREA       = %x",(cfg1>>16)&1);
printf("\n * BAD_BLOCK_BYTE_NUM            = %x",(cfg1>>6)&0x3ff);
printf("\n * CS_ACTIVE_BSY                 = %x",(cfg1>>5)&1);
printf("\n * NAND_RECOVERY_CYCLES          = %x",(cfg1>>2)&7);
printf("\n * WIDE_FLASH                    = %x",(cfg1>>1)&1);
printf("\n * ECC_DISABLE                   = %x",(cfg1)&1);
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
@ file            - запуск командного пакета из указанного файла\n\
d adr [len]       - прсмотр дампа адресного пространства системы\n\
m adr word ...    - записать слова по указанному адресу\n\
r block page sect - чтение блока флешки в секторный буфер \n\
s                 - прсмотр дампа сектороного буфера NAND-контроллера\n\
n                 - просмотр содежримого регистров NAND-контроллера\n\
k                 - разбор содержимого конфигурационных регистров\n\
i [s]             - запуск процедуры HELLO, s - без настройки конфигурации\n\
f [n]             - включение(1)/отключение(0)/просмотр состояния HDLC-режима\n\
x                 - выход из программы\n\
\n");
    break;
  // обработка командного пакета
  case 'c':  
   ascii_cmd(cmdline+1);
   break;
 
  case '@':  
   binary_cmd(cmdline+1);
   break;

  case 'f':
    hdlcswitch(cmdline+1);
    break;
    
  // активация загрузчика 
  case 'i':
    sptr=strtok(cmdline+1," "); // адрес
    if (sptr == 0) {
      hello(1);
      break;
    }
    if (sptr[0] != 's') {
      printf("\n Недопустимы параметр в команде i");
      break;
    }
    hello(2);
    break;
    
  // дамп памяти
  case 'd':  
   sptr=strtok(cmdline+1," "); // адрес
   if (sptr == 0) {printf("\n Не указан адрес"); return;}
   sscanf(sptr,"%x",&adr);
   sptr=strtok(0," "); // длина
   if (sptr != 0) sscanf(sptr,"%x",&len);
   if (memread(membuf,adr,len)) dump(membuf,len,adr); 
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
   printf("\n* 048 NAND_FLASH_READ_ID2    = %08x",*((unsigned int*)&membuf[0x48]));
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

  case 'k':
    decode_cfg0();
    printf("\n");
    decode_cfg1();
    printf("\n");
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
char oldcmdline[200]="";
#else
char line[200];
#endif
char scmdline[200]={0};
#ifndef WIN32
char devname[50]="/dev/ttyUSB0";
#else
char devname[50]="";
#endif
int opt,helloflag=0;

while ((opt = getopt(argc, argv, "p:ic:hek:f")) != -1) {
  switch (opt) {
   case 'h': 
     printf("\nИнтерактивная оболочка для ввода команд в загрузчик\n\n\
Допустимы следующие ключи:\n\n\
-p <tty>       - указывает имя устройства последовательного порта для общения с загрузчиком\n\
-i             - запускает процедуру HELLO для инициализации загрузчика\n\
-e             - запрещает передачу префикса 7E перед командой\n\
-f             - отключает HDLC-форматирование командных пакетов\n\
-k #           - код чипсета (-kl - получить список кодов)\n\
-c \"<команда>\" - запускает указанную команду и завершает работу\n");
    return;
     
   case 'p':
    strcpy(devname,optarg);
    break;

   case 'f':
     hdlcflag=0;
     break;
    
   case 'k':
    define_chipset(optarg);
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
if (helloflag) hello(1);
printf("\n");

// запуск команды из ключа -C, если есть
if (strlen(scmdline) != 0) {
  process_command(scmdline);
  return;
}
 
// Основной цикл обработки команд
#ifndef WIN32
 // загрузка истории команд
 read_history("qcommand.history");
 write_history("qcommand.history");
#endif 

for(;;)  {
#ifndef WIN32
 line=readline(">");
#else
 printf(">");
 fgets(line, sizeof(line), stdin);
#endif
 if (line == 0) {
    printf("\n");
    return;
 }   
 if (strlen(line) <1) continue; // слишком короткая команда
#ifndef WIN32
 if (strcmp(line,oldcmdline) != 0) {
   add_history(line); // в буфер ее для истории
   append_history(1,"qcommand.history");
   strcpy(oldcmdline,line);
 }  
#endif
 process_command(line);
#ifndef WIN32
 free(line);
#endif
} 
}
