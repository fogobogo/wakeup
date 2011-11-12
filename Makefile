CC=gcc
CFLAGS=-Wall -Os
LDFLAGS=-lrt

PREFIX=/usr/local
BINDIR=/bin

SOURCES=wakeup.c
OBJECTS=$(SOURCES:.c=.o)
EXECUTABLE=wakeup

all : obj bin

bin :
	$(CC) $(OBJECTS) -o $(EXECUTABLE) $(CFLAGS) $(LDFLAGS)

obj:
	$(CC) -c $(CFLAGS) $(SOURCES) -o $(OBJECTS)

install: 
	install -d $(DESTDIR)$(PREFIX)$(BINDIR) # create directory if nonexistant 
	install $(EXECUTABLE) $(DESTDIR)$(PREFIX)$(BINDIR)

clean:
	rm $(EXECUTABLE) $(OBJECTS)
