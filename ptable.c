//
//  Процедуры работы с таблицей разделов флешки
//

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
  memcpy(&fptable,buf,sizeof(fptable));

  if ((fptable.magic1 != LASH_PART_MAGIC1) || (fptable.magic1 != LASH_PART_MAGIC1)) continue;
    // нашли таблицу - читаем хвост
    validpart=1;
    return 1; // все - таблица найдна, более тут делать нечего
  }
}  
validpart=0;
return 0;  
}

