## CC=clang

# Size of dictionary...
#

# MAX_DICT_CELLS=0xffff		# 128KB
MAX_DICT_CELLS=0x7fff		# 64KB
# MAX_DICT_CELLS=0x3fff		# 32KB
# MAX_DICT_CELLS=0x1fff		# 16KB
# MAX_DICT_CELLS=0xfff		# 8KB

# TOTAL_RAM_CELLS=4096		# 16KB
TOTAL_RAM_CELLS=2048		# 8KB
# TOTAL_RAM_CELLS=1024		# 4KB

CFLAGS=-Wall -O -DMAX_DICT_CELLS=$(MAX_DICT_CELLS) -DTOTAL_RAM_CELLS=$(TOTAL_RAM_CELLS)
LDFLAGS= -O

tbforth-posix: tbforth-posix.o tbforth.o
	$(CC) $(CFLAGS) -o tbforth-posix tbforth-posix.o  tbforth.o $(LDFLAGS) -lreadline -lm
	echo "save-image tbforth.img" | ./tbforth-posix

tbforth.o: tbforth.c tbforth.h
tbforth-posix.o: tbforth.h


arduino-stage: tbforth-posix
#	sed 's/TOTAL_RAM_CELLS\s+(.+)/TOTAL_RAM_CELLS $(TOTAL_RAM_CELLS)/' tbforth.h > /tmp/foo
	cp tbforth.img.h tbforth.c tbforth.h arduino/esp32/toolboxforth
	cp tbforth.img.h tbforth.c tbforth.h arduino/generic/toolboxforth
	cp tbforth.img.h tbforth.c tbforth.h arduino/rp-pico/toolboxforth

clean:
	-rm -f tbforth.img* *.o *.exe *~ *.stackdump *.aft-TOC tbforth-posix
