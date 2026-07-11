#include <IcsHardSerialClass.h>
const byte EN_PIN = 2;
IcsHardSerialClass krs(&Serial1, EN_PIN, 115200, 100);
// 読み込み
// bool IcsBaseClass::getEEPROM(byte id, byte eeprom[64])
bool getEEPROM(byte id, byte eeprom[64])
{
    byte txCmd[2];
    byte rxCmd[66];

     // idMax はprivateだから使えない
    // if(id != idMax(id)) 
        // return false;

    // txCmd[0] = 0xA0 + id;
    txCmd[0] = 0xA0 + id; // CMD（読み出し）
    txCmd[1] = 0x00; // SC（EEPROM）

    if(!krs.synchronize(txCmd, sizeof(txCmd), rxCmd, sizeof(rxCmd)))
        return false;

    memcpy(eeprom, &rxCmd[4], 64);

    return true;
}

  byte A[64];
  int B;

void setup() {
  Serial.begin(115200);
  Serial1.setTX(0);
  Serial1.setRX(1);
  krs.begin();
}

void loop() {
  if (getEEPROM(0, A)) { 
    for (int i = 0; i < 64; i++) { 
      Serial.print("A["); 
      Serial.print(i); 
      Serial.print("] = "); 
      Serial.println(A[i]); 
    }
    else { 
      Serial.println("EEPROMの読み取りに失敗しました。"); 
    } 
    delay(1000);
  }

  //温度読みだし
  B=krs.getTmp(0);
  delay(100);
  Seial.println(B);

}

