#include <IcsHardSerialClass.h>

constexpr uint8_t LOADCELL_L = A0;
constexpr uint8_t LOADCELL_R = A1;
constexpr uint8_t POT = A3;

constexpr int NEUTRAL_POS = 7500;
constexpr float MAX_ANGLE = 15/2;

const uint8_t EN_PIN = D2;
const int BAUDLATE = 115200;
const int TIMEOUT = 100;
// SerialPIO myserial(D7, D6);
IcsHardSerialClass krs(&Serial1, EN_PIN, BAUDLATE, TIMEOUT);

constexpr float loadcell_min = 1000;
constexpr float loadcell_max = 2300;

float deadzone = 3.0; //[%]

void setup() {
  analogReadResolution(12);
  pinMode(LOADCELL_L, INPUT);
  pinMode(LOADCELL_R, INPUT);
  pinMode(POT, INPUT);
  Serial1.setTX(D6);
  Serial1.setRX(D7);
  krs.begin();
  Serial.begin(115200);
  delay(3000);
  Serial.println("begin");
}

void loop() {
  float valL = analogRead(LOADCELL_L) - loadcell_min;
  float valR = analogRead(LOADCELL_R) - loadcell_min;
  float trim = ((float) analogRead(POT) - 2048.0) / 4096.0 * 1500.0;

  if (valL < 0) valL = 0;
  if (valR < 0) valR = 0;

  float percentageL = valL / (loadcell_max - loadcell_min) * 100;
  float percentageR = valR / (loadcell_max - loadcell_min) * 100;

  float percentageSum = percentageR - percentageL;

  if (abs(percentageSum) < deadzone) percentageSum = 0;
  // Serial.println("here3");

  float outputOffset = MAX_ANGLE / 135 * 4000 * (percentageSum / 100.0);

  int target_pos = NEUTRAL_POS + trim + (int)(outputOffset);

  // Serial.println("here4");

  krs.setPos(0, target_pos);

  // Serial.println("here5");

  // Serial.print("Left:"); 
  Serial.print(valL);
  Serial.print(", ");
  // Serial.print("Right:"); 
  Serial.print(valR);
  Serial.print(", ");
  Serial.print(trim);
  Serial.print(", ");
  Serial.print(percentageSum);
  Serial.print(", ");
  Serial.print(target_pos);
  Serial.println();

  // Serial.print("LR_Total:"); 
  // Serial.print(total);
  // Serial.print("Control:"); 
  // Serial.print(percentage);

  // Serial.print("Target:"); 
  // Serial.println(target_pos);

  delay(10);
}
