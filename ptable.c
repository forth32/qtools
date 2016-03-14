//
//  Процедуры работы с таблицей разделов флешки
//
#include "include.h"


// хранилище таблицы разделов флешки
struct flash_partition_table fptable;
int validpart=0; // валидность таблицы


//*************************************
//* чтение таблицы разделов из flash
//*************************************
int load_ptable_flash() {

unsigned int udsize=512;
unsigned int blk;
unsigned char buf[4096];

if (get_udflag()) udsize=516;

for (blk=0;blk<12;blk++) {
  // Ищем блок с картами
  flash_read(blk, 0, 0);     // Страница 0 сектор 0 - заголовок блока MIBIB
  memread(buf,sector_buf, udsize);
  // проверяем сигнатуру заголовка MIBIB
  if (memcmp(buf,"\xac\x9f\x56\xfe\x7a\x12\x7f\xcd",8) != 0) continue; // сигнатура не найдена - ищем дальше

  // найден блок MiBIB - страница 1 содержит системную таблицу разделов
  // загружаем эту страницу в наш буфер
  flash_read(blk, 1, 0);     // Страница 1 сектор 0 - системная таблица разделов
  memread(buf,sector_buf, udsize);
  mempoke(nand_exec,1);     // сектор 1 - продолжение таблицы
  nandwait();
  memread(buf+udsize,sector_buf, udsize);

  // копируем образ таблицы в структуру
  memcpy(&fptable,buf,sizeof(fptable));
  // проверяем сигнатуру системной таблицы
  if ((fptable.magic1 != FLASH_PART_MAGIC1) || (fptable.magic2 != FLASH_PART_MAGIC2)) continue;
    // нашли таблицу 
    validpart=1;
    // корректируем длину последнего раздела
    if ((maxblock != 0) && (fptable.part[fptable.numparts-1].len == 0xffffffff)) 
        fptable.part[fptable.numparts-1].len=maxblock-fptable.part[fptable.numparts-1].offset; // если длина - FFFF, то есть растущий раздел
    return 1; // все - таблица найдна, более тут делать нечего
}  
validpart=0;
return 0;  
}

//***************************************************
//* Загрузка таблицы разделов из внешнего файла
//*
//*  Имя файла, состоящее из одного занка минуса '-' - 
//*   загрузка из файла ptable/current-r.bin
//***************************************************
int load_ptable_file(char* name) {

char filename[200];
unsigned char buf[4096];
FILE* pf;

if (name[0] == '-') strcpy(filename,"ptable/current-r.bin");
else strncpy(filename,name,199);
  
pf=fopen(filename,"rb");
if (pf == 0) {
   printf("\n! Ошибка открытия файла таблицы разделов %s\n",filename);
   return 0;
} 
fread(buf,1024,1,pf); // читаем таблицу разделов из файла
fclose(pf);
// копируем образ таблицы в структуру
memcpy(&fptable,buf,sizeof(fptable));
validpart=1;
return 1;
}

//*****************************************************
//* Универсальная процедура загрузки таблицы разделов
//*
//*  Возможные варианты name:
//*
//*    @ - загрузка таблицы с флешки
//*    - - загрузка из файла ptable/current-r.bin
//*  имя - загрузка из указанного файла
//*
//*****************************************************
int load_ptable(char* name) {

if (name[0]== '@') return load_ptable_flash();
else return load_ptable_file(name);
}
  
//***************************************************
//* Вывод заголовка таблицы разделов
//***************************************************
void print_ptable_head() {

  printf("\n #  начало  размер   A0 A1 A2 F#  формат ------ Имя------");     
  printf("\n============================================================\n");
}


//***************************************************
//* Вывод информции о разделе по его номеру
//***************************************************
int show_part(int pn) {
  
if (!validpart) return 0; // таблица еще не загружена
if (pn>=fptable.numparts) return 0; // неправильный номер раздела
printf("\r%02u  %6x",
       pn,
       fptable.part[pn].offset);

if (fptable.part[pn].len != 0xffffffff)
  printf("  %6.6x   ",fptable.part[pn].len);
else printf("  ------   ");

printf("%02x %02x %02x %02x   %s   %.16s\n",
       fptable.part[pn].attr1,
       fptable.part[pn].attr2,
       fptable.part[pn].attr3,
       fptable.part[pn].which_flash,
       (fptable.part[pn].attr2==1)?"LNX":"STD",
       fptable.part[pn].name);

return 1;  
}


//*************************************
//* Вывод таблицы разделов на экран
//*************************************
void list_ptable() {
  
int i;

if (!validpart) return; // таблица еще не загружена
print_ptable_head();
for (i=0;i<fptable.numparts;i++) show_part(i);
printf("============================================================");
printf("\n Версия таблицы разделов: %i\n",fptable.version);
}

//*************************************
// Получение имени по номеру раздела
//*************************************
char* part_name(int pn) {
  
return fptable.part[pn].name;
}

//*********************************************
// Получение стартового блока по номеру раздела
//*********************************************
int part_start(int pn) {
  
return fptable.part[pn].offset;
}

//*********************************************
// Получение длины раздела по номеру раздела
//*********************************************
int part_len(int pn) {
  
return fptable.part[pn].len;
}

//************************************************************
// Получение номера раздела, в который входит указанный блок
//************************************************************
int block_to_part(int block) {

int i;
for(i=0;i<fptable.numparts;i++) {
  if ((block>=part_start(i)) && (block < (part_start(i)+part_len(i)))) 
     return i;
}
// раздел не найден
return -1; 
}
