CFLAGS  := -Wall -Wextra -pedantic -Os $(CFLAGS)
LDFLAGS := -lrt $(LDFLAGS)

ifndef PREFIX
PREFIX   = /usr/local
endif

ifndef BINDIR
BINDIR   = /bin
endif

SOURCES     = wakeup.c
OBJECTS     = $(SOURCES:.c=.o)
EXECUTABLE  = wakeup

all: $(EXECUTABLE)

install: all
	install -d $(DESTDIR)$(PREFIX)$(BINDIR) # create directory if nonexistant 
	install $(EXECUTABLE) $(DESTDIR)$(PREFIX)$(BINDIR)

clean:
	$(RM) $(EXECUTABLE) $(OBJECTS)
