//
//  Процедуры работы с таблицей разделов флешки
//
#include "include.h"

// глобальные переменные

// хранилище таблицы разделов флешки
struct flash_partition_table fptable;
int validpart=0; // валидность таблицы


//*************************************
//* чтение таблицы разделов из flash
//*************************************
int load_ptable() {

unsigned int udsize=512;
unsigned int blk,pg;
unsigned char buf[4096];

if (get_udflag()) udsize=516;

for (blk=0;blk<12;blk++) {
  // Ищем блок с картами
  flash_read(blk, 0, 0);     // сектор 0 - заголовок блока MIBIB
  memread(buf,sector_buf, udsize);
  if (memcmp(buf,"\xac\x9f\x56\xfe\x7a\x12\x7f\xcd",8) != 0) continue; // сигнатура не найдена - ищем дальше

  // найден блок MiBIB - секторы 1 и 2 его содержат таблицу разделов
  mempoke(nand_exec,1);     // сектор 1 - начало таблицы
  nandwait();
  memread(buf,sector_buf, udsize);
  mempoke(nand_exec,1);     // сектор 2 - продолжение таблицы
  nandwait();
  memread(buf+udsize,sector_buf, udsize);
  // копируем образ таблицы в структуру
  memcpy(&fptable,buf,sizeof(fptable));

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
//* Вывод заголовка таблицы разделов
//***************************************************
void print_ptable_head() {

  printf("\n #  адрес   размер  A0 A1 A2 F#  формат ------ Имя------\n");     
}


//***************************************************
//* Вывод информции о разделе по его номеру
//***************************************************
int show_part(int pn) {
  
if (pn>=fptable.numparts) return 0; // неправильный номер раздела
printf("\r%02u  %6x  %6x   %02x %02x %02x %02x     %s   %.16s\n",
       pn,
       fptable.part[pn].offset,
       fptable.part[pn].len,
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

printf("\n Версия таблицы разделов: %i",fptable.version);
print_ptable_head();
for (i=0;i<fptable.numparts;i++) show_part(i);
}

