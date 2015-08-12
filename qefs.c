#include "include.h"
#include <time.h>

//%%%%%%%%%  Общие переменные %%%%%%%%%%%%%%%%

unsigned int altflag=0;   // флаг альтернативной EFS
unsigned int fixname=0;   // индикатор явного уазания имени файла
char filename[50];        // имя выходного файла

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
//* Вывод списка файлов указанного каталога 
//*  mode - режим вывода fl_*
//*  dirname - начальный путь, по умолчанию /
//****************************************************
void show_files (int lmode, char* dirname) {
  
char chdir[120]={0x4b, 0x13, 0x0b, 0x00, 0x2f, 0};
char cmdfile[]= {0x4b, 0x13, 0x0c, 0x00, 0x01, 0, 0, 0, 0, 0, 0, 0};
char closedir[]={0x4b, 0x13, 0x0d, 00, 1, 00, 00, 00};
unsigned char iobuf[2048];
int iolen;

char dnlist[200][100]; // список каталогов
unsigned short ndir=0;

int i,nfile;
time_t* filetime;
unsigned int filesize;
unsigned int fileattr,filelcnt;
char timestr[100];
char ftype;
char targetname[200];
int filecnt=0;

strcat(chdir+5,dirname);
// reset
iolen=send_cmd_base(closedir,8,iobuf,0);
// chdir
iolen=send_cmd_base(chdir,strlen(dirname)+6,iobuf,0);
if ((iolen == 0) || (memcmp(iobuf,"\x4b\x13\x0b",3) != 0))  {
  printf("\n ! Доступ в каталог %s запрещен\n",dirname);
  return;
}

if (lmode == fl_full) printf("\n *** Каталог %s ***",strlen(dirname) == 0?"/":dirname);
// поиск файлов
for(nfile=1;;nfile++) {
 *((int*)&cmdfile[8])=nfile;
 iolen=send_cmd_base(cmdfile,12,iobuf,0);
 if(iobuf[0x28] == 0) break; // конец списка
 filecnt++;
 // разбираем блок атрибутов
 filelcnt=*((unsigned int*)&iobuf[0x10]);
 filesize=*((unsigned int*)&iobuf[0x18]);
 fileattr=*((unsigned int*)&iobuf[0x14]);
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
   strcat(targetname,iobuf+0x28);
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
   if (ftype == 'D') { 
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
  for(i=0;i<ndir;i++) {
   strcpy(targetname,dirname);
   strcat(targetname,"/");
   strcat(targetname,dnlist[i]);
   show_files(lmode,targetname);
  }
}  
}

   

//@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@
//@@@@@@@@@@@@ Головная программа
void main(int argc, char* argv[]) {

unsigned int opt;
  
enum{
  MODE_BACK_EFS,
  MODE_FILELIST
}; 

int mode=-1;
int lmode=-1;

#ifndef WIN32
char devname[50]="/dev/ttyUSB0";
#else
char devname[50]="";
#endif

while ((opt = getopt(argc, argv, "hp:o:ab:r:l:")) != -1) {
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
-lf       - показать полный список файлов EFS\n  (Для всех -l ключей можео указать начальный путь к каталогу)\n\
* Ключи-модификаторы:\n\
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
       printf("\n В командной строке задано более 1 ключа режима работы");
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
       printf("\n В командной строке задано более 1 ключа режима работы");
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
       
   //  === группа ключей read ==
   case 'r':
     if (mode != -1) {
       printf("\n В командной строке задано более 1 ключа режима работы");
       return;
     }  
     switch(*optarg) {
	 
	 
       default:
	 printf("\n Неправильно задано значение ключа -r\n");
	 return;
      }
      break;
      
   case 'p':
    strcpy(devname,optarg);
    break;

   case 'a':
     altflag=1;
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
  case MODE_BACK_EFS:
    back_efs();
    break;
  
  case MODE_FILELIST:
    tspace=0;
    if (optind == argc) show_files(lmode,"");
    else show_files(lmode,argv[optind]);
    break;
    
  default:
    printf("\n Не указан ключ выполняемой операции\n");
    return;

}    
printf("\n");

}

