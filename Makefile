CC       = gcc
LIBS     = -lreadline
CFLAGS   = -O2 -g -Wno-unused-result

.PHONY: all clean

all:    qcommand qread qrflash qdload mibibsplit qwflash

clean: 
	rm *.o
	rm qcommand qread qrflash qdload mibibsplit

qcommand: qcommand.o qcio.o
	@gcc $^ -o $@ $(LIBS)

qread: qread.o qcio.o
	@gcc $^ -o $@ $(LIBS)

qrflash: qrflash.o qcio.o
	@gcc $^ -o $@ $(LIBS)

qwflash: qwflash.o qcio.o
	@gcc $^ -o $@ $(LIBS)

qdload: qdload.o qcio.o
	@gcc $^ -o $@ $(LIBS)
	
mibibsplit: mibibsplit.o qcio.o
	@gcc $^ -o $@ $(LIBS)
