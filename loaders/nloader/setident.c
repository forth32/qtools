#include <stdio.h>

void main(int argc, char* argv[]) {

FILE* ldr;
unsigned int code,adr;
unsigned int i;

  
if ((argc!=2)&&(argc!=4)) {
  printf("\n Формат командной строки:\n\
  %s  <файл загрузчика> <код чипсета> <адрес загрузки> - запись блока идентификации в загрузчик\n\
  %s  <файл загрузчика> - просмотр блока идентификации\n",argv[0],argv[0]);
  return;
}

ldr=fopen(argv[1],"r+");
if (ldr == 0) {
  printf("\n Ошибка открытия файла %s\n",argv[1]);
  return;
}

//------------ режим просмотра блока идентификации ---------------
if (argc == 2) {
  fseek(ldr,-12,SEEK_END);
  fread(&i,1,4,ldr);
  if (i != 0xdeadbeef) {
     printf("\n Загрузчик не содержит в себе блок идентификации\n");
     return;
   }
  fread(&code,4,1,ldr);
  fread(&adr,4,1,ldr);
  printf("\n Параметры идентификации загрузчика %s:",argv[1]);
  printf("\n * Код чипсета = 0x%x",code);
  printf("\n * Адрес загрузки = 0x%08x\n\n",adr);
  return;
}

//----------- Режим записи блока идентификации ---------------

sscanf(argv[2],"%x",&code);
if (code == 0) {
  printf("\n Неправильный код чипсета>\n");
  return;
}

sscanf(argv[3],"%x",&adr);
if (adr == 0) {
  printf("\n Неправильный код чипсета>\n");
  return;
}


fseek(ldr,-12,SEEK_END);
fread(&i,1,4,ldr);
if (i == 0xdeadbeef) {
  printf("\n Загрузчик уже содержит в себе блок идентификации - перезаписываем\n");
}
else {
  fclose(ldr);
  ldr=fopen(argv[1],"a");
  i=0xdeadbeef;
  fwrite(&i,4,1,ldr);
}  
fwrite(&code,4,1,ldr);
fwrite(&adr,4,1,ldr);
}

