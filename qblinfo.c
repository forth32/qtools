 #include <stdio.h>
#include <string.h>

void main(int argc, char* argv[]) {
  
FILE* in;
unsigned int fl,i,j,daddr,ncmd,tablefound=0,hwidfound=0;
unsigned char buf[1024*512];
unsigned char hwidstr[7]="HW_ID1";
unsigned char hwid[17];

in=fopen(argv[1],"rb");
if (in == 0) {
  printf("\n Cannot open file %s\n",argv[1]);
  return;
}

fl=fread(&buf,1,sizeof(buf),in);
//laddr=buf[3]&0xffff0000;
printf("\n** %s: %i bytes\n",argv[1],fl);

for (i=0;i<fl;i++) {
 if (hwidfound == 0) {
	if (((memcmp(buf+i,hwidstr,6)) == 0) && (buf[i-17] == 0x30)) { // ищем строку "HW_ID" в сертификате
		memcpy(hwid,buf+i-17,16); // нашли, сохраняем значение
		hwid[16]=0;
		hwidfound=1; // и больше не ищем
	}
 }
 if ((i >= 0x3000) && (i%4 == 0) && (tablefound == 0)) { // таблица выровнена по границе слова и ещё не обработана
	daddr=*((unsigned int*)&buf[i]); // адрес заглушки
	if ((daddr<0x10000) || (daddr>0xffff0000)) continue;
	if ( (daddr != *((unsigned int*)&buf[i-4])) && (daddr == *((unsigned int*)&buf[i+8])) && (daddr == *((unsigned int*)&buf[i+16])) && (daddr == *((unsigned int*)&buf[i+24])) && (daddr == *((unsigned int*)&buf[i+32])) && (daddr == *((unsigned int*)&buf[i+40]))) {
		// таблица найдена	
		tablefound=1;
		// цикл выбра команд из таблицы
		ncmd=0;
		for (j=0;j<0x100;j++) {
			if (*((unsigned int*)&buf[i-4+4*j]) != daddr) {
			if ((*((unsigned int*)&buf[i-4+4*j])&0xfffc0000) != (daddr&0xfffc0000)) break; // это не адрес, конец таблицы
			ncmd++;
			printf("\n CMD %02x = %08x",j+1,*((unsigned int*)&buf[i-4+4*j]));
			}
		}  
		if (ncmd != 0) {
			printf("\n CMD table offset: %x",i-4);
			printf("\n Invalid CMD handler: %08x",daddr);
		} else 	printf ("\n No CMD table found. Image is probably not a *PRG.");  
	}
 }
}
 if (hwidfound == 1) {
	printf("\n\n HW_ID = 0x%.16s",hwid);
	printf("\n MSM_ID = 0x%.8s\n OEM_ID = 0x%.4s\n MODEL_ID = 0x%.4s\n",hwid,hwid+8,hwid+12);
} else printf("\n\n Unsigned code or no HW_ID value in Subject field\n");

printf("\n");
}
