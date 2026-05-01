#include <IcsHardSerialClass.h>
const byte EN_PIN = 4;
IcsHardSerialClass krs(&Serial, EN_PIN);

#define loadcell_L A0
#define loadcell_R A1

const float neutral_pos = 7500.0;
const float maxangle_val = (15.0 / 135.0) * 4000.0;

void setup() {
  pinMode(loadcell_L, INPUT);
  pinMode(loadcell_R, INPUT);
  krs.begin(115200, 10);
  Serial.begin(115200);
}

void loop() {
  float valL = analogRead(loadcell_L);
  float valR = analogRead(loadcell_R);

  if (valL < 0) valL = 0;
  if (valR < 0) valR = 0;

  float average = (valL + valR) / 2.0;

  float rawControl = 0;
  float total = valL + valR;

  if (total > 20.0){
    rawControl = 100.0 * (valL - valR) / total;
  }
  else{
    rawControl = 0;
  }

  if (abs(rawControl) < 3.0) rawControl = 0;

  float outputOffset = rawControl * (maxangle_val / 100.0);

  long target_pos = (long)(neutral_pos + outputOffset);

  krs.setPos(0, target_pos);

  Serial.print("Left:"); Serial.print(valL);
  Serial.print("Right:"); Serial.print(valR);
  Serial.print("LR_Total:"); Serial.print(total);
  Serial.print("Avg:"); Serial.print(average);
  Serial.print("Control:"); Serial.print(rawControl);

  Serial.print("Target:"); Serial.println(target_pos);

  delay(50);
}
