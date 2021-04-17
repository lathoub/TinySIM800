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
  Modified by lathoub (2021)
 ****************************************************/

#include <Arduino.h>

#include "TinySim800.h"

TinySIM800::TinySIM800(Stream &port)
    : mySerial(port)
{
  apn = 0;
  apnusername = 0;
  apnpassword = 0;
  ok_reply = F("OK");
}

bool TinySIM800::reset()
{
  resetting(this, NULL);

  return init();
}

bool TinySIM800::init()
{
  DEBUG_PRINTLN(F("Attempting to open comm with ATs"));
  // give 7 seconds to reboot
  int16_t timeout = 7000;

  while (timeout > 0)
  {
    while (mySerial.available())
      mySerial.read();
    if (sendCheckReply(F("AT"), ok_reply))
      break;
    while (mySerial.available())
      mySerial.read();
    if (sendCheckReply(F("AT"), F("AT")))
      break;
    delay(500);
    timeout -= 500;
  }

  if (timeout <= 0)
  {
    DEBUG_PRINTLN(F("Timeout: No response to AT... last ditch attempt."));

    sendCheckReply(F("AT"), ok_reply);
    delay(100);
    sendCheckReply(F("AT"), ok_reply);
    delay(100);
    sendCheckReply(F("AT"), ok_reply);
    delay(100);
  }

  // turn off Echo!
  sendCheckReply(F("ATE0"), ok_reply);
  delay(100);

  if (!sendCheckReply(F("ATE0"), ok_reply))
  {
    return false;
  }

  // turn on hangupitude
  sendCheckReply(F("AT+CVHU=0"), ok_reply);

  delay(100);
  flushInput();
  return true;
}

void TinySIM800::allowRoaming(bool value)
{
  _allowRoaming = value;
}

bool TinySIM800::setBaudrate(uint32_t baud)
{
  return sendCheckReply(F("AT+IPREX="), baud, ok_reply);
}

/* returns value in mV (uint16_t) */
bool TinySIM800::getBattVoltage(uint16_t *v)
{
  return sendParseReply(F("AT+CBC"), F("+CBC: "), v, ',', 2);
}

char *TinySIM800::getIMEI()
{
  getReply(F("AT+GSN"));

  return replybuffer;
}

char *TinySIM800::getVersion()
{
  getReply(F("ATI"));

  return replybuffer;
}

char *TinySIM800::getFirmware()
{
  getReply(F("AT+GMR"));

  return replybuffer;
}

// During sleep, the SIM800 module has its serial communication disabled. In
// order to reestablish communication pull the DRT-pin of the SIM800 module
// LOW for at least 50ms. Then use this function to disable sleep mode. The
// DTR-pin can then be released again.
bool TinySIM800::sleepEnable(bool enable = true)
{
    return sendCheckReply(F("AT+CSCLK="), enable, ok_reply);
}

/********* NETWORK *******************************************************/

bool TinySIM800::isRegistered()
{
  uint16_t status;

  if (!sendParseReply(F("AT+CREG?"), F("+CREG: "), &status, ',', 1))
    return 0;

  if (_allowRoaming)
    return (status == 1 || status == 5);
  else
    return (status == 1);
}

uint8_t TinySIM800::getRSSI()
{
  uint16_t reply;

  if (!sendParseReply(F("AT+CSQ"), F("+CSQ: "), &reply))
    return 0;

  return reply;
}

bool TinySIM800::sendUSSD(char *ussdmsg, char *ussdbuff, uint16_t maxlen, uint16_t *readlen)
{
  if (!sendCheckReply(F("AT+CUSD=1"), ok_reply))
    return false;

  char sendcmd[30] = "AT+CUSD=1,\"";
  strncpy(sendcmd + 11, ussdmsg, 30 - 11 - 2); // 11 bytes beginning, 2 bytes for close quote + null
  sendcmd[strlen(sendcmd)] = '\"';

  if (!sendCheckReply(sendcmd, ok_reply))
  {
    *readlen = 0;
    return false;
  }
  else
  {
    readline(10000); // read the +CUSD reply, wait up to 10 seconds!!!
    //DEBUG_PRINT("* "); DEBUG_PRINTLN(replybuffer);
    char *p = prog_char_strstr(replybuffer, PSTR("+CUSD: "));
    if (p == 0)
    {
      *readlen = 0;
      return false;
    }
    p += 7; //+CUSD
    // Find " to get start of ussd message.
    p = strchr(p, '\"');
    if (p == 0)
    {
      *readlen = 0;
      return false;
    }
    p += 1; //"
    // Find " to get end of ussd message.
    char *strend = strchr(p, '\"');

    uint16_t lentocopy = min(maxlen - 1, strend - p);
    strncpy(ussdbuff, p, lentocopy + 1);
    ussdbuff[lentocopy] = 0;
    *readlen = lentocopy;
  }
  return true;
}

bool TinySIM800::enableNetworkTimeSync(bool onoff)
{
  if (onoff)
  {
    if (!sendCheckReply(F("AT+CLTS=1"), ok_reply))
      return false;
  }
  else
  {
    if (!sendCheckReply(F("AT+CLTS=0"), ok_reply))
      return false;
  }

  flushInput(); // eat any 'Unsolicted Result Code'

  return true;
}

char *TinySIM800::getTime()
{
  getReply(F("AT+CCLK?"), (uint16_t)10000);
  if (strncmp(replybuffer, "+CCLK: ", 7) != 0)
    return NULL;

  char *p = replybuffer + strlen("+CCLK: \"");
  p[strlen(p) - 4] = 0;

  readline(); // eat OK

  return p;
}

bool TinySIM800::connectGPRS(const __FlashStringHelper *apn,
                             const __FlashStringHelper *username,
                             const __FlashStringHelper *password)
{
  if (isGPRSconnected())
    return true;

  //modem.isRegistered()

  // set bearer profile! connection type GPRS
  if (!sendCheckReply(F("AT+SAPBR=3,1,\"CONTYPE\",\"GPRS\""),
                      ok_reply, 10000))
    return false;

  // set bearer profile access point name
  if (apn)
  {
    // Send command AT+SAPBR=3,1,"APN","<apn value>" where <apn value> is the configured APN value.
    if (!sendCheckReplyQuoted(F("AT+SAPBR=3,1,\"APN\","), apn, ok_reply, 10000))
      return false;

    // send AT+CSTT,"apn","user","pass"
    flushInput();

    mySerial.print(F("AT+CSTT=\""));
    mySerial.print(apn);
    if (apnusername)
    {
      mySerial.print("\",\"");
      mySerial.print(apnusername);
    }
    if (apnpassword)
    {
      mySerial.print("\",\"");
      mySerial.print(apnpassword);
    }
    mySerial.println("\"");

    if (!expectReply(ok_reply))
      return false;

    // set username/password
    if (apnusername)
    {
      // Send command AT+SAPBR=3,1,"USER","<user>" where <user> is the configured APN username.
      if (!sendCheckReplyQuoted(F("AT+SAPBR=3,1,\"USER\","), apnusername, ok_reply, 10000))
        return false;
    }
    if (apnpassword)
    {
      // Send command AT+SAPBR=3,1,"PWD","<password>" where <password> is the configured APN password.
      if (!sendCheckReplyQuoted(F("AT+SAPBR=3,1,\"PWD\","), apnpassword, ok_reply, 10000))
        return false;
    }
  }

  // open GPRS context
  if (!sendCheckReply(F("AT+SAPBR=1,1"), ok_reply, 30000))
    return false;

  // bring up wireless connection
  if (!sendCheckReply(F("AT+CIICR"), ok_reply, 10000))
    return false;

  gprsConnected(this, NULL);

  return true;
}

bool TinySIM800::isGPRSconnected()
{
  uint16_t state;

  if (!sendParseReply(F("AT+CGATT?"), F("+CGATT: "), &state))
    return false;

  return (1 == state);
}

bool TinySIM800::disconnectGPRS()
{
  // disconnect all sockets
  if (!sendCheckReply(F("AT+CIPSHUT"), F("SHUT OK"), 20000))
    return false;

  // close GPRS context
  if (!sendCheckReply(F("AT+SAPBR=0,1"), ok_reply, 10000))
    return false;

  if (!sendCheckReply(F("AT+CGATT=0"), ok_reply, 10000))
    return false;

  gprsDisconnected(this, NULL);

  return true;
}

// TCP

bool TinySIM800::TCPconnect(char *server, uint16_t port)
{
  flushInput();

  // close all old connections
  if (!sendCheckReply(F("AT+CIPSHUT"), F("SHUT OK"), 20000))
    return false;

  // single connection at a time
  if (!sendCheckReply(F("AT+CIPMUX=0"), ok_reply))
    return false;

  // manually read data
  if (!sendCheckReply(F("AT+CIPRXGET=1"), ok_reply))
    return false;

  mySerial.print(F("AT+CIPSTART=\"TCP\",\""));
  mySerial.print(server);
  mySerial.print(F("\",\""));
  mySerial.print(port);
  mySerial.println(F("\""));

  if (!expectReply(ok_reply))
    return false;
  if (!expectReply(F("CONNECT OK")))
    return false;

  // looks like it was a success (?)
  return true;
}

bool TinySIM800::TCPclose()
{
  return sendCheckReply(F("AT+CIPCLOSE"), ok_reply);
}

bool TinySIM800::TCPconnected()
{
  if (!sendCheckReply(F("AT+CIPSTATUS"), ok_reply, 100))
    return false;
  readline(100);

  return (strcmp(replybuffer, "STATE: CONNECT OK") == 0);
}

bool TinySIM800::TCPsend(char *packet, uint8_t len)
{
  mySerial.print(F("AT+CIPSEND="));
  mySerial.println(len);
  readline();

  if (replybuffer[0] != '>')
    return false;

  mySerial.write(packet, len);
  readline(3000); // wait up to 3 seconds to send the data

  return (strcmp(replybuffer, "SEND OK") == 0);
}

uint16_t TinySIM800::TCPavailable()
{
  uint16_t avail;

  if (!sendParseReply(F("AT+CIPRXGET=4"), F("+CIPRXGET: 4,"), &avail, ',', 0))
    return false;

  DEBUG_PRINT(avail);
  DEBUG_PRINTLN(F(" bytes available"));

  return avail;
}

uint16_t TinySIM800::TCPread(uint8_t *buff, uint8_t len)
{
  uint16_t avail;

  mySerial.print(F("AT+CIPRXGET=2,"));
  mySerial.println(len);

  readline();

  if (!parseReply(F("+CIPRXGET: 2,"), &avail, ',', 0))
  {
    return false;
  }

  readRaw(avail);

  DEBUG_PRINT(avail);
  DEBUG_PRINTLN(F(" bytes read"));
  for (uint8_t i = 0; i < avail; i++)
  {
    DEBUG_PRINT(F(" 0x"));
    DEBUG_PRINT(replybuffer[i], HEX);
  }
  DEBUG_PRINTLN();

  memcpy(buff, replybuffer, avail);

  return avail;
}

// HTTP

bool TinySIM800::initiateHTTP(const char *url, const char *headers)
{
  beforeHTTPConnect(this, NULL);

  // Init HTTP connection
  if (!sendCheckReply(F("AT+HTTPINIT"), ok_reply, 100))
    return false;

  // Connect HTTP through GPRS bearer
  if (!sendCheckReply(F("AT+HTTPPARA=\"CID\",1"), ok_reply, 100))
    return false;

  mySerial.print(F("AT+HTTPPARA=\"URL\",\""));
  mySerial.print(url);
  mySerial.println(F("\""));
  if (!expectReply(ok_reply))
    return;

  // expecting a json reply
  if (!sendCheckReply(F("AT+HTTPPARA=\"CONTENT\",\"application/json\"\r\n"), ok_reply, 100))
    return false;

  if (headers != NULL)
    if (!sendCheckReply(F("AT+HTTPPARA=\"USERDATA\","), ok_reply, 100))
      return false;

  return true;
}

bool TinySIM800::postHTTP(const char *url,
                          const char *headers,
                          uint16_t (*ptrMeasureBody)(),
                          void (*ptrStreamBody)(const Stream &),
                          void (*ptrStatusCode)(const uint16_t),
                          void (*ptrResponse)(char *))
{
  if (!initiateHTTP(url, headers))
    return false;

  mySerial.print(F("AT+HTTPDATA="));
  mySerial.print(ptrMeasureBody());
  mySerial.print(F(","));
  mySerial.println(10000);

  if (!expectReply(F("DOWNLOAD")))
    return;

  ptrStreamBody(mySerial);

  delay(100);

  // do POST, initial answer is OK, second part is +HTTPACTION:
  if (!sendCheckReply(F("AT+HTTPACTION=1"), ok_reply, 100))
    return;
  readline(10000);
  uint16_t statusCode = 0;
  if (!parseReply(F("+HTTPACTION: 1,"), &statusCode, ',', 0))
    return false;
  uint16_t dataLength = 0;
  if (!parseReply(F("+HTTPACTION: 1,"), &dataLength, ',', 1))
    return false;

  if (ptrStatusCode)
    ptrStatusCode(statusCode);

  if (ptrResponse)
  {
    int step = sizeof(replybuffer);
    for (int i = 0; i < dataLength; i += step)
    {
      auto amount = (dataLength - i) > step ? step : (dataLength - i);

      mySerial.print(F("AT+HTTPREAD="));
      mySerial.print(i);
      mySerial.print(",");
      mySerial.println(amount);

      expectReply(F("+HTTPREAD: "));

      readRaw(amount);

      ptrResponse(replybuffer);

      flushInput();
    }
  }

  if (!terminateHTTP())
    return false;

  return true;
}

bool TinySIM800::terminateHTTP()
{
  if (!sendCheckReply(F("AT+HTTPTERM"), ok_reply, 100))
    return false;

  afterHTTPDisconnect(this, NULL);

  return true;
}

/********* HELPERS *********************************************/

bool TinySIM800::expectReply(const __FlashStringHelper *reply,
                             uint16_t timeout)
{
  readline(timeout);

  return (prog_char_strcmp(replybuffer, (prog_char *)reply) == 0);
}

void TinySIM800::flushInput()
{
  // Read all available serial input to flush pending data.
  uint16_t timeoutloop = 0;
  while (timeoutloop++ < 40)
  {
    while (mySerial.available())
    {
      mySerial.read();
      timeoutloop = 0; // If char was received reset the timer
    }
    delay(1);
  }
}

uint16_t TinySIM800::readRaw(uint16_t b)
{
  uint16_t idx = 0;

  while (b && (idx < sizeof(replybuffer) - 1))
  {
    if (mySerial.available())
    {
      replybuffer[idx] = mySerial.read();
      idx++;
      b--;
    }
  }
  replybuffer[idx] = 0;

  return idx;
}

uint8_t TinySIM800::readline(uint16_t timeout, bool multiline)
{
  uint16_t replyidx = 0;

  while (timeout--)
  {
    if (replyidx >= 127)
    {
      //DEBUG_PRINTLN(F("SPACE"));
      break;
    }

    while (mySerial.available())
    {
      char c = mySerial.read();
      if (c == '\r')
        continue;
      if (c == 0xA)
      {
        if (replyidx == 0) // the first 0x0A is ignored
          continue;

        if (!multiline)
        {
          if (0 == strncmp(replybuffer, "*PSNWID:", strlen("*PSNWID:")))
          {
            DEBUG_PRINTLN(F("### Network name updated."));
            replyidx = 0;
            timeout = 30000;
          }
          else if (0 == strncmp(replybuffer, "*PSUTTZ:", strlen("*PSUTTZ:")))
          {
            DEBUG_PRINTLN(F("### Network time and time zone updated."));
            replyidx = 0;
            timeout = 30000;
          }
          else if (0 == strncmp(replybuffer, "DST:", strlen("DST:")))
          {
            DEBUG_PRINTLN(F("### Refresh Network Daylight Saving Time by network."));
            replyidx = 0;
            timeout = 30000;
          }
          else if (0 == strncmp(replybuffer, "+CTZV:", strlen("+CTZV:")))
          {
            DEBUG_PRINTLN(F("### Network time zone updated."));
            replyidx = 0;
            timeout = 30000;
          }
          else if (0 == strncmp(replybuffer, "+HTTPACTION:", strlen("+HTTPACTION:")))
          {
            timeout = 100;
          }
          else if (0 == strncmp(replybuffer, "+HTTPREAD:", strlen("+HTTPREAD:")))
          {
            timeout = 0;
          }
          else
            timeout = 0; // the second 0x0A is the end of the line
          break;
        }
      }
      replybuffer[replyidx] = c;
      //      DEBUG_PRINT(c, HEX); DEBUG_PRINT("#"); DEBUG_PRINTLN(c);
      replyidx++;
    }

    if (timeout == 0)
    {
      DEBUG_PRINTLN(F("TIMEOUT"));
      break;
    }

    delay(1);
  }

  replybuffer[replyidx] = 0; // null term
  return replyidx;
}

uint8_t TinySIM800::getReply(char *send, uint16_t timeout)
{
  flushInput();

  mySerial.println(send);

  uint8_t l = readline(timeout);

  return l;
}

uint8_t TinySIM800::getReply(const __FlashStringHelper *send, uint16_t timeout)
{
  flushInput();

  mySerial.println(send);

  uint8_t l = readline(timeout);

  return l;
}

// Send prefix, suffix, and newline. Return response (and also set replybuffer with response).
uint8_t TinySIM800::getReply(const __FlashStringHelper *prefix, char *suffix, uint16_t timeout)
{
  flushInput();

  mySerial.print(prefix);
  mySerial.println(suffix);

  uint8_t l = readline(timeout);

  return l;
}

// Send prefix, suffix, and newline. Return response (and also set replybuffer with response).
uint8_t TinySIM800::getReply(const __FlashStringHelper *prefix, int32_t suffix, uint16_t timeout)
{
  flushInput();

  mySerial.print(prefix);
  mySerial.println(suffix, DEC);

  uint8_t l = readline(timeout);

  return l;
}

// Send prefix, suffix, suffix2, and newline. Return response (and also set replybuffer with response).
uint8_t TinySIM800::getReply(const __FlashStringHelper *prefix, int32_t suffix1, int32_t suffix2, uint16_t timeout)
{
  flushInput();

  mySerial.print(prefix);
  mySerial.print(suffix1);
  mySerial.print(',');
  mySerial.println(suffix2, DEC);

  uint8_t l = readline(timeout);

  return l;
}

// Send prefix, ", suffix, ", and newline. Return response (and also set replybuffer with response).
uint8_t TinySIM800::getReplyQuoted(const __FlashStringHelper *prefix, const __FlashStringHelper *suffix, uint16_t timeout)
{
  flushInput();

  mySerial.print(prefix);
  mySerial.print('"');
  mySerial.print(suffix);
  mySerial.println('"');

  uint8_t l = readline(timeout);

  return l;
}

bool TinySIM800::sendCheckReply(char *send, char *reply, uint16_t timeout)
{
  if (!getReply(send, timeout))
    return false;
  /*
  for (uint8_t i=0; i<strlen(replybuffer); i++) {
  DEBUG_PRINT(replybuffer[i], HEX); DEBUG_PRINT(" ");
  }
  DEBUG_PRINTLN();
  for (uint8_t i=0; i<strlen(reply); i++) {
    DEBUG_PRINT(reply[i], HEX); DEBUG_PRINT(" ");
  }
  DEBUG_PRINTLN();
  */
  return (strcmp(replybuffer, reply) == 0);
}

bool TinySIM800::sendCheckReply(const __FlashStringHelper *send, const __FlashStringHelper *reply, uint16_t timeout)
{
  if (!getReply(send, timeout))
    return false;

  return (prog_char_strcmp(replybuffer, (prog_char *)reply) == 0);
}

bool TinySIM800::sendCheckReply(char *send, const __FlashStringHelper *reply, uint16_t timeout)
{
  if (!getReply(send, timeout))
    return false;
  return (prog_char_strcmp(replybuffer, (prog_char *)reply) == 0);
}

// Send prefix, suffix, and newline.  Verify FONA response matches reply parameter.
bool TinySIM800::sendCheckReply(const __FlashStringHelper *prefix, char *suffix, const __FlashStringHelper *reply, uint16_t timeout)
{
  getReply(prefix, suffix, timeout);
  return (prog_char_strcmp(replybuffer, (prog_char *)reply) == 0);
}

// Send prefix, suffix, and newline.  Verify FONA response matches reply parameter.
bool TinySIM800::sendCheckReply(const __FlashStringHelper *prefix, int32_t suffix, const __FlashStringHelper *reply, uint16_t timeout)
{
  getReply(prefix, suffix, timeout);
  return (prog_char_strcmp(replybuffer, (prog_char *)reply) == 0);
}

// Send prefix, suffix, suffix2, and newline.  Verify FONA response matches reply parameter.
bool TinySIM800::sendCheckReply(const __FlashStringHelper *prefix, int32_t suffix1, int32_t suffix2, const __FlashStringHelper *reply, uint16_t timeout)
{
  getReply(prefix, suffix1, suffix2, timeout);
  return (prog_char_strcmp(replybuffer, (prog_char *)reply) == 0);
}

// Send prefix, ", suffix, ", and newline.  Verify FONA response matches reply parameter.
bool TinySIM800::sendCheckReplyQuoted(const __FlashStringHelper *prefix, const __FlashStringHelper *suffix, const __FlashStringHelper *reply, uint16_t timeout)
{
  getReplyQuoted(prefix, suffix, timeout);
  return (prog_char_strcmp(replybuffer, (prog_char *)reply) == 0);
}

bool TinySIM800::parseReply(const __FlashStringHelper *toreply,
                            uint16_t *v, char divider, uint8_t index)
{
  char *p = prog_char_strstr(replybuffer, (prog_char *)toreply); // get the pointer to the voltage
  if (p == 0)
    return false;
  p += prog_char_strlen((prog_char *)toreply);
  //DEBUG_PRINTLN(p);
  for (uint8_t i = 0; i < index; i++)
  {
    // increment dividers
    p = strchr(p, divider);
    if (!p)
      return false;
    p++;
    //DEBUG_PRINTLN(p);
  }
  *v = atoi(p);

  return true;
}

bool TinySIM800::parseReply(const __FlashStringHelper *toreply,
                            char *v, char divider, uint8_t index)
{
  uint8_t i = 0;
  char *p = prog_char_strstr(replybuffer, (prog_char *)toreply);
  if (p == 0)
    return false;
  p += prog_char_strlen((prog_char *)toreply);

  for (i = 0; i < index; i++)
  {
    // increment dividers
    p = strchr(p, divider);
    if (!p)
      return false;
    p++;
  }

  for (i = 0; i < strlen(p); i++)
  {
    if (p[i] == divider)
      break;
    v[i] = p[i];
  }

  v[i] = '\0';

  return true;
}

// Parse a quoted string in the response fields and copy its value (without quotes)
// to the specified character array (v).  Only up to maxlen characters are copied
// into the result buffer, so make sure to pass a large enough buffer to handle the
// response.
bool TinySIM800::parseReplyQuoted(const __FlashStringHelper *toreply,
                                  char *v, int maxlen, char divider, uint8_t index)
{
  uint8_t i = 0, j;
  // Verify response starts with toreply.
  char *p = prog_char_strstr(replybuffer, (prog_char *)toreply);
  if (p == 0)
    return false;
  p += prog_char_strlen((prog_char *)toreply);

  // Find location of desired response field.
  for (i = 0; i < index; i++)
  {
    // increment dividers
    p = strchr(p, divider);
    if (!p)
      return false;
    p++;
  }

  // Copy characters from response field into result string.
  for (i = 0, j = 0; j < maxlen && i < strlen(p); ++i)
  {
    // Stop if a divier is found.
    if (p[i] == divider)
      break;
    // Skip any quotation marks.
    else if (p[i] == '"')
      continue;
    v[j++] = p[i];
  }

  // Add a null terminator if result string buffer was not filled.
  if (j < maxlen)
    v[j] = '\0';

  return true;
}

bool TinySIM800::sendParseReply(const __FlashStringHelper *tosend,
                                const __FlashStringHelper *toreply,
                                uint16_t *v, char divider, uint8_t index)
{
  getReply(tosend);

  if (!parseReply(toreply, v, divider, index))
    return false;

  readline(); // eat 'OK'

  return true;
}
