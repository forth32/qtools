#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#ifndef WIN32
#include <strings.h>
#include <termios.h>
#include <unistd.h>
#else
#include <windows.h>
#include "printf.h"
#endif

#include "qcio.h"

unsigned int nand_cmd=0x1b400000;

#ifndef WIN32
struct termios sioparm;
#else
static HANDLE hSerial;
#endif
int siofd; // fd для работы с Последовательным портом

//****************************************************************
//* Ожидание завершения операции, выполняемой контроллером nand  *
//****************************************************************
void nandwait() { 
  while ((mempeek(nand_status)&0xf) != 0); 
}


//***********************
//* Дамп области памяти *
//***********************

void dump(char buffer[],int len,long base) {
unsigned int i,j;
char ch;

for (i=0;i<len;i+=16) {
  printf("%06lx: ",(long)(base+i));
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


//*************************************************
//*  Вычисление CRC-16 
//*************************************************
unsigned short crc16(char* buf, int len) {

unsigned short crctab[] = {
    0x0000, 0x1189, 0x2312, 0x329b, 0x4624, 0x57ad, 0x6536, 0x74bf,   // 0
    0x8c48, 0x9dc1, 0xaf5a, 0xbed3, 0xca6c, 0xdbe5, 0xe97e, 0xf8f7,   // 8
    0x1081, 0x0108, 0x3393, 0x221a, 0x56a5, 0x472c, 0x75b7, 0x643e,   //16
    0x9cc9, 0x8d40, 0xbfdb, 0xae52, 0xdaed, 0xcb64, 0xf9ff, 0xe876,   //24
    0x2102, 0x308b, 0x0210, 0x1399, 0x6726, 0x76af, 0x4434, 0x55bd,   //32
    0xad4a, 0xbcc3, 0x8e58, 0x9fd1, 0xeb6e, 0xfae7, 0xc87c, 0xd9f5,   //40
    0x3183, 0x200a, 0x1291, 0x0318, 0x77a7, 0x662e, 0x54b5, 0x453c,   //48
    0xbdcb, 0xac42, 0x9ed9, 0x8f50, 0xfbef, 0xea66, 0xd8fd, 0xc974,   //56
    0x4204, 0x538d, 0x6116, 0x709f, 0x0420, 0x15a9, 0x2732, 0x36bb,   //64
    0xce4c, 0xdfc5, 0xed5e, 0xfcd7, 0x8868, 0x99e1, 0xab7a, 0xbaf3,
    0x5285, 0x430c, 0x7197, 0x601e, 0x14a1, 0x0528, 0x37b3, 0x263a,
    0xdecd, 0xcf44, 0xfddf, 0xec56, 0x98e9, 0x8960, 0xbbfb, 0xaa72,
    0x6306, 0x728f, 0x4014, 0x519d, 0x2522, 0x34ab, 0x0630, 0x17b9,
    0xef4e, 0xfec7, 0xcc5c, 0xddd5, 0xa96a, 0xb8e3, 0x8a78, 0x9bf1,
    0x7387, 0x620e, 0x5095, 0x411c, 0x35a3, 0x242a, 0x16b1, 0x0738,
    0xffcf, 0xee46, 0xdcdd, 0xcd54, 0xb9eb, 0xa862, 0x9af9, 0x8b70,
    0x8408, 0x9581, 0xa71a, 0xb693, 0xc22c, 0xd3a5, 0xe13e, 0xf0b7,
    0x0840, 0x19c9, 0x2b52, 0x3adb, 0x4e64, 0x5fed, 0x6d76, 0x7cff,
    0x9489, 0x8500, 0xb79b, 0xa612, 0xd2ad, 0xc324, 0xf1bf, 0xe036,
    0x18c1, 0x0948, 0x3bd3, 0x2a5a, 0x5ee5, 0x4f6c, 0x7df7, 0x6c7e,
    0xa50a, 0xb483, 0x8618, 0x9791, 0xe32e, 0xf2a7, 0xc03c, 0xd1b5,
    0x2942, 0x38cb, 0x0a50, 0x1bd9, 0x6f66, 0x7eef, 0x4c74, 0x5dfd,
    0xb58b, 0xa402, 0x9699, 0x8710, 0xf3af, 0xe226, 0xd0bd, 0xc134,
    0x39c3, 0x284a, 0x1ad1, 0x0b58, 0x7fe7, 0x6e6e, 0x5cf5, 0x4d7c,
    0xc60c, 0xd785, 0xe51e, 0xf497, 0x8028, 0x91a1, 0xa33a, 0xb2b3,
    0x4a44, 0x5bcd, 0x6956, 0x78df, 0x0c60, 0x1de9, 0x2f72, 0x3efb,
    0xd68d, 0xc704, 0xf59f, 0xe416, 0x90a9, 0x8120, 0xb3bb, 0xa232,
    0x5ac5, 0x4b4c, 0x79d7, 0x685e, 0x1ce1, 0x0d68, 0x3ff3, 0x2e7a,
    0xe70e, 0xf687, 0xc41c, 0xd595, 0xa12a, 0xb0a3, 0x8238, 0x93b1,
    0x6b46, 0x7acf, 0x4854, 0x59dd, 0x2d62, 0x3ceb, 0x0e70, 0x1ff9,
    0xf78f, 0xe606, 0xd49d, 0xc514, 0xb1ab, 0xa022, 0x92b9, 0x8330,
    0x7bc7, 0x6a4e, 0x58d5, 0x495c, 0x3de3, 0x2c6a, 0x1ef1, 0x0f78
};
int i;
unsigned short crc=0xffff;  

for(i=0;i<len;i++)  crc=crctab[(buf[i]^crc)&0xff]^((crc>>8)&0xff);  
return (~crc)&0xffff;
}

#ifdef WIN32

static int read(int siofd, unsigned char* buf, int len)
{
    DWORD bytes_read = 0;
    DWORD t = GetTickCount();

    do {
        ReadFile(hSerial, buf, len, &bytes_read, NULL);
    } while (bytes_read == 0 && GetTickCount() - t < 1000);

    return bytes_read;
}

static int write(int siofd, unsigned char* buf, int len)
{
    DWORD bytes_written = 0;

    WriteFile(hSerial, buf, len, &bytes_written, NULL);

    return bytes_written;
}

#endif

//***************************************************
//*  Отсылка команды в порт и получение результата  *
//*
//* prefixflag=0 - не посылать префикс 7E
//*            1 - посылать
//***************************************************
int send_cmd_base(unsigned char* incmdbuf, int blen, unsigned char* iobuf, int prefixflag) {
  
int i,iolen,escflag,bcnt,incount;
unsigned short datalen;
unsigned char c;
unsigned char cmdbuf[14096],outcmdbuf[14096];
unsigned int res;

bcnt=blen;
memcpy(cmdbuf,incmdbuf,blen);
*((unsigned short*)(cmdbuf+bcnt))=crc16(cmdbuf,bcnt);
bcnt+=2;

// Пребразование данных с экранированием ESC-последовательностей
iolen=0;
outcmdbuf[iolen++]=cmdbuf[0];  // первый байт копируем без модификаций
for(i=1;i<bcnt;i++) {
   switch (cmdbuf[i]) {
     case 0x7e:
       outcmdbuf[iolen++]=0x7d;
       outcmdbuf[iolen++]=0x5e;
       break;
      
     case 0x7d:
       outcmdbuf[iolen++]=0x7d;
       outcmdbuf[iolen++]=0x5d;
       break;
      
     default:
       outcmdbuf[iolen++]=cmdbuf[i];
   }
 }
outcmdbuf[iolen++]=0x7e; // завершающий байт
outcmdbuf[iolen]=0;
 
// отсылка команды в модем
#ifndef WIN32
tcflush(siofd,TCIOFLUSH);  // сбрасываем недочитанный буфер ввода
#else
PurgeComm(hSerial, PURGE_RXCLEAR);
#endif
if (prefixflag) write(siofd,"\x7e",1);  // отсылаем префикс если надо

//if (outcmdbuf[0] == 7) dump(outcmdbuf,iolen,0);

if (write(siofd,outcmdbuf,iolen) == 0) {   printf("\n Ошибка записи команды");return 0;  }
#ifndef WIN32
tcdrain(siofd);  // ждем окончания вывода блока
#else
FlushFileBuffers(hSerial);
#endif

incount=0;
if (read(siofd,&c,1) != 1) {
  printf("\n Нет ответа от модема");
  return 0; // модем не ответил или ответил неправильно
}
if (c != 0x7e) {
  printf("\n Первый байт ответа - не 7e: %02x",c);
//  return 0; // модем не ответил или ответил неправильно
}
iobuf[incount++]=c;

// чтение массива данных единым блоком при обработке команды 03
if ((cmdbuf[0] == 3)&&(blen == 10)) {
 datalen=*((unsigned short*)(cmdbuf+5)); // заказанная длина поля данных
 res=read(siofd,cmdbuf+1,datalen+7);
 if (res != (datalen+7)) {
   printf("\nСлишком короткий ответ от модема: %i байт\n",res+1);
   dump(cmdbuf,res+1,0);
   return 0;
 }  
 incount=incount+datalen+7;
}

// принимаем оставшийся хвост буфера
while (read(siofd,&c,1) == 1)  {
 cmdbuf[incount++]=c;
 if (c == 0x7e) break;
}

// Преобразование принятого буфера для удаления ESC-знаков
escflag=0;
iolen=0;
for (i=0;i<incount;i++) { 
  c=cmdbuf[i];
  if ((c == 0x7e)&&(iolen != 0)) {
    iobuf[iolen++]=0x7e;
    break;
  }  
  if (c == 0x7d) {
    escflag=1;
    continue;
  }
  if (escflag == 1) { 
    c|=0x20;
    escflag=0;
  }  
  iobuf[iolen++]=c;
}  
return iolen;

}




//***************************************************
//*    Отсылка команды с префиксом
//***************************************************
int send_cmd(unsigned char* incmdbuf, int blen, unsigned char* iobuf) {
   return send_cmd_base(incmdbuf, blen, iobuf,0);
}

//*************************************
// Настройка Последовательного порта
//*************************************

int open_port(char* devname)
{
#ifndef WIN32
siofd = open(devname, O_RDWR | O_NOCTTY |O_SYNC);
if (siofd == -1) return 0;

bzero(&sioparm, sizeof(sioparm)); // готовим блок атрибутов termios
sioparm.c_cflag = B115200 | CS8 | CLOCAL | CREAD ;
sioparm.c_iflag = 0;  // INPCK;
sioparm.c_oflag = 0;
sioparm.c_lflag = 0;
sioparm.c_cc[VTIME]=10; // timeout  
sioparm.c_cc[VMIN]=0;  
tcsetattr(siofd, TCSANOW, &sioparm);
return 1;
#else
    char device[20] = "\\\\.\\COM";
    DCB dcbSerialParams = {0};

    strcat(device, devname);
    
    hSerial = CreateFileA(device, GENERIC_READ | GENERIC_WRITE, 0, 0, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, 0);
    if (hSerial == INVALID_HANDLE_VALUE)
        return 0;

    dcbSerialParams.DCBlength=sizeof(dcbSerialParams);
    if (!GetCommState(hSerial, &dcbSerialParams))
    {
        CloseHandle(hSerial);
        return 0;
    }
    dcbSerialParams.BaudRate=CBR_115200;
    dcbSerialParams.ByteSize=8;
    dcbSerialParams.StopBits=ONESTOPBIT;
    dcbSerialParams.Parity=NOPARITY;
    if(!SetCommState(hSerial, &dcbSerialParams))
    {
        CloseHandle(hSerial);
        return 0;
    }

    return 1;
#endif
}

//*************************************
// Настройка Последовательного порта
//*************************************

void port_timeout(int timeout) {
#ifndef WIN32
bzero(&sioparm, sizeof(sioparm)); // готовим блок атрибутов termios
sioparm.c_cflag = B115200 | CS8 | CLOCAL | CREAD ;
sioparm.c_iflag = 0;  // INPCK;
sioparm.c_oflag = 0;
sioparm.c_lflag = 0;
sioparm.c_cc[VTIME]=timeout; // timeout  
sioparm.c_cc[VMIN]=0;  
tcsetattr(siofd, TCSANOW, &sioparm);
#endif
}

//***********************************8
//* Чтение области памяти
//***********************************8

int memread(char* membuf,int adr, int len) {
char iobuf[11600];
char cmdbuf[]={3,0,0,0,0,0,2};
int i,iolen;
int blklen=512;

// Чтение блоками по 512 байт  
for(i=0;i<len;i+=512)  {  
 *((unsigned int*)&cmdbuf[1])=i+adr;  //вписываем адрес
 if ((i+512) > len) {
   blklen=len-i;
   *((unsigned short*)&cmdbuf[5])=blklen;  //вписываем длину
 }  
 iolen=send_cmd(cmdbuf,7,iobuf);
 if (iolen <blklen) {
   printf("\n Ошибка в процессе обработки команды, iolen=%i\n",iolen);
   memcpy(membuf+i,iobuf+6,blklen);
   return 0;
 }  
 memcpy(membuf+i,iobuf+6,blklen);
} 
return 1;
}

//***********************************8
//* Чтение слова из памяти
//***********************************8
int mempeek(int adr) {

unsigned char iobuf[600];
unsigned char cmdbuf[]={3,0,0,0,0,4,0};
int i,iolen;

*((unsigned int*)&cmdbuf[1])=adr;  //вписываем адрес
iolen=send_cmd(cmdbuf,7,iobuf);
return *((unsigned int*)&iobuf[6]);
}  
  
//******************************************
//*  Запись слова в память
//******************************************
int mempoke(int adr, int data) {

unsigned char iobuf[300];
unsigned char cmdbuf[]={5,0,0,0,0,0,0,0,0,0};
int iolen;

*((unsigned int*)&cmdbuf[2])=adr;  //вписываем адрес
*((unsigned int*)&cmdbuf[6])=data;  //вписываем данные

iolen=send_cmd(cmdbuf,10,iobuf);

if (iobuf[2] != 06) {
  printf("\n Загрузчик не поддерживает команду 05:\n");
  printf("\n cmdbuf: "); dump(cmdbuf,10,0);
  printf("\n ответ : "); dump(iobuf,iolen,0);
  printf("\n");
  return 0;
}  
return 1;
}


//*************************************88
//* Установка адресных регистров 
//*************************************
void setaddr(int block, int page) {

int adr;  
  
adr=block*ppb+page;
mempoke(nand_addr0,adr<<16);
mempoke(nand_addr1,(adr>>16)&0xff);
}

//*********************************************
//* Чтение блока флешки по указанному адресу 
//*********************************************

int flash_read(int block, int page, int sect) {
  
unsigned char iobuf[300];
int iolen;
int i;

mempoke(nand_cfg1,0x6745d); // ECC off
mempoke(nand_cs,4); // data mover

mempoke(nand_cmd,1); // Сброс всех операций контроллера
mempoke(nand_exec,0x1);
nandwait();
// адрес
setaddr(block,page);
// устанавливаем код команды
mempoke(nand_cmd,0x34); // чтение data+ecc+spare
for(i=0;i<(sect+1);i++) {
  mempoke(nand_exec,0x1);
  nandwait();
}  

return 1;
}

//**********************************************8
//* Процедура активации загрузчика hello
//**********************************************8
void hello() {

int i;  
char rbuf[1024];
char hellocmd[]="\x01QCOM fast download protocol host\x03### ";

printf(" Отсылка hello...");
i=send_cmd(hellocmd,strlen(hellocmd),rbuf);
if (rbuf[1] != 2) {
   printf(" hello возвратил ошибку!\n");
   dump(rbuf,i,0);
   return;
}  
//dump(rbuf,i,0);
i=rbuf[0x2c];
rbuf[0x2d+i]=0;
printf("ok\n Flash: %s",rbuf+0x2d);
printf("\n Версия протокола: %i",rbuf[0x22]);
printf("\n Максимальный размер пакета: %i байта",*((unsigned int*)&rbuf[0x24]));
printf("\n");
}

//*************************************
//* чтение таблицы разделв из flash
//*************************************
void load_ptable(char* ptable) {
  
memset(ptable,0,1024); // обнуляем таблицу
flash_read(2, 1, 0);  // блок 2 страница 1 - здесь лежит таблица разделов  
memread(ptable,sector_buf, 512);
flash_read(2, 1, 1);  // продолжение таблицы разделов
memread(ptable+512,sector_buf, 512);
}
