CFLAGS  := -ansi -Wall -Wextra -pedantic -Os $(CFLAGS) -ggdb
LDFLAGS := -lrt $(LDFLAGS) -pthread

ifndef PREFIX
	PREFIX = /usr/local
endif

ifndef BINDIR
	BINDIR = /bin
endif

SRC      = wakeup.c
OBJ      = $(SRC:.c=.o)
OUT      = wakeup

all: $(OUT)

$(OUT): $(OBJ)
	$(CC) -o $@ $(OBJ) $(LDFLAGS)

install: all
	install -Dm755 $(OUT) $(DESTDIR)$(PREFIX)$(BINDIR)/$(OUT)

clean:
	$(RM) $(OUT) $(OBJ)
