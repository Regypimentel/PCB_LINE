CC     = gcc
CFLAGS = -Wall -Wextra -O2 -pthread
LDLIBS = -lrt -pthread
SRC    = $(wildcard src/*.c)

pcbline: $(SRC) src/common.h
	$(CC) $(CFLAGS) -o $@ $(SRC) $(LDLIBS)

run: pcbline
	./pcbline

race: pcbline
	./pcbline -r -t 4 -n 5

clean:
	rm -f pcbline
