//
//  Процедуры работы с адресным пространоством модема через загрузчик
//
#include "include.h"



//***********************
//* Дамп области памяти *
//***********************

void dump(unsigned char buffer[],unsigned int len,unsigned int base) {
unsigned int i,j;
char ch;

for (i=0;i<len;i+=16) {
  printf("%08x: ",base+i);
  for (j=0;j<16;j++){
   if ((i+j) < len) printf("%02x ",buffer[i+j]&0xff);
   else printf("   ");
  }
  printf(" *");
  for (j=0;j<16;j++) {
   if ((i+j) < len) {
    ch=buffer[i+j];
    if ((ch < 0x20)|(ch > 0x80)) ch='.';
    putchar(ch);
   }
   else putchar(' ');
  }
  printf("*\n");
}
}

//***********************************8
//* Чтение области памяти
//***********************************8

int memread(unsigned char* membuf,int adr, int len) {
char iobuf[11600];
int tries;       // число попыток повтора команды
int errcount=0;      // счктчик ошибок

// параметры апплета чтения - смащения:  
const int adroffset=0x2E;  // адрес записи
const int lenoffset=0x32;  // размер данных 

char cmdbuf[]={
   0x11,0x00,0x24,0x30,0x9f,0xe5,0x24,0x40,0x9f,0xe5,0x12,0x00,0xa0,0xe3,0x04,0x00,
   0x81,0xe4,0x04,0x00,0x83,0xe0,0x04,0x20,0x93,0xe4,0x04,0x20,0x81,0xe4,0x00,0x00,
   0x53,0xe1,0xfb,0xff,0xff,0x3a,0x04,0x40,0x84,0xe2,0x1e,0xff,0x2f,0xe1,0x00,0x00,
   0x00,0x00,0x00,0x00,0x00,0x00,
};  


int i,iolen;
// начальная длина фрагмента
int blklen=1000;
*((unsigned int*)&cmdbuf[lenoffset])=blklen;  

///////////////////
//printf("\n!ms adr=%08x len=%08x",adr,len);

// Чтение блоками по 1000 байт  
for(i=0;i<len;i+=1000)  {
 tries=20; // число попыток чтениия блока данных  
 *((unsigned int*)&cmdbuf[adroffset])=i+adr;  //вписываем адрес
 if ((i+1000) > len) {
 // последний блок данных - может быть коротким  
   blklen=len-i;
   *((unsigned int*)&cmdbuf[lenoffset])=blklen;  //вписываем длину
 }
 
 // делаем несколько попыток послать команду и прочитать данные
 while (tries>0) {
  iolen=send_cmd_massdata(cmdbuf,sizeof(cmdbuf),iobuf,blklen+4);
  if (iolen <(blklen+4)) {
     // короткий ответ от загрузчика
     tries--;
     usleep(1000);
//     printf("\n!t%i! %i < %i",tries,iolen,blklen+4);
  }
  else break; // нормальный ответ - заканчиваем с этим блоком данных
 }
 if (tries == 0) { 
    printf("\n Ошибка обработки команды чтения памяти, требуется %i байт, получено %i adr=%08x\n",blklen,iolen,adr);
    memset(membuf+i,0xeb,blklen);
    errcount++;
 }   
 else memcpy(membuf+i,iobuf+5,blklen);
}
return !errcount;
}

//***********************************8
//* Чтение слова из памяти
//***********************************8
unsigned int mempeek(int adr) {

unsigned int data;
memread ((unsigned char*)&data,adr,4);
return data;
}  


//******************************************
//*  Запись массива в память
//******************************************
int memwrite(unsigned int adr, unsigned char* buf, unsigned int len) {

// параметры апплета записи - смащения:  
const int adroffset=0x32;  // адрес записи
const int lenoffset=0x36;  // размер данных 
const int dataoffset=0x3a; // начало данных
  
char cmdbuf[1028]={
  0x11,0x00,0x38,0x00,0x80,0xe2,0x24,0x30,0x9f,0xe5,0x24,0x40,0x9f,0xe5,0x04,0x40,
  0x83,0xe0,0x04,0x20,0x90,0xe4,0x04,0x20,0x83,0xe4,0x04,0x00,0x53,0xe1,0xfb,0xff,
  0xff,0x3a,0x12,0x00,0xa0,0xe3,0x00,0x00,0xc1,0xe5,0x01,0x40,0xa0,0xe3,0x1e,0xff,
  0x2f,0xe1,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00
};

unsigned char iobuf[1024];
  
if (len>1000) exit(1);  //   ограничитель на размер буфера
memcpy(cmdbuf+dataoffset,buf,len);
*((unsigned int*)&cmdbuf[adroffset])=adr;
*((unsigned int*)&cmdbuf[lenoffset])=len;
send_cmd(cmdbuf,len+dataoffset,iobuf);

return 1;
}


//******************************************
//*  Запись слова в память
//******************************************
int mempoke(int adr, int data) {

unsigned int data1=data;  
return (memwrite(adr,(unsigned char*)&data1,4));
}

