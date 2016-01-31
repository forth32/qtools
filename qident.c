#include "include.h"


//@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@
//* Идентификация чипсетов устройств (с DMSS-протоколом)
//

void main(int argc, char* argv[]) {
  
unsigned char iobuf[2048];
unsigned int iolen;

char id_cmd[2]={0,0};
char flashid_cmd[4]={0x4b, 0x13, 0x15, 0};

unsigned short chip_code;
int chip_id;
int opt;

struct  {
  int32 hdr;
  int32 diag_errno;            /* Error code if error, 0 otherwise  */
  int32 total_no_of_blocks;    /* Total number of blocks in the device */
  int32 no_of_pages_per_block; /* Number of pages in a block */
  int32 page_size;             /* Size of page data region in bytes */
  int32 total_page_size;       /* Size of total page_size */
  int32 maker_id;              /* Device maker ID */
  int32 device_id;             /* Device ID */
  uint8 device_type;           /* 0 indicates NOR device 1 NAND */
  uint8 device_name[15];       /* Device name */
} fid;

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
  printf("\n Чипсет не опознан, код чипсета - %04x\n",chip_code);
  return;
}
// чипсет найден
set_chipset(chip_id);
printf("\n Чипсет: %s  (%08x)\n",get_chipname(),nand_cmd); fflush(stdout);

// Определяем тип и параметры флешки
iolen=send_cmd_base(flashid_cmd,4,iobuf,0);
if (iolen == 0) {
  printf("\n Ошибка идентификации flash\n");
  return;
}

memcpy(&fid,iobuf,sizeof(fid));
if (fid.diag_errno != 0) {
  printf("\n Ошибка идентификации flash\n");
  return;
}
printf("\n Flash: %s %s, vid=%02x  pid=%02x",(fid.device_type?"NAND":"NOR"),fid.device_name,fid.maker_id,fid.device_id);
//printf("\n Размер EFS:      %i блоков",fid.total_no_of_blocks);
printf("\n Страниц на блок: %i",fid.no_of_pages_per_block);
printf("\n Размер страницы: %i",fid.page_size);
printf("\n Размер ООВ:      %i",fid.total_page_size-fid.page_size);
printf("\n");
} 
