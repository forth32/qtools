#include "include.h"

//--------------------------------------------------------------------------------
//* Терминальная программа для работы с командными портами модемов
//--------------------------------------------------------------------------------

unsigned int hexflag=0;         // hex-флаг
unsigned int wrapperlen=0;      // размер строки (0 - без заворота строк)
unsigned int waittime=1;        // время ожидания ответа
unsigned int monitorflag=0;     // режим монитора
unsigned int autoflag=1;        // режим автодобавления АТ
char outcmd[500];
char ibuf[6000];

//*****************************************************
//*   Получение и пеать ответа модема
//*****************************************************
void read_responce() {

int i;
int dlen=0;  // полная длина ответа в буфере
int rlen;    // длина полученной части ответа
int clen;    // длина фрагмена ответа шириной в строку терминала 

// цикл получения частей ответа и сборки ответа в единый буфер

do {
//  usleep(waittime*100); // задержка ожидания ответа - не нужна, termios сам ее отрабатывает
  rlen=read(siofd,ibuf+dlen,5000);   // ответ команды
  if ((dlen+rlen) >= 5000) break; // переполнение буфера
  dlen+=rlen;
} while (rlen != 0);
  
  
if (dlen == 0) return; // ответа нет

// Получен ответ - выводим его на экран
if (hexflag) {
  printf("\n");rlen=-1;
  dump(ibuf,dlen,0);
  printf("\n");
}
else {
  ibuf[dlen]=0; // конец строки
  printf("\n");
  if (wrapperlen == 0) puts(ibuf);
  else {
    clen=wrapperlen;
    for(i=0;i<dlen;i+=wrapperlen) {
       if ((dlen-i) < wrapperlen) clen=dlen-i; // длина последней строки
       fwrite(ibuf+i,1,clen,stdout);
       printf("\n");
       fflush(stdout);
    }
  }
}  
fflush(stdout);
}

//*****************************************************
//*  Отсылка команды в модем и получение результата   *
//*****************************************************
void process_command(char* cmdline) {

outcmd[0]=0;

// автодобавление префикса АТ
if ( autoflag &&
    (((cmdline[0] != 'a') && (cmdline[0] != 'A')) ||
    ((cmdline[1] != 't') && (cmdline[1] != 'T') && (cmdline[1] != '/'))) 
   )  strcpy(outcmd,"AT");
strcat(outcmd,cmdline);
strcat(outcmd,"\r");   // добавляем CR в конец строки

// отправка команды
ttyflush();  // очистка выходного буфера
write(siofd,outcmd,strlen(outcmd));  // отсылка команды
// 
read_responce();
}

//@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@

void main(int argc,char* argv[]) {
  
#ifndef WIN32
char* line;
char oldcmdline[200]="";
#else
char line[200];
#endif
char scmdline[200]={0};
#ifndef WIN32
char devname[50]="/dev/ttyUSB0";
#else
char devname[50]="";
#endif
int opt;

while ((opt = getopt(argc, argv, "p:xw:c:hd:ma")) != -1) {
  switch (opt) {
   case 'h': 
     printf("\nТерминальная программа для ввода АТ-команд в модем\n\n\
Допустимы следующие ключи:\n\n\
-p <tty>       - указывает имя устройства последовательного порта\n\
-d <time>      - задает время ожидания ответа модема в ms\n\
-x             - выводит ответ модема в виде HEX-дампа\n\
-w <len>       - длина строки в режиме заворота длинных строк (0 - заворота нет)\n\
-m             - режим монитора порта\n\
-a             - запретить автодобавление букв AT в начало команды\n\
-c \"<команда>\" - запускает указанную команду и завершает работу\n");
    return;
     
   case 'p':
    strcpy(devname,optarg);
    break;

   case 'c':
     strcpy(scmdline,optarg);
     break;

   case 'd':
     sscanf(optarg,"%i",&waittime);
     break;
     
   case 'w':
     sscanf(optarg,"%i",&wrapperlen);
     break;

   case 'x':
     hexflag=1;
     break;
     
   case 'a':
     autoflag=0;
     break;
     
   case 'm':
     monitorflag=1;
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

// настрока таймаута порта
port_timeout(waittime);

// режим монитора
if (monitorflag) 
  for (;;) read_responce();

// запуск команды из ключа -C, если есть
if (strlen(scmdline) != 0) {
  process_command(scmdline);
  return;
}
 
// Основной цикл обработки команд
#ifndef WIN32
 // загрузка истории команд
 read_history("qcommand.history");
 write_history("qcommand.history");
#endif 

for(;;)  {
#ifndef WIN32
 line=readline(">");
#else
 printf(">");
 fgets(line, sizeof(line), stdin);
#endif
 if (line == 0) {
    printf("\n");
    return;
 }   
 if (strlen(line) == 0) continue; // пустая команда
#ifndef WIN32
 if (strcmp(line,oldcmdline) != 0) {
   add_history(line); // в буфер ее для истории
   append_history(1,"qcommand.history");
   strcpy(oldcmdline,line);
 }  
#endif
 process_command(line);
#ifndef WIN32
 free(line);
#endif
} 
}
