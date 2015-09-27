//
//  Процедуры работы с конфигурацией чипсета
//
#include "include.h"

// Глбальные переменные - собираем их здесь

// тип чипcета:
unsigned int chip_type=0; 
unsigned int maxchip=0;

// описатели чипсетов
struct {
  unsigned int nandbase;   // адрес контроллера
  unsigned char udflag;    // udsize таблицы разделов, 0-512, 1-516
  unsigned char name[20];  // имя чипсета
  unsigned int ctrl_type;  // схема расположения регистров NAND-контроллера
  unsigned short chip_code;  // код идентификации чипсета
}  chipset[]= {
//  адрес NAND  UDflag  имя    ctrl  msm_id        ##
  { 0xffffffff,   0, "Unknown", 0,   0xffff},  //  0
  { 0xA0A00000,   0, "MDM8200", 0,   0x04e0},  //  1
  { 0x81200000,   0, "MDM9x00", 0,   0x03f1},  //  2
  { 0xf9af0000,   1, "MDM9x25", 0,   0x07f1},  //  3
  { 0x1b400000,   0, "MDM9x15", 0,   0x0740},  //  4
  { 0x70000000,   0, "MDM6600", 0,   0xffff},  //  5
  { 0x60000300,   0, "MDM6800", 0,   0xffff},  //  6
  { 0x60000000,   0, "MSM6246", 1,   0x0120},  //  7
  { 0xf9af0000,   1, "MDM9x3x", 0,   0xffff},  //  8
  { 0xA0A00000,   0, "MSM7x27", 0,   0xffff},  //  9
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
//*   Поиск чипсета по msm_id
//************************************************
int find_chipset(unsigned short chip_code) {
int i;

for(i=1;chipset[i].nandbase != 0 ;i++) {
  if (chipset[i].chip_code == chip_code) return i;
}
// не найдено
return -1;
}  

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

