CFLAGS	= 	-O2 -Wall -Wextra -Werror
OFILES	= 	main.o spad.o

.PHONY:		all clean debug
all:		spad

spad:	$(OFILES)
	$(CC) $(CFLAGS) -o $@ $(OFILES) -lusb-1.0

spad.o:		spad.c spad.h
main.o:		spad.h main.c

clean:
	-rm spad $(OFILES) *~

debug:		clean
	$(MAKE) CFLAGS=-g
