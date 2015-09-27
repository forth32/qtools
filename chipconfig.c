//
//  Процедуры работы с конфигурацией чипсета
//
#include "include.h"

// Глбальные переменные - собираем их здесь

// тип чипcета:
int chip_type=0; // индекс текущего чипсета в таблице чипсетов 
int maxchip=-1;   // число чипсетов в конфиге

// описатели чипсетов
struct {
  unsigned int id;         // код (id) чипcета
  unsigned int nandbase;   // адрес контроллера
  unsigned char udflag;    // udsize таблицы разделов, 0-512, 1-516
  unsigned char name[20];  // имя чипсета
  unsigned int ctrl_type;  // схема расположения регистров NAND-контроллера
}  chipset[100];

// таблица кодов идентификации чипсета, не более 20 кодов на 1 чипсет
unsigned short chip_code[100][20];  

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
  { 0,   4,     8,    0xc,   0x10 , 0x14, 0x20  ,0x24,  0x28,  0x40, 0x100 }, //  ctrl=0 - новые
  {0x304,0x300,0xffff,0x30c,0xffff,0x308, 0xffff,0x328,0xffff, 0x320,0     }  //  ctrl=1 - старые
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
  { 0x01,  0x32,  0x34,    0x36,     0x39,     0x3a,   0x0b },   // ctrl=0 - новые
  { 0x07,  0x01,  0xffff,  0x03,   0xffff,     0x04,   0x05 }    // ctrl=1 - старые
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
//*   Загрузка конфига чипсетов
//************************************************
int load_config() {
  
char line[300];
char* tok1, *tok2;
int index;
int msmidcount;
int i;

char vname[50];
char vval[100];

FILE* in=fopen("chipset.cfg","r");  
if (in == 0) {
  printf("\n! Файл конфигурации чипсетов chipset.cfg не найден\n");
  return 0;  // конфиг не найден
}

while(fgets(line,300,in) != 0) {
  if (strlen(line)<3) continue; // слишком короткая строка
  if (line[0] == '#') continue; // комментарий
  index=strspn(line," ");  // получаем указатель на начало информативной части строки
  tok1=line+index;
  
  if (strlen(tok1) == 0) continue; // строка из одних пробелов
  
  // начало описателя очередного чипсета
  if (tok1[0] == '[') {
//     printf("\n@@ описатель чипсета:");
   tok2=strchr(tok1,']');
   if (tok2 == 0) {
      printf("\n! Файл конфигурации содержит ошибку в заголовке чипсета\n %s\n",line);
      return 0;
    }   
    tok2[0]=0;
    // начинаем создание структуры описания очередного чипсета
    maxchip++; // индекс в структуре
    chipset[maxchip].id=0;         // код (id) чипcета 0 - несуществующий чипсет
    chipset[maxchip].nandbase=0;   // адрес контроллера
    chipset[maxchip].udflag=0;    // udsize таблицы разделов, 0-512, 1-516
    strcpy(chipset[maxchip].name,tok1+1);  // имя чипсета
    chipset[maxchip].ctrl_type=0;  // схема расположения регистров NAND-контроллера
    memset(chip_code[maxchip],0xffff,40);  // таблица msm_id заполняется FF
    msmidcount=0;
//       printf("\n@@ %s",tok1+1);
    continue;
  }

  if (maxchip == -1) {
      printf("\n! Файл конфигурации содержит строки вне секции описания чипсетов\n");
      return 0;
  }   
  
  // строка является одной из переменных, описывающих чипсет
  bzero(vname,sizeof(vname));
  bzero(vval,sizeof(vval));
  // выделяем токен-описатель
  index=strspn(line," ");  // получаем указатель на начало информативной части строки
  tok1=line+index; // начало токена
  index=strcspn(tok1," ="); // конец токена
  memcpy(vname,tok1,index); // получаем имя переменной
  
  tok1+=index; 
  if (strchr(tok1,'=') == 0) {
     printf("\n! Файл конфигурации: нет значения переменной\n%s\n",line);
     return 0;
  }
  tok1+=strspn(tok1,"= "); // пропускаем разделитель
  strncpy(vval,tok1,strcspn(tok1," \r\n"));

//   printf("\n @@@ vname = <%s>   vval = <%s>",vname,vval); 
  
  // разбираем имена переменных
  
  // id чипсета
  if (strcmp(vname,"id") == 0) {
     chipset[maxchip].id=atoi(vval);         // код (id) чипcета 0 - несуществующий чипсет
     if (chipset[maxchip].id == 0) {
      printf("\n! Файл конфигурации: id=0 недопустимо\n%s\n",line);
      return 0;
     }
     continue;
  }
  
  // адрес контроллера
  if (strcmp(vname,"addr") == 0) {
    sscanf(vval,"%x",&chipset[maxchip].nandbase);
    continue;
  }
  
  // udflag
  if (strcmp(vname,"udflag") == 0) {
    chipset[maxchip].udflag=atoi(vval);
    continue;
  }
  
  // тип контроллера
  if (strcmp(vname,"ctrl") == 0) {
    chipset[maxchip].ctrl_type=atoi(vval);
    continue;
  }

  // таблица msm_id
  if (strcmp(vname,"msmid") == 0) {
    sscanf(vval,"%hx",&chip_code[maxchip][msmidcount++]);
    continue;
  }
  
  // остальные имена
  printf("\n! Файл конфигурации: недопустимое имя переменной\n%s\n",line);
  return 0;

} 
fclose(in);
if (maxchip == -1) {
  printf("\n! Файл конфигурации не содержит ни одного описателя чипсетов\n");
  return 0;
}  
maxchip++;
return 1;
}

  
//************************************************
//*   Поиск чипсета по msm_id
//************************************************
int find_chipset(unsigned short code) {
int i,j;

for(i=0;i<maxchip;i++) {
  for (j=0;j<20;j++) {
   if (code == chip_code[i][j]) return i;
   if (chip_code[i][j] == 0xffff) break;
  }    
}
// не найдено
return -1;
}  

//************************************************
//* Печать списка поддерживаемых чипсетов
//***********************************************
void list_chipset() {
  
int i;
printf("\n Код     Имя    Адрес NAND   Тип  udflag\n--------------------------------------------");
for(i=0;i<maxchip;i++) {
//  if (i == 0)  printf("\n  0 (по умолчанию) автоопределение чипсета");
  printf("\n %2i  %9.9s    %08x    %1i     %1i",chipset[i].id,chipset[i].name,chipset[i].nandbase,
    chipset[i].ctrl_type,chipset[i].udflag);
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

if (maxchip == -1) 
  if (!load_config()) exit(0); // конфиг не загрузился
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


int i;
chip_type=-1;  

if (maxchip == -1) 
  if (!load_config()) exit(0); // конфиг не загрузился

// получаем размер массива чипсетов
for(i=0;i<maxchip ;i++) 

// проверяем наш номер
  if (chipset[i].id == c) chip_type=i;

if (chip_type == -1) {
  printf("\n - Неверный код чипсета - %i",chip_type);
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

//************************************************************
//*  Проверка имени чипсета
//************************************************************
int is_chipset(char* name) {
  if (strcmp(chipset[chip_type].name,name) == 0) return 1;
  return 0;
}


//**************************************************************
//* Получение udsize
//**************************************************************
unsigned int get_udflag() {
  return chipset[chip_type].udflag;
}  
