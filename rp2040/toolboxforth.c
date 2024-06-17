#include <stdio.h>
#include <pico/stdlib.h>
#include <hardware/uart.h>
#include <hardware/flash.h>
#include <hardware/sync.h>
#include "../tbforth.h"

struct dict *dict;

#define DICT_SECTORS ((sizeof (struct dict) / FLASH_SECTOR_SIZE) + 1)
// #define TOP_OF_DICT ((XIP_BASE + PICO_FLASH_SIZE_BYTES) - (DICT_SECTORS * FLASH_SECTOR_SIZE))
// #define TOP_OF_DICT_SECT ((PICO_FLASH_SIZE_BYTES / DICT_SECTORS) - DICT_SECTORS - 1)
#define TOP_OF_DICT_SECT (256*1024)
#define TOP_OF_DICT (XIP_BASE + TOP_OF_DICT_SECT)

void save_image (void) {
  printf("Saving: %d bytes (%d sectors) to addr 0x%08lX (sector 0x%04X)\n",
	 sizeof (struct dict), DICT_SECTORS, TOP_OF_DICT, TOP_OF_DICT_SECT);
  fflush(stdout);
#if 1
  int32_t ints = save_and_disable_interrupts();
  flash_range_erase (TOP_OF_DICT_SECT, DICT_SECTORS * FLASH_SECTOR_SIZE);

  flash_range_program (TOP_OF_DICT_SECT, (uint8_t*) dict,
		       ((sizeof (struct dict) / FLASH_PAGE_SIZE)+1) * FLASH_PAGE_SIZE);
  restore_interrupts(ints);
#endif
}

int load_image (void) {
  memcpy (dict, (uint8_t*)TOP_OF_DICT, sizeof (struct dict));
}


uint8_t rxc(void) {
  return getc(stdin);
}

void txc(uint8_t c) {
  fputc(c, stdout);
  fflush(stdout);
}

void txs(char* s, int cnt) {
  fwrite(s,cnt,1,stdout);
  fflush(stdout);
}
#define txs0(s) txs(s,strlen(s))

tbforth_stat c_handle(void) {
  RAMC r2, r1 = dpop();
  switch(r1) {
  case OS_POLL:
    break;
  case OS_SECS:
    break;	
  case OS_MS:		/* milliseconds */
    sleep_ms(dpop());
    break;
  case OS_EMIT:			/* emit */
    txc(dpop()&0xff);
    break;
  case OS_KEY:			/* key */
    dpush((CELL)rxc());
    break;
  case OS_SAVE_IMAGE:			/* save image */
    save_image();
    break; 
  case MCU_UART_BEGIN:
    
    break;
  return U_OK;
  }
}


static char linebuf[128];
char *line;
int get_line (char* buf, int max) {
  int idx = -1;
  int ch;
  do {
    ch = getchar_timeout_us(100);
    if (ch != PICO_ERROR_TIMEOUT) {
      putchar(ch);fflush(stdin);
      if (ch == 3) return 0;
      if (ch == 8) {
	if (idx > 0) idx--;
	continue;
      }
      if (ch != 13 && ch != '\n') {
	buf[++idx] = ch & 0xff;
      }
    }
  } while (ch != 13 && ch != '\n' && idx < (max-1));
  if (idx > -1)
    buf[idx+1] = '\0';
  return idx;
}
  
int interpret() {
  int stat;
  int16_t lineno = 0;
  while (1) {
    ++lineno;
    txs0(" ok\r\n");
    if (get_line(linebuf,127) < 0) break;
    line = linebuf;
    if (line[0] == '\n' || line[0] == '\0') continue;
    stat = tbforth_interpret(line);
    switch(stat) {
    case E_NOT_A_WORD:
    case E_NOT_A_NUM:
      txs0("Huh? >>> ");
      txs(&tbforth_iram->tib[tbforth_iram->tibwordidx],tbforth_iram->tibwordlen);
      txs0(" <<< ");
      txs(&tbforth_iram->tib[tbforth_iram->tibwordidx + tbforth_iram->tibwordlen],
	  tbforth_iram->tibclen - 
	  (tbforth_iram->tibwordidx + tbforth_iram->tibwordlen));
      txs0("\r\n");
      return -1;
    case E_ABORT:
      txs0("Abort!:<"); txs0(line); txs0(">\n");
      return -1;
    case E_STACK_UNDERFLOW:
      txs0("Stack underflow!\n");
      return -1;
    case E_DSTACK_OVERFLOW:
      txs0("Stack overflow!\n");
      return -1;
    case E_RSTACK_OVERFLOW:
      txs0("Return Stack overflow!\n");
      return -1;
    case U_OK:
      break;
    default:
      txs0("Ugh\n");
      return -1;
    }
  }
  return 0;
}

#include <sys/types.h>
#include <sys/stat.h>

int main() {
  int stat = -1;
  dict = malloc(sizeof(struct dict));

  setup_default_uart();
  stdio_init_all();

  load_image();

  if (dict->version != DICT_VERSION)  {
    dict->version = DICT_VERSION;
    dict->word_size = sizeof(CELL);
    dict->max_cells = MAX_DICT_CELLS;
    dict->here =  0;
    dict->last_word_idx = 0;
    dict->varidx = 1;
    tbforth_bootstrap();
  } else {
    tbforth_init();
    tbforth_interpret("init");
  }

  do {
    stat=interpret();
  } while (1);

}
 
