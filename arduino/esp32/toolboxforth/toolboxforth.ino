// ESP32 Toolboxforth.
//
#include <SPI.h>
#include <Wire.h>

extern "C" {
#include "tbforth.h"
#include "tbforth.img.h"
}

#include <esp_task_wdt.h>

// define if using USB_CDC for console and don't want delays when not plugged in
//
// #define USB_CDC_NO_DELAY
#define HAVE_FS
// #define FATFS


#ifdef HAVE_FS
#ifdef FATFS
#include <FFat.h>
#define FLASH_FS FFat
#else
#ifdef LITTLEFS
#include <LittleFS.h>
#define FLASH_FS LittleFS
#else
#include <SPIFFS.h>
#define FLASH_FS SPIFFS
#endif
#endif
#endif


void load_ext_words () {
  OS_WORDS();
  MCU_WORDS();
}


#if defined(ARDUINO_ESP32S2_DEV) || defined(ARDUINO_ESP32C3_DEV)
HardwareSerial Serial2(1);
#endif
HardwareSerial HS [] = {  Serial1, Serial2 };

tbforth_stat c_handle(void) {
  RAMC r1 = dpop();

  if (Serial.available() && Serial.read() == '~') {
      tbforth_abort_request(ABORT_CTRL_C);
      tbforth_abort();	
      return E_ABORT;
  }

  switch(r1) {
    RAMC pin, mode, val;
  case MCU_GPIO_MODE:
    pin = dpop();
    mode = dpop();
    pinMode(pin,mode);
    break;
  case MCU_GPIO_READ:
    dpush(digitalRead(dpop()));
    break;
  case MCU_GPIO_WRITE:
    pin = dpop();
    val = dpop();
    digitalWrite(pin, val);
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
      RAMC rx  = dpop();
      RAMC tx  = dpop();
      HS[r1].begin(baud, SERIAL_8N1, rx, tx);
    }
    break;
  case MCU_SPI_CFG:
    {
      RAMC clkpin = dpop();
      RAMC misopin = dpop();
      RAMC mosipin = dpop();
      SPI.begin(clkpin,misopin,mosipin);
    }
    break;
  case MCU_SPI_BEGIN_TRANS:
    {
      RAMC speed = dpop();
      RAMC bitorder = dpop();
      RAMC mode = dpop();
      RAMC cspin = dpop();
      pinMode(cspin, OUTPUT);
      digitalWrite(cspin, HIGH);
      SPI.beginTransaction(SPISettings(((unsigned long)speed), bitorder, mode));
      digitalWrite(cspin, LOW);
    }
    break;
  case MCU_SPI_WRITE:
    SPI.transfer(dpop());
    break;
  case MCU_SPI_READ:
    dpush(SPI.transfer(0));
    break;
  case MCU_SPI_END_TRANS:
    digitalWrite(dpop(), HIGH);
    SPI.endTransaction();
    break;
  case MCU_COLD:
#ifdef HAVE_FS
    if (FLASH_FS.exists("/TBFORTH.IMG")) {
      FLASH_FS.remove("/TBFORTH.IMG");
    }
#endif
  case MCU_RESTART:
    esp_restart();
    break;
  case MCU_WDT_CONFIG:
    if (r1 > 0) {
      esp_task_wdt_init(r1, true);    
      esp_task_wdt_add (NULL);
    } else {
      esp_task_wdt_delete (NULL);
      esp_task_wdt_deinit();
    }
    break;
  case MCU_WDT_RESET:
    esp_task_wdt_reset();
    break;
  case MCU_DELAY:
    delay(dpop());
    break;
  case MCU_WAKE_REASON:
    dpush(esp_sleep_get_wakeup_cause());
    break;
  case MCU_SLEEP:
    {
      RAMC deepsleep = dpop();
      RAMC ms = dpop();
      RAMC wake_on_up = dpop();
      RAMC maskL = dpop();
      RAMC maskH = dpop();
      uint64_t mask = maskH << 32 | maskL;

      esp_sleep_disable_wakeup_source (ESP_SLEEP_WAKEUP_ALL);

      if (ms > 0)
	esp_sleep_enable_timer_wakeup((uint64_t)ms * 1000ULL);
	
#ifndef ARDUINO_ESP32C3_DEV
      //  Just for UART wakes???
      //    gpio_sleep_set_direction(GPIO_NUM_??, GPIO_MODE_INPUT);
      // https://github.com/espressif/arduino-esp32/issues/6976 ?
      if (wake_on_up) {
	esp_sleep_enable_ext1_wakeup(mask,ESP_EXT1_WAKEUP_ANY_HIGH);
      } else {
	esp_sleep_enable_ext1_wakeup(mask,ESP_EXT1_WAKEUP_ALL_LOW);
      }
#else
      if (wake_on_up) {
	esp_deep_sleep_enable_gpio_wakeup(mask,ESP_GPIO_WAKEUP_GPIO_HIGH);
      } else {
	esp_deep_sleep_enable_gpio_wakeup(mask,ESP_GPIO_WAKEUP_GPIO_LOW);
      }
#endif
      if (deepsleep) {
#ifndef ARDUINO_ESP32C3_DEV
	gpio_deep_sleep_hold_en();
#endif
	esp_deep_sleep_start();
      } else {
	esp_light_sleep_start();
      }
    }
    break;
  case MCU_MAC:
    {
      uint64_t _macid = 0LL;
      esp_efuse_mac_get_default((uint8_t*) (&_macid));
      dpush(_macid >> 32);
      dpush(_macid & 0xFFFFFFFF);
    }
    break;
  case OS_SECS:
    {
      time_t now;
      time(&now);
      dpush(now);
    }
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
#ifdef HAVE_FS
  case OS_INCLUDE:
    (void)tbforth_next_word();    
    break;
  case OS_SAVE_IMAGE:			/* save image */
    {
      //      FLASH_FS.begin(true);
      File fp;
      uint32_t dict_size= (dict_here())*sizeof(CELL);
      fp = FLASH_FS.open("/TBFORTH.IMG", FILE_WRITE);
      fp.write(DICT_VERSION);
      fp.write((dict_size >> 24) & 0xFF);
      fp.write((dict_size >> 16) & 0xFF);
      fp.write((dict_size >> 8) & 0xFF);
      fp.write(dict_size & 0xFF);
      fp.write((uint8_t*)dict, dict_size);
      fp.close();
    }
    break;
#endif
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

struct dict  *dict = &flashdict;

void setup () {
  Serial.begin(115200);
#ifdef USB_CDC_NO_DELAY
  Serial.setTxTimeoutMs(0);
#endif
}

void loop() {
#ifdef HAVE_FS
  if (FLASH_FS.begin(true) && FLASH_FS.exists("/TBFORTH.IMG")) {
    txs0("trying to read TBFORTH.IMG");
    File fp = FLASH_FS.open("/TBFORTH.IMG");
    if (fp && fp.read() == DICT_VERSION) {
      uint32_t dsize = fp.read() << 24 | fp.read() << 16 | fp.read() << 8 | fp.read();
      fp.read((uint8_t*)dict, dsize);
      fp.close();
      tbforth_init();
    } else {
      txs0("Can't load /TBFORTH.IMG...");
    }
  }
#endif
  tbforth_init();

  tbforth_interpret("init");
  tbforth_interpret("cr memory cr");
  console();
}

