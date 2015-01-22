CC       = gcc
LIBS     = -lreadline
CFLAGS   = -O2 -g

.PHONY: all clean

all:    qcommand qread qrflash

clean: 
	rm *.o
	rm qcommand

qcommand: qcommand.o qcio.o
	@gcc $^ -o $@ $(LIBS)

qread: qread.o qcio.o
	@gcc $^ -o $@ $(LIBS)

qrflash: qrflash.o qcio.o
	@gcc $^ -o $@ $(LIBS)
