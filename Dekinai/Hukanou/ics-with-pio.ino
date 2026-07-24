/**
 * @file ics-with-pio.ino
 * @brief ICS変換基板不要・SIG線1本直結でICS通信およびEEPROM読み書きを行うArduinoスケッチ
 */

#include "IcsPio.h"

constexpr uint8_t ICS_SIG_PIN = 2;
constexpr long    ICS_BAUDRATE = 115200;

IcsPio krs(ICS_SIG_PIN, ICS_BAUDRATE);

void setup() {
  Serial.begin(115200);
  while (!Serial && millis() < 3000);

  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, HIGH);

  krs.begin();
  Serial.println(F("ICS Single-Pin Communication Initialized!"));
}

void loop() {
  // LEDのブリンクで生存確認 (static変数でトグル)
  static unsigned long lastBlink = 0;
  static bool ledState = false;
  if (millis() - lastBlink >= 500) {
    lastBlink = millis();
    ledState = !ledState;
    digitalWrite(LED_BUILTIN, ledState ? HIGH : LOW);
  }
}
