#ifdef WIN32
#define _USE_32BIT_TIME_T
#endif
#include "include.h"
#include <time.h>
#include "efsio.h"

//%%%%%%%%%  Общие переменные %%%%%%%%%%%%%%%%

unsigned int fixname=0;   // индикатор явного уазания имени файла
char filename[50];        // имя выходного файла

char* fbuf;  // буфер для файловых операций

int recurseflag=0;
int fullpathflag=0;

char iobuf[44096];
int iolen;

#ifdef WIN32
struct tm* localtime_r(const time_t *clock, struct tm *result) {
       if (!clock || !result) return NULL;
       memcpy(result,localtime(clock),sizeof(*result));
       return result;
}
#endif

// режимы вывода списка файлов:

enum {
  fl_tree,    // дерево
  fl_ftree,   // дерево с файлами
  fl_list,    // листинг файлов
  fl_full,    // полный листинг файлов
  fl_mid      // листинг файлов в формате MC extfs
};  

  
int tspace; // отступ для формирования дерева файлов


//****************************************************
//* Чтение дампа EFS (efs.mbn)
//****************************************************

void back_efs() {


FILE* out;
struct efs_factimage_rsp rsp;
rsp.stream_state=0;
rsp.info_cluster_sent=0;
rsp.cluster_map_seqno=0;
rsp.cluster_data_seqno=0;

strcpy(filename,"efs.mbn");
out=fopen(filename,"w");
if (out == 0) {
  printf("\nОшибка открытия выходного файла %s\n",filename);
  return;
}  

if (efs_prep_factimage() != 0) {
  printf("\n Ошибка входа в режим Factory Image, код %d\n",efs_get_errno());
  fclose(out);
  return;
}

if (efs_factimage_start() != 0) {
  printf("\n Ошибка запуска чтения EFS, код %d\n",efs_get_errno());
  fclose(out);
  return;
}

printf("\n");

// главный цикл получения efs.mbn
while(1) {
  printf("\r Чтение: sent=%i map=%i data=%i",rsp.info_cluster_sent,rsp.cluster_map_seqno,rsp.cluster_data_seqno);
  fflush(stdout);
  if (efs_factimage_read(rsp.stream_state, rsp.info_cluster_sent, rsp.cluster_map_seqno, 
                    rsp.cluster_data_seqno, &rsp) != 0) {  
    printf("\n Ошибка чтения, код=%d\n",efs_get_errno());
    return;
  }
  if (rsp.stream_state == 0) break; // конец потока данных
  fwrite(rsp.page,512,1,out);
}
// закрываем EFS
efs_factimage_end();
fclose(out);
  
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
static char atrstr[15];

void fattr(int mode, char* str) {
  
memset(str,'-',3);
str[3]=0;
if ((mode&4) != 0) str[0]='r';
if ((mode&2) != 0) str[1]='w';
if ((mode&1) != 0) str[2]='x';
}

//*********************************************
char* cfattr(int mode) {

char str[5];
  
atrstr[0]=0;
fattr((mode>>6)&7,str);
strcat(atrstr,str);
fattr((mode>>3)&7,str);
strcat(atrstr,str);
fattr(mode&7,str);
strcat(atrstr,str);
return atrstr;
}

//****************************************************
//*  Получение символического описания атрибута файла
//****************************************************
char* str_filetype(int attr,char* buf) {
  
strcpy(buf,"Unknown");
     if ((attr&S_IFMT) == S_IFIFO)  strcpy(buf,"fifo");
else if ((attr&S_IFMT) == S_IFCHR)  strcpy(buf,"Character device");
else if ((attr&S_IFMT) == S_IFDIR)  strcpy(buf,"Directory");
else if ((attr&S_IFMT) == S_IFBLK)  strcpy(buf,"Block device");
else if ((attr&S_IFMT) == S_IFREG)  strcpy(buf,"Regular file");
else if ((attr&S_IFMT) == S_IFLNK)  strcpy(buf,"Symlink");
else if ((attr&S_IFMT) == S_IFSOCK) strcpy(buf,"Socket");
else if ((attr&S_IFMT) == S_IFITM)  strcpy(buf,"Item File");
return buf;
}

//****************************************************
//*  Получение односимвольного описания атрибута файла
//****************************************************
char chr_filetype(int attr) {
  
     if ((attr&S_IFMT) == S_IFIFO)   return 'p';
else if ((attr&S_IFMT) == S_IFCHR)   return 'c';
else if ((attr&S_IFMT) == S_IFDIR)   return 'd';
else if ((attr&S_IFMT) == S_IFBLK)   return 'b';
else if ((attr&S_IFMT) == S_IFREG)   return '-';
else if ((attr&S_IFMT) == S_IFLNK)   return 'l';
else if ((attr&S_IFMT) == S_IFSOCK)  return 's';
else if ((attr&S_IFMT) == S_IFITM)   return 'i';
return '-';
}

//****************************************************
//* Преобразование даты в ascii-строку
//* Формат:
//* 0 - нормальный
//* 1 - для Midnight Commander
//****************************************************
char* time_to_ascii(int32 time, int format) {
  
time_t xtime;      // то же самое время, только разрядности time_t
struct tm lt;      // структура для сохранения преобразованной даты
static char timestr[100];

xtime=time;
if (localtime_r(&xtime,&lt) != 0)  {
 if (format == 0) strftime(timestr,100,"%d-%b-%y %H:%M",localtime(&xtime));
 else             strftime(timestr,100,"%m-%d-%y %H:%M",localtime(&xtime));
}
else strcpy(timestr,"---------------");
return timestr;
}

//****************************************************
//* Вывод детальной информации о регулярном файле
//****************************************************
void show_efs_filestat(char* filename, struct efs_filestat* fi) {
  
char sfbuf[50]; // буфер для сохранения описания типа файла

printf("\n Имя файла: %s",filename);
printf("\n Размер   : %i байт",fi->size);
printf("\n Тип файла: %s",str_filetype(fi->mode,sfbuf));
printf("\n Счетчик ссылок: %d",fi->nlink);
printf("\n Атрибуты доступа: %s",cfattr(fi->mode));
printf("\n Дата создания: %s",time_to_ascii(fi->ctime,0));
printf("\n Дата модификации: %s",time_to_ascii(fi->mtime,0));
printf("\n Дата последнего доступа: %s\n",time_to_ascii(fi->atime,0));
}


//****************************************************
//* Вывод дерева файлов указанного каталога 
//*  lmode - режим вывода fl_*:
//*     fl_tree   - дерево
//*     fl_ftree, - дерево с файлами

//*  fname - начальный путь, по умолчанию /
//****************************************************
void show_tree (int lmode, char* fname) {
  
struct efs_dirent dentry; // описатель элемента каталога
unsigned char dirname[100];	
int dirp=0;  // указатель на открытый каталог

int i,nfile;
char targetname[200];

if (strlen(fname) == 0) strcpy(dirname,"/"); // по умолчанию открываем корневой каталог
else strcpy(dirname,fname);

// chdir
dirp=efs_opendir(dirname);
if (dirp == 0) {
  printf("\n ! Доступ в каталог %s запрещен, errno=%i\n",dirname,efs_get_errno());
  return;
}

// Цикл выборки записей каталога
for(nfile=1;;nfile++) {
 // выбираем следующую запись
 if (efs_readdir(dirp, nfile, &dentry) == -1) continue; // при ошибке чтения очередной структуры
 if (dentry.name[0] == 0) break;   // конец списка

 // Определяем полное имя файла
   strcpy(targetname,dirname);
//   strcat(targetname,"/");
   strcat(targetname,dentry.name); // пропускаем начальный "/"
   if(dentry.entry_type == 1) strcat (targetname,"/"); // это каталог
 
   if ((lmode == fl_tree) && (dentry.entry_type != 1)) continue; // пропускаем регулярные файлы в режиме дерева каталогов

   if (fullpathflag) printspace(targetname);
   else {
     for(i=strlen(targetname)-2;i>=0;i--) {
       if (targetname[i] == '/') break;
     } 
     i++;
     printspace(targetname+i);
   }  
   if (dentry.entry_type == 1) {
     // данная запись является каталогом - обрабатываем вложенный подкаталог
     tspace++;
     efs_closedir(dirp);
     show_tree(lmode,targetname); 
     dirp=efs_opendir(dirname);
     tspace--;
   }  
 }
efs_closedir(dirp); 
}

//****************************************************
//* Вывод списка файлов указанного каталога 
//*  lmode - режим вывода fl_*
//*   fl_list - краткий листинг файлов
//*   fl_full - полный листинг файлов
//*   fl_mid  - полный листинг файлов в формате midnight commander
//*  fname - начальный путь, по умолчанию /
//****************************************************
void show_files (int lmode, char* fname) {
  
struct efs_dirent dentry; // описатель элемента каталога
char dnlist[200][100]; // список каталогов
unsigned short ndir=0;
unsigned char dirname[100];	
int dirp=0;  // указатель на открытый каталог

int i,nfile;
char ftype;
char targetname[200];

if (strlen(fname) == 0) strcpy(dirname,"/"); // по умолчанию открываем корневой каталог
else strcpy(dirname,fname);

// opendir
dirp=efs_opendir(dirname);
if (dirp == 0) {
  if (lmode != fl_mid) printf("\n ! Доступ в каталог %s запрещен, errno=%i\n",dirname,efs_get_errno());
//  printf("\n ! Доступ в каталог %s запрещен\n",dirname);
  return;
}
if (lmode == fl_full) printf("\n *** Каталог %s ***\n",dirname);
// поиск файлов
for(nfile=1;;nfile++) {
 // выбираем следующую запись
 if (efs_readdir(dirp, nfile, &dentry) == -1) {
   continue; // при ошибке чтения очередной структуры
 }  
 if (dentry.name[0] == 0) {
   break;   // конец списка
 }  
 ftype=chr_filetype(dentry.mode);
 if ((dentry.entry_type) == 1) { 
   // Формируем список подкаталогов
   strcpy(dnlist[ndir++],dentry.name);
 }  

 // Определяем полное имя файла
   strcpy(targetname,dirname);
//   strcat(targetname,"/");
   strcat(targetname,dentry.name); // пропускаем начальный "/"
   if(ftype == 'd') strcat (targetname,"/");
 
 
 // режим простого списка файлов
 if (lmode == fl_list) {
   printf("\n%s",targetname);
   if ((ftype == 'd') && (recurseflag == 1)) { 
     show_files(lmode,targetname);
   } 
   continue;
 }
 
 // режим полного списка файлов
if (lmode == fl_full) 
  printf ("%c%s %9i %s %s\n",
      ftype,
      cfattr(dentry.mode),
      dentry.size,
      time_to_ascii(dentry.mtime,0),
      dentry.name);

// РЕжим midnight Commander
  
if (lmode == fl_mid) {
  if (ftype == 'i') ftype='-';
  printf("%c%s",
      ftype,                          // attr
      cfattr(dentry.mode));           // mode

  printf(" %d root root",ftype == 'd'?2:1);     // nlink, owner
  printf(" %9d %s %s/%s\n", 
	 dentry.size,                   // size
      time_to_ascii(dentry.mtime,1),      // date
      dirname,	 
      dentry.name);                     // name
}
}
// данный каталог обработан - обрабатываем вложенные подкаталоги в режиме полного просмотра

efs_closedir(dirp);  
if (lmode == fl_full) printf("\n  * Файлов: %i\n",nfile);
if (((lmode == fl_full) && recurseflag) || (lmode == fl_mid)) {
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

struct efs_filestat fi;
int i,blk;
int fd;

efs_close(1);
switch (efs_stat(filename,&fi)) {
   case 0:
     printf("\nОбъект %s не найден\n",filename);
     return 0;
 
   case 2: // каталог
     printf("\nОбъект %s является каталогом\n",filename);
     return 0;
}    
if (fi.size == 0) {
  printf("\nФайл %s не содержит данных\n",filename);
  return 0;
}
fbuf=malloc(fi.size);
fd=efs_open(filename,O_RDONLY);
if (fd == -1) {
  printf("\nОшибка открытия файла %s",filename);
  return 0;
}

blk=512;
for (i=0;i<(fi.size);i+=512) {
 if ((i+512) > fi.size) blk=fi.size-i;
 if (efs_read(fd, fbuf+i, blk, i)<=0) 
   return 0; // ошибка чтения
}
efs_close(fd);
return fi.size;
}

/////////////////////////////////////////////////////////////////
//**************************************************   
//* Запись файла 
//**************************************************   
unsigned int write_file(char* file, char* path) {	

struct efs_filestat fi;
int i,blk;
FILE* in;
long filesize;
int fd;

efs_close(1);

// готовим имя выходного файла

strcpy(filename,path);

// проверяем существование файла

switch (efs_stat(filename,&fi)) {
   case 1:
     printf("\nОбъект %s уже существует\n",filename);
     return 0;
 
   case 2: // каталог
     strcat(filename,"/");
     strcat(filename,file);
}    

// читаем файл в буфер
in=fopen(file,"r");
if (in == 0) {
  printf("\n ошибка открытия файла %s",file);
  return 0;
}  
fseek(in,0,SEEK_END);
filesize=ftell(in);
fbuf=malloc(filesize);
fseek(in,0,SEEK_SET);
fread(fbuf,1,filesize,in);
fclose(in);

fd=efs_open(filename,O_CREAT);
if (fd == -1) {
  printf("\nОшибка открытия файла %s на запись",filename);
  return 0;
}

blk=512;
for (i=0;i<(filesize);i+=512) {
 if ((i+512) > filesize) {
   blk=filesize-i;
 }
 efs_write(fd, fbuf+i, blk, i);
 usleep(3000);
}
free(fbuf);
efs_close(fd);
return 1;
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
if (flen == 0) {
  printf("Ошибка чтения файла %s",filename);
  return;
}  
if (!mode) fwrite(fbuf,flen,1,stdout);
else dump(fbuf,flen,0);
free(fbuf);
}

//******************************************************
//*  Копирование единичного файла из EFS в текущий каталог
//******************************************************
void get_file(char* name, char* dst) {
  
unsigned int flen;
char* fnpos;
FILE* out;
struct stat fs;
char filename[200];

flen=readfile(name);
if (flen == 0) {
  printf("Ошибка чтения файла %s",filename);
  return;
}  

// выделяем имя файла из полного пути
fnpos=strrchr(name,'/');
if (fnpos == 0) fnpos=name;
else fnpos++;

if (dst == 0) {
  // не указан выходной каталог или файл
  strcpy(filename,fnpos);
}
else {
  // выходной каталог или файл указан
  if ((stat(dst,&fs) == 0) && S_ISDIR(fs.st_mode)) {
    // выходной файл является каталогом
    strcpy(filename,dst);
    strcat(filename,"/");
    strcat(filename,fnpos);
  }
  // выходной файл не существует или является регулярным файлом
  else strcpy(filename,dst);
}      
out=fopen(filename,"w");
if (out == 0) {
  printf("Ошибка открытия выходного файла\n");
  exit(1);
}  
fwrite(fbuf,1,flen,out);
fclose(out);
}



//@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@
//@@@@@@@@@@@@ Головная программа
void main(int argc, char* argv[]) {

unsigned int opt;
int i;
struct efs_filestat fi;
char filename[100];
  
enum{
  MODE_BACK_EFS,
  MODE_FILELIST,
  MODE_TYPE,
  MODE_GETFILE,
  MODE_WRITEFILE,
  MODE_DELFILE,
  MODE_MKDIR
}; 


enum {
  T_TEXT,
  T_DUMP
};  

enum {
  G_FILE,
  G_ALL,
  G_DIR
};  

int mode=-1;
int lmode=-1;
int tmode=-1;
int gmode=-1;

#ifndef WIN32
char devname[50]="/dev/ttyUSB0";
#else
char devname[50]="";
#endif

while ((opt = getopt(argc, argv, "hp:o:ab:g:l:rt:w:e:fm:")) != -1) {
  switch (opt) {
   case 'h': 
    printf("\n  Утилита предназначена для работы с разделом efs \n\
%s [ключи] [путь или имя файла] [имя выходного файла]\n\
Допустимы следующие ключи:\n\n\
* Ключи, определяюще выполняемую операцию:\n\
-be       - дамп efs\n\n\
-ld       - показать дерево каталогов EFS (без регулярных файлов)\n\
-lt       - показать дерево каталогов и файлов EFS\n\
-ll       - показать короткий список файлов указанного каталога\n\
-lf       - показать полный список файлов указанного каталога\n  (Для всех -l ключей можео указать начальный путь к каталогу)\n\n\
-tt       - просмотр файла в текстовом виде\n\
-td       - просмотр файла в виде дампа\n\n\
-gf file [dst] - читает указанный файл из EFS в файл dst или в текущий каталог\n\
-wf file path - записывает указанный файл по указанному пути\n\
-ef file  - удаляет указанный файл\n\
-ed dir   - удаляет указанный каталог\n\
-md dir   - создает каталог с указанными правами доступа\n\n\
* Ключи-модификаторы:\n\
-r        - обработка всех подкаталогов при выводе листинга\n\
-f        - вывод полного пути к каждому каталогу при просмотре дерева\n\
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
       case 'm':
         lmode=fl_mid;
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

  //  === группа ключей извлечения файла (get) ==
   case 'g':
     if (mode != -1) {
       printf("\n В командной строке задано более 1 ключа режима работы\n");
       return;
     }  
     mode=MODE_GETFILE;
     switch(*optarg) {
       case 'f':
	 gmode=G_FILE;
	 break;
	 
       default:
	 printf("\n Неправильно задано значение ключа -g\n");
	 return;
      }
      break;
      
  //  === группа ключей записи файла (write) ==
   case 'w':
     if (mode != -1) {
       printf("\n В командной строке задано более 1 ключа режима работы\n");
       return;
     }  
     mode=MODE_WRITEFILE;
     switch(*optarg) {
       case 'f':
	 gmode=G_FILE;
	 break;
	 
       default:
	 printf("\n Неправильно задано значение ключа -g\n");
	 return;
      }
      break;      

  //  === группа ключей удаления файла (erase) ==
   case 'e':
     if (mode != -1) {
       printf("\n В командной строке задано более 1 ключа режима работы\n");
       return;
     }  
     mode=MODE_DELFILE;
     switch(*optarg) {
       case 'f':
	 gmode=G_FILE;
	 break;
	 
       case 'd':
	 gmode=G_DIR;
	 break;
	 
       default:
	 printf("\n Неправильно задано значение ключа -g\n");
	 return;
      }
      break;      

  // ==== Ключ создания каталога ====
   case 'm':
     if (mode != -1) {
       printf("\n В командной строке задано более 1 ключа режима работы\n");
       return;
     }  
     mode=MODE_MKDIR;
     if (*optarg != 'd') {
       printf("\n Недопустимый ключ m%c",*optarg);
       return;
     }
     break;
   
 //===================== Вспомогательные ключи ====================    
      
   case 'p':
    strcpy(devname,optarg);
    break;

   case 'a':
     set_altflag(1);
     break;
     
   case 'r':
     recurseflag=1;
     break;
     
   case 'f':
     fullpathflag=1;
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

// Закрываем все открытые хендлы каталогов

for(i=1;i<10;i++) efs_closedir(i);

// Запуск нужных процедур в зависимости от режима работы

switch (mode) {

//============================================================================  
// Дамп EFS  
  case MODE_BACK_EFS:
    back_efs();
    break;

//============================================================================  
// просмотр каталога    
  case MODE_FILELIST:
    tspace=0;
    // путь не указан - работаем с корневым каталогом
    if (optind == argc)    strcpy(filename,"/");
    // путь указан
    else strcpy(filename,argv[optind]);
    // Проверяем наличие файла, и является ли он каталогом
    i=efs_stat(filename,&fi);
    switch (i) {
      case 0:
        printf("\nОбъект %s не найден\n",filename);
        break;
    
      case 1: // регулярный файл
        show_efs_filestat(filename,&fi);
        break;
	
      case 2: // каталог
        if ((lmode == fl_tree) || (lmode == fl_ftree)) show_tree(lmode,filename);
	else show_files(lmode,filename);
	break;
	
      case -1: // ошибка
	printf("\nОбъект %s недоступен, код %d",filename,efs_get_errno());
	break;
    }    
    break;

//============================================================================  
// Просмотр файлов
  case MODE_TYPE:
    if (optind == argc) {
      printf("\n Не указано имя файла");
      break;
    }  
    list_file(argv[optind],tmode);
    break;

//============================================================================  
// Извлечение файла
  case MODE_GETFILE:
     if (optind < (argc-2)) {
      printf("\n Недостаточно параметров в командной строке");
      break;
    }  
    if (optind == (argc-1)) get_file(argv[optind],0);
    else get_file(argv[optind],argv[optind+1]);	
    break;
    
//============================================================================  
// Запись файла
  case MODE_WRITEFILE:
    if (optind != (argc-2)) {
      printf("\n Недостаточно параметров в командной строке");
      break;
    }  
    write_file(argv[optind],argv[optind+1]);
    break;

//============================================================================  
// Удаление файла
  case MODE_DELFILE:
    if (optind == argc) {
      printf("\n Не указано имя файла");
      break;
    }  
    switch (gmode) {
      case G_FILE:
        efs_unlink(argv[optind]);
	break;

      case G_DIR:
        efs_rmdir(argv[optind]);
	break;
    }	
    break;

//============================================================================  
// Создание каталога
  case MODE_MKDIR:
    if (optind == argc) {
      printf("\n Недостаточно параметров в командной строке");
      break;
    }  
    if (efs_mkdir(argv[optind],7) != 0) {
      printf("\nОшибка создания каталога %s, код %d",argv[optind],efs_get_errno());
    }  
    break;
    
//============================================================================  
  default:
    printf("\n Не указан ключ выполняемой операции\n");
    return;

}    
if (lmode != fl_mid) printf("\n");

}

