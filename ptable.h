#ifndef __PTABLE_H__
#define __PTABLE_H__

//####################################################################### 
//#         Описание структуры таблицы разделов
//#######################################################################


// Длина имени раздела. Она фиксирована, и имя не обязательно заканчивается на 0
//
#define FLASH_PART_NAME_LENGTH 16

// Сигнатура системной таблицы разделов (таблицы чтения)
#define FLASH_PART_MAGIC1     0x55EE73AA
#define FLASH_PART_MAGIC2     0xE35EBDDB

// Сигнатура пользовательской таблицы разделов (таблицы записи)
#define FLASH_USR_PART_MAGIC1     0xAA7D1B9A
#define FLASH_USR_PART_MAGIC2     0x1F7D48BC

// Идентификатор размера раздела, расширяющегося в сторону конца флешки
#define FLASH_PARTITION_GROW  0xFFFFFFFF

// Значения по умолчанию для атрибутов
#define FLASH_PART_FLAG_BASE  0xFF
#define FLASH_PART_ATTR_BASE  0xFF

/* Attributes for NAND paritions */
#define FLASH_PART_HEX_SIZE   0xFE

/* Attribute Masks */
#define FLASH_PART_ATTRIBUTE1_BMSK 0x000000FF
#define FLASH_PART_ATTRIBUTE2_BMSK 0x0000FF00
#define FLASH_PART_ATTRIBUTE3_BMSK 0x00FF0000

/* Attribute Shift Offsets */
#define FLASH_PART_ATTRIBUTE1_SHFT 0
#define FLASH_PART_ATTRIBUTE2_SHFT 8
#define FLASH_PART_ATTRIBUTE3_SHFT 16


#define FLASH_MI_BOOT_SEC_MODE_NON_TRUSTED 0x00
#define FLASH_MI_BOOT_SEC_MODE_TRUSTED     0x01

#define FLASH_PART_ATTRIB( val, attr ) (((val) & (attr##_BMSK)) >> attr##_SHFT)


// Описание атрибута 1
typedef enum {
  FLASH_PARTITION_DEFAULT_ATTRB = 0xFF,  // по умолчанию
  FLASH_PARTITION_READ_ONLY = 0,         // только чтение
} flash_partition_attrb_t;


// Описание атрибута 2
typedef enum {
  // По умолчанию - обычный раздел (512-байтовые секторы)
  FLASH_PARTITION_DEFAULT_ATTRB2 = 0xFF,

  // То же самое - 512-байтовые секторы
  FLASH_PARTITION_MAIN_AREA_ONLY = 0,

  // Линуксовый формат - 516-байтовые секторы, часть spare с тегом защищена ЕСС
  FLASH_PARTITION_MAIN_AND_SPARE_ECC,

  
} flash_partition_attrb2_t;


// Описание атрибута 3
typedef enum {
  
  // по умолчанию
  FLASH_PART_DEF_ATTRB3 = 0xFF,

  // разрешено обновление по имени раздела
  FLASH_PART_ALLOW_NAMED_UPGRD = 0,

} flash_partition_attrb3_t;


//*******************************************************************
//* Описание раздела в системной таблица разделов (таблица чтения)
//*******************************************************************
struct flash_partition_entry;
typedef struct flash_partition_entry *flash_partentry_t;
typedef struct flash_partition_entry *flash_partentry_ptr_type;

struct flash_partition_entry {

  // имя раздела
  char name[FLASH_PART_NAME_LENGTH];

  // смещение в блоках до начала раздела
  uint32 offset;

  // размер раздела в блоках
  uint32 len;

  // атрибуты раздела
  uint8 attr1;
  uint8 attr2;
  uint8 attr3;

  // Флешка, на которой находится раздел (0 - первичная, 1 - вторичная)
  uint8 which_flash;
};

//*************************************************************************
//* Описание раздела в пользовательской таблице разделов (таблица записи)
//*************************************************************************
struct flash_usr_partition_entry;
typedef struct flash_usr_partition_entry *flash_usr_partentry_t;
typedef struct flash_usr_partition_entry *flash_usr_partentry_ptr_type;

struct flash_usr_partition_entry {

  // имя раздела
  char name[FLASH_PART_NAME_LENGTH];

  // Размер раздела в KB 
  uint32 img_size;

  // Размер резервируемой (на бедблоки) области раздела в КВ
  uint16 padding;

  // Флешка, на которой находится раздел (0 - первичная, 1 - вторичная)
  uint16 which_flash;

  // Атрибуты раздела (те же что и в таблице чтения)
  uint8 reserved_flag1;
  uint8 reserved_flag2;
  uint8 reserved_flag3;

  uint8 reserved_flag4; 

};

/*  Number of partition tables which will fit in 512 byte page, calculated
 *  by subtracting the partition table header size from 512 and dividing
 *  the result by the size of each entry.  There is no easy way to do this
 *  with calculations made by the compiler.
 *
 *     16 bytes for header
 *     28 bytes for each entry
 *     512 - 16 = 496
 *     496/28 = 17.71 = round down to 16 entries
 */

// Максимальное число разделов в таблице		
#define FLASH_NUM_PART_ENTRIES  32

//*************************************************************************
//* Структура системной таблицы разделов (таблицы чтения)
//*************************************************************************
struct flash_partition_table {

  // Сигнатуры таблицы
  uint32 magic1;
  uint32 magic2;
  // Версия таблицы
  uint32 version;

  // Число определенных разделов
  uint32 numparts;   
  // Список разделов
  struct flash_partition_entry part[FLASH_NUM_PART_ENTRIES];
//  int8 trash[112]; 
};


//*************************************************************************
//* Структура пользовательской таблицы разделов (таблицы записи)
//*************************************************************************
struct flash_usr_partition_table {

  // Сигнатуры таблицы
  uint32 magic1;
  uint32 magic2;
  // Версия таблицы
  uint32 version;
  // Число определенных разделов
  uint32 numparts;   /* number of partition entries */
  // Список разделов
  struct flash_usr_partition_entry part[FLASH_NUM_PART_ENTRIES];
};

int load_ptable_flash();
int load_ptable_file(char* filename);
int load_ptable(char* name);
void print_ptable_head();
int show_part(int pn);
void list_ptable();
char* part_name(int pn);
int part_start(int pn);
int part_len(int pn);
int block_to_part(int block);

extern struct flash_partition_table fptable;
extern int validpart; // валидность таблицы разделов

#endif // __PTABLE_H__

