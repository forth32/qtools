#include "include.h"

// Установка параметров flash-контроллера
//

void main(int argc, char* argv[]) {
  
#ifndef WIN32
char devname[20]="/dev/ttyUSB0";
#else
char devname[20]="";
#endif

// локальные параметры для установки
int lud=-1, lecc=-1, lspare=-1;
int sflag=0;
int opt;

while ((opt = getopt(argc, argv, "hp:s:u:e:")) != -1) {
  switch (opt) {
   case 'h': 
     printf("\n Утилита предназначена установки параметров NAND-контроллера\n\n\
Допустимы следующие ключи:\n\n\
-p <tty> - указывает имя устройства последовательного порта для общения с загрузчиком\n\
-s nnn   - установка размера поля spare на сектор\n\
-u nnn   - установка размера поля данных сектора\n\
-e nnn   - установка размера поля ECC на сектор\n");
    return;
     
   case 'p':
    strcpy(devname,optarg);
    break;
    
   case 's':
     sscanf(optarg,"%d",&lspare);
     sflag=1;
     break;

   case 'u':
     sscanf(optarg,"%d",&lud);
     sflag=1;
     break;

   case 'e':
     sscanf(optarg,"%d",&lecc);
     sflag=1;
     break;
    
   case '?':
   case ':':  
     return;
  }
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

if (!sflag) {
 hello(1);
 return;
}

hello(0);

if (lspare != -1) set_sparesize(lspare);
if (lud != -1) set_udsize(lud);
if (lecc != -1) set_eccsize(lecc);

} 
