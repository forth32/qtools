#include "include.h"

//@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@
//* Идентификация чипсетов устройств (с DMSS-протоколом)
//

void main(int argc, char* argv[]) {
  
unsigned char iobuf[2048];
unsigned int iolen;

char id_cmd[2]={0,0};

unsigned short chip_code;
int chip_id;
int opt;

#ifndef WIN32
char devname[20]="/dev/ttyUSB0";
#else
char devname[20]="";
#endif

while ((opt = getopt(argc, argv, "p:")) != -1) {
  switch (opt) {
   case 'h': 
     printf("\n Утилита предназначена для идентификации чипсета устройств, поддерживающих DMSS-протокол.\n\
 Утилита работает с диагностическим портом устройства, находящегося в нормальном (рабочем) режиме.\n\n\
Допустимы следующие ключи:\n\n\
-p <tty>  - указывает имя устройства последовательного порта для общения с загрузчиком\n\
");
    return;
     
   case 'p':
    strcpy(devname,optarg);
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

iolen=send_cmd_base(id_cmd,1,iobuf,0);
if (iolen == 0) {
  printf("\n Нет ответа на команду идентификации\n");
  return;
}

if (iobuf[0] != 0) {
  printf("\n Неправильный ответ на команду идентификации\n");
  return;
}
// выделяем код чипсета
chip_code=*((unsigned short*)&iobuf[0x35]);
// переворачиваем chip id
chip_code=( ((chip_code&0xff)<<8) | ((chip_code&0xff00)>>8) );
// получаем ID чипсета по коду
chip_id=find_chipset(chip_code);
if (chip_id == -1) {	
  printf("\n Чипсет не опознан, код чипсета - %04x",chip_id);
  return;
}
// чипсет найден
set_chipset(chip_id);
printf("\n Чипсет: %s  (%08x)\n",get_chipname(),nand_cmd); fflush(stdout);

} 
