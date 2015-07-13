#include "include.h"

void main(int argc, char* argv[]) {
  
FILE* in;
FILE* out;
unsigned char buf[0x40000];
int i;
int npar;
unsigned int rflag=0,wflag=0,padr=0;
unsigned int sig1,sig2;

if (argc != 2) {
  printf("\n Не указано имя файла MIBIB\n");
  return;
}
in=fopen(argv[1],"rb");
if (in == 0) {
  printf("\n Ошибка открытия файла MIBIB\n");
  return;
}  


// ищем начало блока таблиц разделов
while (!feof(in)) {
 fread(&sig1,1,4,in);  
 fread(&sig2,1,4,in);  
 if ((sig1 == 0xfe569fac) && (sig2 == 0xcd7f127a)) break; // найдена сигнатура блока таблиц разделов
 fseek(in,504,SEEK_CUR);  // пропускаем остаток сектора
 padr+=512;
} 
if (feof(in)) {
   printf("\n Сигнатура блока таблиц разделов не найдена\n");
   fclose(in);
   return;
}
// теперь padr - адрес блока разделов внутри файла
// читаем в буфер весь MIBIB до этого адреса - это будет SBL1 с хвостом
fseek(in,0,SEEK_SET);
fread(buf,padr,1,in);  

// ищем реальный хвост раздела
for(i=padr-1;i>0;i--) {
  if (buf[i] != 0xff) break;
}
out=fopen("sbl1.bin","wb");
fwrite(buf,i+1,1,out);
fclose (out);
  
// Ищем сигнатуры таблиц разделов
fseek(in,padr,SEEK_SET);
while(!feof(in)) {
 fread(&sig1,1,4,in);  
 fread(&sig2,1,4,in);  
 if ((!rflag) && (sig1 == 0x55ee73aa) && (sig2 == 0xe35ebddb)) {
   // найдена таблица чтения
   fseek(in,-8,SEEK_CUR); // отъезжаем на начало сигнатуры
   fread(buf,2048,1,in);
   npar=*((unsigned int*)&buf[12]); // число разделов
   out=fopen("ptable-r.bin","wb");
   fwrite(buf,16+28*npar,1,out);
   fclose (out);
   rflag=1;  // таблица чтения у нас есть
   continue; // пропускаем дальнейшую обработку сектора
 }
 
 if ((!wflag) && (sig1 == 0xaa7d1b9a) && (sig2 == 0x1f7d48bc)) {
   // найдена таблица записи
   fseek(in,-8,SEEK_CUR); // отъезжаем на начало сигнатуры
   fread(buf,2048,1,in);
   npar=*((unsigned int*)&buf[12]); // число разделов
   out=fopen("ptable-w.bin","wb");
   fwrite(buf,16+28*npar,1,out);
   fclose (out);
   wflag=1;  // таблица чтения у нас есть
   continue; // пропускаем дальнейшую обработку сектора
 }
 if (rflag && wflag)
 {
     fclose(in);
     return;   // найдены обе таблицы
 }
 fseek(in,504,SEEK_CUR);  // пропускаем остаток сектора
}  
if (!rflag) printf("\n Таблица чтения не найдена\n");
if (!wflag) printf("\n Таблица записи не найдена\n");
    fclose(in);
}
