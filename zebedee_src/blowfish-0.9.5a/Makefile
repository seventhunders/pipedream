# $Id: Makefile,v 1.1 2000/09/28 21:30:11 nwinton Exp nwinton $

CC=	gcc
CPP=	$(CC) -E
INCLUDES=-I.

CFLAG=-O3

# Run bf_opts (or "make optimize") to determine the best configuration for
# your platform. It could be -DBF_PTR, -DBF_PTR2 or -DBF_DEFAULT_OPTIONS.

OPT=

MAKE=		make -f Makefile
MAKEFILE=	Makefile
AR=		ar r
RANLIB=		ranlib

CFLAGS= $(INCLUDES) $(OPT) $(CFLAG)

LIB=libblowfish.a
LIBSRC=bf_skey.c bf_ecb.c bf_enc.c bf_cfb64.c bf_ofb64.c 
LIBOBJ=bf_skey.o bf_ecb.o bf_enc.o bf_cfb64.o bf_ofb64.o

SRC= $(LIBSRC)

EXHEADER= blowfish.h
HEADER=	bf_pi.h bf_locl.h $(EXHEADER)

all:	lib

lib:	$(LIBOBJ)
	$(AR) $(LIB) $(LIBOBJ)
	$(RANLIB) $(LIB)
	@touch lib

tags:
	ctags $(SRC)

tests: bftest bfspeed
	./bftest
	./bfspeed

bftest: bftest.o
	$(CC) -o bftest bftest.o $(LIB)

bfspeed: bfspeed.o
	$(CC) -o bfspeed bfspeed.o $(LIB)

optimize: lib bf_opts
	./bf_opts | tail -1 > opts
	$(MAKE) clean
	$(MAKE) OPT=`cat opts`
	rm -f opts

bf_opts: bf_opts.o
	$(CC) -o bf_opts bf_opts.o $(LIB)

lint:
	lint -DLINT $(INCLUDES) $(SRC)>fluff

clean:
	rm -f $(LIB) bftest bf_opts *.o *.obj lib tags core .pure .nfs* *.old *.bak fluff
