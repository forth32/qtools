@
@  Патч команды 11 для загрузчиков, работающих в режиме Thumb-2
@
.syntax unified 
.code 16           @  Thumb-2 

pkt_data_len_off=10
		.ORG		0x0011607c
leavecmd:

		.ORG		0x00116110
cmd_11_exec:
		PUSH		{R4,LR}
		LDR		R1, cmd_reply_code_ptr 	     @ Адрес ответнго буфера
		ADD		R0, R0,	#8	             @ R0 теперь показывает на байт +4 от начала
							     @ командного буфера
		MCR 		p15, 0, R1,c7,c5,0           @ сброс I-кеша
		BLX		R0               	     @ передаем туда управление
		LDR		R0, cmd_processor_data_ptr   @  Структура данных командного обработчика
		STRH		R4, [R0,#pkt_data_len_off]   @ сохраняем размер ответного буфера
		B		leavecmd
@ блок идентификации
                .word 		0xdeadbeef   @ сигнатура
                .byte           3            @ код чипсета
		
		.ORG  0x00116140
		
cmd_reply_code_ptr:      .word  0
cmd_processor_data_ptr:  .word  0

