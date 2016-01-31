#include "include.h"

//@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@
//* Чтение флеша модема в файл 
//
//

// флаги способов обработки бедблоков
enum {
  BAD_UNDEF,
  BAD_FILL,
  BAD_SKIP,
  BAD_IGNORE,
  BAD_DISABLE
};

int bad_processing_flag=BAD_UNDEF;
unsigned char blockbuf[220000];

//********************************************************************************
//* Загрузка блока в блочный буфер
//*
//*  возвращает 0, если блок дефектный, или 1, если он нормально прочитался
//* cwsize - размер читаемого сектора (включая ООВ, если надо)
//********************************************************************************
unsigned int load_block(int blk, int cwsize) {

int pg,sec;  
if (bad_processing_flag == BAD_DISABLE) hardware_bad_off();
else if (bad_processing_flag != BAD_IGNORE) {
   if (check_block(blk)) {
    // обнаружен бедблок
    memset(blockbuf,0xbb,cwsize*spp*ppb); // заполняем блочный буфер заполнителем
    return 0; // возвращаем признак бедблока
  }
} 
// хороший блок, или нам насрать на бедблоки - читаем блок 
nand_reset();
mempoke(nand_cmd,0x34); // чтение data+ecc+spare
for(pg=0;pg<ppb;pg++) {
  setaddr(blk,pg);
  for (sec=0;sec<spp;sec++) {
   mempoke(nand_exec,0x1); 
   nandwait();
   memread(blockbuf+(pg*spp+sec)*cwsize,sector_buf, cwsize);
//   printf("\n--- blk %x  pg %i  sec  %i ---\n",blk,pg,sec);
//   dump(blockbuf+(pg*spp+sec)*cwsize,cwsize,0);
  } 
}  
if (bad_processing_flag == BAD_DISABLE) hardware_bad_on();
return 1; // заебись - блок прочитан
}
  
//***************************************
//* Чтение блока данных в выходной файл
//***************************************
unsigned int read_block(int block,int cwsize,FILE* out) {

unsigned int okflag=0;

okflag=load_block(block,cwsize);
if (okflag || (bad_processing_flag != BAD_SKIP)) {
  // блок прочитался, или не прочитался, но мы бедблоки пропускаем
   fwrite(blockbuf,1,cwsize*spp*ppb,out); // записываем его в файл
}
return !okflag;
} 

//****************************************************************
//* Чтение блока данных с восстановлением китайского изврата
//****************************************************************
unsigned int read_block_resequence(int block, FILE* out) {
unsigned int page,sec;
unsigned int okflag;
char iobuf[4096];

okflag=load_block(block,sectorsize+4);
if (!okflag && (bad_processing_flag == BAD_IGNORE)) return 1; // обнаружен бедблок

// цикл по страницам
for(page=0;page<ppb;page++)  {
  // по секторам  
  for(sec=0;sec<spp;sec++) {
   if (sec != (spp-1)) 
     // Для непоследних секторов
     fwrite(blockbuf+(page*spp+sec)*(sectorsize+4),1,sectorsize+4,out);    // Тело сектора + 4 байта OBB
   else 
     // для последнего сектора
     fwrite(blockbuf+(page*spp+sec)*(sectorsize+4),1,sectorsize-4*(spp-1),out);   // Тело сектора - хвост oob
  }
 } 
return 0; 
} 



//*****************************
//* чтение сырого флеша
//*****************************
void read_raw(int start,int len,int cwsize,FILE* out, unsigned int rflag) {
  
int block;  
unsigned int badflag;

printf("\n Чтение блоков %08x - %08x",start,start+len-1);
printf("\n Формат данных: %u+%i\n",sectorsize,cwsize-sectorsize);
// главыный цикл
// по блокам
for (block=start;block<(start+len);block++) {
  printf("\r Блок: %08x",block); fflush(stdout);
  if (rflag != 2) badflag=read_block(block,cwsize,out);
  else            badflag=read_block_resequence(block,out); 
  if (badflag != 0) printf(" - Badblock\n");   
} 
printf("\n"); 
}


//*********************************
//* Построение списка бедблоков
//*********************************
void defect_list(int start, int len) {
  
FILE* out; 
int blk,pg=0;
int badcount=0;
int vptable=0; // флаг валидности таблицы разделов
int i,npar;
int pstart,plen;
unsigned char ptable[1100]; // таблица разделов

out=fopen("badblock.lst","w");
fprintf(out,"Список дефектных блоков");
if (out == 0) {
  printf("\n Невозможно создать файл badblock.lst\n");
  return;
}

// загружаем таблицу разделов
load_ptable(ptable);
if (strncmp(ptable,"\xAA\x73\xEE\x55\xDB\xBD\x5E\xE3",8) == 0) vptable=1;
npar=*((unsigned int*)&ptable[12]);


printf("\nПостроение списка дефектных блоков в интервале %08x - %08x\n",start,start+len);
for(blk=start;blk<(start+len);blk++) {
 printf("\r Проверка блока %08x",blk); fflush(stdout);
 if (check_block(blk)) {
     printf(" - badblock");
     fprintf(out,"\n%08x",blk);
     if (vptable) 
       // поиск раздела, в котором лежит этот блок
       for(i=0;i<npar;i++) {
         pstart=*((unsigned int*)&ptable[32+28*i]);   // адрес блока начала раздела
         plen=*((unsigned int*)&ptable[36+28*i]);     // размер раздела
	 if ((blk>pstart) && (blk<(pstart+plen))) {
	   printf(" (%s+%x)",ptable+16+28*i+2,blk-pstart);
	   fprintf(out," (%s+%x)",ptable+16+28*i,blk-pstart);
	   break;
	 }  
    }
     badcount++;
     printf("\n");
    }
}     // blk
fclose (out);
printf("\r * Всего дефектных блоков: %i\n",badcount);
}
 

//@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@
void main(int argc, char* argv[]) {
  
unsigned char iobuf[2048];
unsigned char partname[17]={0}; // имя раздела
unsigned char filename[300]="qflash.bin";
unsigned int i,sec,bcnt,iolen,page,block,filepos,lastpos;
int res;
unsigned char c;
unsigned char* sptr;
unsigned int start=0,len=0,opt;
unsigned int partlist[60]; // список разделов, разрешенных для чтения
unsigned int cwsize;  // размер порции данных, читаемых из секторного буфера за один раз

FILE* out;
FILE* part=0;
int partflag=0;  // 0 - сырой флеш, 1 - таблица разделов из файла, 2 - таблица разделов из флеша
int eccflag=1;  // 1 - отключить ECC,  0 - включить
int partnumber=-1; // флаг выбра раздела для чтения, -1 - все разделы, 1 - по списку
int rflag=0;     // формат разделов: 0 - авто, 1 - стандартный, 2 - линуксокитайский
int listmode=0;    // 1- вывод карты разделов
int truncflag=0;  //  1 - отрезать все FF от конца раздела
int xflag=0;      // 
int dflag=0;      // режим дефектоскопа
unsigned int badflag;

int attr; // арибуты
unsigned int npar; // число разедлов в таблице


#ifndef WIN32
char devname[50]="/dev/ttyUSB0";
#else
char devname[50]="";
#endif
unsigned char ptable[1100]; // таблица разделов

// параметры флешки
oobsize=0;      // оов на 1 страницу
memset(partlist,0,sizeof(partlist)); // очищаем список разрешенных к чтению разделов

while ((opt = getopt(argc, argv, "hp:b:l:o:xs:ef:mtk:r:z:du:")) != -1) {
  switch (opt) {
   case 'h': 
     
printf("\n Утилита предназначена для чтения образа флеш-памяти через модифицированный загрузчик\n\
 Допустимы следующие ключи:\n\n\
-p <tty> - последовательный порт для общения с загрузчиком\n\
-d - вывести список имеющихся дефектных блоков\n\
-e - включить коррекцию ECC при чтении\n\
-x - читать полный сектор - данные+oob (без ключа - только данные)\n\n\
-k # - код чипсета (-kl - получить список кодов)\n\
-z # - размер OOB на страницу, в байтах (перекрывает автоопределенный размер)\n\
-u <x> - определяет режим обработки дефектных блоков:\n\
   -uf - заполнить образ дефектного блока в выходном файле байтом 0xbb\n\
   -us - пропустить дефектные блоки при чтении\n\
   -ui - игнорировать маркер дефектного блока (читать как читается)\n\
   -ux - отключить аппаратный контроль дефекных блоков\n");
printf("\n * Для режима неформатированного чтения и проверки дефектных блоков:\n\
-b <blk> - начальный номер читаемого блока (по умолчанию 0)\n\
-l <num> - число читаемых блоков, 0 - до конца флешки\n\
-o <file> - имя выходного файла (по умолчанию qflash.bin)(только для режима чтения)\n\n\
 * Для режима чтения разделов:\n\n\
-s <file> - взять карту разделов из файла\n\
-s @ - взять карту разделов из флеша\n\
-s - - взять карту разделов из файла ptable/current-r.bin\n\
-f n - читать только раздел c номером n (может быть указан несколько раз для формирования списка разделов)\n\
-t - отрезать все FF за последним значимым байтом раздела\n\
-r <x> - формат данных:\n\
    -rs - стандартный формат (512-байтные блоки)\n\
    -rl - линуксовый формат (516-байтные блоки, кроме последнего на странице)\n\
    -ra - (по умолчанию и только для режима разделов) автоопределение формата по атрибуту раздела\n\
-m - вывести полную карту разделов\n");
    return;
    
   case 'k':
    define_chipset(optarg);
    break;
    
   case 'p':
    strcpy(devname,optarg);
    break;
    
   case 'o':
    strcpy(filename,optarg);
    break;
    
   case 'b':
     sscanf(optarg,"%x",&start);
     break;

   case 'l':
     sscanf(optarg,"%x",&len);
     break;

   case 'd':
     dflag=1;
     break;
     
   case 'z':
     sscanf(optarg,"%u",&oobsize);
     break;

   case 'u':
     if (bad_processing_flag != BAD_UNDEF) {
       printf("\n Дублированный ключ u - ошибка\n");
       return;
     }  
     switch(*optarg) {
       case 'f':
	 bad_processing_flag=BAD_FILL;
	 break; 
       case 'i':
	 bad_processing_flag=BAD_IGNORE;
	 break; 
       case 's':
	 bad_processing_flag=BAD_SKIP;
	 break; 
       case 'x':
	 bad_processing_flag=BAD_DISABLE;
	 break;
       default:
	 printf("\n Недопустимое значение ключа u\n");
	 return;
     } 
     break;
	 

   case 'r':
     switch(*optarg) {
       case 'a':
	 rflag=0;   // авто
	 break;     
       case 's':
	 rflag=1;   // стандартный
	 break;
       case 'l':
	 rflag=2;   // линуксовый
	 break;
       default:
	 printf("\n Недопустимое значение ключа r\n");
	 return;
     } 
     break;
     
   case 'x':
     xflag=1;
     break;
     
   case 's':
     if (optarg[0] == '@')  partflag=2; // загружаем таблицу разделов из флеша
     else {
       // загружаем таблицу разделов из файла
       partflag=1;
       if (optarg[0] == '-')   part=fopen("ptable/current-r.bin","rb");
       else part=fopen(optarg,"rb");
       if (part == 0) {
         printf("\nОшибка открытия файла таблицы разделов\n");
         return;
       } 
       fread(ptable,1024,1,part); // читаем таблицу разделов из файла
       fclose(part);
     } 
     break;
     
   case 'm':
     listmode=1;
     break;
     
   case 'f':
     partnumber=1;
     sscanf(optarg,"%u",&i);
     partlist[i]=1;
     break;
     
   case 't':
     truncflag=1;
     break;

   case '?':
   case ':':  
     return;
  }
}  


// Определяем значение ключа -u по умолчанию
if (bad_processing_flag==BAD_UNDEF) {
  if (partflag == 0) bad_processing_flag=BAD_FILL; // для чтения диапазона блоков
  else bad_processing_flag=BAD_SKIP;               // для чтения разделов
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

if ((truncflag == 1)&&(xflag == 1)) {
  printf("\nКлючи -t и -x несовместимы\n");
  return;
}  
hello(0);
cwsize=sectorsize;
if (xflag) cwsize+=oobsize/spp; // наращиваем размер codeword на размер порции OOB на каждый сектор

if (partflag == 2) load_ptable(ptable); // загружаем таблицу разделов

mempoke(nand_cfg1,mempeek(nand_cfg1)&0xfffffffe|eccflag); // ECC on/off
//mempoke(nand_cs,4); // data mover
//mempoke(nand_cs,0); // data mover

mempoke(nand_cmd,1); // Сброс всех операций контроллера
mempoke(nand_exec,0x1);
nandwait();

//###################################################
// Режим списка дефектных блоков:
//###################################################
if (dflag) {
  if (len == 0) len=maxblock-start; //  до конца флешки
  defect_list(start,len);
  return;
}  

// устанавливаем код команды
//if (cwsize == sectorsize) mempoke(nand_cmd,0x32); // чтение только данных
//else  

// В принципе, проще всегда читать data+oob, а уже программа сама разберется, что из этого реально нужно.
mempoke(nand_cmd,0x34); // чтение data+ecc+spare

// чистим секторный буфер
for(i=0;i<cwsize;i+=4) mempoke(sector_buf+i,0xffffffff);

//###################################################
// Режим чтения сырого флеша
//###################################################

if (len == 0) len=maxblock-start; //  до конца флешки

if (partflag == 0) { 
  out=fopen(filename,"wb");
  read_raw(start,len,cwsize,out,rflag);
  fclose(out);
  return;
}  


//###################################################
// Режим чтения по таблице разделов
//###################################################

if (strncmp(ptable,"\xAA\x73\xEE\x55\xDB\xBD\x5E\xE3",8) != 0) {
   printf("\nТаблица разделов повреждена\n");
   return;
}

npar=*((unsigned int*)&ptable[12]);
printf("\n Версия таблицы разделов: %i",*((unsigned int*)&ptable[8]));
if ((partnumber != -1) && (partnumber>=npar)) {
  printf("\nНедопустимый номер раздела: %i, всего разделов %u\n",partnumber,npar);
  return;
}  
printf("\n #  адрес    размер   атрибуты ------ Имя------\n");     
for(i=0;i<npar;i++) {
   // разбираем элементы таблицы разделов
      strncpy(partname,ptable+16+28*i,16);       // имя
      start=*((unsigned int*)&ptable[32+28*i]);   // адрес
      len=*((unsigned int*)&ptable[36+28*i]);     // размер
      attr=*((unsigned int*)&ptable[40+28*i]);    // атрибуты
      if (((start+len) >maxblock)||(len == 0xffffffff)) len=maxblock-start; // если длина - FFFF, или выходит за пределы флешки
  // Выводим описание раздела - для всех разделов или для конкретного заказанного
    if ((partnumber == -1) || (partlist[i]==1))  printf("\r%02u %08x  %08x  %08x  %s\n",i,start,len,attr,partname);
  // Читаем раздел - если не указан просто вывод карты. 
    if (listmode == 0) 
      // Все разделы или один конкретный  
      if ((partnumber == -1) || (partlist[i]==1)) {
        // формируем имя файла
        if (cwsize == sectorsize) sprintf(filename,"%02u-%s.bin",i,partname); 
        else                   sprintf(filename,"%02u-%s.oob",i,partname);  
        if (filename[4] == ':') filename[4]='-';    // заменяем : на -
        out=fopen(filename,"wb");  // открываем выходной файл
	if (out == 0) {
	  printf("\n Ошибка открытия выходного файла %s\n",filename);
	  return;
	}  
        for(block=start;block<(start+len);block++) {
          printf("\r * R: блок %06x [start+%03x] (%i%%)",block,block-start,(block-start+1)*100/len); fflush(stdout);
	  
    //	  Собственно чтение блока
	  switch (rflag) {
	    case 0: // автовыбор формата
               if ((attr != 0x1ff)||(cwsize>sectorsize)) 
	       // сырое чтение или чтение неизварщенных разделов
	           badflag=read_block(block,cwsize,out);
	       else 
	       // чтение китайсколинуксовых разделов
	           badflag=read_block_resequence(block,out);
	       break;
	       
	    case 1: // стандартный формат  
	      badflag=read_block(block,cwsize,out);
	      break;
	      
	    case 2: // китайсколинуксовый формат  
               badflag=read_block_resequence(block,out);
	      break;
	 }  
        if (badflag != 0) printf(" - Badblock \n");
        }
     // Обрезка всех FF хвоста
      fclose(out);
      if (truncflag) {
       out=fopen(filename,"r+b");  // переоткрываем выходной файл
       fseek(out,0,SEEK_SET);  // перематываем файл на начало
       lastpos=0;
       for(filepos=0;;filepos++) {
	c=fgetc(out);
	if (feof(out)) break;
	if (c != 0xff) lastpos=filepos;  // нашли значимый байт
       }
#ifndef WIN32
       ftruncate(fileno(out),lastpos+1);   // обрезаем файл
#else
       _chsize(_fileno(out),lastpos+1);   // обрезаем файл
#endif
       fclose(out);
      }	
    }	
}
printf("\n"); 
    
} 
