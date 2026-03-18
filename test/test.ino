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

// サーボモータの初期設定
const byte EN_PIN = 2; // ENピンの指定
const long BAUDRATE = 115200; // 通信速度
const int TIMEOUT = 10; // タイムアウト

// インスタンス化およびENピン(2番ピン)とUARTの指定
IcsHardSerialClass krs(&Serial1,EN_PIN,BAUDRATE,TIMEOUT);

void setup() {
  // ピン割り当て
  pinMode(LED_BUILTIN, OUTPUT); // 動作確認用LED
  // サーボモータの通信初期設定
  // Serial1.setTX(0);
  // Serial1.setRX(1);
  // Serial1.begin(115200, SERIAL_8E1);
  // Serial1.setTimeout(10);
  krs.begin();
  
  // デバッグ用シリアルを開始
  Serial.begin(115200, SERIAL_8E1);
  Serial.println("SERIAL READY");

  Serial2.setTX(4);
  Serial2.setRX(5);
  Serial2.begin(115200, SERIAL_8E1);

  delay(5);
  digitalWrite(LED_BUILTIN, HIGH);
}


float servo_pos = 7500.0;
int inputAngle = 0;

void loop() {

  if (Serial.available()) {
    String str = Serial.readStringUntil('\n');
    inputAngle = str.toInt();
    int diffPos = (float)inputAngle * 4000.0 / 135.0;
    servo_pos = 7500 + diffPos;
  }


  Serial.print(servo_pos);
  Serial.print(" : ");
  Serial.print(inputAngle);
  Serial.println("deg");

  // Serial.print("RealAngle -> ");
  // Serial.println(realAngle);
  

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