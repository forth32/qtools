#include "include.h"

//%%%%%%%%%  Общие переменные %%%%%%%%%%%%%%%%55

unsigned int fixname=0;   // индикатор явного уазания имени файла
unsigned int zeroflag=0;  // флаг пропуска пустых записей nvram
         int sysitem=-1;     // номер раздела nvram (-1 - все разделы)
char filename[50];        // имя выходного файла


//*******************************************
//* Проверка границ номера раздела
//*******************************************
void verify_item(int item) {

if (item>0xffff) {
  printf("\n Неправильно указан номер ячейки - %x\n",item);
  exit(1);
}
}

//*******************************************
//*   Получение раздела nvram из модема
//*
//*  0 -не прочитался
//*  1 -прочитался
//*******************************************
int get_nvitem(int item, char* buf) {

unsigned char iobuf[140];
unsigned char cmd_rd[16]=  {0x26, 0,0};
unsigned int iolen;

*((unsigned short*)&cmd_rd[1])=item;
iolen=send_cmd_base(cmd_rd,3,iobuf,0);
if (iolen != 136) return 0;
memcpy(buf,iobuf+3,130);
return 1;
}

//*******************************************
//*  Дамп раздела nvram
//*******************************************
void nvdump(int item) {
  
char buf[134];
int len;

if (!get_nvitem(item,buf)) {
  printf("\n! Ячейка %04x не читается\n",item);
  return;
}
if (zeroflag && (test_zero(buf,128) == 0)) {
  printf("\n! Ячейка %04x пуста\n",item);
  return;
}  
printf("\n *** NVRAM: Ячкйка %04x  атрибут %04x\n--------------------------------------------------\n",
       item,*((unsigned short*)&buf[128]));

// отрезаем хвостовые нули 
if (zeroflag) {
 for(len=127;len>=0;len--)
   if (buf[len]!=0) break;
 len++;  
}
else len=128;

dump(buf,len,0);
}

//*******************************************
//*  Чтение всех заисей NVRAM
//*******************************************
void read_all_nvram_items() {
  
char buf[130];
unsigned int nv;
char filename[50];
FILE* out;
unsigned int start=0;
unsigned int end=0x10000;

// Создаем каталог nv для сбора файлов
if (mkdir("nv",0777) != 0) 
  if (errno != EEXIST) {
    printf("\n Невозможно создать каталог nv/");
    return;
  }  

// установка границ читаемых записей
if (sysitem != -1) {
  start=sysitem;
  end=start+1;
}  
  
printf("\n");  
for(nv=start;nv<end;nv++) {
  printf("\r NV %04x:  ok",nv); fflush(stdout);
  if (!get_nvitem(nv,buf)) continue;
  if (zeroflag && (test_zero(buf,128) == 0)) continue;
  sprintf(filename,"nv/%04x.bin",nv);
  out=fopen(filename,"w");
  fwrite(buf,1,130,out);
  fclose(out);
}  
}  

//*******************************************
//* Запись раздела nvram из буфера
//*******************************************
void write_item(int item, char*buf) {
  
unsigned char cmdwrite[200]={0x27,0,0};
unsigned char iobuf[200];
int iolen;

*((unsigned short*)&cmdwrite[1])=item;
memcpy(cmdwrite+3,buf,130);
iolen=send_cmd_base(cmdwrite,133,iobuf,0);
if ((iolen != 136) || (iobuf[0] != 0x27)) {
  printf("\n Ошибка записи ячейки\n");
  dump(iobuf,iolen,0);
}  
}

//*******************************************
//*  Запись раздела nvram из файла
//*******************************************
void write_nvram() {
  
FILE* in;
char buf[135];

in=fopen(filename,"r");
if (in == 0) {
  printf("\n Ошибка открытия файла %s\n",filename);
  exit(1);
}
if (fread(buf,1,130,in) != 130) {
  printf("\n Файл %s слишком мал\n",filename);
  exit(1);
}
fclose (in);
write_item(sysitem,buf);
}

//*******************************************
//*  Запись всех разделов nvram 
//*******************************************
void write_all_nvram() {

int i;
FILE* in;
char buf[135];

printf("\n");
for (i=0;i<0x10000;i++) {
  sprintf(filename,"nv/%04x.bin",i);
  in=fopen(filename,"r");
  if (in == 0) continue;
  printf("\r %04x: ",i);
  if (fread(buf,1,130,in) != 130) {
    printf(" Файл %s слишком мал - пропускаем\n",filename);
    fclose (in);
    continue;
  }
  fclose (in);
  write_item(i,buf);
  printf(" OK");
}
}  
  
//**********************************************
//* Генерация nvitem 226 с указанным IMEI
//**********************************************
void write_imei(char* src) {

unsigned char binimei[15];
unsigned char imeibuf[0x84];
int i,j,f;  
char cbuf[7];
int csum;

memset(imeibuf,0,0x84);
if (strlen(src) != 15) {
  printf("\n Неправильная длина IMEI");
  return;
}

for (i=0;i<15;i++) {
  if ((src[i] >= '0') && (src[i] <='9')) {
    binimei[i] = src[i]-'0'; 
    continue;
  }  
  printf("\n Неправильный символ в строке IMEI - %c\n",src[i]);
  return;
}

// Проверяем контрольную сумму IMEI
j=0;
for (i=1;i<14;i+=2) cbuf[j++]=binimei[i]*2;
csum=0;
for (i=0;i<7;i++) {
  f=(int)cbuf[i]/10;
  csum = csum + f + (int)((cbuf[i]*10) - f*100)/10;
}  
for (i=0;i<13;i+=2) csum += binimei[i];
 
if ((((int)csum/10)*10) == csum) csum=0;
else csum=( (int)csum/10 + 1)*10 - csum;
if (binimei[14] != csum) {
  printf("\n IMEI имеет неправильную контрольную сумму !\n Правильный IMEI = ");
  for (i=0;i<14;i++) printf("%1i",binimei[i]);
  printf("%1i",csum);
  printf("\n Исправить (y,n)?");
  i=getchar();
  if (i == 'y') binimei[14]=csum;
}  

// Формируем IMEI в квалкомовском формате
imeibuf[0]=8;
imeibuf[1]=(binimei[0]<<4)|0xa;
j=2;
for (i=1;i<15;i+=2) {
  imeibuf[j++] = binimei[i] | (binimei[i+1] <<4);
}
write_item(0x226,imeibuf);
//dump(imeibuf,9,0);
}
  
  
//@@@@@@@@@@@@ Головная программа @@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@2
void main(int argc, char* argv[]) {

unsigned int opt;
  
enum{
  MODE_BACK_NVRAM,
  MODE_READ_NVRAM,
  MODE_SHOW_NVRAM,
  MODE_WRITE_NVRAM,
  MODE_WRITE_ALL,
  MODE_WRITE_IMEI
}; 

int mode=-1;
char* imeiptr;

#ifndef WIN32
char devname[50]="/dev/ttyUSB0";
#else
char devname[50]="";
#endif

while ((opt = getopt(argc, argv, "hp:o:b:r:w:j:")) != -1) {
  switch (opt) {
   case 'h': 
    printf("\n  Утилита предназначена для работы с nvram модема \n\
%s [ключи] [параметр или имя файла]\n\
Допустимы следующие ключи:\n\n\
Ключи, определяюще выполняемую операцию:\n\
-bn           - дамп nvram\n\
-ri[z] [item] - чтение всех или только указанной записей nvram в отдельные файлы (z-пропускать пустые записи)\n\
-rd[z]        - дамп указанного раздела nvram (z-отрезать хвостовые нули)\n\n\
-wi item file - запись раздела item из файла file\n\
-wa           - запись всех разделов, имеющихся в каталоге nv/\n\
-j imei       - запись указаного IMEI в nv226\n\
\n\
Ключи-модификаторы:\n\
-p <tty>  - указывает имя устройства диагностического порта модема\n\
-o <file> - имя файла для сохранения nvram\n\
\n",argv[0]);
    return;
    
   case 'o':
     strcpy(filename,optarg);
     fixname=1;
     break;
//----------------------------------------------	 
   //  === группа ключей backup ==
   case 'b':
     if (mode != -1) {
       printf("\n В командной строке задано более 1 ключа режима работы");
       return;
     }  
     switch(*optarg) {
     
       case 'n':
         mode=MODE_BACK_NVRAM;
         break;
	 
       default:
	 printf("\n Неправильно задано значение ключа -b\n");
	 return;
      }
      break;

//----------------------------------------------	 
   //  === группа ключей read ==
   case 'r':
     if (mode != -1) {
       printf("\n В командной строке задано более 1 ключа режима работы");
       return;
     }  
     switch(*optarg) {
       case 'i':
	 // rn - чтение в файл одного или всех разделов
         mode=MODE_READ_NVRAM;
	 if (optarg[1] == 'z') zeroflag=1;
         break;

       case 'd':
         mode=MODE_SHOW_NVRAM;
	 if (optarg[1] == 'z') zeroflag=1;
         break;

       default:
	 printf("\n Неправильно задано значение ключа -r\n");
	 return;
      }
      break;
//----------------------------------------------	 
    //  === группа ключей write ==
    case 'w':
     if (mode != -1) {
       printf("\n В командной строке задано более 1 ключа режима работы");
       return;
     }  
     switch(*optarg) {
       case 'i':
         mode=MODE_WRITE_NVRAM;
	 break;

       case 'a':
         mode=MODE_WRITE_ALL;
	 break;

       default:
	 printf("\n Неправильно задано значение ключа -w\n");
	 return;
      }
      break;
	 
//----------------------------------------------	 
//  === запись IMEI ==
    case 'j':
     if (mode != -1) {
       printf("\n В командной строке задано более 1 ключа режима работы");
       return;
     }  
     mode=MODE_WRITE_IMEI;
     imeiptr=optarg;
     break;
     
//------------- прочие ключи --------------------      
   case 'p':
    strcpy(devname,optarg);
    break;

   case '?':
   case ':':  
     return;
  }
}  

if (mode == -1) {
  printf("\n Не указан ключ выполняемой операции\n");
  return;
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


//////////////////

switch (mode) {
    
  case MODE_READ_NVRAM:
    if (optind<argc) {
      // указан номер раздела - выделяем его
      sscanf(argv[optind],"%x",&sysitem);
      verify_item(sysitem);
    }  
    read_all_nvram_items();
    break;

  case MODE_SHOW_NVRAM:
    if (optind>=argc) {
      printf("\n Не указан номер раздела nvram");
      break;
    }
    sscanf(argv[optind],"%x",&sysitem);
    verify_item(sysitem);
    nvdump(sysitem);
    break;

  case MODE_WRITE_NVRAM:
    if (optind != (argc-2)) {
       printf("\n Неверное число параметров в командной строке\n");
       exit(1);
    }   
    sscanf(argv[optind],"%x",&sysitem);
    verify_item(sysitem);
    strcpy(filename,argv[optind+1]);
    write_nvram();
    break;
 
  case MODE_WRITE_ALL:
    write_all_nvram();
    break;
   
  case MODE_WRITE_IMEI:
    write_imei(imeiptr);
    break;
    
  default:
    printf("\n Не указан ключ выполняемой операции\n");
    return;

}    
printf("\n");

}

