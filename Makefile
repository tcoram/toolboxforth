CC=cc
CFLAGS=-Wall -g
LDFLAGS= -g -lm 


toolboxforth-posix: toolboxforth-posix.o toolboxforth.h
	$(CC) $(CFLAGS) -o toolboxforth-posix toolboxforth-posix.o $(LDFLAGS) -lreadline -lm
	echo "save-image toolboxforth.img" | ./toolboxforth-posix

arduino-stage: toolboxforth-posix
	cp toolboxforth.img.h toolboxforth.h arduino/*/toolboxforth

clean:
	-rm -f toolboxforth.img* *.o *.exe *~ *.stackdump *.aft-TOC toolboxforth-posix
