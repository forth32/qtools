CFLAGS ?= -g -O2
override CFLAGS += -Wno-unused-result -Wunused

OBJS     = hdlc.o  qcio.o memio.o chipconfig.o

.PHONY: all clean

all:    qcommand qrmem qrflash qdload mibibsplit qwflash qwdirect qefs qnvram qblinfo qident qterminal qbadblock qflashparm

clean:
	rm *.o
	rm $(all)

#.c.o:
#	$(CC) -o $@ $(LIBS) $^ qcio.o

qcio.o: qcio.c
hdlc.o: hdlc.c
sahara.o: sahara.c
chipconfig.o: chipconfig.c
memio.o: memio.c
ptable.o: ptable.c

qcommand: qcommand.o  $(OBJS)

qrmem: qrmem.o $(OBJS)

qrflash: qrflash.o $(OBJS) ptable.o

qwflash: qwflash.o $(OBJS)

#qwimage: qwimage.o $(OBJS)

qdload: qdload.o sahara.o $(OBJS)  ptable.o

qwdirect: qwdirect.o $(OBJS)  ptable.o

qefs  : qefs.o efsio.o $(OBJS)

qnvram  : qnvram.o $(OBJS)

mibibsplit: mibibsplit.o $(OBJS)

qblinfo:    qblinfo.o $(OBJS)

qident:      qident.o $(OBJS)

qterminal:   qterminal.o $(OBJS)

qbadblock:   qbadblock.o $(OBJS)  ptable.o

qflashparm:  qflashparm.o $(OBJS)

qterminal qcommand: override LDLIBS+=-lreadline -lncurses
