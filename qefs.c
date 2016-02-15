#ifdef WIN32
#define _USE_32BIT_TIME_T
#endif
#include "include.h"
#include <time.h>
#include "efsio.h"

//%%%%%%%%%  Общие переменные %%%%%%%%%%%%%%%%

unsigned int altflag=0;   // флаг альтернативной EFS
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
  fl_full     // полный листинг файлов
};  

  
int tspace; // отступ для формирования дерева файлов


//****************************************************
//* Чтение дампа EFS (efs.mbn)
//****************************************************

void back_efs() {

unsigned char cmd_efs_dump_prepare[16]=  {0x4B, 0x13,0x19, 0};
unsigned char cmd_efsopen[16]=  {0x4B, 0x13,0x16, 0};
unsigned char cmd_efsdata[16]={0x4B, 0x13,0x17,0,0,0,0,0,0,0,0,0};
unsigned char cmd_efsclose[16]=  {0x4B, 0x13,0x18, 0};

FILE* out;

// настройка на альтернативную EFS
if (altflag) {
 cmd_efs_dump_prepare[1]=0x3e;
 cmd_efsopen[1]=0x3e;
 cmd_efsdata[1]=0x3e;
 cmd_efsclose[1]=0x3e;
 if (!fixname) strcpy(filename,"efs_alt.mbn");
}
// настройка на стандартную EFS
else if (!fixname) strcpy(filename,"efs.mbn");
   
out=fopen(filename,"w");
  
iolen=send_cmd_base(cmd_efs_dump_prepare, 4, iobuf, 0);
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

memset(str,'-',3);
if ((mode&4) != 0) str[0]='r';
if ((mode&2) != 0) str[1]='w';
if ((mode&1) != 0) str[2]='x';
return str;
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
//* Вывод детальной информации о регулярном файле
//****************************************************
void show_efs_filestat(char* filename, struct efs_filestat* fi) {
  
char timestr[100];
struct tm lt;      // структура для сохранения преобразованной даты
char sfbuf[50]; // буфер для сохранения описания типа файла

printf("\n Имя файла: %s",filename);
printf("\n Размер   : %i байт",fi->size);
printf("\n Тип файла: %s",str_filetype(fi->mode,sfbuf));
printf("\n Счетчик ссылок: %d",fi->nlink);
printf("\n Атрибуты доступа: %s%s%s",
      cfattr(fi->mode&7),
      cfattr((fi->mode>>3)&7),
      cfattr((fi->mode>>6)&7));

if (localtime_r(&fi->ctime,&lt) != 0)  {
 strftime(timestr,100,"%d-%b-%y %H:%M",localtime(&fi->ctime));
 printf("\n Дата создания: %s",timestr);
}

if (localtime_r(&fi->mtime,&lt) != 0)  {
 strftime(timestr,100,"%d-%b-%y %H:%M",localtime(&fi->mtime));
 printf("\n Дата модификации: %s",timestr);
}

if (localtime_r(&fi->atime,&lt) != 0)  {
 strftime(timestr,100,"%d-%b-%y %H:%M",localtime(&fi->atime));
 printf("\n Дата последнего доступа: %s\n",timestr);
}
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
//*  fname - начальный путь, по умолчанию /
//****************************************************
void show_files (int lmode, char* fname) {
  
struct efs_dirent dentry; // описатель элемента каталога
char dnlist[200][100]; // список каталогов
unsigned short ndir=0;
unsigned char dirname[100];	
struct tm lt;      // структура для сохранения преобразованной даты
int dirp=0;  // указатель на открытый каталог

int i,nfile;
time_t* filetime;
char timestr[100];
char ftype;
char targetname[200];
int filecnt=0;

if (strlen(fname) == 0) strcpy(dirname,"/"); // по умолчанию открываем корневой каталог
else strcpy(dirname,fname);

// chdir
dirp=efs_opendir(dirname);
if (dirp == 0) {
  printf("\n ! Доступ в каталог %s запрещен, errno=%i\n",dirname,efs_get_errno());
//  printf("\n ! Доступ в каталог %s запрещен\n",dirname);
  return;
}

if (lmode == fl_full) printf("\n *** Каталог %s ***",dirname);
// поиск файлов
for(nfile=1;;nfile++) {
 // выбираем следующую запись
 if (efs_readdir(dirp, nfile, &dentry) == -1) continue; // при ошибке чтения очередной структуры
 if (dentry.name[0] == 0) break;   // конец списка
 ftype=chr_filetype(dentry.mode);
 if ((dentry.entry_type) == 1) { 
   // Формируем список подкаталогов
   strcpy(dnlist[ndir++],dentry.name);
 }  

 // Определяем полное имя файла
   strcpy(targetname,dirname);
//   strcat(targetname,"/");
   strcat(targetname,dentry.name); // пропускаем начальный "/"
   if(ftype == 'D') strcat (targetname,"/");
 
 
 // режим простого списка файлов
 if (lmode == fl_list) {
   printf("\n%s",targetname);
   if ((ftype == 'D') && (recurseflag == 1)) { 
     show_files(lmode,targetname);
   } 
   continue;
 }
 
 // режим полного списка файлов
if (localtime_r(filetime,&lt) != 0) 
 strftime(timestr,100,"%d-%b-%y %H:%M",&lt);
else strcpy(timestr,"---------------");
 printf("\n%c%s%s%s %9i %s %s",
      ftype,
      cfattr(dentry.mode&7),
      cfattr((dentry.mode>>3)&7),
      cfattr((dentry.mode>>6)&7),
      dentry.size,
      timestr,
      dentry.name);
}

// данный каталог обработан - обрабатываем вложенные подкаталоги в режиме полного просмотра

efs_closedir(dirp);  
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
//* Открытие EFS-файла 
//*
// mode=0 - чтение
//      1 - запись
//**************************************************
int efs_open(char* filename, int mode) {

char open_cmd[2][100]={
  {0x4b, 0x13, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x24, 0x01, 0x00, 0x00},
  {0x4b, 0x13, 0x02, 0x00, 0x41, 0x02, 0x00, 0x00, 0xb6, 0x01, 0x00, 0x00}
};  

strcpy(open_cmd[mode]+0xc,filename);
iolen=send_cmd_base(open_cmd[mode],13+strlen(filename),iobuf,0);
if (*((unsigned int*)&iobuf[4]) != 0) {
  printf("\n Ошибка открытия EFS-файла %s",filename);
  dump(iobuf,iolen,0);
  free(fbuf);
  return 0;
}
return 1;
}

//**************************************************   
//* Закрытие файла
//**************************************************   
void closefile() {

char close_cmd[]={0x4b, 0x13, 0x03, 0x00, 0x00, 0x00, 0x00, 0x00}; 
send_cmd_base(close_cmd,8,iobuf,0);
}
  
  
//**************************************************   
//* Чтение файла в буфер
//**************************************************   
unsigned int readfile(char* filename) {	

struct efs_filestat fi;
int i,blk;

// 4b 13 04 00 00 00 00 00 ss ss ss ss oo oo oo oo
char read_cmd[16]={0x4b, 0x13, 0x04, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00};

closefile();
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
if (!efs_open(filename,0)) return 0;

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
closefile();
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

// 4b 13 05 00 00 00 00 00 ss ss ss ss oo oo oo oo
char write_cmd[600]={0x4b, 0x13, 0x05, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};

char iobuf[4096];

closefile();

// готовим имя выходного файла

strcpy(filename,path);
if (strrchr(file,'/') != 0) strcat(filename,strrchr(file,'/')+1);
else strcat(filename,file);

// проверяем существование файла

switch (efs_stat(filename,&fi)) {
   case 1:
     printf("\nОбъект %s уже существует\n",filename);
     return 0;
 
   case 2: // каталог
     printf("\nОбъект %s является каталогом\n",filename);
     return 0;
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

if (!efs_open(filename,1)) return 0;

blk=512;
for (i=0;i<(filesize);i+=512) {
 *((unsigned int*)&write_cmd[8])=i;
 if ((i+512) > filesize) {
   blk=filesize-i;
 }
 memcpy(write_cmd+12,fbuf+i,blk);
 send_cmd_base(write_cmd,blk+12,iobuf,0);
#ifndef WIN32
 usleep(3000);
#else
 Sleep(3);
#endif
}
free(fbuf);
closefile();
return 1;
}
////////////////////////////////////////////////////////////////////////////////////


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

//******************************************************
//*  Копирование единичного файла из EFS в текущий каталог
//******************************************************
void get_file(char* name) {
  
unsigned int flen;
char* fnpos;
FILE* out;

flen=readfile(name);
if (flen == 0) return;
// выделяем имя файла из полного пути
fnpos=strrchr(name,'/');
if (fnpos == 0) fnpos=name;
else fnpos++;
out=fopen(fnpos,"w");
fwrite(fbuf,1,flen,out);
fclose(out);
}

//******************************************************
//*  Удаление файла по имени
//******************************************************
void erase_file(char* name) {

char erase_cmd[600]={0x4b, 0x13, 0x08, 0x00};

memset(erase_cmd+4,0,590);
strcpy(erase_cmd+4,name);
iolen=send_cmd_base(erase_cmd,strlen(name)+5,iobuf,0);

} 

//******************************************************
//*  Создание каталога
//******************************************************
void efs_mkdir(char* dirname, char* dmode) {
  
char mkdir_cmd[600]={0x4b, 0x13, 0x09, 0x00,0x0,0};
int i;

// вписываем имя файла
memset(mkdir_cmd+6,0,580);
strcpy(mkdir_cmd+6,dirname);

// вписываем права доступа
for(i=0;i<strlen(dmode);i++) {
  switch (dmode[i]) {
    case 'r':
      mkdir_cmd[4]|=0x4;
      break;

    case 'w':
      mkdir_cmd[4]|=0x2;
      break;

    case 'x':
      mkdir_cmd[4]|=0x1;
      break;

    default:
      printf("\n Неправильно указаны права доступа: %s",dmode);
      return;
  }
}  
//dump(mkdir_cmd,32,0);
iolen=send_cmd_base(mkdir_cmd,strlen(dirname)+7,iobuf,0);
}

//******************************************************
//*  Удаление каталога
//******************************************************
void efs_rmdir(char* dirname) {
  
char rmdir_cmd[600]={0x4b, 0x13, 0x0a, 0x00};
  
memset(rmdir_cmd+4,0,580);
strcpy(rmdir_cmd+4,dirname);
iolen=send_cmd_base(rmdir_cmd,strlen(dirname)+5,iobuf,0);
}


//@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@
//@@@@@@@@@@@@ Головная программа
void main(int argc, char* argv[]) {

unsigned int opt,i;
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
%s [ключи] [путь или имя файла]\n\
Допустимы следующие ключи:\n\n\
* Ключи, определяюще выполняемую операцию:\n\
-be       - дамп efs\n\n\
-ld       - показать дерево каталогов EFS (без регулярных файлов)\n\
-lt       - показать дерево каталогов и файлов EFS\n\
-ll       - показать короткий список файлов указанного каталога\n\
-lf       - показать полный список файлов указанного каталога\n  (Для всех -l ключей можео указать начальный путь к каталогу)\n\n\
-tt       - просмотр файла в текстовом виде\n\
-td       - просмотр файла в виде дампа\n\n\
-gf file  - читает указанный файл из EFS в текущий каталог\n\
-wf file path - записывает указанный файл по указанному пути\n\
-ef file  - удаляет указанный файл\n\
-ed dir   - удаляет указанный каталог\n\
-md dir [r][w][x]  - создает каталог с указанными правами доступа\n\n\
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

  //  === группа ключей чтения файла (get) ==
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
     altflag=1;
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
    if (optind == argc)    strcpy(filename,"");
    // путь указан
    else strcpy(filename,argv[optind]);
    // Проверяем наличие файла, и является ли он каталогом
    switch (efs_stat(filename,&fi)) {
      case 0:
        printf("\nОбъект %s не найден\n",argv[optind]);
        break;
    
      case 1: // регулярный файл
        show_efs_filestat(argv[optind],&fi);
        break;
	
      case 2: // каталог
        if ((lmode == fl_tree) || (lmode == fl_ftree)) show_tree(lmode,argv[optind]);
	else show_files(lmode,argv[optind]);
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
    get_file(argv[optind]);
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
        erase_file(argv[optind]);
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
    if (optind == (argc-1)) efs_mkdir(argv[optind],"");
    else 	            efs_mkdir(argv[optind],argv[optind+1]);
    break;
    
//============================================================================  
  default:
    printf("\n Не указан ключ выполняемой операции\n");
    return;

}    
printf("\n");

}

