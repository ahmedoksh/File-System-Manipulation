CC = gcc
CFLAGS = -Wall

all: diskinfo disklist diskget diskput

diskinfo: main.c
	$(CC) $(CFLAGS) -DDISKINFO -o diskinfo main.c

disklist: main.c
	$(CC) $(CFLAGS) -DDISKLIST -o disklist main.c

diskget: main.c
	$(CC) $(CFLAGS) -DDISKGET -o diskget main.c

diskput: main.c
	$(CC) $(CFLAGS) -DDISKPUT -o diskput main.c

clean:
	rm -f diskinfo disklist diskget diskput