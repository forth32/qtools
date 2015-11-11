#include "include.h"

//****************************************
//* Загрузка через сахару
//****************************************
int dload_sahara() {

FILE* in;
char infilename[200]="loaders/";
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
	  strcat(infilename,get_nprg());
	break;

	case 0x0d:
	  strcat(infilename,get_nprg());
	break;

	default:
          printf("\n Неизвестный идентификатор - нет такого образа!\n");
	return 1;
}
printf("\n Загружаем %s...\n",infilename); fflush(stdout);
in=fopen(infilename,"rb");
if (in == 0) {
  printf("\n Ошибка открытия входного файла %s\n",infilename);
  return 1;
}

// Основной цикл передачи кода загрузчика
printf("\n Передаём загрузчик в устройство...\n");
while(replybuf[0] != 4) { // сообщение EOIT
 if (replybuf[0] != 3) { // сообщение Read Data
    printf("\n Пакет с недопустимым кодом - прерываем загрузку!");
    dump(replybuf,iolen,0);
    fclose(in);
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
    fclose(in);
    return 1;
  }  
}
// получили EOIT, конец загрузки
write(siofd,donemes,8);   // отправляем пакет Done
iolen=read(siofd,replybuf,12); // ожидаем Done Response
if (iolen == 0) {
  printf("\n Нет ответа от устройства\n");
  fclose(in);
  return 1;
} 
// получаем статус
donestat=*((unsigned int*)&replybuf[12]); 
if (donestat == 0) {
  printf("\n Загрузчик передан успешно\n");
} else {
  printf("\n Ошибка передачи загрузчика\n");
}
fclose(in);

return donestat;

}

