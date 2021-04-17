#include <ArduinoJson.h>

#define SerialMon Serial
#include <TinyDebug.h>

#define GPS_BAUDRATE 9600
#define MODEM_BAUDRATE 38400

#define FONA_RX 9
#define FONA_TX 8
#define FONA_RST 4
#define FONA_RI 7

#include <SoftwareSerial.h>
SoftwareSerial SerialAT(FONA_TX, FONA_RX);

// See all AT commands, if wanted
#define DUMP_AT_COMMANDS

#include <TinySIM800.h>

#ifdef DUMP_AT_COMMANDS
#include <StreamDebugger.h>
StreamDebugger debugger(SerialAT, SerialMon);
TinySIM800 modem(debugger);
#else
TinySIM800 modem(SerialAT);
#endif

char buffer[255];

const char regUrl[] PROGMEM = "kontich.synology.me:8081/tracker/v1.0/Register";
const char obUrl[96] PROGMEM;

void onResetting(void *sender, EventArgs *e)
{
  pinMode(FONA_RST, OUTPUT);
  digitalWrite(FONA_RST, HIGH);
  delay(10);
  digitalWrite(FONA_RST, LOW);
  delay(100);
  digitalWrite(FONA_RST, HIGH);
}

void onPinCode(void *sender, EventArgs *e)
{
}

void setup()
{
#ifdef SerialMon
  SerialMon.begin(115200);
  while (!SerialMon) {}
  SerialMon.println(F("\nBooting..."));
#endif

  Serial1.begin(GPS_BAUDRATE);

  SerialAT.begin(MODEM_BAUDRATE);

  modem.resetting += onResetting;
  modem.pinCode += onPinCode;

  modem.allowRoaming(true);
  modem.reset();

  strcpy(imei, modem.getIMEI());

  while (!modem.isRegistered()) {
    delay(100);
  }

  modem.connectGPRS(F("telenetwap.be"));
}

void registerDevice()
{
  StaticJsonDocument<48> doc;
  doc["serial"] = "sdfdfgsdfg";
  serializeJson(doc, buffer, sizeof(buffer)); // can't capture in lamdba functions

  modem.postHTTP("", NULL,
    []() -> uint16_t { return strlen(buffer); },
    [](const SoftwareSerial & serial) {
      serial.print(buffer);
    },
    [](const uint16_t statusCode) {  },
    [](const char * s)
    {
      StaticJsonDocument<96> doc;
      deserializeJson(doc, s, 255);
      // get major elements
      strcpy(obUrl, doc[F("service")]);
    }
  );
}

void loop()
{
}
