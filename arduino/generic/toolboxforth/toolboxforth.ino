// MSP430 Arduino
//
// #include <HardwareSerial.h>
extern "C" {
#include "tbforth.h"
#include "tbforth.img.h"
}

void load_ext_words () {
  OS_WORDS();
  MCU_WORDS();
}


HardwareSerial HS[]  = { Serial2};

tbforth_stat c_handle(void) {
  RAMC r1 = dpop();

  switch(r1) {
  case MCU_GPIO_MODE:
    pinMode(dpop(),dpop());
    break;
  case MCU_GPIO_READ:
    dpush(digitalRead(dpop()));
    break;
  case MCU_GPIO_WRITE:
    digitalWrite(dpop(), dpop());
    break;
  case MCU_UART_WRITE:
    r1 = dpop();
    HS[r1].write(dpop());
    break;
  case MCU_UART_READ:
    r1 = dpop();
    dpush(HS[r1].read());
    break;
  case MCU_UART_AVAIL:
    r1 = dpop();
    dpush(HS[r1].available());
    break;
  case MCU_UART_END:
    r1 = dpop();
    HS[r1].flush(); HS[r1].end();
    break;
  case MCU_UART_BEGIN:
    {
      r1 = dpop();
      RAMC baud = dpop();
      HS[r1].begin(baud);
    }
    break;
  case MCU_DELAY:
    delay(dpop());
    break;
  case OS_MS:		/* milliseconds */
    dpush(millis());
    break;
  case OS_EMIT:			/* emit */
    txc(dpop()&0xff);
    break;
  case OS_KEY:			/* key */
    dpush((CELL)rxc());
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
    case '\n':
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
      txs0("\r\nHuh? >>> ");
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

struct dict  *dict; // &flashdict;

void setup () {
  dict = &flashdict;
  Serial.begin(115200);
}

void loop() {
  while(!Serial);
  tbforth_init();
  tbforth_interpret("init");
  tbforth_interpret("cr memory cr");
  //  tbforth_interpret("quit");
  console();
}

