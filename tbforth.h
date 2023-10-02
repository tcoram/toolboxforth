#include <stdio.h>
#include <stdint.h>
#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

/* Configuration */

#define TBFORTH_VERSION "3.0"
#define DICT_VERSION 9

/* 
   The Dictionary: Max is 64K words (64KB * 2 bytes).
   Pick a size suitable for your target.
   This may, depending on your target architecture, live in RAM or Flash.
   Note: A Dictionary CELL is always 2 bytes!
*/
#define MAX_DICT_CELLS 		(65535)


/*
 Total user RAM (each ram cell is 4 bytes)
*/
#define TOTAL_RAM_CELLS		(2048) /* 8KB */
#define PAD_SIZE		160  /* bytes */
#define TIB_SIZE  PAD_SIZE

/*
  Task RAM -- Each cell is 4 bytes. This is a subset of TOTAL_RAM_CELLS
*/
#define TASK0_DS_CELLS 		150
#define TASK0_RS_CELLS 		100
#define TASK0_URAM_CELLS	500


#define FIXED_PT_DIVISOR	((double)(1000000.0))
#define FIXED_PT_PLACES		6

#define FIXED_PT_MULT(x)	 (x*(1000000))

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
typedef int32_t RAMC;



typedef enum { U_OK=0, COMPILING, E_NOT_A_WORD, E_STACK_UNDERFLOW, E_RSTACK_OVERFLOW,
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
extern void tbforth_load_prims(void);
extern void tbforth_abort(void);
extern tbforth_stat tbforth_interpret(char*);
extern tbforth_stat c_handle(void);
extern void tbforth_cdef (char*, int);
extern char* tbforth_next_word (void);

/*
 The following structures are overlays on pre-allocated buffers.
*/

/*
  iram. 
*/
struct tbforth_iram {
  RAMC state;		/* 0=interpreting .. */
  RAMC total_ram;		/* Total ram available */
  RAMC compiling_word;	 /* 0=none */
  RAMC curtask_idx;
  RAMC tibidx;		      /* current index into buffer */
  RAMC tibwordidx;		/* point to current word in inbufptr */
  RAMC tibwordlen;	/* length of current word in inbufptr */
  RAMC tibclen;		      /* size of data in the tib buffer */
  char tib[TIB_SIZE];		/* input buffer for interpreter */
};

struct tbforth_uram {
  RAMC len;		/* size of URAM */
  RAMC base;			/* numeric base for I/O */
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
    strncpy((char*)((tbforth_dict )+(dict->here)),src,len);			\
    dict->here += (len/BYTES_PER_CELL) + (len % BYTES_PER_CELL); \
}
# define dict_end_def()

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
#define rtop() tbforth_iram->rs[tbforth_uram->ridx]

// Some useful OS/Platform extensions...you can handle in c_handle() or not...
//
enum { EMIT=1, KEY, SAVE_IMAGE, INCLUDE, OPEN, CLOSE,
  READB, WRITEB, MS};


extern void tbforth_cdef (char* name, int val);
