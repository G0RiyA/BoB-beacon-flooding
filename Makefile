CFLAGS += -Wall -O2

all: beacon-flooding

beacon-flooding: main.c
	$(CC) $(CFLAGS) main.c -o beacon-flooding

clean:
	rm -f beacon-flooding *.o

