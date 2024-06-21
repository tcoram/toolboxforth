#include <stdio.h>
#include <pico/stdlib.h>
#include <hardware/gpio.h>
#include <hardware/uart.h>
#include <hardware/spi.h>
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
  //  printf("Saving: %d bytes (%d sectors) to addr 0x%08lX (sector 0x%04X)\n",
  //	 sizeof (struct dict), DICT_SECTORS, TOP_OF_DICT, TOP_OF_DICT_SECT);
  int32_t ints = save_and_disable_interrupts();
  flash_range_erase (TOP_OF_DICT_SECT, DICT_SECTORS * FLASH_SECTOR_SIZE);

  flash_range_program (TOP_OF_DICT_SECT, (uint8_t*) dict,
		       ((sizeof (struct dict) / FLASH_PAGE_SIZE)+1) * FLASH_PAGE_SIZE);
  restore_interrupts(ints);
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

enum {
  RP_X_FETCH32 = 200,
  RP_X_STORE32 = 201,
  RP_ERASE_FLASH = 202,
  RP_WRITE_FLASH = 203,
};

// Use Arduino values...for compatibility
//
enum {
  INPUT_PULLDOWN_MODE = 9,
  INPUT_PULLUP_MODE = 5,
  OUTPUT_MODE = 3,
  INPUT_MODE = 1,
};

tbforth_stat c_handle(void) {
  uint32_t *regw;
  uint8_t *regb;
  RAMC r3, r2, r1 = dpop();
  switch(r1) {
  case RP_ERASE_FLASH:
    {
      int32_t count = dpop();
      uint32_t start = dpop();
      uint32_t ints = save_and_disable_interrupts();
      flash_range_erase (start, count);
      restore_interrupts(ints);
    }
    break;
  case RP_WRITE_FLASH:
    {
      int32_t count = dpop();
      uint32_t dest = dpop();
      int32_t from = dpop();
      uint32_t ints = save_and_disable_interrupts();
      flash_range_program (dest, (uint8_t*) from, count);
      restore_interrupts(ints);
    }
    break;
  case RP_X_FETCH32 :
    regw = (uint32_t*)dpop();
    dpush(*regw);
    break;
  case RP_X_STORE32 :
    regw = (uint32_t*)dpop();
    *regw = (uint32_t)dpop();
    break;
  case MCU_SPI_CFG:
    {
      RAMC clkpin = dpop();
      RAMC misopin = dpop();
      RAMC mosipin = dpop();
      gpio_set_function(clkpin,1);
      gpio_set_function(misopin,1);
      gpio_set_function(mosipin,1);
      spi_init(spi0, 10000000);
    }
    break;
  case MCU_SPI_BEGIN_TRANS:
    {
      RAMC speed = dpop();
      RAMC bitorder = dpop();
      RAMC mode = dpop();
      RAMC cspin = dpop();

      spi_set_baudrate(spi0,speed);
      {
	int cpol, cpa;
	switch(mode) {
	case 1: cpol = 0; cpa = 1; break;
	case 2: cpol = 1; cpa = 0; break;
	case 3: cpol = 1; cpa = 1; break;
	default:
	case 0: cpol = 0; cpa = 0; break;
	}
	spi_set_format(spi0, 8, cpol, cpa, bitorder);
      }
      gpio_set_dir(cspin, 1);
      gpio_put(cspin, 0);
    }
    break;
  case MCU_SPI_WRITE:
    { char ch = (char)dpop();
      spi_write_blocking(spi0, &ch, 1);
    }
    break;
  case MCU_SPI_READ:
    { char ch;
      spi_read_blocking(spi0, 0, &ch, 1);
      dpush(ch);
    }
    break;
  case MCU_SPI_END_TRANS:
    gpio_put(dpop(), 1);
    break;
  case MCU_GPIO_MODE :
    r2 = dpop();
    r3 = dpop();
    gpio_init(r2);
    switch (r3) {
    case OUTPUT_MODE:
      gpio_set_dir(r2, 1);
      break;
    case INPUT_PULLUP_MODE:
      gpio_pull_up(r2);
      break;
    case INPUT_PULLDOWN_MODE:
      gpio_pull_down(r2);
      break;
    case INPUT_MODE:
    default:
      gpio_set_dir(r2, 0);
      break;
    }
    break;
  case MCU_GPIO_READ :
    r2 = dpop();
    dpush(gpio_get(r2));
    break;
  case MCU_GPIO_WRITE :
    r2 = dpop();
    gpio_put(r2, dpop());
    break;
  case MCU_COLD:
    {
      int32_t ints = save_and_disable_interrupts();
      flash_range_erase (TOP_OF_DICT_SECT, 1);
      restore_interrupts(ints);
    }
  case MCU_RESTART:
#define AIRCR_Register (*((volatile uint32_t*)(PPB_BASE + 0x0ED0C)))
    AIRCR_Register = 0x5FA0004;
    break;
  case OS_SECS:
    { uint64_t ts = time_us_64();
      dpush(ts/1000000);
    }
    break;
  case OS_MS:
    { uint64_t ts = time_us_64();
      dpush(ts/1000);
    }
    break;	
  case MCU_DELAY:	
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
  }
  return U_OK;
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
  while (1) {
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


int main() {
  int stat = -1;
  dict = malloc(sizeof(struct dict));

  setup_default_uart();
  stdio_init_all();

  load_image();

  // Check version & word_size & max_cells as a "signature" that we have
  // a valid image saved.
  //
  if (dict->version == DICT_VERSION &&
      dict->word_size == sizeof(CELL) &&
      dict->max_cells == MAX_DICT_CELLS)  {
    tbforth_init();
    tbforth_interpret("init");
  } else {
    // Bootstrap a raw Forth. You are going to have to send core.f, util.f, etc.
    //
    dict->version = DICT_VERSION;
    dict->word_size = sizeof(CELL);
    dict->max_cells = MAX_DICT_CELLS;
    dict->here =  0;
    dict->last_word_idx = 0;
    dict->varidx = 1;

    tbforth_bootstrap();
  }

  do {
    stat=interpret();
  } while (1);

}
 
