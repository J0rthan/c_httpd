CC=gcc
CFLAGS=-Wall -Wextra -O2
SRC=src/main.c src/server.c
OUT=httpd

all: $(OUT)

$(OUT): $(SRC)
	$(CC) $(CFLAGS) -o $@ $(SRC)

clean:
	rm -f $(OUT)