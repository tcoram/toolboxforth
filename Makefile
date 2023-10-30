## CC=clang
CFLAGS=-Wall -O
LDFLAGS= -O


tbforth-posix: tbforth-posix.o tbforth.o
	$(CC) $(CFLAGS) -o tbforth-posix tbforth-posix.o  tbforth.o $(LDFLAGS) -lreadline -lm
	echo "save-image tbforth.img" | ./tbforth-posix


tbforth.o: tbforth.c tbforth.h

arduino-stage: tbforth-posix
	cp tbforth.img.h tbforth.h tbforth.c arduino/esp32/toolboxforth
	cp tbforth.img.h tbforth.h tbforth.c arduino/generic/toolboxforth

clean:
	-rm -f tbforth.img* *.o *.exe *~ *.stackdump *.aft-TOC tbforth-posix
