#include "include.h"
#include <time.h>

//%%%%%%%%%  Общие переменные %%%%%%%%%%%%%%%%

unsigned int altflag=0;   // флаг альтернативной EFS
unsigned int fixname=0;   // индикатор явного уазания имени файла
char filename[50];        // имя выходного файла

char* fbuf;  // буфер для файловых операций

int recurseflag=0;

// режимы вывода списка файлов:

enum {
  fl_tree,    // дерево
  fl_ftree,   // дерево с файлами
  fl_list,    // листинг файлов
  fl_full     // полный листинг файлов
};  

 // Формат пакета состояния файла
 //------------------------------------------
 // pkt+08 - атрибуты
 // pkt+0c - размер
 // pkt+10 - счетчик ссылок
 // pkt+14 - дата создания
 // pkt+18 - дата модификации
 // pkt+1c - дата последнего доступа

struct fileinfo {
  unsigned int attr;
  unsigned int size;
  unsigned int filecnt;
  time_t ctime;
  time_t mtime;
  time_t atime;
};
  
int tspace; // отступ для формирования дерева файлов

//****************************************************
//*   Получение блока описания файла
//* 
//*   Возвращаемое значение - тип файла:
//*
//*  0 - файл не найден
//*  1 - файл регулярный или item
//*  2 - каталог
//*
//****************************************************
int getfileinfo(char* filename, struct fileinfo* fi) {
  
unsigned char cmd_fileinfo[100]={0x4b, 0x13, 0x0f, 0x00};
unsigned char iobuf[4096];
unsigned int iolen;

strcpy(cmd_fileinfo+4,filename);
iolen=send_cmd_base(cmd_fileinfo,strlen(filename)+5, iobuf, 0);
if (iobuf[4] != 0) return 0; // файл не найден 
memcpy(fi,iobuf+8,24);
if (S_ISDIR(fi->attr)) return 2;
return 1;
}

//****************************************************
//* Чтение дампа EFS (efs.mbn)
//****************************************************

void back_efs() {

unsigned char iobuf[14048];
unsigned char cmd_efsh1[16]=  {0x4B, 0x13,0x19, 0};
unsigned char cmd_efsopen[16]=  {0x4B, 0x13,0x16, 0};
unsigned char cmd_efsdata[16]={0x4B, 0x13,0x17,0,0,0,0,0,0,0,0,0};
unsigned char cmd_efsclose[16]=  {0x4B, 0x13,0x18, 0};

int iolen;
FILE* out;

// настройка на альтернативную EFS
if (altflag) {
 cmd_efsh1[1]=0x3e;
 cmd_efsopen[1]=0x3e;
 cmd_efsdata[1]=0x3e;
 cmd_efsclose[1]=0x3e;
 if (!fixname) strcpy(filename,"efs_alt.mbn");
}
// настройка на стандартную EFS
else strcpy(filename,"efs.mbn");
   
out=fopen(filename,"w");
  
iolen=send_cmd_base(cmd_efsh1, 4, iobuf, 0);
if ((iolen != 11) || test_zero(iobuf+3,5)) {
  printf("\n Неправильный ответ на команду 19\n");
  dump(iobuf,iolen,0);
  fclose(out);
  return;
}

iolen=send_cmd_base(cmd_efsopen, 4, iobuf, 0);
if ((iolen != 11) || test_zero(iobuf+3,5)) {
  printf("\n Неправильный ответ на команду открытия (16)\n");
  dump(iobuf,iolen,0);
  fclose(out);
  return;
}
printf("\n");

// главный цикл получения efs.mbn
while(1) {
  iolen=send_cmd_base(cmd_efsdata, 12, iobuf, 0);
  if (iolen != 532) {
    printf("\n Неправильный ответ на команду 4b 13 17\n");
    dump(iobuf,iolen,0);
    return;
  }
  printf("\r Читаем раздел: %i   блок: %08x",*((unsigned int*)&iobuf[8]),*((unsigned int*)&iobuf[12]));
  fflush(stdout);
  if (iobuf[8] == 0) break;
  fwrite(iobuf+16,512,1,out);
  memcpy(cmd_efsdata+4,iobuf+8,8);
}
// закрываем EFS
iolen=send_cmd_base(cmd_efsclose,4,iobuf,0);
if ((iolen != 11) || test_zero(iobuf+3,5)) {
  printf("\n Неправильный ответ на команду закрытия (18)\n");
  dump(iobuf,iolen,0);
  fclose(out);

}
}

//***************************************************
//*  Вывод имени файла с учетом отступа дерева
//***************************************************
void printspace(char* name) {

int i;
printf("\n");
if (tspace != 0) for (i=0;i<tspace*3;i++) printf(" ");
printf("%s",name);
}

//*********************************************
//*  Вывод атрибута доступа файла
//*********************************************
char* cfattr(int mode) {
  
static char str[5]={0,0,0,0,0};
int i;

memset(str,'-',3);
if ((mode&1) != 0) str[0]='r';
if ((mode&2) != 0) str[1]='w';
if ((mode&4) != 0) str[2]='x';
return str;
}

//****************************************************
//* Вывод детальной информации о регулярном файле
//****************************************************
void show_fileinfo(char* filename, struct fileinfo* fi) {

char timestr[100];

printf("\n Имя файла: %s",filename);
printf("\n Размер   : %i байт",fi->size);
printf("\n Тип файла: %s",((fi->attr&S_IFBLK) == S_IFBLK)?"Item":"Regular");
printf("\n Атрибуты доступа: %s%s%s",
      cfattr(fi->attr&7),
      cfattr((fi->attr>>3)&7),
      cfattr((fi->attr>>6)&7));

strftime(timestr,100,"%d-%b-%y %H:%M",localtime(&fi->ctime));
printf("\n Дата создания: %s",timestr);

strftime(timestr,100,"%d-%b-%y %H:%M",localtime(&fi->mtime));
printf("\n Дата модификации: %s",timestr);

strftime(timestr,100,"%d-%b-%y %H:%M",localtime(&fi->atime));
printf("\n Дата последнего доступа: %s\n",timestr);
}


//****************************************************
//* Вывод списка файлов указанного каталога 
//*  mode - режим вывода fl_*
//*  dirname - начальный путь, по умолчанию /
//****************************************************
void show_files (int lmode, char* fname) {
  
char chdir[120]={0x4b, 0x13, 0x0b, 0x00, 0x2f, 0};
char cmdfile[]= {0x4b, 0x13, 0x0c, 0x00, 0x01, 0, 0, 0, 0, 0, 0, 0};
char closedir[]={0x4b, 0x13, 0x0d, 00, 1, 00, 00, 00};
unsigned char iobuf[2048];
int iolen;

char dnlist[200][100]; // список каталогов
unsigned short ndir=0;
unsigned char dirname[100];	

int i,nfile;
time_t* filetime;
unsigned int filesize;
unsigned int fileattr,filelcnt;
char timestr[100];
char ftype;
char targetname[200];
int filecnt=0;

if (strlen(fname) == 0) strcpy(dirname,"/");
else strcpy(dirname,fname);

strcat(chdir+5,dirname);
// reset
iolen=send_cmd_base(closedir,8,iobuf,0);
// chdir
iolen=send_cmd_base(chdir,strlen(dirname)+6,iobuf,0);
if ((iolen == 0) || (memcmp(iobuf,"\x4b\x13\x0b",3) != 0))  {
  printf("\n ! Доступ в каталог %s запрещен\n",dirname);
  return;
}

if (lmode == fl_full) printf("\n *** Каталог %s ***",dirname);
// поиск файлов
for(nfile=1;;nfile++) {
 *((int*)&cmdfile[8])=nfile;
 iolen=send_cmd_base(cmdfile,12,iobuf,0);
 if(iobuf[0x28] == 0) break; // конец списка
 filecnt++;
 // разбираем блок атрибутов
 // формат блока:
 //------------------------------------------
 // pkt+10 - счетчик ссылок
 // pkt+14 - атрибуты
 // pkt+18 - размер
 // pkt+1c - дата создания
 // pkt+20 - дата модификации
 // pkt+24 - дата последнего доступа
 // pkt+28 и до конца пакета - имя файла (заканчивается 0)
 //
 filelcnt=*((unsigned int*)&iobuf[0x10]);
 fileattr=*((unsigned int*)&iobuf[0x14]);
 filesize=*((unsigned int*)&iobuf[0x18]);
 filetime=(time_t*)&iobuf[0x1c];
 ftype='-';
 if ((fileattr&S_IFDIR) == S_IFDIR) { 
   ftype='D';
   // Формируем список подкаталогов
   strcpy(dnlist[ndir++],iobuf+0x28);
 }  
 if ((fileattr&S_IFBLK) == S_IFBLK) ftype='I';

 // Определяем полное имя файла
   strcpy(targetname,dirname);
   strcat(targetname,"/");
   strcat(targetname,iobuf+0x29); // пропускаем начальный "/"
   if(fileattr == 'D') strcat (targetname,"/");
 
 // Режим дерева
 if ((lmode == fl_tree) || (lmode == fl_ftree)) {
   if ((lmode == fl_tree) && (ftype != 'D')) continue; // пропускаем регулярные файлы в режиме дерева каталогов
   printspace(targetname);
   if (ftype == 'D') {
     tspace++;
     show_files(lmode,targetname); // обрабатываем вложенный подкаталог
     // reset
     iolen=send_cmd_base(closedir,8,iobuf,0);
     // chdir
     iolen=send_cmd_base(chdir,strlen(dirname)+6,iobuf,0);
     tspace--;
   }  
   continue;    
 }
 
 // режим простого списка файлов
 if (lmode == fl_list) {
   printf("\n%s",targetname);
   if ((ftype == 'D') && (recurseflag == 1)) { 
     show_files(lmode,targetname);
     // reset
     iolen=send_cmd_base(closedir,8,iobuf,0);
     // chdir
     iolen=send_cmd_base(chdir,strlen(dirname)+6,iobuf,0);
   } 
   continue;
 }
 
 // режим полного списка файлов
 strftime(timestr,100,"%d-%b-%y %H:%M",localtime(filetime));
 printf("\n%c%s%s%s %2i %9i %s %s",
      ftype,
      cfattr(fileattr&7),
      cfattr((fileattr>>3)&7),
      cfattr((fileattr>>6)&7),
      filelcnt,
      filesize,
      timestr,
      iobuf+0x28);
}

// данный каталог обработан - обрабатываем вложенные подкаталоги в режиме полного просмотра

if (lmode == fl_full) {
  printf("\n  * Файлов: %i\n",filecnt);
  if (recurseflag) 
   for(i=0;i<ndir;i++) {
    strcpy(targetname,dirname);
    strcat(targetname,"/");
    strcat(targetname,dnlist[i]);
    show_files(lmode,targetname);
   }
}  
}

//**************************************************   
//* Чтение файла в буфер
//**************************************************   
unsigned int readfile(char* filename) {	

struct fileinfo fi;
int i,blk;

char open_cmd[100]={0x4b, 0x13, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x24, 0x01, 0x00, 0x00};
char close_cmd[]={0x4b, 0x13, 0x03, 0x00, 0x00, 0x00, 0x00, 0x00}; 
// 4b 13 04 00 00 00 00 00 ss ss ss ss oo oo oo oo
char read_cmd[16]={0x4b, 0x13, 0x04, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00};

char iobuf[4096];
int iolen;

iolen=send_cmd_base(close_cmd,8,iobuf,0);
switch (getfileinfo(filename,&fi)) {
   case 0:
     printf("\nОбъект %s не найден\n",filename);
     return 0;
 
   case 2: // каталог
     printf("\nОбъект %s является каталогом\n",filename);
     return 0;
}    
if (fi.size == 0) {
  printf("\nФайл %s не содержит данных\n",filename);
  return;
}
fbuf=malloc(fi.size);
strcpy(open_cmd+0xc,filename);
iolen=send_cmd_base(open_cmd,13+strlen(filename),iobuf,0);
if (*((unsigned int*)&iobuf[4]) != 0) {
  printf("\n Ошибка открытия файла %s",filename);
  free(fbuf);
  return 0;
}
blk=512;
for (i=0;i<(fi.size);i+=512) {
 *((unsigned int*)&read_cmd[0x0c])=i;
 if ((i+512) > fi.size) {
   blk=fi.size-i;
   *((unsigned int*)&read_cmd[8])=blk;
 }
 iolen=send_cmd_base(read_cmd,16,iobuf,0);
 memcpy(fbuf+i,iobuf+0x14,blk);
}
iolen=send_cmd_base(close_cmd,8,iobuf,0);
return fi.size;
}

//***************************************
//* Просмотр файла на экране
//*
//* mode=0 - просмотр в виде текста
//*      1 - просмотр в виде дампа
//***************************************
void list_file(char* filename,int mode) {
  
unsigned int flen;

flen=readfile(filename);
if (flen == 0) return;
if (!mode) fwrite(fbuf,flen,1,stdout);
else dump(fbuf,flen,0);
free(fbuf);
}



//@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@
//@@@@@@@@@@@@ Головная программа
void main(int argc, char* argv[]) {

unsigned int opt;
struct fileinfo fi;
  
enum{
  MODE_BACK_EFS,
  MODE_FILELIST,
  MODE_TYPE
}; 


enum {
  T_TEXT,
  T_DUMP
};  

int mode=-1;
int lmode=-1;
int tmode=-1;

#ifndef WIN32
char devname[50]="/dev/ttyUSB0";
#else
char devname[50]="";
#endif

while ((opt = getopt(argc, argv, "hp:o:ab:g:l:rt:")) != -1) {
  switch (opt) {
   case 'h': 
    printf("\n  Утилита предназначена для работы с разделом efs \n\
%s [ключи] [путь или имя файла]\n\
Допустимы следующие ключи:\n\n\
* Ключи, определяюще выполняемую операцию:\n\
-be       - дамп efs\n\n\
-ld       - показать дерево каталогов EFS\n\
-lt       - показать дерево каталогов и файлов EFS\n\
-ll       - показать список файлов EFS\n\
-lf       - показать полный список файлов EFS\n  (Для всех -l ключей можео указать начальный путь к каталогу)\n\n\
-tt       - просмотр файла в текстовом виде\n\
-td       - просмотр файла в виде дампа\n\n\
* Ключи-модификаторы:\n\
-r        - обработка всех подкаталогов при выводе листинга\n\
-p <tty>  - указывает имя устройства диагностического порта модема\n\
-a        - использовать альтернативную EFS\n\
-o <file> - имя файла для сохранения efs\n\
\n",argv[0]);
    return;
    
   case 'o':
     strcpy(filename,optarg);
     fixname=1;
     break;
   //  === группа ключей backup ==
   case 'b':
     if (mode != -1) {
       printf("\n В командной строке задано более 1 ключа режима работы\n");
       return;
     }  
     switch(*optarg) {
       case 'e':
         mode=MODE_BACK_EFS;
         break;
	 
       default:
	 printf("\n Неправильно задано значение ключа -b\n");
	 return;
      }
      break;

   // Список файлов
   case 'l':   
     if (mode != -1) {
       printf("\n В командной строке задано более 1 ключа режима работы\n");
       return;
     }  
     mode=MODE_FILELIST;
     switch(*optarg) {
       case 'd':
         lmode=fl_tree;
         break;
       case 't':
         lmode=fl_ftree;
         break;
       case 'l':
         lmode=fl_list;
         break;
       case 'f':
         lmode=fl_full;
         break;
       default:
	 printf("\n Неправильно задано значение ключа -l\n");
	 return;
     }  
     break;

   //  === группа ключей просмотра файла

    case 't':   
     if (mode != -1) {
       printf("\n В командной строке задано более 1 ключа режима работы\n");
       return;
     }  
     mode=MODE_TYPE;
     switch(*optarg) {
       case 't':
         tmode=T_TEXT;
         break;
       
       case 'd':
         tmode=T_DUMP;
         break;
       
       default:
	 printf("\n Неправильно задано значение ключа -t\n");
	 return;
      }
     break; 
   //  === группа ключей get ==
   case 'g':
     if (mode != -1) {
       printf("\n В командной строке задано более 1 ключа режима работы\n");
       return;
     }  
     switch(*optarg) {
	 
	 
       default:
	 printf("\n Неправильно задано значение ключа -g\n");
	 return;
      }
      break;
      
   case 'p':
    strcpy(devname,optarg);
    break;

   case 'a':
     altflag=1;
     break;
     
   case 'r':
     recurseflag=1;
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
  
// Дамп EFS  
  case MODE_BACK_EFS:
    back_efs();
    break;

// просмотр каталога    
  case MODE_FILELIST:
    tspace=0;
    // путь не указан - работаем с корневым каталогом
    if (optind == argc) {
      show_files(lmode,"");
      break;
    }  
    // Проверяем наличие файла, и является ли он каталогом
    switch (getfileinfo(argv[optind],&fi)) {
      case 0:
        printf("\nОбъект %s не найден\n",argv[optind]);
        break;
    
      case 1: // регулярный файл
        show_fileinfo(argv[optind],&fi);
        break;
	
      case 2: // каталог
        show_files(lmode,argv[optind]);
	break;
    }    
    break;

// Просмотр файлов
  case MODE_TYPE:
    if (optind == argc) {
      printf("\n Не указано имя файла");
      break;
    }  
    list_file(argv[optind],tmode);
    break;
    
  default:
    printf("\n Не указан ключ выполняемой операции\n");
    return;

}    
printf("\n");

}

