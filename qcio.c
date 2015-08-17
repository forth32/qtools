//
//  Драйверы для работы с Flash модема через обращения к NAND-контроллеру и процедурам загрузчика
//
#include "include.h"

// Глбальные переменные - собираем их здесь

unsigned int nand_cmd=0x1b400000;
unsigned int spp=0;
unsigned int pagesize=0;
unsigned int sectorsize=512;
unsigned int maxblock=0;     // Общее число блоков флешки
char flash_mfr[30]={0};
char flash_descr[30]={0};
unsigned int oobsize=0;
unsigned int bad_loader=0;

// тип чипcета:
unsigned int chip_type=0; 
unsigned int maxchip=0;

// описатели чипсетов
struct {
  unsigned int nandbase;   // адрес контроллера
  unsigned char udflag;    // udsize таблицы разделов, 0-512, 1-516
  unsigned char name[20];  // имя чипсета
  unsigned int ctrl_type;  // схема расположения регистров NAND-контроллера
}  chipset[]= {
//  адрес NAND  UDflag  имя    ctrl      ##
  { 0xffffffff,   0, "Unknown", 0},  //  0
  { 0xA0A00000,   0, "MDM8200", 0},  //  1
  { 0x81200000,   0, "MDM9x00", 0},  //  2
  { 0xf9af0000,   1, "MDM9x25", 0},  //  3
  { 0x1b400000,   0, "MDM9x15", 0},  //  4
  { 0x70000000,   0, "MDM6600", 0},  //  5
  { 0x60000300,   0, "MDM6800", 0},  //  6
  { 0x60000000,   0, "MSM6246", 1},  //  7
  { 0xA0A00000,   0, "MSM7x27", 0},  //  8
  { 0,0,0 }
};

// Адреса регистров чипcета (смещения относительно базы)
struct {
  unsigned int nand_cmd;
  unsigned int nand_addr0;
  unsigned int nand_addr1;
  unsigned int nand_cs;   
  unsigned int nand_exec; 
  unsigned int nand_status;
  unsigned int nand_cfg0;  
  unsigned int nand_cfg1;  
  unsigned int nand_ecc_cfg;
  unsigned int NAND_FLASH_READ_ID; 
  unsigned int sector_buf;
} nandreg[]={
// cmd  adr0   adr1    cs    exec  stat   cfg0   cfg1   ecc     id   sbuf 
  { 0,   4,     8,    0xc,   0x10 , 0x14, 0x20  ,0x24,  0x28,  0x40, 0x100 }, // Тип 0 - MDM 
  {0x304,0x300,0xffff,0x30c,0xffff,0x308, 0xffff,0x328,0xffff, 0x320,0     }  // Тип 1 - MSM6246 и подобные
};  

// Команды контроллера
struct {
  unsigned int stop;      // Остановка операций кнтроллера
  unsigned int read;      // только данные
  unsigned int readall;   // данные+ECC+spare
  unsigned int program;    // только данные
  unsigned int programall; // данные+ECC+spare
  unsigned int erase;
  unsigned int identify;
} nandopcodes[] = {
//  stop   read  readall  program  programall erase  indetify
  { 0x01,  0x32,  0x34,    0x36,     0x39,     0x3a,   0x0b },   // MDM
  { 0x07,  0x01,  0xffff,  0x03,   0xffff,     0x04,   0x05 }    // MSM
};  

// глобальные хранилища адресов кнтроллера

unsigned int nand_addr0;
unsigned int nand_addr1;
unsigned int nand_cs;   
unsigned int nand_exec; 
unsigned int nand_status;
unsigned int nand_cfg0;  
unsigned int nand_cfg1;  
unsigned int nand_ecc_cfg;
unsigned int NAND_FLASH_READ_ID; 
unsigned int sector_buf;

// глобальные хранилища кодов команд

unsigned int nc_stop,nc_read,nc_readall,nc_program,nc_programall,nc_erase,nc_identify;

//************************************************
//* Печать списка поддерживаемых чипсетов
//***********************************************
void list_chipset() {
  
int i;
printf("\n Код     Имя    Адрес NAND\n-----------------------------------");
for(i=1;chipset[i].nandbase != 0 ;i++) {
//  if (i == 0)  printf("\n  0 (по умолчанию) автоопределение чипсета");
  printf("\n %2i  %9.9s    %08x",i,chipset[i].name,chipset[i].nandbase);
}
printf("\n\n");
exit(0);
}

//*******************************************************************************
//*   Установка типа чипсета по коду чипсета из командной сроки
//* 
//* arg - указатель на optarg, указанный в ключе -к
//****************************************************************
void define_chipset(char* arg) {

unsigned int c;  
  
// проверяем на -kl
if (optarg[0]=='l') list_chipset();

// получаем код чипсета из аргумента
sscanf(arg,"%u",&c);
set_chipset(c);
}

//****************************************************************
//*   Установка параметров контрллера по типу чипсета
//****************************************************************
void set_chipset(unsigned int c) {

chip_type=c;  

// получаем размер массива чипсетов
for(maxchip=0;chipset[maxchip].nandbase != 0 ;maxchip++);  
// проверяем наш номер
if (chip_type>=maxchip) {
  printf("\n - Неверный код чипсета - %u",chip_type);
  exit(1);
}
// устанавливаем адреса регистров чипсета
#define setnandreg(name) name=chipset[chip_type].nandbase+nandreg[chipset[chip_type].ctrl_type].name;
setnandreg(nand_cmd)
setnandreg(nand_addr0)
setnandreg(nand_addr1)
setnandreg(nand_cs)   
setnandreg(nand_exec)
setnandreg(nand_status)
setnandreg(nand_cfg0)  
setnandreg(nand_cfg1)  
setnandreg(nand_ecc_cfg)
setnandreg(NAND_FLASH_READ_ID)
setnandreg(sector_buf)
}

//**************************************************************
//* Получение имени текущего чипсета
//**************************************************************
unsigned char* get_chipname() {
  return chipset[chip_type].name;
}  

//**************************************************************
//* Получение типа nand-контроллера
//**************************************************************
unsigned int get_controller() {
  return chipset[chip_type].ctrl_type;
}  


//****************************************************************
//* Ожидание завершения операции, выполняемой контроллером nand  *
//****************************************************************
void nandwait() { 
  if (get_controller() == 0) 
    while ((mempeek(nand_status)&0xf) != 0);  // MDM
  else
    while ((mempeek(nand_status)&0x3) != 0);  // MSM
}



//*************************************88
//* Установка адресных регистров 
//*************************************
void setaddr(int block, int page) {

int adr;  
  
adr=block*ppb+page;  // # страницы от начала флешки

if (get_controller() == 0) {
  // MDM
  mempoke(nand_addr0,adr<<16);         // младшая часть адреса. 16 бит column address равны 0
  mempoke(nand_addr1,(adr>>16)&0xff);  // единственный байт старшей части адреса
}  
else {
  // MSM
  mempoke(nand_addr0,adr<<8);
}
}

//***************************************************************
//* Запуск на выполнение команды контроллера NAND с ожиданием
//***************************************************************
void exec_nand(int cmd) {

if (get_controller() == 0) {
  // MDM  
  mempoke(nand_cmd,cmd); // Сброс всех операций контроллера
  mempoke(nand_exec,0x1);
  nandwait();
}
else {
  // MSM
  mempoke(nand_cmd,cmd); // Сброс всех операций контроллера
  nandwait();
}
}



//*********************************************
//* Сброс контроллера NAND
//*********************************************
void nand_reset() {

exec_nand(1);  
}

//*********************************************
//* Чтение сектора флешки по указанному адресу 
//*********************************************

int flash_read(int block, int page, int sect) {
  
int i;

nand_reset();
// адрес
setaddr(block,page);
if (get_controller() == 0) {
  // MDM - устанавливаем код команды один раз
  mempoke(nand_cmd,0x34); // чтение data+ecc+spare
  // цикл чтения сектров до нужного нам
  for(i=0;i<(sect+1);i++) {
    mempoke(nand_exec,0x1);
    nandwait();
  }  
}
else {
  // MSM - код команды в регистр команд вводится каждый раз
  for(i=0;i<(sect+1);i++) {
    mempoke(nand_cmd,0x34); // чтение data+ecc+spare
    nandwait();
  }  
}  
return 1;
}

//**********************************************8
//* Процедура активации загрузчика hello
//*
//* mode=0 - автоопределение нужности hello
//* mode=1 - принудительный запуск hello
//* mode=2 - принудительный запуск hello без настройки конфигурации 
//**********************************************8
void hello(int mode) {

int i;  
unsigned char rbuf[1024];
char hellocmd[]="\x01QCOM fast download protocol host\x03### ";

// апплет проверки работоспособности загрузчика
unsigned char cmdbuf[]={
  0x11,0x00,0x12,0x00,0xa0,0xe3,0x00,0x00,
  0xc1,0xe5,0x01,0x40,0xa0,0xe3,0x1e,0xff,
  0x2f,0xe1
};
unsigned int cfg1,ecccfg;

// режим тихой инициализации
if (mode == 0) {
  i=send_cmd(cmdbuf,sizeof(cmdbuf),rbuf);
  ttyflush(); 
  i=rbuf[1];
  // Проверяем, не инициализировался ли загрузчик ранее
  if (i == 0x12) {
     if (!test_loader()) {
       printf("\n Используется непатченный загрузчик - продолжение работы невозможно\n");
        exit(1);
     }  
//     printf("\n chipset = %i  base = %i",chip_type,name);
     get_flash_config();
     return;
  }  
  read(siofd,rbuf,1024);   // вычищаем хвост буера с сообщением об ошибке
}  

printf(" Отсылка hello...");
i=send_cmd(hellocmd,strlen(hellocmd),rbuf);
if (rbuf[1] != 2) {
   i=send_cmd(hellocmd,strlen(hellocmd),rbuf);
   if (rbuf[1] != 2) {
     printf(" повторный hello возвратил ошибку!\n");
     dump(rbuf,i,0);
     return;
   }  
}  
i=rbuf[0x2c];
rbuf[0x2d+i]=0;
printf("ok");
if (mode == 2) {
   // тихий запуск - обходим настройку чипсета
   printf(", флеш-память: %s\n",rbuf+0x2d);
   return; 
 }  
ttyflush(); 
if (!test_loader()) {
  printf("\n Используется непатченный загрузчик - продолжение работы невозможно\n");
  exit(1);
}  
set_chipset(chip_type);
printf("\n Чипсет: %s  (%08x)",get_chipname(),nand_cmd); fflush(stdout);
if (chip_type == 3) disable_bam(); // отключаем NANDc BAM, если работаем с 9x25
cfg1=mempeek(nand_cfg1);
if (nand_ecc_cfg != 0xffff) ecccfg=mempeek(nand_ecc_cfg);
else ecccfg=0;
get_flash_config();
printf("\n Флеш-память: %s %s, %s",flash_mfr,rbuf+0x2d,flash_descr); fflush(stdout);
printf("\n Версия протокола: %i",rbuf[0x22]); fflush(stdout);
printf("\n Максимальный размер пакета: %i байта",*((unsigned int*)&rbuf[0x24]));fflush(stdout);
printf("\n Размер сектора: %u байт",sectorsize);fflush(stdout);
printf("\n Размер страницы: %u байт (%u секторов)",pagesize,spp);fflush(stdout);
printf("\n Размер OOB: %u байт",oobsize); fflush(stdout);
printf("\n Тип ECC: %s",(cfg1&(1<<27))?"BCH":"R-S"); fflush(stdout);
if (nand_ecc_cfg != 0xffff) printf(", %i бит",(cfg1&(1<<27))?(((ecccfg>>4)&3)?(((ecccfg>>4)&3)+1)*4:4):4);fflush(stdout);
printf("\n Общий размер флеш-памяти = %u блоков (%i MB)",maxblock,maxblock*ppb/1024*pagesize/1024);fflush(stdout);
printf("\n");
}

//*************************************
//* чтение таблицы разделов из flash
//*************************************
void load_ptable(unsigned char* buf) {

unsigned int udsize=512;
unsigned int blk,pg;

if (chipset[chip_type].udflag) udsize=516;

memset(buf,0,1024); // обнуляем таблицу

for (blk=0;blk<12;blk++) {
  // Ищем блок с картами
  flash_read(blk, 0, 0);     
  memread(buf,sector_buf, udsize);
  if (memcmp(buf,"\xac\x9f\x56\xfe\x7a\x12\x7f\xcd",8) != 0) continue; // сигнатура не найдена - ищем дальше

  // нашли блок с таблицами - теперь ищем страницу с таблицей чтения
  for (pg=0;pg<ppb;pg++) {
    flash_read(blk, pg, 0);     
    memread(buf,sector_buf, udsize);
    if (memcmp(buf,"\xAA\x73\xEE\x55\xDB\xBD\x5E\xE3",8) != 0) continue; // сигнатура таблицы чтения не найдена
    // нашли таблицу - читаем хвост
    mempoke(nand_exec,1);     // сектор 1 - продолжение таблицы
    nandwait();
    memread(buf+udsize,sector_buf, udsize);
    return; // все - таблица найдна, более тут делать нечего
  }
}  
printf("\n Таблица разделов не найдена. Завершаем работу.");
exit(1);
}

//**********************************
//* Закрытие потока данных раздела
//**********************************
int qclose(int errmode) {
unsigned char iobuf[600];
unsigned char cmdbuf[]={0x15};
int iolen;

iolen=send_cmd(cmdbuf,1,iobuf);
if (!errmode) return 1;
if (iobuf[1] == 0x16) return 1;
show_errpacket("close()",iobuf,iolen);
return 0;

}  

//************************
//* Стирание блока флешки  
//************************

void block_erase(int block) {
  
int oldcfg;  
  
mempoke(nand_addr0,block*ppb);         // младшая часть адреса - # страницы
mempoke(nand_addr1,0);                 // старшая часть адреса - всегда 0

oldcfg=mempeek(nand_cfg0);
mempoke(nand_cfg0,oldcfg&~(0x1c0));    // устанавливаем CW_PER_PAGE=0, как требует даташит

mempoke(nand_cmd,0x3a); // стирание. Бит Last page установлен
mempoke(nand_exec,0x1);
nandwait();
mempoke(nand_cfg0,oldcfg);   // восстанавливаем CFG0
}

//**********************************************************
//*  Получение параметров формата флешки из контроллера
//**********************************************************
void get_flash_config() {
  
unsigned int cfg0, cfg1, nandid, pid, fid, blocksize, devcfg, chipsize;
int i;

struct {
  char* type;   // тектовое описание типа
  unsigned int id;      // ID флешки 
  unsigned int chipsize; // размер флешки в мегабайтах
} nand_ids[]= {

	{"NAND 16MiB 1,8V 8-bit",	0x33, 16},
	{"NAND 16MiB 3,3V 8-bit",	0x73, 16}, 
	{"NAND 16MiB 1,8V 16-bit",	0x43, 16}, 
	{"NAND 16MiB 3,3V 16-bit",	0x53, 16}, 

	{"NAND 32MiB 1,8V 8-bit",	0x35, 32},
	{"NAND 32MiB 3,3V 8-bit",	0x75, 32},
	{"NAND 32MiB 1,8V 16-bit",	0x45, 32},
	{"NAND 32MiB 3,3V 16-bit",	0x55, 32},

	{"NAND 64MiB 1,8V 8-bit",	0x36, 64},
	{"NAND 64MiB 3,3V 8-bit",	0x76, 64},
	{"NAND 64MiB 1,8V 16-bit",	0x46, 64},
	{"NAND 64MiB 3,3V 16-bit",	0x56, 64},

	{"NAND 128MiB 1,8V 8-bit",	0x78, 128},
	{"NAND 128MiB 1,8V 8-bit",	0x39, 128},
	{"NAND 128MiB 3,3V 8-bit",	0x79, 128},
	{"NAND 128MiB 1,8V 16-bit",	0x72, 128},
	{"NAND 128MiB 1,8V 16-bit",	0x49, 128},
	{"NAND 128MiB 3,3V 16-bit",	0x74, 128},
	{"NAND 128MiB 3,3V 16-bit",	0x59, 128},

	{"NAND 256MiB 3,3V 8-bit",	0x71, 256},

	/*512 Megabit */
	{"NAND 64MiB 1,8V 8-bit",	0xA2, 64},   
	{"NAND 64MiB 1,8V 8-bit",	0xA0, 64},
	{"NAND 64MiB 3,3V 8-bit",	0xF2, 64},
	{"NAND 64MiB 3,3V 8-bit",	0xD0, 64},
	{"NAND 64MiB 1,8V 16-bit",	0xB2, 64},
	{"NAND 64MiB 1,8V 16-bit",	0xB0, 64},
	{"NAND 64MiB 3,3V 16-bit",	0xC2, 64},
	{"NAND 64MiB 3,3V 16-bit",	0xC0, 64},

	/* 1 Gigabit */
	{"NAND 128MiB 1,8V 8-bit",	0xA1,128},
	{"NAND 128MiB 3,3V 8-bit",	0xF1,128},
	{"NAND 128MiB 3,3V 8-bit",	0xD1,128},
	{"NAND 128MiB 1,8V 16-bit",	0xB1,128},
	{"NAND 128MiB 3,3V 16-bit",	0xC1,128},
	{"NAND 128MiB 1,8V 16-bit",     0xAD,128},

	/* 2 Gigabit */
	{"NAND 256MiB 1.8V 8-bit",	0xAA,256},
	{"NAND 256MiB 3.3V 8-bit",	0xDA,256},
	{"NAND 256MiB 1.8V 16-bit",	0xBA,256},
	{"NAND 256MiB 3.3V 16-bit",	0xCA,256},

	/* 4 Gigabit */
	{"NAND 512MiB 1.8V 8-bit",	0xAC,512},
	{"NAND 512MiB 3.3V 8-bit",	0xDC,512},
	{"NAND 512MiB 1.8V 16-bit",	0xBC,512},
	{"NAND 512MiB 3.3V 16-bit",	0xCC,512},

	/* 8 Gigabit */
	{"NAND 1GiB 1.8V 8-bit",	0xA3,1024},
	{"NAND 1GiB 3.3V 8-bit",	0xD3,1024},
	{"NAND 1GiB 1.8V 16-bit",	0xB3,1024},
	{"NAND 1GiB 3.3V 16-bit",	0xC3,1024},

	/* 16 Gigabit */
	{"NAND 2GiB 1.8V 8-bit",	0xA5,2048},
	{"NAND 2GiB 3.3V 8-bit",	0xD5,2048},
	{"NAND 2GiB 1.8V 16-bit",	0xB5,2048},
	{"NAND 2GiB 3.3V 16-bit",	0xC5,2048},

	/* 32 Gigabit */
	{"NAND 4GiB 1.8V 8-bit",	0xA7,4096},
	{"NAND 4GiB 3.3V 8-bit",	0xD7,4096},
	{"NAND 4GiB 1.8V 16-bit",	0xB7,4096},
	{"NAND 4GiB 3.3V 16-bit",	0xC7,4096},

	/* 64 Gigabit */
	{"NAND 8GiB 1.8V 8-bit",	0xAE,8192},
	{"NAND 8GiB 3.3V 8-bit",	0xDE,8192},
	{"NAND 8GiB 1.8V 16-bit",	0xBE,8192},
	{"NAND 8GiB 3.3V 16-bit",	0xCE,8192},

	/* 128 Gigabit */
	{"NAND 16GiB 1.8V 8-bit",	0x1A,16384},
	{"NAND 16GiB 3.3V 8-bit",	0x3A,16384},
	{"NAND 16GiB 1.8V 16-bit",	0x2A,16384},
	{"NAND 16GiB 3.3V 16-bit",	0x4A,16384},
                                                  
	/* 256 Gigabit */
	{"NAND 32GiB 1.8V 8-bit",	0x1C,32768},
	{"NAND 32GiB 3.3V 8-bit",	0x3C,32768},
	{"NAND 32GiB 1.8V 16-bit",	0x2C,32768},
	{"NAND 32GiB 3.3V 16-bit",	0x4C,32768},

	/* 512 Gigabit */
	{"NAND 64GiB 1.8V 8-bit",	0x1E,65536},
	{"NAND 64GiB 3.3V 8-bit",	0x3E,65536},
	{"NAND 64GiB 1.8V 16-bit",	0x2E,65536},
	{"NAND 64GiB 3.3V 16-bit",	0x4E,65536},
	{0,0,0},
};


struct  {
  unsigned int id;
  char* name;
}  nand_manuf_ids[] = {
	{0x98, "Toshiba"},
	{0xec, "Samsung"},
	{0x04, "Fujitsu"},
	{0x8f, "National"},
	{0x07, "Renesas"},
	{0x20, "ST Micro"},
	{0xad, "Hynix"},
	{0x2c, "Micron"},
	{0xc8, "Elite Semiconductor"},
	{0x01, "Spansion/AMD"},
	{0x0, 0}
};

mempoke(nand_cmd,0x8000b); // команда Extended Fetch ID
mempoke(nand_exec,1); 
nandwait();
nandid=mempeek(NAND_FLASH_READ_ID); // получаем ID флешки
chipsize=0;

fid=(nandid>>8)&0xff;
pid=nandid&0xff;

// Определяем производителя флешки
i=0;
while (nand_manuf_ids[i].id != 0) {
	if (nand_manuf_ids[i].id == pid) {
	strcpy(flash_mfr,nand_manuf_ids[i].name);
	break;
	}
i++;
}  
    
// Определяем емкость флешки
i=0;
while (nand_ids[i].id != 0) {
if (nand_ids[i].id == fid) {
	chipsize=nand_ids[i].chipsize;
	strcpy(flash_descr,nand_ids[i].type);
	break;
	}
i++;
}  
if (chipsize == 0) {
	printf("\n Неопределенный Flash ID = %02x",fid);
}  

// Вынимаем параметры конфигурации

sectorsize=512;
devcfg = (nandid>>24) & 0xff;
pagesize = 1024 << (devcfg & 0x3); // размер страницы в байтах
blocksize = 64 << ((devcfg >> 4) & 0x3);  // размер блока в килобайтах
spp = pagesize/sectorsize; // секторов в странице

cfg0=mempeek(nand_cfg0);
if ((((cfg0>>6)&7)|((cfg0>>2)&8)) == 0) {
  // для старых чипсетов младшие 2 байта CFG0 надо настраивать руками
  if (!bad_loader) mempoke(nand_cfg0,(cfg0|0x40000|(((spp-1)&8)<<2)|(((spp-1)&7)<<6)));
}  

// Для чипсета 8200 требуется настройка конфигурации 1 - позиции маркера бедблока
if (nand_cmd == 0xa0a00000) {
  cfg1=mempeek(nand_cfg1);
  cfg1&=~(0x7ff<<6);  //сбрасываем BAD_BLOCK_BYTE_NUM и BAD_BLOCK_IN_SPARE_AREA
  cfg1|=(0x1d1<<6); // BAD_BLOCK_BYTE_NUM
  mempoke(nand_cfg1,cfg1);
}  

if (chipsize != 0)   maxblock=chipsize*1024/blocksize;
else                 maxblock=0x800;

if (oobsize == 0) {
	// Micron MT29F4G08ABBEA3W: на самом деле 224, определяется 128, 
	// реально используется 160, для raw-режима нагляднее 256 :)
	if (nandid == 0x2690ac2c) oobsize = 256; 
	else oobsize = (8 << ((devcfg >> 2) & 0x1)) * (pagesize >> 9);
} 

}

//****************************************
//* Загрузка через сахару
//****************************************
int dload_sahara() {

FILE* in;
char* infilename;
unsigned char sendbuf[131072];
unsigned char replybuf[128];
unsigned int iolen,offset,len,donestat,imgid;
unsigned char helloreply[60]={
 02, 00, 00, 00, 48, 00, 00, 00, 02, 00, 00, 00, 01, 00, 00, 00,
 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00,
 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00
}; 
unsigned char donemes[8]={5,0,0,0,8,0,0,0};

printf("\n Ожидаем пакет Hello от устройства...\n");
port_timeout(100); // пакета Hello будем ждать 10 секунд
iolen=read(siofd,replybuf,48);  // читаем Hello
if ((iolen != 48)||(replybuf[0] != 1)) {
	sendbuf[0]=0x3a; // может быть любое число
	write(siofd,sendbuf,1); // инициируем отправку пакета Hello 
	iolen=read(siofd,replybuf,48);  // пробуем читать Hello ещё раз
	if ((iolen != 48)||(replybuf[0] != 1)) { // теперь всё - больше ждать нечего
		printf("\n Пакет Hello от устройства не получен\n");
		dump(replybuf,iolen,0);
		return 1;
	}
}

// Получили Hello, 
ttyflush();  // очищаем буфер приема
port_timeout(10); // теперь обмен пакетами пойдет быстрее - таймаут 1 с
write(siofd,helloreply,48);   // отправляем Hello Response с переключением режима
iolen=read(siofd,replybuf,20); // ответный пакет
  if (iolen == 0) {
    printf("\n Нет ответа от устройства\n");
    return 1;
  }  
// в replybuf должен быть запрос первого блока загрузчика
imgid=*((unsigned int*)&replybuf[8]); // идентификатор образа
printf("\n Идентификатор образа для загрузки: %08x\n",imgid);
switch (imgid) {

	case 0x07:
	infilename="loaders/NPRG9x25p.bin";
	break;

	case 0x0d:
	infilename="loaders/ENPRG9x25p.bin";
	break;

	default:
	printf("\n Неизвестный идентификатор - нет такого образа!\n");
	return 1;
}
printf("\n Загружаем %s...\n",infilename); 
in=fopen(infilename,"rb");
if (in == 0) {
  printf("\n Ошибка открытия входного файла\n");
  return 1;
}

// Основной цикл передачи кода загрузчика
printf("\n Передаём загрузчик в устройство...\n");
while(replybuf[0] != 4) { // сообщение EOIT
 if (replybuf[0] != 3) { // сообщение Read Data
    printf("\n Пакет с недопустимым кодом - прерываем загрузку!");
    dump(replybuf,iolen,0);
    fclose(in);
    return 1;
 }
  // выделяем параметры фрагмента файла
  offset=*((unsigned int*)&replybuf[12]);
  len=*((unsigned int*)&replybuf[16]);
//  printf("\n адрес=%08x длина=%08x",offset,len);
  fseek(in,offset,SEEK_SET);
  fread(sendbuf,1,len,in);
  // отправляем блок данных сахаре
  write(siofd,sendbuf,len);
  // получаем ответ
  iolen=read(siofd,replybuf,20);      // ответный пакет
  if (iolen == 0) {
    printf("\n Нет ответа от устройства\n");
    fclose(in);
    return 1;
  }  
}
// получили EOIT, конец загрузки
write(siofd,donemes,8);   // отправляем пакет Done
iolen=read(siofd,replybuf,12); // ожидаем Done Response
if (iolen == 0) {
  printf("\n Нет ответа от устройства\n");
  fclose(in);
  return 1;
} 
// получаем статус
donestat=*((unsigned int*)&replybuf[12]); 
if (donestat == 0) {
  printf("\n Загрузчик запущен успешно\n");
} else {
  printf("\n Ошибка запуска загрузчика\n");
}
fclose(in);

return donestat;

}

//****************************************
//* Отключение NANDc BAM
//****************************************
void disable_bam() {

unsigned int i,nandcstate[256];

for (i=0;i<0xec;i+=4) nandcstate[i]=mempeek(nand_cmd+i); // сохраняем состояние контроллера NAND
mempoke(0xfc401a40,1); // GCC_QPIC_BCR
mempoke(0xfc401a40,0); // полный асинхронный сброс QPIC
for (i=0;i<0xec;i+=4) mempoke(nand_cmd+i,nandcstate[i]);  // восстанавливаем состояние
mempoke(nand_exec,1); // фиктивное чтение для снятия защиты адресных регистров контроллера от записи
}


//****************************************************
//* Проверка массива на ненулевые значения
//*
//*  0 - в массиве одни нули
//*  1 - в массиве есть ненули
//****************************************************
int test_zero(unsigned char* buf, int len) {
  
int i;
for (i=0;i<len;i++)
  if (buf[i] != 0) return 1;
return 0;
}

//***************************************************************
//* Идентификация чипсета через апплет по сигнатуре загрузчика
//*
//* return -1 - загрузчик не поддерживает команду 11
//*         0 - в загрузчике не найдена сигнатура идентификации чипсета
//*         остальное - код чипсета из загрузчика 
//***************************************************************
int identify_chipset() {

char cmdbuf[]={ 
  0x11,0x00,0x04,0x10,0x2d,0xe5,0x0e,0x00,0xa0,0xe1,0x03,0x00,0xc0,0xe3,0xff,0x30,
  0x80,0xe2,0x34,0x10,0x9f,0xe5,0x04,0x20,0x90,0xe4,0x01,0x00,0x52,0xe1,0x03,0x00,
  0x00,0x0a,0x03,0x00,0x50,0xe1,0xfa,0xff,0xff,0x3a,0x00,0x00,0xa0,0xe3,0x00,0x00,
  0x00,0xea,0x00,0x00,0x90,0xe5,0x04,0x10,0x9d,0xe4,0x01,0x00,0xc1,0xe5,0xaa,0x00,
  0xa0,0xe3,0x00,0x00,0xc1,0xe5,0x02,0x40,0xa0,0xe3,0x1e,0xff,0x2f,0xe1,0xef,0xbe,
  0xad,0xde
};
unsigned char iobuf[1024];
int iolen;
iolen=send_cmd(cmdbuf,sizeof(cmdbuf),iobuf);
//printf("\n--ident--\n");
//dump(iobuf,iolen,0);
if (iobuf[1] != 0xaa) return -1;
return iobuf[2];
}

//*******************************************************
//* Проверка работы патча загрузчика
//*
//* Возвращает 0, если команда 11 не поддерживается
//* и устанавливает глобальную переменную bad_loader=1
//*******************************************************
int test_loader() {

int i;

i=identify_chipset();
//printf("\n ident = %i\n",i);
if (i<=0) {
  bad_loader=1;
  return 0;
}
if (chip_type == 0) set_chipset(i); // если чипсет не был явно задан
return 1;
}

