#include <SPI.h>
#include <Wire.h>
extern "C" {
#include "toolboxforth.h"
}
#include "toolboxforth.img.h"

enum {
  LOAD_ESP32_WORDS=100,
  GPIO_MODE,
  GPIO_READ,
  GPIO_WRITE,
  SPI_BEGIN,
  SPI_BEGIN_TRANS,
  SPI_WRITE,
  SPI_READ,
  SPI_END_TRANS,
  UART_BEGIN,
  UART_END,
  UART_AVAIL,
  UART_READ,
  UART_WRITE
};
  
void tbforth_cdef (char* name, int val) {
  char n[80];
  snprintf(n, 80, ": %s %d cf ;", name, val);
  tbforth_interpret(n);
}

void load_esp32_words () {
  tbforth_cdef("gpio-mode", GPIO_MODE);
  tbforth_cdef("gpio-read", GPIO_READ);
  tbforth_cdef("gpio-write", GPIO_WRITE);
  tbforth_cdef("spi-begin", SPI_BEGIN);
  tbforth_cdef("spi-begin-trans", SPI_BEGIN_TRANS);
  tbforth_cdef("spi-write", SPI_WRITE);
  tbforth_cdef("spi-read", SPI_READ);
  tbforth_cdef("spi-end-trans", SPI_END_TRANS);
  tbforth_cdef("uart-begin", UART_BEGIN);
  tbforth_cdef("uart-end", UART_END);
  tbforth_cdef("uart-avail", UART_AVAIL);
  tbforth_cdef("uart-read", UART_READ);
  tbforth_cdef("uart-write", UART_WRITE);
}


SPIClass *spiclass = &SPI;

#if defined(ARDUINO_ESP32S2_DEV) || defined(ARDUINO_ESP32C3_DEV)
HardwareSerial Serial2(1);
#endif
HardwareSerial HS [] = { Serial0, Serial1, Serial2 };

tbforth_stat c_handle(void) {
  RAMC r1 = dpop();

  switch(r1) {
  case LOAD_ESP32_WORDS:
    load_esp32_words ();
    break;
  case GPIO_MODE:
    pinMode(dpop(),dpop());
    break;
  case GPIO_READ:
    dpush(digitalRead(dpop()));
    break;
  case GPIO_WRITE:
    digitalWrite(dpop(), dpop());
    break;
  case UART_WRITE:
    r1 = dpop();
    HS[r1].write(dpop());
    break;
  case UART_READ:
    r1 = dpop();
    dpush(HS[r1].read());
    break;
  case UART_AVAIL:
    r1 = dpop();
    dpush(HS[r1].available());
    break;
  case UART_END:
    r1 = dpop();
    HS[r1].flush(); HS[r1].end();
    break;
  case UART_BEGIN:
    {
      r1 = dpop();
      RAMC baud = dpop();
      RAMC rx  = dpop();
      RAMC tx  = dpop();
      HS[r1].begin(baud, SERIAL_8N1, rx, tx);
    }
    break;
  case SPI_BEGIN:
    {
      RAMC clkpin = dpop();
      RAMC misopin = dpop();
      RAMC mosipin = dpop();
      SPI.begin(clkpin,misopin,mosipin);
      spiclass->begin();
    }
    break;
  case SPI_BEGIN_TRANS:
    {
      RAMC speed = dpop();
      RAMC bitorder = dpop();
      RAMC mode = dpop();
      RAMC cspin = dpop();
      SPI.beginTransaction(SPISettings(((unsigned long)speed), bitorder, mode));
      digitalWrite(cspin, LOW);
    }
    break;
  case SPI_WRITE:
    SPI.transfer(dpop());
    break;
  case SPI_READ:
    dpush(SPI.transfer(0));
    break;
  case SPI_END_TRANS:
    digitalWrite(dpop(), HIGH);
    SPI.endTransaction();
    break;
  case UF_MS:		/* milliseconds */
    {
      dpush(millis());
    }
    break;
  case UF_EMIT:			/* emit */
    txc(dpop()&0xff);
    break;
  case UF_KEY:			/* key */
    dpush((CELL)rxc());
    break;
  case UF_READB:
    break;
  case UF_WRITEB:

    break;
    }
  return U_OK;
}

/* A static variable for holding the line. */
static char line_read[128];


inline char rxc(){
  while (!Serial.available());
  return Serial.read();
}

void txc(uint8_t c) {
  Serial.write(c);
}
void txs(char* s, int cnt) {
  int i;
  
  for(i = 0;i < cnt;i++) {
    txc((uint8_t)s[i]);
  }
}
#define txs0(s) txs(s,strlen(s))

/* Read a string, and return a pointer to it.
   Returns NULL on EOF. */
char * rl_gets () {
  int i = 0;
  char c;
  while (i < 128) {
    c = rxc();
    txc(c);
    switch(c) {
    case 3:
      txs0(" ^C\n");
      line_read[0] = '\0';
      return line_read;
      break;
    case '\r':
      line_read[i] = '\0';
      txc('\n');
      return line_read;
    case 8:
      if (i > 0) i--;
      break;
    default:
      line_read[i++] = c;
      break;
    }
  }
  line_read[127] = 0;
  return line_read;
}


char *line;
void console() {
  int stat;
  int16_t lineno = 0;
  while (1) {
    ++lineno;
    txs0(" ok\r\n");
    line=rl_gets(); // if (line==NULL) exit(0);
    if (line[0] == '\r' || line[0] == '\0') continue;
    stat = tbforth_interpret(line);
    switch(stat) {
    case E_NOT_A_WORD:
      txs0("Huh? >>> ");
      txs(&tbforth_iram->tib[tbforth_iram->tibwordidx],tbforth_iram->tibwordlen);
      txs0(" <<< ");
      txs(&tbforth_iram->tib[tbforth_iram->tibwordidx + tbforth_iram->tibwordlen],
	  tbforth_iram->tibclen - 
	  (tbforth_iram->tibwordidx + tbforth_iram->tibwordlen));
      txs0("\r\n");
      break;
    case E_ABORT:
      txs0("Abort!:<"); txs0(line); txs0(">\n");
      break;
    case E_STACK_UNDERFLOW:
      txs0("Stack underflow!\n");
      break;
    case E_DSTACK_OVERFLOW:
      txs0("Stack overflow!\n");
      break;
    case E_RSTACK_OVERFLOW:
      txs0("Return Stack overflow!\n");
      break;
    case U_OK:
      break;
    default:
      txs0("Ugh\n");
      break;
    }
  }
}

struct dict  *dict = &flashdict;

void setup () {
  Serial.begin(115200);
}

void loop() {
  tbforth_init();

  tbforth_interpret("init");
  tbforth_cdef("init-esp32", LOAD_ESP32_WORDS);
  tbforth_interpret("cr memory cr");
  console();
}

