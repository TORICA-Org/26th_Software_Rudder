/**
 * @file ics-with-pio.ino
 * @brief ICS変換基板不要・SIG線1本直結でICS通信およびEEPROM読み書きを行うArduinoスケッチ
 */

#include "IcsPio.h"

// ----------------------------------------------------------------------------
// ピン・通信設定
// ----------------------------------------------------------------------------
constexpr uint8_t ICS_SIG_PIN = 2;       // サーボのSIG(信号線)に直結するGPIOピン
constexpr long    ICS_BAUDRATE = 115200;  // ICS通信速度 (115200 bps)

// IcsPio オブジェクトの生成
IcsPio krs(ICS_SIG_PIN, ICS_BAUDRATE);

void printHelp() {
  Serial.println(F("\n====== ICS Direct Communication (No Converter Board Required) ======"));
  Serial.println(F(" Commands:"));
  Serial.println(F("  POS <id> <deg>         : Move servo to angle (-135.0 to +135.0)"));
  Serial.println(F("  FREE <id>              : Disable servo torque (Free)"));
  Serial.println(F("  GETPOS <id>            : Read current servo position"));
  Serial.println(F("  STR <id> [val]         : Get or Set Stretch (1-127)"));
  Serial.println(F("  SPD <id> [val]         : Get or Set Speed (1-127)"));
  Serial.println(F("  DB <id> [val]          : Get or Set Deadband (0-10, Hunting Prevention)"));
  Serial.println(F("  TMP <id>               : Read servo temperature"));
  Serial.println(F("  CUR <id>               : Read servo current"));
  Serial.println(F("  EEP_READ <id>          : Dump 64-byte EEPROM content"));
  Serial.println(F("  DEBUG ON/OFF           : Toggle raw packet debug log"));
  Serial.println(F("  HELP                   : Display this help message"));
  Serial.println(F("===================================================================\n"));
}

void setup() {
  Serial.begin(115200);
  while (!Serial && millis() < 3000); // シリアル接続待機

  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, HIGH);

  // ICS通信ドライバの初期化
  krs.begin();
  krs.setDebug(true); // デフォルトでデバッグログを有効化

  Serial.println(F("ICS Single-Pin Communication Initialized!"));
  printHelp();
}

void processCommand(String input) {
  input.trim();
  if (input.length() == 0) return;

  int space1 = input.indexOf(' ');
  String cmd = (space1 == -1) ? input : input.substring(0, space1);
  cmd.toUpperCase();

  String args = (space1 == -1) ? "" : input.substring(space1 + 1);
  args.trim();

  // HELP
  if (cmd == "HELP") {
    printHelp();
    return;
  }

  // DEBUG ON / OFF
  if (cmd == "DEBUG") {
    args.toUpperCase();
    if (args == "ON") {
      krs.setDebug(true);
      Serial.println(F("ICS Debug Log: ENABLED"));
    } else {
      krs.setDebug(false);
      Serial.println(F("ICS Debug Log: DISABLED"));
    }
    return;
  }

  // EEP_READ <id>
  if (cmd == "EEP_READ") {
    int id = args.toInt();
    krs.printEEPROM(id, Serial);
    return;
  }

  // POS <id> <deg>
  if (cmd == "POS") {
    int sp = args.indexOf(' ');
    if (sp != -1) {
      int id = args.substring(0, sp).toInt();
      float deg = args.substring(sp + 1).toFloat();
      int pos = IcsPio::degPos(deg);
      int resPos = krs.setPos(id, pos);
      if (resPos >= 0) {
        Serial.printf("Servo ID:%d -> Target Deg: %.1f° (Pos:%d), Response Pos: %d (%.1f°)\n",
                      id, deg, pos, resPos, IcsPio::posDeg(resPos));
      } else {
        Serial.printf("Servo ID:%d -> Communication Error (No Response)\n", id);
      }
    } else {
      Serial.println(F("Usage: POS <id> <deg>"));
    }
    return;
  }

  // FREE <id>
  if (cmd == "FREE") {
    int id = args.toInt();
    int resPos = krs.setFree(id);
    if (resPos >= 0) {
      Serial.printf("Servo ID:%d Set Free! (Current Pos: %d / %.1f°)\n", id, resPos, IcsPio::posDeg(resPos));
    } else {
      Serial.printf("Servo ID:%d -> Communication Error!\n", id);
    }
    return;
  }

  // GETPOS <id>
  if (cmd == "GETPOS") {
    int id = args.toInt();
    int pos = krs.getPos(id);
    if (pos >= 0) {
      Serial.printf("Servo ID:%d Position: %d (%.1f°)\n", id, pos, IcsPio::posDeg(pos));
    } else {
      Serial.printf("Servo ID:%d -> Read Failed!\n", id);
    }
    return;
  }

  // STR <id> [val]
  if (cmd == "STR") {
    int sp = args.indexOf(' ');
    if (sp != -1) {
      int id = args.substring(0, sp).toInt();
      int val = args.substring(sp + 1).toInt();
      int res = krs.setStretch(id, val);
      Serial.printf("Servo ID:%d Set Stretch to %d -> Result: %d\n", id, val, res);
    } else {
      int id = args.toInt();
      int res = krs.getStretch(id);
      Serial.printf("Servo ID:%d Stretch: %d\n", id, res);
    }
    return;
  }

  // SPD <id> [val]
  if (cmd == "SPD") {
    int sp = args.indexOf(' ');
    if (sp != -1) {
      int id = args.substring(0, sp).toInt();
      int val = args.substring(sp + 1).toInt();
      int res = krs.setSpeed(id, val);
      Serial.printf("Servo ID:%d Set Speed to %d -> Result: %d\n", id, val, res);
    } else {
      int id = args.toInt();
      int res = krs.getSpeed(id);
      Serial.printf("Servo ID:%d Speed: %d\n", id, res);
    }
    return;
  }

  // DB <id> [val] (Deadband)
  if (cmd == "DB" || cmd == "DEADBAND") {
    int sp = args.indexOf(' ');
    if (sp != -1) {
      int id = args.substring(0, sp).toInt();
      int val = args.substring(sp + 1).toInt();
      int res = krs.setDeadband(id, val);
      Serial.printf("Servo ID:%d Set Deadband to %d -> Result: %d\n", id, val, res);
    } else {
      int id = args.toInt();
      int res = krs.getDeadband(id);
      Serial.printf("Servo ID:%d Deadband: %d\n", id, res);
    }
    return;
  }

  // TMP <id>
  if (cmd == "TMP") {
    int id = args.toInt();
    int tmp = krs.getTmp(id);
    if (tmp >= 0) {
      Serial.printf("Servo ID:%d Temperature: %d °C\n", id, tmp);
    } else {
      Serial.printf("Servo ID:%d -> Read Temperature Failed!\n", id);
    }
    return;
  }

  // CUR <id>
  if (cmd == "CUR") {
    int id = args.toInt();
    int cur = krs.getCur(id);
    if (cur >= 0) {
      Serial.printf("Servo ID:%d Current: %d\n", id, cur);
    } else {
      Serial.printf("Servo ID:%d -> Read Current Failed!\n", id);
    }
    return;
  }

  Serial.println(F("Unknown Command. Type HELP for command list."));
}

void loop() {
  if (Serial.available() > 0) {
    String input = Serial.readStringUntil('\n');
    processCommand(input);
  }

  // LEDのブリンクで生存確認
  static unsigned long lastBlink = 0;
  static bool ledState = false;
  if (millis() - lastBlink >= 500) {
    lastBlink = millis();
    ledState = !ledState;
    digitalWrite(LED_BUILTIN, ledState ? HIGH : LOW);
  }
}
