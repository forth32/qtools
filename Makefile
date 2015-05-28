CC       = gcc
LIBS     = -lreadline
CFLAGS   = -O2 -g -Wno-unused-result

.PHONY: all clean

all:    qcommand qrmem qrflash qdload mibibsplit qwflash qwimage qwdirect qefs qnvram

clean: 
	rm *.o
	rm $(all)

#.c.o:
#	$(CC) -o $@ $(LIBS) $^ qcio.o

qcio.o: qcio.c
#	$(CC) -c qcio.c

qcommand: qcommand.o qcio.o
	gcc $^ -o $@ $(LIBS)

qcommand.o: qcommand.c

qrmem: qrmem.o qcio.o
	gcc $^ -o $@ $(LIBS)

qrflash: qrflash.o qcio.o
	gcc $^ -o $@ $(LIBS)

qwflash: qwflash.o qcio.o
	gcc $^ -o $@ $(LIBS)

qwimage: qwimage.o qcio.o
	gcc $^ -o $@ $(LIBS)

qdload: qdload.o qcio.o
	gcc $^ -o $@ $(LIBS)

qwdirect: qwdirect.o qcio.o
	gcc $^ -o $@ $(LIBS)
	
qefs  : qefs.o qcio.o
	gcc $^ -o $@ $(LIBS)

qnvram  : qnvram.o qcio.o
	gcc $^ -o $@ $(LIBS)
	
mibibsplit: mibibsplit.o qcio.o
	gcc $^ -o $@ $(LIBS)
