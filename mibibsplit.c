#include <stdio.h>

void main(int argc, char* argv[]) {
  
FILE* in;
FILE* out;
unsigned char buf[0x40000];
int i;

if (argc != 2) {
  printf("\n Не указано имя файла MIBIB\n");
  return;
}
in=fopen(argv[1],"r");
if (in == 0) {
  printf("\n Ошибка открытия файла MIBIB\n");
  return;
}  
  
  
fread(buf,0x40000,1,in);  // читаем весь SBL1  
// ищем реальный хвост раздела
for(i=0x3ffff;i>0;i--) {
  if (buf[i] != 0xff) break;
}
out=fopen("sbl1.mbn","w");
fwrite(buf,i,1,out);
fclose (out);
  
// читаем таблицу разделов
fseek(in,0x1000,SEEK_CUR);
fread(buf,2048,1,in);
// ищем хвост
for(i=0x7ff;i>0;i--) {
  if (buf[i] != 0xff) break;
}
i=(i+16)&0xfffffff0; // округляем хвост по границе 16 байт
out=fopen("partition.mbn","w");
fwrite(buf,i,1,out);
fclose (out);
}
