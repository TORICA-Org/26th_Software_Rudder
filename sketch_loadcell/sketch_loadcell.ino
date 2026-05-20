#include <IcsHardSerialClass.h>
const byte EN_PIN = 2;
IcsHardSerialClass krs(&Serial1, EN_PIN);

#define loadcell_L A7
#define loadcell_R A8

const float neutral_pos = 7500.0;
const float maxangle_val = (15.0 / 135.0) * 4000.0;

float deadzone = 3.0; //[%]

void setup() {
  analogReadResolution(12);
  pinMode(loadcell_L, INPUT);
  pinMode(loadcell_R, INPUT);
  krs.begin();
  Serial.begin(115200);
  delay(3000);
  Serial.println("begin");
}

void loop() {
  float valL = analogRead(loadcell_L);
  float valR = analogRead(loadcell_R);

  Serial.println("here0");

  if (valL < 0) valL = 0;
  if (valR < 0) valR = 0;

  Serial.println("here1");


  float percentage = 0;
  float total = valL + valR;
  
  Serial.println("here2");

  percentage = 100.0 * (valL - valR) / total;
 

  if (abs(percentage) < deadzone) percentage = 0;
  Serial.println("here3");

  float outputOffset = maxangle_val * (percentage / 100.0);

  unsigned int target_pos = (unsigned int)(neutral_pos + outputOffset);

  Serial.println("here4;");

  //krs.setPos(0, target_pos);

  Serial.println("here5");

  Serial.print("Left:"); Serial.print(valL);
  Serial.print("Right:"); Serial.print(valR);
  Serial.print("LR_Total:"); Serial.print(total);
  Serial.print("Control:"); Serial.print(percentage);

  Serial.print("Target:"); Serial.println(target_pos);

  delay(50);
}
