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
#define USB_CDC_NO_DELAY
// #define HAVE_FS
#define FATFS
#define MBED_TLS

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

enum {
  LOAD_ESP32_WORDS=100,
  GPIO_MODE,
  GPIO_READ,
  GPIO_WRITE,
  SPI_CFG,
  SPI_BEGIN_TRANS,
  SPI_WRITE,
  SPI_READ,
  SPI_END_TRANS,
  UART_BEGIN,
  UART_END,
  UART_AVAIL,
  UART_READ,
  UART_WRITE,
  WDT_CONFIG,
  WDT_RESET,
  DELAY,
  GPIO_WAKE,
  ENCRYPT,
  DECRYPT,
  ENCODE64,
  DECODE64,
  MAC,
  SLEEP
};
  

void load_esp32_words () {
  tbforth_interpret("mark ESP32");
  tbforth_cdef("gpio-mode", GPIO_MODE);
  tbforth_cdef("gpio-read", GPIO_READ);
  tbforth_cdef("gpio-write", GPIO_WRITE);
  tbforth_cdef("spi-config", SPI_CFG);
  tbforth_cdef("spi\{", SPI_BEGIN_TRANS);
  tbforth_cdef("spi!", SPI_WRITE);
  tbforth_cdef("spi@", SPI_READ);
  tbforth_cdef("\}spi", SPI_END_TRANS);
  tbforth_cdef("uart-begin", UART_BEGIN);
  tbforth_cdef("uart-end", UART_END);
  tbforth_cdef("uart?", UART_AVAIL);
  tbforth_cdef("uart@", UART_READ);
  tbforth_cdef("uart!", UART_WRITE);
  tbforth_cdef("/wdt", WDT_CONFIG);
  tbforth_cdef("wdt-rst", WDT_RESET);
  tbforth_cdef("delay", DELAY);
  tbforth_cdef("sleep", SLEEP);
  tbforth_cdef("gcm-encrypt", ENCRYPT);
  tbforth_cdef("gcm-decrypt", DECRYPT);
  tbforth_cdef("encode64", ENCODE64);
  tbforth_cdef("decode64", DECODE64);
  tbforth_cdef("mac", MAC);
}




#ifdef MBED_TLS
#include "mbedtls/gcm.h"
#include "mbedtls/base64.h"
mbedtls_gcm_context aes;

void encrypt (int dir, uint8_t *key, uint8_t *iv, uint8_t *in, uint8_t *out, int blocks) {
  mbedtls_gcm_init( &aes );
  mbedtls_gcm_setkey( &aes,MBEDTLS_CIPHER_ID_AES , (const unsigned char*) key, 256);
  mbedtls_gcm_crypt_and_tag( &aes, dir ? MBEDTLS_GCM_ENCRYPT : MBEDTLS_GCM_DECRYPT,
			     blocks*16,
			     (const unsigned char*)iv, 96/8,
			     NULL, 0,
  			     (const unsigned char*)in,
			     out,
			     4, out+(blocks*16));
  mbedtls_gcm_free( &aes );
}

#endif


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
  case SPI_CFG:
    {
      RAMC clkpin = dpop();
      RAMC misopin = dpop();
      RAMC mosipin = dpop();
      SPI.begin(clkpin,misopin,mosipin);
    }
    break;
  case SPI_BEGIN_TRANS:
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
  case WDT_CONFIG:
    r1 = dpop();
    if (r1 > 0) {
      esp_task_wdt_init(r1, true);    
      esp_task_wdt_add (NULL);
    } else {
      esp_task_wdt_delete (NULL);
      esp_task_wdt_deinit();
    }
    break;
  case WDT_RESET:
    esp_task_wdt_reset();
    break;
  case DELAY:
    delay(dpop());
    break;
  case SLEEP:
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
  case MAC:
    {
      uint64_t _macid = 0LL;
      esp_efuse_mac_get_default((uint8_t*) (&_macid));
      dpush(_macid >> 32);
      dpush(_macid & 0xFFFFFFFF);
    }
    break;

#ifdef MBED_TLS
  case ENCODE64:
    {
      RAMC inaddr = dpop();
      RAMC outaddr = dpop();
      size_t olen;
      uint8_t *in = (uint8_t*)&tbforth_ram[inaddr+1];
      size_t inlen = tbforth_ram[inaddr];
      uint8_t *out = (uint8_t*)&tbforth_ram[outaddr+1];
      size_t oslen = tbforth_ram[outaddr];
      mbedtls_base64_encode( out, oslen, &olen, in, inlen);
      dpush(olen);
    }
    break;
  case DECODE64:
    {
      RAMC inaddr = dpop();
      RAMC outaddr = dpop();
      size_t olen;
      uint8_t *in = (uint8_t*)&tbforth_ram[inaddr+1];
      size_t inlen = tbforth_ram[inaddr];
      uint8_t *out = (uint8_t*)&tbforth_ram[outaddr+1];
      size_t oslen = tbforth_ram[outaddr];
      mbedtls_base64_decode(out, oslen, &olen, in, inlen);
      dpush(olen);
    }
    break;
  case ENCRYPT:
  case DECRYPT:
    {
      RAMC inaddr = dpop();
      RAMC outaddr = dpop();
      RAMC blocks = dpop();
      RAMC keyaddr = dpop();
      RAMC ivaddr = dpop();
      uint8_t *key = (uint8_t*)&tbforth_ram[keyaddr+1];
      uint8_t *iv = (uint8_t*)&tbforth_ram[ivaddr+1];
      uint8_t *in = (uint8_t*)&tbforth_ram[inaddr+1];
      uint8_t *out = (uint8_t*)&tbforth_ram[outaddr+1];
      encrypt (r1 == ENCRYPT, key, iv, in, out, blocks);
    }
    break;
#endif
  case MS:		/* milliseconds */
    dpush(millis());
    break;
  case EMIT:			/* emit */
    txc(dpop()&0xff);
    break;
  case KEY:			/* key */
    dpush((CELL)rxc());
    break;
#ifdef HAVE_FS
  case SAVE_IMAGE:			/* save image */
    {
      FLASH_FS.begin(true);
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
      tbforth_init();
      tbforth_cdef("init-esp32", LOAD_ESP32_WORDS);
      tbforth_interpret("init-esp32");
    }
  }
#else
  tbforth_init();
  tbforth_cdef("init-esp32", LOAD_ESP32_WORDS);
  tbforth_interpret("init-esp32");
#endif

  tbforth_interpret("init");
  tbforth_interpret("cr memory cr");
  //  tbforth_interpret("quit");
  console();
}

