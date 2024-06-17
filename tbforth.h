#include <stdio.h>
#include <stdint.h>
#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

/* Configuration */

#define TBFORTH_VERSION "4.08"
#define DICT_VERSION 22

// Some (minimal) memory protection for ! and dict_write()
//
// #define GUARD_RAILS

/* 
   The Dictionary: Max is 64K words (64KB * 2 bytes). 
   Pick a size suitable for your target.
   This may, depending on your target architecture, live in RAM or Flash.
   Note: A Dictionary CELL is always 2 bytes!
*/
#ifndef MAX_DICT_CELLS
#define MAX_DICT_CELLS 		(0xFFFF)
#endif

/*
 Total user RAM (each ram cell is 4 bytes): includes stacks and (Scratch) PAD
*/
#ifndef TOTAL_RAM_CELLS
#define TOTAL_RAM_CELLS		(4096) /* 16KB */
#endif

#define PAD_SIZE		(1024)  /* bytes */
/* Data Stack and Return Stack sizes (1 cells = 4 byte) */
#define DS_CELLS 		50
#define RS_CELLS 		50

/*
 Input buffer... longest line you can give tbforth to interpret.
*/
#define TIB_SIZE		(40*4)  /* 160 bytes.. RAMC CELLS */


/*
  The dictionary is stored as 16 bit ints, so endianess matters.
*/
#define USE_LITTLE_ENDIAN

// Define this if you want to allow floating point notation (which will
// be converted to FIXED point.
//
#define SUPPORT_FLOAT_FIXED

/*
 Note: A Dictionary CELL is 2 bytes.
*/
typedef uint16_t CELL;

/*
  RAM cell size is 4 bytes.
*/
typedef uint32_t RAMC;


typedef enum { U_OK=0, COMPILING, E_NOT_A_WORD, E_NOT_A_NUM,
  E_STACK_UNDERFLOW, E_RSTACK_OVERFLOW,
  E_DSTACK_OVERFLOW, E_ABORT, E_EXIT } tbforth_stat;

/*
 Abort reasons.
*/
typedef enum { NO_ABORT=0, ABORT_CTRL_C=1, ABORT_NAW=2,
	       ABORT_ILLEGAL=3, ABORT_WORD=4 } abort_t;

extern abort_t _tbforth_abort_request;
#define tbforth_abort_request(why) _tbforth_abort_request = why
#define tbforth_aborting() (_tbforth_abort_request != NO_ABORT)
#define tbforth_abort_reason() _tbforth_abort_request
#define tbforth_abort_clr() (_tbforth_abort_request = NO_ABORT)

char* tbforth_count_str(CELL addr,CELL* new_addr);

extern RAMC tbforth_ram[];
extern struct dict  *dict;

extern void tbforth_init(void);
extern void tbforth_bootstrap(void);
extern void tbforth_load_prims(void);
extern void tbforth_abort(CELL idx);
extern tbforth_stat tbforth_interpret(char*);
extern tbforth_stat c_handle(void);
extern void tbforth_cdef (char*, int);
extern char* tbforth_next_word (void);

/*
 The following structures are overlays on pre-allocated buffers.
*/

struct tbforth_iram {
  RAMC state;		/* 0=interpreting .. */
  RAMC total_ram;		/* Total ram available */
  RAMC compiling_word;	 /* 0=none */
  RAMC tibidx;		      /* current index into buffer */
  RAMC tibwordidx;		/* point to current word in inbufptr */
  RAMC tibwordlen;	/* length of current word in inbufptr */
  RAMC tibclen;		      /* size of data in the tib buffer */
  char tib[TIB_SIZE];		/* input buffer for interpreter */
};

struct tbforth_uram {
  RAMC len;		/* size of URAM */
  RAMC base;			/* numeric base for I/O */
  RAMC fixedp;		/* fixed point places */
  RAMC didx;			/* data stack index */
  RAMC ridx;			/* return stack index */
  RAMC dsize;			/* size of data stack */
  RAMC rsize;			/* size of return stack */
  RAMC ds[];		/* data & return stack */
};

struct dict {
  CELL version;			/* dictionary version number */
  CELL word_size;		/* size of native word */
  CELL max_cells;		/* MAX_DICT_CELLS */
  CELL here;			/* top of dictionary */
  CELL last_word_idx;		/* top word's code token (used for searches) */
  CELL varidx;			/* keep track of next variable slot (neg #) */
  CELL d[MAX_DICT_CELLS];	/* dictionary */
};

/*
 All dictionary writing/updating is captured here.
*/
# define dict_start_def()
# define dict_here() dict->here
# define dict_set_last_word(cell) dict->last_word_idx=cell
# define dict_incr_varidx(n) (dict->varidx += n)
# define dict_incr_here(n) dict->here += n
# define dict_append(cell) tbforth_dict[dict->here] = cell, dict_incr_here(1)
# define dict_write(idx,cell) tbforth_dict[idx] = cell
# define dict_append_string(src,len) { \
    memcpy((char*)((tbforth_dict )+(dict->here)),src,len);			\
    dict->here += (len/BYTES_PER_CELL) + (len % BYTES_PER_CELL); \
}
# define dict_end_def()

extern CELL *tbforth_dict;
extern struct tbforth_iram *tbforth_iram;
extern struct tbforth_uram *tbforth_uram;
/*
 Convenient short-cuts. data stack grows up, return stack grows down
*/

#define dpush(n) (tbforth_uram->ds[1+tbforth_uram->didx] = n, tbforth_uram->didx++)
#define dpop() tbforth_uram->ds[tbforth_uram->didx--]
#define dpick(n) tbforth_uram->ds[tbforth_uram->didx-n]

#define rpush(n) (tbforth_uram->ds[--tbforth_uram->ridx] = n)
#define rpop() tbforth_uram->ds[tbforth_uram->ridx++]
#define rpick(n) tbforth_uram->ds[tbforth_uram->ridx+n]

#define dtop() tbforth_uram->ds[tbforth_uram->didx]
#define dtop2() tbforth_uram->ds[(tbforth_uram->didx)-1]
#define dtop3() tbforth_uram->ds[(tbforth_uram->didx)-2]

extern void tbforth_cdef (char* name, int val);

// Some useful OS extensions...you can handle in c_handle() or not... its up to you
// If you don't have them, just ignore them.
//
enum { OS_EMIT=1, OS_KEY, OS_SAVE_IMAGE, OS_INCLUDE, OS_OPEN, OS_SEEK,OS_CLOSE, OS_DELETE,
  OS_READB, OS_WRITEB, OS_READBUF, OS_WRITEBUF, OS_MS, OS_US, OS_SECS, OS_POLL, OS_TCP_CONN, OS_TCP_DISCONN, OS_RAND};

#define OS_WORDS() \
  tbforth_cdef("secs", OS_SECS); \
  tbforth_cdef("ms", OS_MS); \
  tbforth_cdef("us", OS_US); \
  tbforth_cdef("(emit)", OS_EMIT); \
  tbforth_cdef("poll", OS_POLL); \
  tbforth_cdef("(key)", OS_KEY); \
  tbforth_cdef("save-image", OS_SAVE_IMAGE); \
  tbforth_cdef("include", OS_INCLUDE); \
  tbforth_cdef("open-file", OS_OPEN); \
  tbforth_cdef("seek", OS_SEEK); \
  tbforth_cdef("close-file", OS_CLOSE); \
  tbforth_cdef("delete-file", OS_DELETE); \
  tbforth_cdef("connect-tcp", OS_TCP_CONN); \
  tbforth_cdef("close-tcp", OS_TCP_DISCONN); \
  tbforth_cdef("write-byte", OS_WRITEB); \
  tbforth_cdef("read-byte", OS_READB); \
  tbforth_cdef("write-buf", OS_WRITEBUF); \
  tbforth_cdef("read-buf", OS_READBUF); \
  tbforth_cdef("random-bytes", OS_RAND);

  

// Some useful MCU extensions...you can handle in c_handle() or not... its up to you
//
enum {
  MCU_GPIO_MODE = 100,
  MCU_GPIO_READ,
  MCU_GPIO_WRITE,
  MCU_SPI_CFG,
  MCU_SPI_BEGIN_TRANS,
  MCU_SPI_WRITE,
  MCU_SPI_READ,
  MCU_SPI_END_TRANS,
  MCU_I2C_CFG,
  MCU_I2C_START,
  MCU_I2C_RESTART,
  MCU_I2C_WRITE,
  MCU_I2C_READ,
  MCU_I2C_STOP,
  MCU_UART_BEGIN,
  MCU_UART_END,
  MCU_UART_AVAIL,
  MCU_UART_READ,
  MCU_UART_WRITE,
  MCU_WDT_CONFIG,
  MCU_WDT_RESET,
  MCU_DELAY,
  MCU_GPIO_WAKE,
  MCU_ENCRYPT,
  MCU_DECRYPT,
  MCU_MAC,
  MCU_SLEEP,
  MCU_WAKE_REASON,
  MCU_COLD,
  MCU_RESTART,
  MCU_CPU_FREQUENCY,
  MCU_POKE,
  MCU_PEEK,
  MCU_PWM_FREQ,
  MCU_PWM_RANGE,
  MCU_PWM_DUTY
};

#define MCU_WORDS()		     \
  tbforth_interpret("mark MCU-HAL"); \
  tbforth_cdef("gpio-mode", MCU_GPIO_MODE); \
  tbforth_cdef("gpio-read", MCU_GPIO_READ); \
  tbforth_cdef("gpio-write", MCU_GPIO_WRITE); \
  tbforth_cdef("spi-config", MCU_SPI_CFG); \
  tbforth_cdef("spi\{", MCU_SPI_BEGIN_TRANS); \
  tbforth_cdef("spi!", MCU_SPI_WRITE); \
  tbforth_cdef("spi@", MCU_SPI_READ); \
  tbforth_cdef("}spi", MCU_SPI_END_TRANS); \
  tbforth_cdef("i2c-config", MCU_I2C_CFG); \
  tbforth_cdef("i2c-start", MCU_I2C_START); \
  tbforth_cdef("i2c-restart", MCU_I2C_RESTART); \
  tbforth_cdef("i2c!", MCU_I2C_WRITE); \
  tbforth_cdef("i2c@", MCU_I2C_READ); \
  tbforth_cdef("i2c-stop", MCU_I2C_STOP); \
  tbforth_cdef("uart-begin", MCU_UART_BEGIN); \
  tbforth_cdef("uart-end", MCU_UART_END); \
  tbforth_cdef("uart?", MCU_UART_AVAIL); \
  tbforth_cdef("uart@", MCU_UART_READ); \
  tbforth_cdef("uart!", MCU_UART_WRITE); \
  tbforth_cdef("/wdt", MCU_WDT_CONFIG); \
  tbforth_cdef("wdt-rst", MCU_WDT_RESET); \
  tbforth_cdef("delay", MCU_DELAY); \
  tbforth_cdef("sleep", MCU_SLEEP); \
  tbforth_cdef("gcm-encrypt", MCU_ENCRYPT); \
  tbforth_cdef("gcm-decrypt", MCU_DECRYPT); \
  tbforth_cdef("mac", MCU_MAC); \
  tbforth_cdef("wake-reason", MCU_WAKE_REASON); \
  tbforth_cdef("cold", MCU_COLD); \
  tbforth_cdef("restart", MCU_RESTART); \
  tbforth_cdef("cpufreq", MCU_CPU_FREQUENCY);	\
  tbforth_cdef("poke", MCU_POKE);	\
  tbforth_cdef("peek", MCU_PEEK);	\
  tbforth_cdef("pwm-freq", MCU_PWM_FREQ); \
  tbforth_cdef("pwm-range", MCU_PWM_RANGE); \
  tbforth_cdef("pwm-duty", MCU_PWM_DUTY); \

