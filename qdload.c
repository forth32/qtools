#include "include.h"

// Размер блока загрузки
#define dlblock 1017

//****************************************************
//*  Выделенеи таблиц разделов в отдельные файлы
//****************************************************
void extract_ptable() {
  
unsigned int addr,blk,pg,udsize,npar;
unsigned char buf[4096];
FILE* out;

// получаем размер юзерских даных сектора
udsize=(mempeek(nand_cfg0)&(0x3ff<<9))>>9; 
// вынимаем координаты страницы с таблицей
addr=mempeek(nand_addr0)>>16;
if (addr == 0) { 
  // если адреса таблицы нет в регистре - ищем его
  load_ptable(buf); 
  addr=mempeek(nand_addr0)>>16;
}  
blk=addr/ppb;
pg=addr%ppb;
nandwait(); // ждем окончания всех предыдущих операций

flash_read(blk, pg, 0);  // сектор 0 - начало таблицы разделов  
memread(buf,sector_buf, udsize);

mempoke(nand_exec,1);     // сектор 1 - продолжение таблицы
nandwait();
memread(buf+udsize,sector_buf, udsize);
printf("-----------------------------------------------------");
// проверяем таблицу
if (memcmp(buf,"\xAA\x73\xEE\x55\xDB\xBD\x5E\xE3",8) != 0) {
   printf("\nТаблица разделов режима чтения не найдена\n");
   return;
}

// Определяем размер и записываем таблицу чтения
npar=*((unsigned int*)&buf[12]); // число разделов в таблице
out=fopen("ptable/current-r.bin","wb");
if (out == 0) {
  printf("\n Ошибка открытия выходного файла ptable/current-r.bin");
  return;
}  
fwrite(buf,16+28*npar,1,out);
printf("\n * Найдена таблица разделов режима чтения");
fclose (out);

// Ищем таблицу записи
for (pg=pg+1;pg<ppb;pg++) {
  flash_read(blk, pg, 0);  // сектор 0 - начало таблицы разделов    
  memread(buf,sector_buf, udsize);
  if (memcmp(buf,"\x9a\x1b\x7d\xaa\xbc\x48\x7d\x1f",8) != 0) continue; // сигнатура не найдена - ищем дальше

  // нашли таблицу записи   
  mempoke(nand_exec,1);     // сектор 1 - продолжение таблицы
  nandwait();
  memread(buf+udsize,sector_buf, udsize);
  npar=*((unsigned int*)&buf[12]); // число разделов в таблице
  out=fopen("ptable/current-w.bin","wb");
  if (out == 0) {
    printf("\n Ошибка открытия выходного файла ptable/current-w.bin");
    return;
  }  
  fwrite(buf,16+28*npar,1,out);
  fclose (out);
  printf("\n * Найдена таблица разделов режима записи");
  return;
}
printf("\n - Таблица разделов режима записи не найдена");
}



//@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@2
void main(int argc, char* argv[]) {

int opt;
unsigned int start=0;
#ifndef WIN32
char devname[50]="/dev/ttyUSB0";
#else
char devname[50]="";
#endif
FILE* in;
struct stat fstatus;
unsigned int i,partsize,iolen,adr,helloflag=0;
unsigned int sahara_flag=0;
unsigned int tflag=0;
unsigned int ident_flag=0, iaddr,ichipset;
unsigned int filesize;

unsigned char iobuf[4096];
unsigned char cmd1[]={0x06};
unsigned char cmd2[]={0x07};
unsigned char cmddl[2048]={0xf};
unsigned char cmdstart[2048]={0x5,0,0,0,0};
unsigned int delay=2;



while ((opt = getopt(argc, argv, "p:k:a:histd:q")) != -1) {
  switch (opt) {
   case 'h': 
     printf("\n Утилита предназначена для загрузки программ-прошивальщика (E)NPRG в память модема\n\n\
Допустимы следующие ключи:\n\n\
-p <tty>  - указывает имя устройства последовательного порта, переведенного в download mode\n\
-i        - запускает процедуру HELLO для инициализации загрузчика\n\
-q        - запускает процедуру HELLO в упрощенном режиме без настройки регистров\n\
-t        - вынимает из модема таблицы разделов в файлы ptable/current-r(w).bin\n\
-s        - использовать протокол SAHARA\n\
-k #      - код чипсета (-kl - получить список кодов)\n\
-a <adr>  - адрес загрузки, по умолчанию 41700000\n\
-d <n>    - задержка для инициализации загрузчика, 0.1с\n\
\n");
    return;
     
   case 'p':
    strcpy(devname,optarg);
    break;

   case 'k':
    define_chipset(optarg);
    break;

   case 'i':
    helloflag=1;
    break;
    
   case 'q':
    helloflag=2;
    break;
    
   case 's':
    sahara_flag=1;
    break;
    
   case 't':
    tflag=1;
    break;
    
   case 'a':
     sscanf(optarg,"%x",&start);
     break;

   case 'd':
     sscanf(optarg,"%u",&delay);
     break;
  }     
}

if ((tflag == 1) && (helloflag == 0)) {
  printf("\n Ключ -t без ключа -i указывать нельзя\n");
  exit(1);
}  

delay*=100000; // переводим в микросекунды
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

// Удаляем старые таблицы разделов

unlink("ptable/current-r.bin");
unlink("ptable/current-w.bin");


//----- Вариант загрузки через сахару -------

if (sahara_flag) {
  if (dload_sahara() == 0) {
	#ifndef WIN32
	usleep(200000);   // ждем инициализации загрузчика
	#else
	Sleep(200);   // ждем инициализации загрузчика
	#endif

	if (helloflag) {
	        set_chipset(chip_type);    // все процедуры жестко завязаны на 9x25
		hello(helloflag);
		printf("\n");
		if (tflag && (helloflag != 2)) extract_ptable();  // вынимаем таблицы разделов
	}
  }
  return;
}	

// ---- открываем входной файл
in=fopen(argv[optind],"rb");
if (in == 0) {
  printf("\nОшибка открытия входного файла\n");
  return;
}


// Ищем блок идентификации
fseek(in,-12,SEEK_END);
fread(&i,4,1,in);
if (i == 0xdeadbeef) {
  // нашли блок - разбираем
  printf("\n Найден блок идентификации загрузчика");
  fread(&ichipset,4,1,in);
  fread(&iaddr,4,1,in);
  ident_flag=1;
  if (start == 0) start=iaddr;
  if (chip_type == 0) set_chipset(ichipset);
}
rewind(in);

// проверяем тип чипсета
if ((chip_type == 0)&&(helloflag==1)) {
  printf("\n Не указан тип чипсета - полная инициализация невозможна\n");
  helloflag=2;
}  

if ((helloflag == 0)&& (chip_type != 0))  printf("\n Чипсет: %s",get_chipname());

if (start == 0) {
  printf("\n Не указан адрес загрузки\n");
  fclose(in);
  return;
}  
//------- Вариант загрузки через запись загрузчика в память ----------

printf("\n Файл загрузчика: %s\n Адрес загрузки: %08x",argv[optind],start);
iolen=send_cmd_base(cmd1,1,iobuf,1);
if (iolen != 5) {
   printf("\n Модем не находится в режиме загрузки\n");
//   dump(iobuf,iolen,0);
   fclose(in);
   return;
}   
iolen=send_cmd_base(cmd2,1,iobuf,1);
#ifndef WIN32
fstat(fileno(in),&fstatus);
#else
fstat(_fileno(in),&fstatus);
#endif
filesize=fstatus.st_size;
if (ident_flag) filesize-=12; // отрезаем хвост - блок идентификации
printf("\n Размер файла: %i\n",(unsigned int)filesize);
partsize=dlblock;

// Цикл поблочной загрузки 
for(i=0;i<filesize;i+=dlblock) {  
 if ((filesize-i) < dlblock) partsize=filesize-i;
 fread(cmddl+7,1,partsize,in);          // читаем блок прямо в командный буфер
 adr=start+i;                           // адрес загрузки этого блока
   // Как обычно у убогих китайцев, числа вписываются через жопу - в формате Big Endian
   // вписываем адрес загрузки этого блока
   cmddl[1]=(adr>>24)&0xff;
   cmddl[2]=(adr>>16)&0xff;
   cmddl[3]=(adr>>8)&0xff;
   cmddl[4]=(adr)&0xff;
   // вписываем размер блока 
   cmddl[5]=(partsize>>8)&0xff;
   cmddl[6]=(partsize)&0xff;
 iolen=send_cmd_base(cmddl,partsize+7,iobuf,1);
 printf("\r Загружено: %i",i+partsize);
// dump(iobuf,iolen,0);
} 
// вписываем адрес в команду запуска
printf("\n Запуск загрузчика..."); fflush(stdout);
cmdstart[1]=(start>>24)&0xff;
cmdstart[2]=(start>>16)&0xff;
cmdstart[3]=(start>>8)&0xff;
cmdstart[4]=(start)&0xff;
iolen=send_cmd_base(cmdstart,5,iobuf,1);
close_port();
#ifndef WIN32
usleep(delay);   // ждем инициализации загрузчика
#else
Sleep(delay/1000);   // ждем инициализации загрузчика
#endif
printf("ok\n");
if (helloflag) {
  if (!open_port(devname))  {
#ifndef WIN32
     printf("\n - Последовательный порт %s не открывается\n", devname); 
#else
     printf("\n - Последовательный порт COM%s не открывается\n", devname); 
#endif
     fclose(in);
     return; 
  }
  hello(helloflag);
  if (helloflag != 2)
     if (!bad_loader && tflag) extract_ptable();  // вынимаем таблицы разделов
}  
printf("\n");
fclose(in);
}

