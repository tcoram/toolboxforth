/*
  toolboxForth - A tiny ROMable 16/32-bit FORTH-like scripting language
          for any C99 compiler. From POSIX to microcontrollers.
	  Version 3.0. Based on uForth...

  Copyright � 2009-2024 Todd Coram, todd@maplefish.com, USA.

  Permission is hereby granted, free of charge, to any person obtaining
  a copy of this software and associated documentation files (the
  "Software"), to deal in the Software without restriction, including
  without limitation the rights to use, copy, modify, merge, publish,
  distribute, sublicense, and/or sell copies of the Software, and to
  permit persons to whom the Software is furnished to do so, subject to
  the following conditions:
  
  The above copyright notice and this permission notice shall be
  included in all copies or substantial portions of the Software.
  
  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
  EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
  MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
  NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE
  LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION
  OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
  WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

*/

#include "tbforth.h"

#define min(a,b) ((a < b) ? a : b)


#define BYTES_PER_CELL sizeof(CELL)

#define DICT_HEADER_WORDS	((sizeof(struct dict)/(sizeof (CELL))) - MAX_DICT_CELLS)
#define DICT_INFO_SIZE_BYTES	(sizeof(CELL)*DICT_HEADER_WORDS)

#define IRAM_BYTES (sizeof(struct tbforth_iram))
#define URAM_HDR_BYTES (sizeof(struct tbforth_uram))
#define URAM_START (IRAM_BYTES+URAM_HDR_BYTES)
#define VAR_ALLOTN(n) (IRAM_BYTES/4+URAM_HDR_BYTES/4+dict_incr_varidx(n))
#define VAR_ALLOT_1() (IRAM_BYTES/4+URAM_HDR_BYTES/4+dict_incr_varidx(1))

#define PAD_ADDR (IRAM_BYTES+URAM_HDR_BYTES)
#define PAD_STR (char*)&tbforth_ram[PAD_ADDR+1]
#define PAD_STRLEN tbforth_ram[PAD_ADDR]

#ifdef USE_LITTLE_ENDIAN
# define BYTEPACK_FIRST(b) b
# define BYTEPACK_SECOND(b) ((CELL)b)<<8
#else
# define BYTEPACK_FIRST(b) ((CELL)b)<<8
# define BYTEPACK_SECOND(b) b
#endif

#ifdef SUPPORT_FLOAT_FIXED
#include <math.h>

#define FIXED_PT_PLACES		5
RAMC fixed_pt_mult (double num) {
  return (num*pow(10, tbforth_uram->fixedp));
}

#endif


tbforth_stat interpret_tib();

CELL *tbforth_dict;			/* treat dict struct like array */
abort_t _tbforth_abort_request;	/* for emergency aborts */

struct tbforth_iram *tbforth_iram;
struct tbforth_uram *tbforth_uram;

#ifdef GUARD_RAILS
inline void DICT_WRITE(CELL a, RAMC v) {
  if(a >= 0 && a < MAX_DICT_CELLS) {
    dict_write(a,v);
  } else {
    tbforth_abort_request(ABORT_ILLEGAL);
  }
}
inline void RAM_WRITE(CELL a, RAMC v) {
  if(a >= 0 && a < TOTAL_RAM_CELLS) {
    tbforth_ram[a]=v;
  }else {
    tbforth_abort_request(ABORT_ILLEGAL);
  }
}
inline void DICT_APPEND(CELL c) {
  if (dict->here < MAX_DICT_CELLS) {
    dict_append(c);
  } else {
    tbforth_abort_request(ABORT_ILLEGAL);
  }
}    

inline void DICT_APPEND_STRING(char*s, RAMC l) {
  if (dict->here < MAX_DICT_CELLS) {
    dict_append_string(s,l);
  } else {
    tbforth_abort_request(ABORT_ILLEGAL);
  }
}    
#else
#define DICT_WRITE(a,v) dict_write(a,v)
#define RAM_WRITE(a,v) tbforth_ram[a]=v
#define DICT_APPEND(c) dict_append(c)
#define DICT_APPEND_STRING(s,l) dict_append_string(s,l)
#endif


/*
  Words must be under 64 characters in length
*/
#define WORD_LEN_BITS 0x3F  
#define IMMEDIATE_BIT (1<<7)
#define PRIM_BIT     (1<<6)


RAMC tbforth_ram[TOTAL_RAM_CELLS];

void tbforth_cdef (char* name, int val) {
  snprintf(PAD_STR, PAD_SIZE, ": %s %d cf ;", name, val);
  tbforth_interpret(PAD_STR);
}

RAMC parse_num(char *s, uint8_t base) {
  char *p = s;
  char *endptr;
#ifdef SUPPORT_FLOAT_FIXED
  double f;
  while (*p != '\0' && *p != ' ' && *p != '.') ++p;
  if (*p == '.') {		/* got a dot, must be floating */
    f = strtod(s,NULL);
    return (RAMC)fixed_pt_mult(f);
  }
#endif
  p = s;
  int curbase = tbforth_uram->base;
  switch (*p) {
  case '%':
    tbforth_uram->base = 2; p++; break;
  case '$':
    tbforth_uram->base = 16; p++; break;
  case '#':
    tbforth_uram->base = 10; p++; break;
  }
  // Treat base 10 as 0 so we can handle 0xNN as well as decimal.
  //
  // On (some?) 32 bit hosts, strtol messes with sign and overflow of numbers
  // > 7FFFFFFF.  I've seen this on the RP2040....
  // So we'll use strtoll() to mitigate this... but why???
  //
  RAMC num = strtoll(p,&endptr, tbforth_uram->base == 10 ? 0 : tbforth_uram->base);
  tbforth_uram->base = curbase;
  if (*endptr != 32  && *endptr != '\0' && *endptr != '\n' && *endptr != '\r') {
    tbforth_abort_request(ABORT_NAW);
  }
  return num;
}

RAMC parse_num_cstr(char *s, int cnt, uint8_t base) {
  static char tmp[64];
  memcpy(tmp, s, min(cnt,63));
  tmp[min(cnt,63)] = '\0';
  return parse_num(tmp,base);
}

char* i32toa(int32_t value, char* result, int32_t base) {
  // check that the base if valid
  if (base < 2 || base > 36) { *result = '\0'; return result; }
  
  char *ptr = result, *ptr1 = result, tmp_char;
  int32_t tmp_value = value;
  
  do {
    tmp_value = value;
    value /= base;
    *ptr++ = "zyxwvutsrqponmlkjihgfedcba9876543210123456789abcdefghijklmnopqrstuvwxyz" [35 + (tmp_value - value * base)];
  } while ( value );
  
  // Apply negative sign
  if (tmp_value < 0) *ptr++ = '-';
  *ptr-- = '\0';
  while(ptr1 < ptr) {
    tmp_char = *ptr;
    *ptr-- = *ptr1;
    *ptr1++ = tmp_char;
  }
  return result;
}

char* u32toa(uint32_t value, char* result, int32_t base) {
  // check that the base if valid
  if (base < 2 || base > 36) { *result = '\0'; return result; }
  
  char *ptr = result, *ptr1 = result, tmp_char;
  uint32_t tmp_value = value;
  
  do {
    tmp_value = value;
    value /= base;
    *ptr++ = "zyxwvutsrqponmlkjihgfedcba9876543210123456789abcdefghijklmnopqrstuvwxyz" [35 + (tmp_value - value * base)];
  } while ( value );
  
  *ptr-- = '\0';
  while(ptr1 < ptr) {
    tmp_char = *ptr;
    *ptr-- = *ptr1;
    *ptr1++ = tmp_char;
  }
  return result;
}

CELL find_word(char* s, uint8_t len, RAMC* addr, bool *immediate, char *prim);

/*
 Every entry in the dictionary consists of the following cells:
  [index of previous entry]  
  [flags, < 64 byte name byte count]
  [name [optional pad byte]... [ data ..]
*/
void make_word(char *str, uint8_t str_len) {
  CELL my_head = dict_here();

  DICT_APPEND(dict->last_word_idx);
  DICT_APPEND(str_len);
  DICT_APPEND_STRING(str, str_len);
  dict_set_last_word(my_head);

}

void make_immediate(void) {
  DICT_WRITE((dict->last_word_idx+1), tbforth_dict[dict->last_word_idx+1]|IMMEDIATE_BIT);
}

char next_char(void) {
  if (tbforth_iram->tibidx >= tbforth_iram->tibclen) return 0;
  return tbforth_iram->tib[tbforth_iram->tibidx++];
}
#define EOTIB() (tbforth_iram->tibidx >= tbforth_iram->tibclen)
#define CURR_TIB_WORD &(tbforth_iram->tib[tbforth_iram->tibwordidx])
#define CLEAR_TIB() (tbforth_iram->tibidx=0, tbforth_iram->tibclen=0, tbforth_iram->tibwordlen=0, tbforth_iram->tibwordidx=0)

char* tbforth_next_word (void) {
  char nc; uint8_t cnt = 0;
  do { nc = next_char(); } while (!EOTIB() && isspace(nc));
  if (EOTIB()) {
    return "";
  }
  cnt=1;
  tbforth_iram->tibwordidx = (tbforth_iram->tibidx)-1;
  while(!isspace(next_char()) && !EOTIB()) ++cnt;
  tbforth_iram->tibwordlen = cnt;
  return &(tbforth_iram->tib[tbforth_iram->tibwordidx]);
}


typedef tbforth_stat (*wfunct_t)(void);


void tbforth_init(void) {
  tbforth_dict = (CELL*)dict;
  tbforth_iram = (struct tbforth_iram*) tbforth_ram;
  tbforth_iram->state = 0;
  tbforth_iram->total_ram = TOTAL_RAM_CELLS;
  tbforth_uram = (struct tbforth_uram*)
    ((char*)tbforth_ram + sizeof(struct tbforth_iram));
  tbforth_uram->len = TOTAL_RAM_CELLS - sizeof(struct tbforth_iram);
  tbforth_uram->dsize = DS_CELLS;
  tbforth_uram->rsize = RS_CELLS;
  tbforth_uram->ridx = DS_CELLS + RS_CELLS;
  tbforth_uram->didx = -1;

#ifdef SUPPORT_FLOAT_FIXED
  tbforth_uram->fixedp = FIXED_PT_PLACES;
#endif

  tbforth_abort_clr();
  tbforth_abort(0);

  tbforth_uram->base = 10;
}


// Opcodes
// LIT must be 1!
//
enum { 
  LIT=1, COLD, DLIT, ABORT, DEF, IMMEDIATE, URAM_BASE_ADDR,  STORE_URAM_BASE_ADDR, RTOP,
  RPICK,  HERE, RAM_BASE_ADDR, INCR, DECR,
  ADD, SUB, MULT, DIV, MULT_DIV, MOD, AND, JMP, JMP_IF_ZERO, SKIP_IF_ZERO, EXIT,
  OR, XOR, LSHIFT, RSHIFT, EQ_ZERO, GT_ZERO,  LT_ZERO, EQ, DROP, DUP,  SWAP, OVER, ROT,
  NEXT, CNEXT,  EXEC, LESS_THAN, GREATER_THAN, GREATER_THAN_EQ,
  INVERT, COMMA, DCOMMA, RPUSH, RPOP, FETCH, STORE, 
  COMMA_STRING, CHAR_A_ADDR_STORE,  CHAR_A_B_SWAP,
  CHAR_A_FETCH, CHAR_A_STORE, CHAR_A_INCR, 
  CHAR_A_FETCH_INCR, CHAR_A_STORE_INCR,
  VAR_ALLOT, CALLC,   FIND, CHAR_APPEND, CHAR_STORE, CHAR_FETCH, 
  BYTE_COPY, BYTE_CMP,
  _CREATE, PARSE_NUM,
  INTERP, NUM_TO_STR, UNUM_TO_STR,
  LAST_PRIMITIVE
};

void store_prim(char* str, CELL val) {
  make_word(str,strlen(str));
  DICT_APPEND(val);
  //  DICT_APPEND(EXIT); // not needed since we optimize primitives by inlining them
  DICT_WRITE((dict->last_word_idx+1), tbforth_dict[dict->last_word_idx+1]|PRIM_BIT);
}

/* Bootstrap code */
void tbforth_bootstrap(void) {
  tbforth_init();
  tbforth_load_prims();
  tbforth_interpret("(create) : 1 iram ! (create) 1 iram ! here 2 iram + !  ;");
  OS_WORDS();
  MCU_WORDS();
}

void tbforth_load_prims(void) {
  dict->here = DICT_HEADER_WORDS+1;
  /*
    Store our primitives into the dictionary as "callable" words (for interpret).
    (During compilation references to these word definitions are optimized away).
  */

  // Primary opcodes
  //
  store_prim("lit", LIT);
  store_prim("dlit", DLIT);
  store_prim("drop", DROP);
  store_prim("rot", ROT);
  store_prim("dup", DUP);
  store_prim("swap", SWAP);
  store_prim("over", OVER);
  store_prim("jmp", JMP);
  store_prim("0jmp?", JMP_IF_ZERO);
  store_prim("0skip?", SKIP_IF_ZERO);
  store_prim(",", COMMA);
  store_prim("d,", DCOMMA);
  store_prim("1+", INCR);
  store_prim("1-", DECR);
  store_prim("+", ADD);
  store_prim("-", SUB);
  store_prim("and", AND);
  store_prim("or", OR);
  store_prim("xor", XOR);
  store_prim("invert", INVERT);
  store_prim("lshift", LSHIFT);
  store_prim("rshift", RSHIFT);
  store_prim("*", MULT);
  store_prim("/", DIV);
  store_prim("*/", MULT_DIV);
  store_prim("mod", MOD);
  store_prim("0=", EQ_ZERO);
  store_prim("0>", GT_ZERO);
  store_prim("0<", LT_ZERO);
  store_prim("=", EQ);
  store_prim("<", LESS_THAN);
  store_prim(">", GREATER_THAN);
  store_prim(">=", GREATER_THAN_EQ);
  store_prim("r@", RTOP);
  store_prim("rpick", RPICK);
  store_prim(">r", RPUSH);
  store_prim("r>", RPOP);
  store_prim("!", STORE);
  store_prim("@", FETCH);
  store_prim("A!", CHAR_A_ADDR_STORE);
  store_prim("A<>B", CHAR_A_B_SWAP);
  store_prim("A+", CHAR_A_INCR);
  store_prim("(c@)", CHAR_A_FETCH);
  store_prim("(c!)", CHAR_A_STORE);
  store_prim("(c@+)", CHAR_A_FETCH_INCR);
  store_prim("(c!+)", CHAR_A_STORE_INCR);
  store_prim(",\"", COMMA_STRING); make_immediate();
  store_prim("+c!", CHAR_STORE);
  store_prim("c!+", CHAR_APPEND);
  store_prim("+c@", CHAR_FETCH);
  store_prim("(create)", _CREATE); 
  store_prim("next-word", NEXT);
  store_prim("next-char", CNEXT);
  //  store_prim(":", DEF); 
  //  store_prim(";", EXIT);  make_immediate();
  store_prim(";", EXIT);
  store_prim("immediate", IMMEDIATE);

  // extended opcodes
  //
  store_prim("(allot1)", VAR_ALLOT);
  store_prim("(find)", FIND);
  store_prim("cold", COLD);
  store_prim("abort", ABORT);
  store_prim("bcopy", BYTE_COPY);
  store_prim("bstr=", BYTE_CMP);
  store_prim(">string", NUM_TO_STR);
  store_prim(">num", PARSE_NUM);
  store_prim("u>string", UNUM_TO_STR);
  store_prim("exec", EXEC);
  store_prim("uram", URAM_BASE_ADDR);
  store_prim("uram!", STORE_URAM_BASE_ADDR);
  store_prim("iram", RAM_BASE_ADDR);
  store_prim("interpret", INTERP);
  store_prim("cf", CALLC);
  store_prim("here", HERE);

  // Allocate the scratch pad
  //
  (void)VAR_ALLOTN(PAD_SIZE);
}

/* Return a counted string pointer
 */
char* tbforth_count_str(CELL addr,CELL* new_addr) {
  char *str;
  str =(char*)&tbforth_ram[addr+1];
  *new_addr = tbforth_ram[addr];
  return str;
}

void tbforth_abort(CELL wid) {
  if (tbforth_iram->state == COMPILING) {
    DICT_APPEND(ABORT);
  }
  tbforth_iram->state = 0;
  tbforth_abort_clr();
  tbforth_uram->ridx = tbforth_uram->rsize + tbforth_uram->dsize;
  tbforth_uram->didx = -1;
}


tbforth_stat exec(CELL ip, bool toplevelprim,uint8_t last_exec_rdix) {
  /* Scratch/Register variables for exec */
  static char* A;			/* (char) address register */
  static char* B;			/* (char) address register */
  RAMC r1, r2;
  char *str1, *str2;
  char b;
  CELL cmd;

  while(1) {
    if (ip == 0) {
      tbforth_abort_request(ABORT_ILLEGAL);
      tbforth_abort(ip);		/* bad instruction */
      return E_ABORT;
    }
    cmd = tbforth_dict[ip++];

    switch (cmd) {
    case 0:
      tbforth_abort_request(ABORT_ILLEGAL);
      tbforth_abort(ip-1);		/* bad instruction */
      return E_ABORT;
    case ABORT:
      tbforth_abort_request(ABORT_WORD);
      break;
    case IMMEDIATE:
      make_immediate();
      break;
    case SKIP_IF_ZERO:
      r1 = dpop(); r2 = dpop();
      if (r2 == 0) ip += r1;
      break;
    case DUP:
      dpush(dtop());
      break;
    case SWAP:
      r1 = dtop();
      dtop()=dtop2();
      dtop2()=r1;
      break;
    case OVER:
      dpush(dtop2());
      break;
    case DROP:
      (void)dpop();
      break;
    case ROT:
      r1 = dtop3();
      dtop3() = dtop();
      dtop() = r1;
      break;
    case JMP:
      ip = dpop();
      break;
    case JMP_IF_ZERO:
      r1 = dpop(); r2 = dpop();
      if (r2 == 0) ip = r1;
      break;
    case HERE:
      dpush(dict_here());
      break;
    case COLD:
      tbforth_init();
      break;
    case LIT:  
      dpush(tbforth_dict[ip++]);
      break;
    case DLIT:  
      dpush((((uint32_t)tbforth_dict[ip])<<16) |
	    (uint16_t)tbforth_dict[ip+1]); 
      ip+=2;
      break;
    case INCR:
      dtop()++; 
      break;
    case DECR:
      dtop()--; 
      break;
    case ADD:
      r1 = dpop();
      dtop() += r1;
      break;
    case SUB:
      r1 = dpop();
      dtop() -= r1;
      break;
    case AND:
      r1 = dpop();
      dtop() &= r1;
      break;
    case LSHIFT:
      r1 = dpop(); 
      dtop() <<= r1;
      break;
    case RSHIFT:
      r1 = dpop();
      dtop() >>= r1;
      break;
    case OR:
      r1 = dpop(); 
      dtop() |= r1;
      break;
    case XOR:
      r1 = dpop(); 
      dtop() ^= r1;
      break;
    case INVERT:
      dtop() = ~dtop();
      break;
    case MULT:
      r1 = dpop(); 
      dtop() *= r1;
      break;
    case DIV :
      r1 = dpop(); 
      dtop() /= r1;
      break;
    case MULT_DIV :
      {
	uint64_t tmp;
	r1 = dpop(); r2 = dpop();
	tmp = r1 * r2;
	r1 = dpop();
	dpush(tmp/r1);
      }
      break;
    case MOD :
      r1 = dpop();
      dtop() %= r1;
      break;
    case RTOP:
      dpush(rpick(0));
      break;
    case RPICK:
      r1 = dpop();
      r2 = rpick(r1);
      dpush(r2);
      break;
    case EQ_ZERO:
      dtop() = -(dtop() == 0);
      break;
    case GT_ZERO:
      dtop() = -(dtop() > 0);
      break;
    case LT_ZERO:
      dtop() = -(dtop() < 0);
      break;
    case LESS_THAN:
      r1 = dpop();
      dtop() = -(dtop() < r1);
      break;
    case GREATER_THAN:
      r1 = dpop();
      dtop() = -(dtop() > r1);
      break;
    case GREATER_THAN_EQ:
      r1 = dpop();
      dtop() = -(dtop() >= r1);
      break;
    case EQ:
      r1 = dpop(); 
      dtop() = -(r1 == dtop());
      break;
    case RPUSH:
      rpush(dpop());
      break;
    case RPOP:
      dpush(rpop());
      break;
    case RAM_BASE_ADDR:
      dpush (0x80000000 | 0);
      break;
    case URAM_BASE_ADDR:
      dpush(0x80000000 | (((char*)tbforth_uram - (char*)tbforth_ram)/4));
      break;
    case STORE_URAM_BASE_ADDR:
      tbforth_uram = (struct tbforth_uram*) &tbforth_ram[0x7FFFFFFF & dpop()];
      break;
    case FETCH:
      r1 = dpop();
      if (r1 & 0x80000000) {
	dpush(tbforth_ram[r1 & 0x7FFFFFFF]);
      } else {
	dpush(tbforth_dict[r1]);
      }
      break;
    case STORE:
      r1 = dpop();
      r2 = dpop();
      if (r1 & 0x80000000)
	RAM_WRITE(0x7FFFFFFF & r1,r2);
      else
	DICT_WRITE(r1,r2);
      break;
    case EXEC:
      r1 = dpop();
      rpush(ip);
      ip = r1;
      break;
    case CHAR_A_ADDR_STORE:
      r1 = dpop();
      if (r1 &  0x80000000)
	A =(char*)&tbforth_ram[0x7FFFFFFF & r1];
      else
	A =(char*)&tbforth_dict[r1];
      break;
    case CHAR_A_B_SWAP:
      {
	char *T = A;
	A = B;
	B = T;
      }
      break;
    case CHAR_A_INCR:
      A+=dpop();
      break;
    case CHAR_A_FETCH:
      dpush(0xFF & *A);
      break;
    case CHAR_A_STORE:
      *A = dpop();
      break;
    case CHAR_A_FETCH_INCR:
      dpush(0xFF & *A++);
      break;
    case CHAR_A_STORE_INCR:
      *A++ = dpop();
      break;
    case CHAR_FETCH:
      r1 = dpop();
      r2 = dpop();
      if (r2 & 0x80000000)
	str1 =(char*)&tbforth_ram[0x7FFFFFFF & r2];
      else
	str1 =(char*)&tbforth_dict[r2];
      str1+=r1;
      dpush(0xFF & *str1);
      break;
    case EXIT:
      if (tbforth_uram->ridx > last_exec_rdix) return U_OK;
      ip = rpop();
      break;
    case CNEXT:
      b = next_char();
      dpush(b);
      break;
    case BYTE_COPY:
    case BYTE_CMP:
      {
	RAMC from, dest, fidx, didx, cnt;
	cnt = dpop();
	didx = dpop();
	dest = dpop();
	fidx = dpop();
	from  = dpop();
	str1 = (from & 0x80000000) ? (char*)&tbforth_ram[from & 0x7FFFFFFF] + fidx :
	  (char*)&tbforth_dict[from] + fidx ;
	str2 = (dest & 0x80000000) ? (char*)&tbforth_ram[dest & 0x7FFFFFFF] + didx :
	  (char*)&tbforth_dict[dest] + didx ;
	if (cmd == BYTE_CMP)
	  dpush(-(memcmp (str2, str1, cnt) == 0));
	else
	  memcpy (str2, str1, cnt);
      }
      break;
    case CHAR_STORE:
      r1 = dpop();
      r2 = dpop();
      if (r2 & 0x80000000)
	str1 =(char*)&tbforth_ram[r2 & 0x7FFFFFFF];
      else
	str1 =(char*)&tbforth_dict[r2];
      str1+=r1;
      *str1 = dpop();
      break;
    case CHAR_APPEND:
      r1 = dpop();
      if (r1 & 0x80000000) {
	r1 &= 0x7FFFFFFF;
	r2 = tbforth_ram[r1];
	str1 =(char*)&tbforth_ram[r1+1];
	tbforth_ram[r1]++;
	str1+=r2;
	b = dpop();
	*str1 = b;
      } else
	tbforth_abort_request(ABORT_ILLEGAL);
      break;
    case COMMA_STRING:
      if (tbforth_iram->state == COMPILING) {
	DICT_APPEND(LIT);
	DICT_APPEND(dict_here()+sizeof(RAMC)); /* address of counted string */

	DICT_APPEND(LIT);
	rpush(dict_here());	/* address holding adress  */
	dict_incr_here(1);	/* place holder for jump address */
	DICT_APPEND(JMP);
      }
      rpush(dict_here());
      dict_incr_here(1);	/* place holder for count*/
      r1 = 0;
      do {
	r2 = 0;
	b = next_char();
	if (b == 0 || b == '"') break;
	r2 |= BYTEPACK_FIRST(b);
	++r1;
	b = next_char();
	if (b != 0 && b != '"') {
	  ++r1;
	  r2 |= BYTEPACK_SECOND(b);
	}
	DICT_APPEND(r2);
      } while (b != 0 && b!= '"');
      DICT_WRITE(rpop(),r1);
      if (tbforth_iram->state == COMPILING) {
	DICT_WRITE(rpop(),dict_here());	/* jump over string */
      }
      break;
    case NEXT:
      str2 = PAD_STR;
      str1 = tbforth_next_word();
      memcpy(str2,str1, tbforth_iram->tibwordlen);
      PAD_STRLEN = tbforth_iram->tibwordlen;		/* length */
      dpush(PAD_ADDR | 0x80000000);
      break;
    case CALLC:
      r1 = c_handle();
      if (r1 != U_OK) return (tbforth_stat)r1;
      break;
    case VAR_ALLOT:
      dpush(VAR_ALLOT_1() | 0x80000000);
      break;
    case DEF:
      tbforth_iram->state = COMPILING;
    case _CREATE:
      dict_start_def();
      tbforth_next_word();
      make_word(CURR_TIB_WORD,tbforth_iram->tibwordlen);
      if (cmd == _CREATE) {
	dict_end_def();
      } else {
	tbforth_iram->compiling_word = dict_here();
      }
      break;
    case COMMA:
      DICT_APPEND(dpop());
      break;
    case DCOMMA:
      r1 = dpop();
      DICT_APPEND((uint32_t)r1>>16);
      DICT_APPEND(r1);
      break;
    case PARSE_NUM:
      r1 = dpop();
      if (r1 & 0x80000000) {
	r1 &= 0x7FFFFFFF;
	str1 =(char*)&tbforth_ram[r1+1];
	r2 = tbforth_ram[r1];
      } else {
	str1 =(char*)&tbforth_dict[r1+1];
	r2 = tbforth_dict[r1];
      }
      r2 = parse_num_cstr(str1, r2, tbforth_uram->base);
      if (!tbforth_aborting()) 
	dpush(r2);
      else return E_NOT_A_NUM;
      break;
    case FIND:
      r1 = dpop();
      str1=tbforth_count_str((CELL)r1,(CELL*)&r1);
      r1 = find_word(str1, r1, &r2, 0, &b);
      if (r1 > 0) {
	if (b) r1 = tbforth_dict[r1];
      }
      dpush(r2); dpush(r1);
      break;
    case UNUM_TO_STR:
    case NUM_TO_STR:			/* 32bit to string */
      {
	if (cmd == UNUM_TO_STR)
	  u32toa(dpop(),PAD_STR,tbforth_uram->base);
	else
	  i32toa(dpop(),PAD_STR,tbforth_uram->base);
	PAD_STRLEN=strlen(PAD_STR);
	dpush(PAD_ADDR | 0x80000000);
      }
      break;
    case INTERP:
      dpush (interpret_tib());
      break;
    default:
      if (cmd > LAST_PRIMITIVE) {
	/* Execute user word by calling until we reach primitives */
	rpush(ip);
	ip = tbforth_dict[ip-1]; /* ip-1 is current word */
	//	goto CHECK_STAT;
      } else {
	tbforth_abort_request(ABORT_ILLEGAL);
      }
      break;
    }
    if (tbforth_aborting()) {
      tbforth_abort(ip-1);
      return E_ABORT;
    }
    if (toplevelprim) return U_OK;
  } /* while(1) */
}

CELL find_word(char* s, uint8_t slen, RAMC* addr, bool *immediate, char *primitive) {
  CELL fidx = dict->last_word_idx;
  CELL prev = fidx;
  uint8_t wlen;

  while (fidx != 0) {
    if (addr != 0) *addr = prev ;
    prev = tbforth_dict[fidx++];
    wlen = tbforth_dict[fidx++]; 
    if (immediate) *immediate = (wlen & IMMEDIATE_BIT) ? 1 : 0;
    if (primitive) *primitive = (wlen & PRIM_BIT) ? 1 : 0;
    wlen &= WORD_LEN_BITS;
    if (wlen == slen && strncmp(s,(char*)(tbforth_dict+fidx),wlen) == 0) {
      fidx += (wlen / BYTES_PER_CELL) + (wlen % BYTES_PER_CELL);
      return fidx;
    }
    fidx = prev;
  }
  if (addr != 0) *addr = 0 ;
  return 0;
}

tbforth_stat interpret_tib() {
  tbforth_stat stat;
  char *word;
  CELL wd;
  bool immediate = 0;
  char primitive = 0;
  while(*(word = tbforth_next_word()) != 0) {
    wd = find_word(word,tbforth_iram->tibwordlen,0,&immediate,&primitive);
    switch (tbforth_iram->state) {
    case 0:			/* interpret mode */
      if (wd == 0) {	/* number or trash */
	RAMC num = parse_num(word,tbforth_uram->base);
	if (tbforth_aborting()) {
	  tbforth_abort_request(ABORT_NAW);
	  tbforth_abort(wd);
	  return E_NOT_A_WORD;
	}
	dpush(num);
      } else {
	stat = exec(wd,primitive,tbforth_uram->ridx-1);
	if (stat != U_OK) {
	  tbforth_abort(wd);
	  //	  tbforth_abort_clr();
	  return stat;
	}
      }
      break;
    case COMPILING:			/* in the middle of a colon def */
      if (wd == 0) {	/* number or trash */
	RAMC num = parse_num(word,tbforth_uram->base);
	if (tbforth_aborting()) {
	  tbforth_abort(wd);
	  dict_end_def();
	  return E_NOT_A_WORD;
	}
	DICT_APPEND(DLIT);
	DICT_APPEND(((uint32_t)num)>>16);
	DICT_APPEND(((uint16_t)num)&0xffff);
      }	else if (word[0] == ';') { /* exit from a colon def */
	tbforth_iram->state = 0;
	DICT_APPEND(EXIT);
	dict_end_def();
	tbforth_iram->compiling_word = 0;
      } else if (immediate) {	/* run immediate word */
	stat = exec(wd,primitive,tbforth_uram->ridx-1);
	if (stat != U_OK) {
	  tbforth_abort_request(ABORT_ILLEGAL);
	  tbforth_abort(wd);
	  dict_end_def();
	  return stat;
	}
      } else {			/* just compile word */
	if (primitive) {
	  /* OPTIMIZATION: inline primitive */
	  DICT_APPEND(tbforth_dict[wd]);
	} else {
	  /* OPTIMIZATION: skip null definitions */
	  if (tbforth_dict[wd] != EXIT) {
	    if (wd == tbforth_iram->compiling_word) { 
	      /* Natural recursion for such a small language is dangerous.
		 However, tail recursion is quite useful for getting rid
		 of BEGIN AGAIN/UNTIL/WHILE-REPEAT and DO LOOP in some
		 situations. We don't check to see if this is truly a
		 tail call, but we treat it as such.
	      */
	      DICT_APPEND(LIT);
	      DICT_APPEND(tbforth_iram->compiling_word);
	      DICT_APPEND(JMP);
	    } else {
	      DICT_APPEND(wd);
	    }
	  }
	}
      }
      break;
    }
  }
  return U_OK;
}

tbforth_stat tbforth_interpret(char *str) {
  CLEAR_TIB();
  tbforth_iram->tibclen = min(PAD_SIZE, strlen(str)+1);
  memcpy(tbforth_iram->tib, str, tbforth_iram->tibclen);
  return interpret_tib();
}
