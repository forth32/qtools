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
#include "wingetopt.h"
#endif

#include "qcio.h"

unsigned int nand_cmd=0x1b400000;
unsigned int spp=0;
unsigned int pagesize=0;
unsigned int sectorsize=512;
unsigned int maxblock=0;     // Общее число блоков флешки
char flash_mfr[30]={0};
char flash_descr[30]={0};
unsigned int oobsize=0;
unsigned int bad_loader=0;

// тип чипcета:
unsigned int chip_type=0; 
unsigned int maxchip=0;

// описатели чипсетов
struct {
  unsigned int nandbase;   // адрес контроллера
  unsigned char armflag;   // тип исполняемого кода: 0-ARM, 1-Thumb-2
  unsigned char udflag;    // udsize таблицы разделов, 0-512, 1-516
  unsigned char name[20];  // имя чипсета
}  chipset[]= {
//  адрес NAND   ARM  UDflag  имя           ##
  { 0x1b400000,   1,    0, "MDM9x15"},  //  0
  { 0xA0A00000,   0,    0, "MDM8200"},  //  1
  { 0x81200000,   0,    0, "MDM9x00"},  //  2
  { 0xf9af0000,   1,    1, "MDM9x25"},  //  3
  { 0x70000000,   0,    0, "MDM6600"},  //  4
  { 0,0,0 }
};
  
  
#ifndef WIN32
struct termios sioparm;
#else
static HANDLE hSerial;
COMMTIMEOUTS ct;
#endif
int siofd; // fd для работы с Последовательным портом

//************************************************
//* Печать списка поддерживаемых чипсетов
//***********************************************
void list_chipset() {
  
int i;
printf("\n Код     Имя    Адрес NAND\n-----------------------------------");
for(i=0;chipset[i].nandbase != 0 ;i++) {
  printf("\n %2i  %9.9s    %08x",i,chipset[i].name,chipset[i].nandbase);
  if (i == 0)  printf(" (по умолчанию)");
}
printf("\n\n");
exit(0);
}

//****************************************************************
//*   Установка параметров контроллера по номеру чипсета
//* 
//* arg - указатель на optarg, указанный в ключе -к
//****************************************************************
void define_chipset(char* arg) {

// проверяем на -kl
if (optarg[0]=='l') list_chipset();

// получаем кд чипсета из аргумента
sscanf(arg,"%i",&chip_type);

// получаем размер массива чипсетов
for(maxchip=0;chipset[maxchip].nandbase != 0 ;maxchip++);  
// проверяем наш номер
if (chip_type>=maxchip) {
  printf("\n - Неверный код чипсета - %i",chip_type);
  exit(1);
}
nand_cmd=chipset[chip_type].nandbase;
}

//**************************************************************
//*  Проверяет, требуется ли текущему чипсету ARM или Thumb-2 код
//*
//*   0 - arm
//*   1 - Thumb-2
//***********************************************
unsigned int is_arm_chipset() {
  return chipset[chip_type].armflag;
}  


//****************************************************************
//* Ожидание завершения операции, выполняемой контроллером nand  *
//****************************************************************
void nandwait() { 
  while ((mempeek(nand_status)&0xf) != 0); 
}


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
    } while (bytes_read == 0 && (GetTickCount() - t < 1000));

    return bytes_read;
}

static int write(int siofd, unsigned char* buf, int len)
{
    DWORD bytes_written = 0;

    WriteFile(hSerial, buf, len, &bytes_written, NULL);

    return bytes_written;
}

#endif

// очиска буфера последовательного порта
void ttyflush() { 

#ifndef WIN32
tcflush(siofd,TCIOFLUSH); 
#else
PurgeComm(hSerial, PURGE_RXCLEAR);
#endif
}

//*************************************************
//*    отсылка буфера в модем
//*************************************************
unsigned int send_unframed_buf(char* outcmdbuf, unsigned int outlen, int prefixflag) {

// сбрасываем недочитанный буфер ввода
ttyflush();

if (prefixflag) write(siofd,"\x7e",1);  // отсылаем префикс если надо

//if (outcmdbuf[0] == 7) dump(outcmdbuf,iolen,0);

if (write(siofd,outcmdbuf,outlen) == 0) {   printf("\n Ошибка записи команды");return 0;  }
#ifndef WIN32
tcdrain(siofd);  // ждем окончания вывода блока
#else
FlushFileBuffers(hSerial);
#endif
return 1;
}

//******************************************************************************************
//* Прием буфера с ответом из модема
//*
//*  masslen - число байтов, принимаемых единым блоком без анализа признака конца 7F
//******************************************************************************************

unsigned int receive_reply(char* iobuf, int masslen) {
  
int i,iolen,escflag,bcnt,incount;
unsigned char c;
unsigned int res;
unsigned char replybuf[14000];

incount=0;
if (read(siofd,&c,1) != 1) {
//  printf("\n Нет ответа от модема");
  return 0; // модем не ответил или ответил неправильно
}
//if (c != 0x7e) {
//  printf("\n Первый байт ответа - не 7e: %02x",c);
//  return 0; // модем не ответил или ответил неправильно
//}
replybuf[incount++]=c;

// чтение массива данных единым блоком при обработке команды 03
if (masslen != 0) {
 res=read(siofd,replybuf+1,masslen-1);
 if (res != (masslen-1)) {
   printf("\nСлишком короткий ответ от модема: %i байт, ожидалось %i байт\n",res+1,masslen);
   dump(replybuf,res+1,0);
   return 0;
 }  
 incount+=masslen-1; // у нас в буфере уже есть masslen байт
// printf("\n ------ it mass --------");
// dump(replybuf,incount,0);
}

// принимаем оставшийся хвост буфера
while (read(siofd,&c,1) == 1)  {
 replybuf[incount++]=c;
// printf("\n-- %02x",c);
 if (c == 0x7e) break;
}

// Преобразование принятого буфера для удаления ESC-знаков
escflag=0;
iolen=0;
for (i=0;i<incount;i++) { 
  c=replybuf[i];
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



//***********************************************************
//* Преобразование командного буфера с Escape-подстановкой
//***********************************************************
unsigned int convert_cmdbuf(char* incmdbuf, int blen, char* outcmdbuf) {

int i,iolen,escflag,bcnt,incount;
unsigned char cmdbuf[14096];

bcnt=blen;
memcpy(cmdbuf,incmdbuf,blen);
// Вписываем CRC в конец буфера
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
return iolen;
}



//***************************************************
//*  Отсылка команды в порт и получение результата  *
//*
//* prefixflag=0 - не посылать префикс 7E
//*            1 - посылать
//***************************************************
int send_cmd_base(unsigned char* incmdbuf, int blen, unsigned char* iobuf, int prefixflag) {
  
unsigned char outcmdbuf[14096];
unsigned int  iolen;

iolen=convert_cmdbuf(incmdbuf,blen,outcmdbuf);  
if (!send_unframed_buf(outcmdbuf,iolen,prefixflag)) return 0; // ошибка передачи команды
return receive_reply(iobuf,0);
}

//***************************************************************
//*  Отсылка команды в порт и получение результата в виде 
//*  большого буфера определенного размера
//*
//***************************************************
int send_cmd_massdata(unsigned char* incmdbuf, int blen, unsigned char* iobuf, unsigned int datalen) {
  
unsigned char outcmdbuf[14096];
unsigned int  iolen;

iolen=convert_cmdbuf(incmdbuf,blen,outcmdbuf);  
if (!send_unframed_buf(outcmdbuf,iolen,0)) return 0; // ошибка передачи команды
return receive_reply(iobuf,datalen);
}


//***************************************************
//*    Отсылка команды с префиксом
//***************************************************
int send_cmd(unsigned char* incmdbuf, int blen, unsigned char* iobuf) {
   return send_cmd_base(incmdbuf, blen, iobuf,0);
}

//*************************************
// Открытие и настройка последовательного порта
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
sioparm.c_cc[VTIME]=30; // timeout  
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
// Закрытие последовательного порта
//*************************************

void close_port()
{
#ifndef WIN32
close(siofd);
#else
CloseHandle(hSerial);
#endif
}

//*************************************
// Настройка таймаутов последовательного порта
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
#else
ct.ReadIntervalTimeout=10;
ct.ReadTotalTimeoutMultiplier=0;
ct.ReadTotalTimeoutConstant=100*timeout;
ct.WriteTotalTimeoutMultiplier=0;
ct.WriteTotalTimeoutConstant=0;
SetCommTimeouts(hSerial,&ct);
#endif
}


//*******************************************************
//* Проверка работы патча загрузчика
//*
//* Возвращает 0 если кманда 05 не поддерживается
//* и устанавливает глобальную переменную bad_loader=1
//*******************************************************
int test_loader() {

unsigned char cmdbuf[]={5,0,0,0,0,0,0,0,0,0};
unsigned char iobuf[1024];
unsigned int iolen;		

*((unsigned int*)&cmdbuf[2])=sector_buf;  //вписываем адрес
*((unsigned int*)&cmdbuf[6])=mempeek(sector_buf);  //вписываем данные - те же что там и лежат, чтобы ничего не портить
iolen=send_cmd(cmdbuf,10,iobuf);

if (iobuf[2] != 06) {
  bad_loader=1;
  return 0;
}
return 1;
}



//***********************************8
//* Чтение области памяти
//***********************************8


int memread(char* membuf,int adr, int len) {
char iobuf[11600];
char cmdbuf[]={3,0,0,0,0,0,2};
int i,iolen;
int blklen=sectorsize;

// Чтение блоками по 512 байт  
for(i=0;i<len;i+=sectorsize)  {  
 *((unsigned int*)&cmdbuf[1])=i+adr;  //вписываем адрес
 if ((i+sectorsize) > len) {
   blklen=len-i;
   *((unsigned short*)&cmdbuf[5])=blklen;  //вписываем длину
 }  
 iolen=send_cmd_massdata(cmdbuf,7,iobuf,blklen+8);
 if (iolen <(blklen+8)) {
   printf("\n Ошибка в процессе обработки команды 03, требуется %i байт, получено %i\n",blklen,iolen);
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

if (bad_loader) return 0;  // непатченный загрузчик - запись невозможна
*((unsigned int*)&cmdbuf[2])=adr;  //вписываем адрес
*((unsigned int*)&cmdbuf[6])=data;  //вписываем данные

iolen=send_cmd(cmdbuf,10,iobuf);

if (iobuf[2] != 06) {
  printf("\n Загрузчик не поддерживает команду 05:\n");
  printf("\n cmdbuf: "); dump(cmdbuf,10,0);
  printf("\n ответ : "); dump(iobuf,iolen,0);
  printf("\n");
  exit(1);
}  
  return 1;
}


//*************************************88
//* Установка адресных регистров 
//*************************************
void setaddr(int block, int page) {

int adr;  
  
adr=block*ppb+page;
mempoke(nand_addr0,adr<<16);         // младшая часть адреса. 16 бит column address равны 0
mempoke(nand_addr1,(adr>>16)&0xff);  // единственный байт старшей части адреса
}



//*********************************************
//* Сброс контроллера NAND
//*********************************************
void nand_reset() {

mempoke(nand_cmd,1); // Сброс всех операций контроллера
mempoke(nand_exec,0x1);
nandwait();
}

//*********************************************
//* Чтение сектора флешки по указанному адресу 
//*********************************************

int flash_read(int block, int page, int sect) {
  
unsigned char iobuf[300];
int iolen;
int i;

//mempoke(nand_cfg1,0x6745d); // ECC off
//mempoke(nand_cs,0); // data mover
nand_reset();
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
//*
//* mode=0 - автоопределение нужности hello
//* mode=1 - принудительный запуск hello
//* mode=2 - принудительный запуск hello без настройки конфигурации 
//**********************************************8
void hello(int mode) {

int i;  
unsigned char rbuf[1024];
char hellocmd[]="\x01QCOM fast download protocol host\x03### ";
unsigned char cmdbuf[]={3,0,0,0,0,4,0};
unsigned int cfg1,ecccfg;

*((unsigned int*)&cmdbuf[1])=sector_buf;  //вписываем адрес секторного буфера - он безопасный.

if (mode == 0) {
  i=send_cmd(cmdbuf,7,rbuf);
  ttyflush(); 

  // Проверяем, не инициализировался ли загрузчик ранее
  if (i == 13) {
     get_flash_config();
     return;
  }  
}  

printf(" Отсылка hello...");
i=send_cmd(hellocmd,strlen(hellocmd),rbuf);
if (rbuf[1] != 2) {
   printf(" hello возвратил ошибку!\n");
   dump(rbuf,i,0);
   return;
}  
 printf("ok");
 if (mode == 2) {
   // тихий запуск - обходим настройку чипсета
   printf("\n");
   return; 
 }  
//dump(rbuf,i,0);

if (!test_loader()) {
  printf("\n ! ** Загрузчик не поддерживает команду 05 - загрузчик не содержит патча!!!! **\n");
  return;
}

if (nand_cmd == 0xf9af0000) disable_bam(); // отключаем NANDc BAM, если работаем с 9x25
cfg1=mempeek(nand_cfg1);
ecccfg=mempeek(nand_ecc_cfg);
get_flash_config();
i=rbuf[0x2c];
rbuf[0x2d+i]=0;
printf("\n Флеш-память: %s %s, %s",flash_mfr,rbuf+0x2d,flash_descr);
printf("\n Версия протокола: %i",rbuf[0x22]);
printf("\n Максимальный размер пакета: %i байта",*((unsigned int*)&rbuf[0x24]));
printf("\n Размер сектора: %i байт",sectorsize);
printf("\n Размер страницы: %i байт (%i секторов)",pagesize,spp);
printf("\n Размер OOB: %i байт",oobsize);
printf("\n Тип ECC: %s",(cfg1&(1<<27))?"BCH":"R-S");
printf(", %i бит",(cfg1&(1<<27))?(((ecccfg>>4)&3)?(((ecccfg>>4)&3)+1)*4:4):4);
printf("\n Общий размер флеш-памяти = %i блоков (%i MB)",maxblock,maxblock*ppb/1024*pagesize/1024);
printf("\n");
}

//*************************************
//* чтение таблицы разделов из flash
//*************************************
void load_ptable(unsigned char* buf) {

unsigned int udsize=512;
unsigned int blk,pg;

if (chipset[chip_type].udflag) udsize=516;

memset(buf,0,1024); // обнуляем таблицу

for (blk=0;blk<12;blk++) {
  // Ищем блок с картами
  flash_read(blk, 0, 0);     
  memread(buf,sector_buf, udsize);
  if (memcmp(buf,"\xac\x9f\x56\xfe\x7a\x12\x7f\xcd",8) != 0) continue; // сигнатура не найдена - ищем дальше

  // нашли блок с таблицами - теперь ищем страницу с таблицей чтения
  for (pg=0;pg<ppb;pg++) {
    flash_read(blk, pg, 0);     
    memread(buf,sector_buf, udsize);
    if (memcmp(buf,"\xAA\x73\xEE\x55\xDB\xBD\x5E\xE3",8) != 0) continue; // сигнатура таблицы чтения не найдена
    // нашли таблицу - читаем хвост
    mempoke(nand_exec,1);     // сектор 1 - продолжение таблицы
    nandwait();
    memread(buf+udsize,sector_buf, udsize);
    return; // все - таблица найдна, более тут делать нечего
  }
}  
printf("\n Таблица разделов не найдена. Завершаем работу.");
exit(1);
}

//*****************************************************
//*  Обработка отлупов загрузчика
//* 
//* descr - имя процедуры, посылающей командный пакет
//*****************************************************

void show_errpacket(char* descr, char* pktbuf, int len) {
  
char iobuf[2048];
int iolen,i;
  
if (len == 0) return;
printf("\n! %s вернул ошибку: ",descr);  
if (pktbuf[1] == 0x0e) {
  // текстовый отлуп - печатаем его
  pktbuf[len-4]=0;
  puts(pktbuf+2);
  iolen=receive_reply(iobuf,0);
  if (iolen != 0) {
      i=*((unsigned int*)&iobuf[2]);
      printf("Код ошибки = %08x\n\n",i);
  }
}
else {
  dump(pktbuf,len,0);
}
}

//**********************************
//* Закрытие потока данных раздела
//**********************************
int qclose(int errmode) {
unsigned char iobuf[600];
unsigned char cmdbuf[]={0x15};
int iolen;

iolen=send_cmd(cmdbuf,1,iobuf);
if (!errmode) return 1;
if (iobuf[1] == 0x16) return 1;
show_errpacket("close()",iobuf,iolen);
return 0;

}  

//************************
//* Стирание блока флешки  
//************************

int block_erase(int block) {
  
int oldcfg;  
  
mempoke(nand_addr0,block*ppb);         // младшая часть адреса - # страницы
mempoke(nand_addr1,0);                 // старшая часть адреса - всегда 0

oldcfg=mempeek(nand_cfg0);
mempoke(nand_cfg0,oldcfg&~(0x1c0));    // устанавливаем CW_PER_PAGE=0, как требует даташит

mempoke(nand_cmd,0x3a); // стирание. Бит Last page установлен
mempoke(nand_exec,0x1);
nandwait();
mempoke(nand_cfg0,oldcfg);   // восстанавливаем CFG0
}

//**********************************************************
//*  Получение параметров формата флешки из контроллера
//**********************************************************
void get_flash_config() {
  
unsigned int cfg0, cfg1, nandid, pid, fid, blocksize, devcfg, chipsize;
int i;

struct {
  char* type;   // тектовое описание типа
  unsigned int id;      // ID флешки 
  unsigned int chipsize; // размер флешки в мегабайтах
} nand_ids[]= {

	{"NAND 16MiB 1,8V 8-bit",	0x33, 16},
	{"NAND 16MiB 3,3V 8-bit",	0x73, 16}, 
	{"NAND 16MiB 1,8V 16-bit",	0x43, 16}, 
	{"NAND 16MiB 3,3V 16-bit",	0x53, 16}, 

	{"NAND 32MiB 1,8V 8-bit",	0x35, 32},
	{"NAND 32MiB 3,3V 8-bit",	0x75, 32},
	{"NAND 32MiB 1,8V 16-bit",	0x45, 32},
	{"NAND 32MiB 3,3V 16-bit",	0x55, 32},

	{"NAND 64MiB 1,8V 8-bit",	0x36, 64},
	{"NAND 64MiB 3,3V 8-bit",	0x76, 64},
	{"NAND 64MiB 1,8V 16-bit",	0x46, 64},
	{"NAND 64MiB 3,3V 16-bit",	0x56, 64},

	{"NAND 128MiB 1,8V 8-bit",	0x78, 128},
	{"NAND 128MiB 1,8V 8-bit",	0x39, 128},
	{"NAND 128MiB 3,3V 8-bit",	0x79, 128},
	{"NAND 128MiB 1,8V 16-bit",	0x72, 128},
	{"NAND 128MiB 1,8V 16-bit",	0x49, 128},
	{"NAND 128MiB 3,3V 16-bit",	0x74, 128},
	{"NAND 128MiB 3,3V 16-bit",	0x59, 128},

	{"NAND 256MiB 3,3V 8-bit",	0x71, 256},

	/*512 Megabit */
	{"NAND 64MiB 1,8V 8-bit",	0xA2, 64},   
	{"NAND 64MiB 1,8V 8-bit",	0xA0, 64},
	{"NAND 64MiB 3,3V 8-bit",	0xF2, 64},
	{"NAND 64MiB 3,3V 8-bit",	0xD0, 64},
	{"NAND 64MiB 1,8V 16-bit",	0xB2, 64},
	{"NAND 64MiB 1,8V 16-bit",	0xB0, 64},
	{"NAND 64MiB 3,3V 16-bit",	0xC2, 64},
	{"NAND 64MiB 3,3V 16-bit",	0xC0, 64},

	/* 1 Gigabit */
	{"NAND 128MiB 1,8V 8-bit",	0xA1,128},
	{"NAND 128MiB 3,3V 8-bit",	0xF1,128},
	{"NAND 128MiB 3,3V 8-bit",	0xD1,128},
	{"NAND 128MiB 1,8V 16-bit",	0xB1,128},
	{"NAND 128MiB 3,3V 16-bit",	0xC1,128},
	{"NAND 128MiB 1,8V 16-bit",     0xAD,128},

	/* 2 Gigabit */
	{"NAND 256MiB 1.8V 8-bit",	0xAA,256},
	{"NAND 256MiB 3.3V 8-bit",	0xDA,256},
	{"NAND 256MiB 1.8V 16-bit",	0xBA,256},
	{"NAND 256MiB 3.3V 16-bit",	0xCA,256},

	/* 4 Gigabit */
	{"NAND 512MiB 1.8V 8-bit",	0xAC,512},
	{"NAND 512MiB 3.3V 8-bit",	0xDC,512},
	{"NAND 512MiB 1.8V 16-bit",	0xBC,512},
	{"NAND 512MiB 3.3V 16-bit",	0xCC,512},

	/* 8 Gigabit */
	{"NAND 1GiB 1.8V 8-bit",	0xA3,1024},
	{"NAND 1GiB 3.3V 8-bit",	0xD3,1024},
	{"NAND 1GiB 1.8V 16-bit",	0xB3,1024},
	{"NAND 1GiB 3.3V 16-bit",	0xC3,1024},

	/* 16 Gigabit */
	{"NAND 2GiB 1.8V 8-bit",	0xA5,2048},
	{"NAND 2GiB 3.3V 8-bit",	0xD5,2048},
	{"NAND 2GiB 1.8V 16-bit",	0xB5,2048},
	{"NAND 2GiB 3.3V 16-bit",	0xC5,2048},

	/* 32 Gigabit */
	{"NAND 4GiB 1.8V 8-bit",	0xA7,4096},
	{"NAND 4GiB 3.3V 8-bit",	0xD7,4096},
	{"NAND 4GiB 1.8V 16-bit",	0xB7,4096},
	{"NAND 4GiB 3.3V 16-bit",	0xC7,4096},

	/* 64 Gigabit */
	{"NAND 8GiB 1.8V 8-bit",	0xAE,8192},
	{"NAND 8GiB 3.3V 8-bit",	0xDE,8192},
	{"NAND 8GiB 1.8V 16-bit",	0xBE,8192},
	{"NAND 8GiB 3.3V 16-bit",	0xCE,8192},

	/* 128 Gigabit */
	{"NAND 16GiB 1.8V 8-bit",	0x1A,16384},
	{"NAND 16GiB 3.3V 8-bit",	0x3A,16384},
	{"NAND 16GiB 1.8V 16-bit",	0x2A,16384},
	{"NAND 16GiB 3.3V 16-bit",	0x4A,16384},
                                                  
	/* 256 Gigabit */
	{"NAND 32GiB 1.8V 8-bit",	0x1C,32768},
	{"NAND 32GiB 3.3V 8-bit",	0x3C,32768},
	{"NAND 32GiB 1.8V 16-bit",	0x2C,32768},
	{"NAND 32GiB 3.3V 16-bit",	0x4C,32768},

	/* 512 Gigabit */
	{"NAND 64GiB 1.8V 8-bit",	0x1E,65536},
	{"NAND 64GiB 3.3V 8-bit",	0x3E,65536},
	{"NAND 64GiB 1.8V 16-bit",	0x2E,65536},
	{"NAND 64GiB 3.3V 16-bit",	0x4E,65536},
	{0,0,0},
};


struct  {
  unsigned int id;
  char* name;
}  nand_manuf_ids[] = {
	{0x98, "Toshiba"},
	{0xec, "Samsung"},
	{0x04, "Fujitsu"},
	{0x8f, "National"},
	{0x07, "Renesas"},
	{0x20, "ST Micro"},
	{0xad, "Hynix"},
	{0x2c, "Micron"},
	{0xc8, "Elite Semiconductor"},
	{0x01, "Spansion/AMD"},
	{0x0, 0}
};

mempoke(nand_cmd,0x8000b); // команда Extended Fetch ID
mempoke(nand_exec,1); 
nandwait();
nandid=mempeek(NAND_FLASH_READ_ID); // получаем ID флешки
chipsize=0;

fid=(nandid>>8)&0xff;
pid=nandid&0xff;

// Определяем производителя флешки
i=0;
while (nand_manuf_ids[i].id != 0) {
	if (nand_manuf_ids[i].id == pid) {
	strcpy(flash_mfr,nand_manuf_ids[i].name);
	break;
	}
i++;
}  
    
// Определяем емкость флешки
i=0;
while (nand_ids[i].id != 0) {
if (nand_ids[i].id == fid) {
	chipsize=nand_ids[i].chipsize;
	strcpy(flash_descr,nand_ids[i].type);
	break;
	}
i++;
}  
if (chipsize == 0) {
	printf("\n Неопределенный Flash ID = %02x",fid);
}  

// Вынимаем параметры конфигурации

sectorsize=512;
devcfg = (nandid>>24) & 0xff;
pagesize = 1024 << (devcfg & 0x3); // размер страницы в байтах
blocksize = 64 << ((devcfg >> 4) & 0x3);  // размер блока в килобайтах
spp = pagesize/sectorsize; // секторов в странице

cfg0=mempeek(nand_cfg0);
if ((((cfg0>>6)&7)|((cfg0>>2)&8)) == 0) {
  // для старых чипсетов младшие 2 байта CFG0 надо настраивать руками
  if (!bad_loader) mempoke(nand_cfg0,(cfg0|0x40000|(((spp-1)&8)<<2)|(((spp-1)&7)<<6)));
}  

// Для чипсета 8200 требуется настройка конфигурации 1 - позиции маркера бедблока
if (nand_cmd == 0xa0a00000) {
  cfg1=mempeek(nand_cfg1);
  cfg1&=~(0x7ff<<6);  //сбрасываем BAD_BLOCK_BYTE_NUM и BAD_BLOCK_IN_SPARE_AREA
  cfg1|=(0x1d1<<6); // BAD_BLOCK_BYTE_NUM
  mempoke(nand_cfg1,cfg1);
}  

if (chipsize != 0)   maxblock=chipsize*1024/blocksize;
else                 maxblock=0x800;

if (oobsize == 0) {
	// Micron MT29F4G08ABBEA3W: на самом деле 224, определяется 128, 
	// реально используется 160, для raw-режима нагляднее 256 :)
	if (nandid == 0x2690ac2c) oobsize = 256; 
	else oobsize = (8 << ((devcfg >> 2) & 0x1)) * (pagesize >> 9);
} 

}

//****************************************
//* Загрузка через сахару
//****************************************
int dload_sahara() {

FILE* in;
char* infilename;
unsigned char sendbuf[131072];
unsigned char replybuf[128];
unsigned int iolen,offset,len,donestat,imgid;
unsigned char helloreply[60]={
 02, 00, 00, 00, 48, 00, 00, 00, 02, 00, 00, 00, 01, 00, 00, 00,
 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00,
 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00
}; 
unsigned char donemes[8]={5,0,0,0,8,0,0,0};

printf("\n Ожидаем пакет Hello от устройства...\n");
port_timeout(100); // пакета Hello будем ждать 10 секунд
iolen=read(siofd,replybuf,48);  // читаем Hello
if ((iolen != 48)||(replybuf[0] != 1)) {
	sendbuf[0]=0x3a; // может быть любое число
	write(siofd,sendbuf,1); // инициируем отправку пакета Hello 
	iolen=read(siofd,replybuf,48);  // пробуем читать Hello ещё раз
	if ((iolen != 48)||(replybuf[0] != 1)) { // теперь всё - больше ждать нечего
		printf("\n Пакет Hello от устройства не получен\n");
		dump(replybuf,iolen,0);
		return 1;
	}
}

// Получили Hello, 
ttyflush();  // очищаем буфер приема
port_timeout(10); // теперь обмен пакетами пойдет быстрее - таймаут 1 с
write(siofd,helloreply,48);   // отправляем Hello Response с переключением режима
iolen=read(siofd,replybuf,20); // ответный пакет
  if (iolen == 0) {
    printf("\n Нет ответа от устройства\n");
    return 1;
  }  
// в replybuf должен быть запрос первого блока загрузчика
imgid=*((unsigned int*)&replybuf[8]); // идентификатор образа
printf("\n Идентификатор образа для загрузки: %08x\n",imgid);
switch (imgid) {

	case 0x07:
	infilename="loaders/NPRG9x25p.bin";
	break;

	case 0x0d:
	infilename="loaders/ENPRG9x25p.bin";
	break;

	default:
	printf("\n Неизвестный идентификатор - нет такого образа!\n");
	return 1;
}
printf("\n Загружаем %s...\n",infilename); 
in=fopen(infilename,"rb");
if (in == 0) {
  printf("\n Ошибка открытия входного файла\n");
  return 1;
}

// Основной цикл передачи кода загрузчика
printf("\n Передаём загрузчик в устройство...\n");
while(replybuf[0] != 4) { // сообщение EOIT
 if (replybuf[0] != 3) { // сообщение Read Data
    printf("\n Пакет с недопустимым кодом - прерываем загрузку!");
    dump(replybuf,iolen,0);
    return 1;
 }
  // выделяем параметры фрагмента файла
  offset=*((unsigned int*)&replybuf[12]);
  len=*((unsigned int*)&replybuf[16]);
//  printf("\n адрес=%08x длина=%08x",offset,len);
  fseek(in,offset,SEEK_SET);
  fread(sendbuf,1,len,in);
  // отправляем блок данных сахаре
  write(siofd,sendbuf,len);
  // получаем ответ
  iolen=read(siofd,replybuf,20);      // ответный пакет
  if (iolen == 0) {
    printf("\n Нет ответа от устройства\n");
    return 1;
  }  
}
// получили EOIT, конец загрузки
write(siofd,donemes,8);   // отправляем пакет Done
iolen=read(siofd,replybuf,12); // ожидаем Done Response
if (iolen == 0) {
  printf("\n Нет ответа от устройства\n");
  return 1;
} 
// получаем статус
donestat=*((unsigned int*)&replybuf[12]); 
if (donestat == 0) {
  printf("\n Загрузчик запущен успешно\n");
} else {
  printf("\n Ошибка запуска загрузчика\n");
}
return donestat;

}

//****************************************
//* Отключение NANDc BAM
//****************************************
void disable_bam() {

unsigned int i,nandcstate[256];

for (i=0;i<0xec;i+=4) nandcstate[i]=mempeek(nand_cmd+i); // сохраняем состояние контроллера NAND
mempoke(0xfc401a40,1); // GCC_QPIC_BCR
mempoke(0xfc401a40,0); // полный асинхронный сброс QPIC
for (i=0;i<0xec;i+=4) mempoke(nand_cmd+i,nandcstate[i]);  // восстанавливаем состояние
mempoke(nand_exec,1); // фиктивное чтение для снятия защиты адресных регистров контроллера от записи
}


//****************************************************
//* Проверка массива на ненулевые значения
//*
//*  0 - в массиве одни нули
//*  1 - в массиве есть ненули
//****************************************************
int test_zero(unsigned char* buf, int len) {
  
int i;
for (i=0;i<len;i++)
  if (buf[i] != 0) return 1;
return 0;
}

