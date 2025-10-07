#include <TORICA_WebServer.h>
#include <index.h>

// 最終更新日: 20250517

// 直感的な入力量評価のため，ラダー入力を百分率に変更
// 挙動を安定させるため，フル入力時・無入力時の値を定数化
// ラダーの計算に関する部分をrudderCalculatorに分離

// 入力値の変化は
// Lが減少
// Rが増加
// servo_pos:
// 135°(L)<=>  30°<=> -15°<=> 0°(N)<=> -15°<=> -30°<=> -135°(R)
// 11500  <=> 8389<=> 7944<=> 7500 <=> 7056<=> 6611<=> 3500

#include <Arduino.h>
#include <IcsHardSerialClass.h>

// ピン指定
const int POT = 28; // ポテンショメータ

// サーボモータの初期設定
const byte EN_PIN = 2; // ENピンの指定
const long BAUDRATE = 115200; // 通信速度
const int TIMEOUT = 10; // タイムアウト

// インスタンス化およびENピン(2番ピン)とUARTの指定
IcsHardSerialClass krs(&Serial1,EN_PIN,BAUDRATE,TIMEOUT);

void setup() {
  // ピン割り当て
  pinMode(LED_BUILTIN, OUTPUT); // 動作確認用LED
  pinMode(POT,INPUT);
  // サーボモータの通信初期設定
  // Serial1.setTX(0);
  // Serial1.setRX(1);
  // Serial1.begin(115200, SERIAL_8E1);
  // Serial1.setTimeout(10);
  krs.begin();
  
  pinMode(POT, INPUT);
  analogReadResolution(12);
  // デバッグ用シリアルを開始
  Serial.begin(115200, SERIAL_8E1);
  Serial.println("SERIAL READY");

  Serial2.setTX(4);
  Serial2.setRX(5);
  Serial2.begin(115200, SERIAL_8E1);

  delay(5);
  digitalWrite(LED_BUILTIN, HIGH);
}

const float out_min = 7500.0 - (4000.0*15.0/135.0);
const float out_max = 7500.0 + (4000.0*15.0/135.0);
const float in_min = (float)0 + 1000;
const float in_max = (float)4095 - 1000;

void loop() {
  float pot = (float)analogRead(POT);

  float servo_pos = out_min + (pot - in_min)*(out_max - out_min)/(in_max - in_min);

  if (servo_pos < out_min) {
    servo_pos = out_min;
  }
  else if (out_max < servo_pos) {
    servo_pos = out_max;
  }
  
  Serial.print(pot);
  Serial.print(" : ");
  Serial.print(servo_pos);
  Serial.print("\n");

  // サーボ（ID:0）をservo_posだけ駆動
  krs.setPos(0, servo_pos);
  // Serial.println(setPos(0, servo_pos));
  delay(10);
}


//ICS初期化
// #include <TORICA_ICS.h>
// TORICA_ICS ics(&Serial2);
/*
void setup1() {
}

void loop1() {
  char buff[32];
  sprintf(buff, "ICS: %d", ics.read_Angle());
  Serial.println(buff);
  delay(10);
}
*/