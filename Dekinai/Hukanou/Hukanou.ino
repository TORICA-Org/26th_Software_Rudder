#include <IcsHardSerialClass.h>
const byte EN_PIN = 2;
IcsHardSerialClass krs(&Serial1, EN_PIN, 115200, 100);

constexpr byte CMD_POS = 0x80;   // 0b10000000;
constexpr byte CMD_READ = 0xA0;  // 0b10100000;
constexpr byte CMD_WRITE = 0xC0;  // 0b11000000;
constexpr byte CMD_ID = 0xE0;  // 0b11100000;

constexpr byte SC_EEPROM = 0x00;
constexpr byte SC_STRC = 0x01;
constexpr byte SC_SPD = 0x02;
constexpr byte SC_CUR = 0x03;
constexpr byte SC_TMP = 0x04;

byte txBuf[128];
byte rxBuf[128];

constexpr int MIN_ID = 0x00;
constexpr int MAX_ID = 0x1F;
int id = 0;  // 0(0x00) ~ 31(0x1F)

int txLen = 0;
int rxLen = 0;

void setup() {
  Serial.begin(115200);
  Serial1.setTX(0);
  Serial1.setRX(1);
  krs.begin();

  pinMode(LED_BUILTIN, OUTPUT);
  int led = LOW;

  unsigned long time_prev = -2000;
  while (1) {
    if (millis() - time_prev >= 1000) {
      time_prev = millis();
      Serial.print("ID? >> ");
      led = (int)!(bool)led;
      digitalWrite(LED_BUILTIN, led);
    }
    if (Serial.available()) {
      String str = Serial.readStringUntil('\n');
      int val = str.toInt();
      if (val > MAX_ID || MIN_ID > val) {
        continue;
      }
      id = val;
      Serial.println(id, DEC);
      break;
    }
  }
}

void loop() {
  if (Serial.available()) {
    String str = Serial.readStringUntil('\n');
    str.toUpperCase();

    if (str.startsWith("POS")) {
      str.replace("POS", "");
      str.trim();
      float angle = str.toFloat();
      Serial.println(angle);
      krs.setPos(id, krs.degPos(angle));
      return;
    }

    if (str.startsWith("TMP")) {
      // txBuf[0] = CMD_READ + id;
      // txBuf[1] = SC_TMP;
      // txLen = 2;
      // rxLen = 3;
      Serial.printf("TMP: %d", krs.getTmp(id));
      Serial.println();
      return;
    }

    if (str.startsWith("EEP")) {
      txBuf[0] = CMD_READ + id;
      txBuf[1] = SC_EEPROM;
      txLen = 2;
      rxLen = 68;
    }
    
    bool flag = krs.synchronize(txBuf, txLen, rxBuf, rxLen);

    if (flag) {
      Serial.print("Success!");
    }
    else {
      Serial.print("Fail...");
    }
    Serial.printf("(rxLen: %d)\n", sizeof(rxBuf));
    
    for (int i = 0; i < sizeof(rxBuf); i++) {
      Serial.print("[");
      Serial.print(i);
      Serial.print("] = 0x");
      Serial.println(rxBuf[i], HEX);
    }

    memset(rxBuf, 0, sizeof(rxBuf));
    memset(txBuf, 0, sizeof(txBuf));

    delay(1000);
  }

  delay(10);
}
