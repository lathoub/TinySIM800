/***************************************************
  This is a library for our Adafruit FONA Cellular Module

  Designed specifically to work with the Adafruit FONA
  ----> http://www.adafruit.com/products/1946
  ----> http://www.adafruit.com/products/1963

  These displays use TTL Serial to communicate, 2 pins are required to
  interface
  Adafruit invests time and resources providing this open source code,
  please support Adafruit and open-source hardware by purchasing
  products from Adafruit!

  Written by Limor Fried/Ladyada for Adafruit Industries.
  BSD license, all text above must be included in any redistribution
  
  Modified by Brian Ejike (2017)
 ****************************************************/

#pragma once

#include "Events.h"
#include <TinyDebug.h>

#define FONA_DEFAULT_TIMEOUT_MS 500

#define prog_char char PROGMEM

#define prog_char_strcmp(a, b) strcmp_P((a), (b))
// define prog_char_strncmp(a, b, c)				strncmp_P((a), (b), (c))
#define prog_char_strstr(a, b) strstr_P((a), (b))
#define prog_char_strlen(a) strlen_P((a))
#define prog_char_strcpy(to, fromprogmem) strcpy_P((to), (fromprogmem))
//define prog_char_strncpy(to, from, len)		strncpy_P((to), (fromprogmem), (len))

class TinySIM800
{
public:
        Event<EventFunc> resetting;
        Event<EventFunc> pinCode;
        Event<EventFunc> networkRegistered;
        Event<EventFunc> networkLost;
        Event<EventFunc> gprsConnected;
        Event<EventFunc> gprsDisconnected;
        Event<EventFunc> timeout;
        Event<EventFunc> beforeHTTPConnect;
        Event<EventFunc> afterHTTPDisconnect;

public:
        TinySIM800(Stream &);

        bool reset();
        bool init();

        // FONA 3G requirements
        bool setBaudrate(uint32_t baud);
        void allowRoaming(bool);

        // RTC
        bool enableRTC(uint8_t i);
        bool readRTC(uint8_t *year, uint8_t *month, uint8_t *date, uint8_t *hr, uint8_t *min, uint8_t *sec);

        // Battery and ADC
        bool getADCVoltage(uint16_t *v);
        bool getBattPercent(uint16_t *p);
        bool getBattVoltage(uint16_t *v);

        bool sleepEnable(bool);

        // SIM query
        bool isRegistered();
        uint8_t getRSSI();
        char *getIMEI();
        char *getVersion();
        char *getFirmware();

        bool sendUSSD(char *ussdmsg, char *ussdbuff, uint16_t maxlen, uint16_t *readlen);

        // Time
        bool enableNetworkTimeSync(bool onoff);
        char* getTime();

        // GPRS handling
        bool isGPRSconnected();
        bool connectGPRS(const __FlashStringHelper *apn, const __FlashStringHelper *username = 0, const __FlashStringHelper *password = 0);
        bool disconnectGPRS();

        // TCP raw connections
        bool TCPconnect(char *server, uint16_t port);
        bool TCPclose();
        bool TCPconnected();
        bool TCPsend(char *packet, uint8_t len);
        uint16_t TCPavailable();
        uint16_t TCPread(uint8_t *buff, uint8_t len);

        // HTTP connect
        bool postHTTP(const char *, const char *,
                      uint16_t (*ptrMeasureBody)(),
                      void (*ptrStreamBody)(const Stream&),
                      void (*ptrStatusCode)(const uint16_t),
                      void (*ptr)(char *) = NULL);

        // Helper functions to verify responses.
        bool expectReply(const __FlashStringHelper *reply, uint16_t timeout = 10000);
        bool sendCheckReply(char *send, char *reply, uint16_t timeout = FONA_DEFAULT_TIMEOUT_MS);
        bool sendCheckReply(const __FlashStringHelper *send, const __FlashStringHelper *reply, uint16_t timeout = FONA_DEFAULT_TIMEOUT_MS);
        bool sendCheckReply(char *send, const __FlashStringHelper *reply, uint16_t timeout = FONA_DEFAULT_TIMEOUT_MS);

protected:
        bool _allowRoaming;
        uint8_t _type;

        char replybuffer[255];
        const __FlashStringHelper *apn;
        const __FlashStringHelper *apnusername;
        const __FlashStringHelper *apnpassword;
        const __FlashStringHelper *ok_reply;

        bool initiateHTTP(const char *url, const char *headers = NULL);
        bool terminateHTTP();

        void flushInput();
        uint16_t readRaw(uint16_t b);
        uint8_t readline(uint16_t timeout = FONA_DEFAULT_TIMEOUT_MS, bool multiline = false);
        uint8_t getReply(char *send, uint16_t timeout = FONA_DEFAULT_TIMEOUT_MS);
        uint8_t getReply(const __FlashStringHelper *send, uint16_t timeout = FONA_DEFAULT_TIMEOUT_MS);
        uint8_t getReply(const __FlashStringHelper *prefix, char *suffix, uint16_t timeout = FONA_DEFAULT_TIMEOUT_MS);
        uint8_t getReply(const __FlashStringHelper *prefix, int32_t suffix, uint16_t timeout = FONA_DEFAULT_TIMEOUT_MS);
        uint8_t getReply(const __FlashStringHelper *prefix, int32_t suffix1, int32_t suffix2, uint16_t timeout); // Don't set default value or else function call is ambiguous.
        uint8_t getReplyQuoted(const __FlashStringHelper *prefix, const __FlashStringHelper *suffix, uint16_t timeout = FONA_DEFAULT_TIMEOUT_MS);

        bool sendCheckReply(const __FlashStringHelper *prefix, char *suffix, const __FlashStringHelper *reply, uint16_t timeout = FONA_DEFAULT_TIMEOUT_MS);
        bool sendCheckReply(const __FlashStringHelper *prefix, int32_t suffix, const __FlashStringHelper *reply, uint16_t timeout = FONA_DEFAULT_TIMEOUT_MS);
        bool sendCheckReply(const __FlashStringHelper *prefix, int32_t suffix, int32_t suffix2, const __FlashStringHelper *reply, uint16_t timeout = FONA_DEFAULT_TIMEOUT_MS);
        bool sendCheckReplyQuoted(const __FlashStringHelper *prefix, const __FlashStringHelper *suffix, const __FlashStringHelper *reply, uint16_t timeout = FONA_DEFAULT_TIMEOUT_MS);

        bool parseReply(const __FlashStringHelper *toreply,
                           uint16_t *v, char divider = ',', uint8_t index = 0);
        bool parseReply(const __FlashStringHelper *toreply,
                           char *v, char divider = ',', uint8_t index = 0);
        bool parseReplyQuoted(const __FlashStringHelper *toreply,
                                 char *v, int maxlen, char divider, uint8_t index);

        bool sendParseReply(const __FlashStringHelper *tosend,
                               const __FlashStringHelper *toreply,
                               uint16_t *v, char divider = ',', uint8_t index = 0);

private:
        Stream &mySerial;
};
