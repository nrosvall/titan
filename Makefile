CC=gcc
override CFLAGS+=-std=c99 -Wall -g
PREFIX=/usr/
LIBS=-lcrypto -lsqlite3 -lrt
PROG=titan
OBJS=$(patsubst %.c, %.o, $(wildcard *.c))
HEADERS=$(wildcard *.h)

all: $(PROG)

%.o: %.c $(HEADERS)
	$(CC) $(CFLAGS) -c $< -o $@

$(PROG): $(OBJS)
	$(CC) $(OBJS) $(LIBS) -o $@

clean:
	rm -f *.o
	rm -f $(PROG)

install: all
	if [ ! -d $(PREFIX)/share/man/man1 ];then	\
		mkdir -p $(PREFIX)/share/man/man1;	\
	fi
	cp titan.1 $(PREFIX)/share/man/man1/
	gzip -f $(PREFIX)/share/man/man1/titan.1
	cp titan $(PREFIX)/bin/

uninstall:
	rm $(PREFIX)/bin/titan
	rm $(PREFIX)/share/man/man1/titan.1.gz
