# usarlo per compilare(basta scrivere make sul terminale)
CFLAGS = -D_GNU_SOURCE -Wvla -Wextra -Werror -pedantic

all: alimentatore attivatore atomo master inibitore clear
alimentatore: alimentatore.c util.o
	gcc alimentatore.c util.o -o alimentatore $(CFLAGS)
attivatore: attivatore.c util.o
	gcc attivatore.c util.o -o attivatore $(CFLAGS)
inibitore: inibitore.c util.o
	gcc inibitore.c util.o -o inibitore $(CFLAGS) -lm
atomo: atomo.c util.o
	gcc atomo.c util.o -o atomo $(CFLAGS)
master: master.c util.o
	gcc master.c util.o -o master $(CFLAGS)
util.o: util.c
	gcc util.c -c $(CFLAGS)
clear:
	rm *.o