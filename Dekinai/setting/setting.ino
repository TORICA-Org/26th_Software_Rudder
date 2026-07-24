//---hファイル---
// EEPROMアクセス
bool getEEPROM(byte id, byte eeprom[64]);
bool setEEPROM(byte id, const byte eeprom[64]);

// デッドバンド設定
int setDeadBand(byte id, unsigned int deadband);

//---cppファイル---
// 読み込み
bool IcsBaseClass::getEEPROM(byte id, byte eeprom[64])
{
    byte txCmd[2];
    byte rxCmd[68];

    if(id != idMax(id))
        return false;

    txCmd[0] = 0xA0 + id;
    txCmd[1] = 0x00;

    if(!synchronize(txCmd, sizeof(txCmd), rxCmd, sizeof(rxCmd)))
        return false;

    memcpy(eeprom, &rxCmd[4], 64);

    return true;
}

// 書き込み
bool IcsBaseClass::setEEPROM(byte id, const byte eeprom[64])
{
    byte txCmd[66];
    byte rxCmd[68];

    if(id != idMax(id))
        return false;

    txCmd[0] = 0xC0 + id;
    txCmd[1] = 0x00;

    memcpy(&txCmd[2], eeprom, 64);

    if(!synchronize(txCmd, sizeof(txCmd), rxCmd, sizeof(rxCmd)))
        return false;

    return true;
}

// デッドバンド設定
int IcsBaseClass::setDeadBand(byte id, unsigned int deadband)
{
    byte eeprom[64];

    if(!getEEPROM(id, eeprom))
        return ICS_FALSE;

    unsigned int value = 0x01;

    value = deadband;

    eeprom[8] = (value >> 4) & 0x0F;
    eeprom[9] = value & 0x0F;

    if(!setEEPROM(id, eeprom))
        return ICS_FALSE;

    return value;
}