#include "IcsPio.h"

// RP2040 / Pico SDK の SerialPIO ヘッダーの自動検出
#if defined(ARDUINO_ARCH_RP2040) || __has_include(<SerialPIO.h>)
#include <SerialPIO.h>
#define USE_RP2040_SERIAL_PIO 1
#else
#define USE_RP2040_SERIAL_PIO 0
#endif

// ----------------------------------------------------------------------------
// コンストラクタ / デストラクタ / 初期化
// ----------------------------------------------------------------------------

IcsPio::IcsPio(uint8_t pin, long baud, int timeout)
    : _pin(pin), _baud(baud), _timeout(timeout), _debug(false), _pioSerialPtr(nullptr) {
    _bitUs = 1000000UL / _baud;
}

IcsPio::~IcsPio() {
#if USE_RP2040_SERIAL_PIO
    if (_pioSerialPtr != nullptr) {
        delete (SerialPIO*)_pioSerialPtr;
        _pioSerialPtr = nullptr;
    }
#endif
}

bool IcsPio::begin() {
#if USE_RP2040_SERIAL_PIO
    if (_pioSerialPtr == nullptr) {
        // TX, RXともに同じピンを指定 (1-pin Half-Duplex PIO UART)
        SerialPIO* pioSerial = new SerialPIO(_pin, _pin);
        _pioSerialPtr = (void*)pioSerial;
        // 115200 bps, 8 Data bits, Even Parity, 1 Stop bit (8E1)
        pioSerial->begin(_baud, SERIAL_8E1);
    }
#else
    setPinInput();
#endif
    return true;
}

void IcsPio::setPinOutput() {
    pinMode(_pin, OUTPUT);
    digitalWrite(_pin, HIGH);
}

void IcsPio::setPinInput() {
    pinMode(_pin, INPUT_PULLUP);
}

bool IcsPio::calcEvenParity(byte data) {
    byte count = 0;
    for (int i = 0; i < 8; i++) {
        if (data & (1 << i)) count++;
    }
    return (count % 2 != 0) ? 1 : 0;
}

/**
 * @brief PIO ハードウェア (SerialPIO) ループバック処理対応 Half-Duplex 送受信
 */
bool IcsPio::rawHalfDuplexTransfer(const byte* txBuf, int txLen, byte* rxBuf, int rxLen) {
    if (txLen <= 0 || txBuf == nullptr) return false;

    if (_debug) {
        Serial.print(F("[ICS TX] "));
        for (int i = 0; i < txLen; i++) {
            Serial.printf("0x%02X ", txBuf[i]);
        }
        Serial.println();
    }

#if USE_RP2040_SERIAL_PIO
    SerialPIO* pioSerial = (SerialPIO*)_pioSerialPtr;
    if (pioSerial == nullptr) return false;

    // 1. 送信前に旧受信バッファに残っているゴミデータを消去
    while (pioSerial->available()) {
        pioSerial->read();
    }

    // 2. コマンド送信
    pioSerial->write(txBuf, txLen);
    pioSerial->flush(); // PIO ハードウェア送信完了待ち

    if (rxLen <= 0 || rxBuf == nullptr) return true;

    // 3. 自前送信分のループバック (txLenバイト) + サーボ返信データ (rxLenバイト) の受信
    int totalExpected = txLen + rxLen;
    int totalRead = 0;
    byte tempBuf[128];

    unsigned long startMs = millis();

    while (totalRead < totalExpected) {
        if (millis() - startMs > (unsigned long)_timeout) {
            if (_debug) {
                Serial.printf("[ICS RX Timeout] Read %d/%d bytes (Self-Loopback:%d, Response:%d).\n",
                              totalRead, totalExpected, min(totalRead, txLen), max(0, totalRead - txLen));
            }
            return false;
        }

        if (pioSerial->available() > 0) {
            byte b = pioSerial->read();
            if (totalRead < (int)sizeof(tempBuf)) {
                tempBuf[totalRead] = b;
            }
            totalRead++;
        }
    }

    // 自前送信データ (先頭 txLen バイト) をスキップし、続く rxLen バイトを本物の返信として取得
    memcpy(rxBuf, &tempBuf[txLen], rxLen);

    if (_debug) {
        Serial.print(F("[ICS RX Valid Response] "));
        for (int i = 0; i < rxLen; i++) {
            Serial.printf("0x%02X ", rxBuf[i]);
        }
        Serial.println();
    }

    return true;

#else
    // -------------------------------------------------------------------------
    // 非RP2040環境用のマイクロ秒高精度 Bit-Bang フォールバック
    // -------------------------------------------------------------------------
    const float bitUs = 1000000.0f / (float)_baud;

    noInterrupts();
    setPinOutput();
    digitalWrite(_pin, HIGH);

    uint32_t initWait = micros() + (uint32_t)(bitUs * 2.0f);
    while ((int32_t)(micros() - initWait) < 0);

    float accumulatedUs = (float)micros();

    for (int i = 0; i < txLen; i++) {
        byte b = txBuf[i];
        bool parity = calcEvenParity(b);

        digitalWrite(_pin, LOW);
        accumulatedUs += bitUs;
        while ((int32_t)(micros() - (uint32_t)accumulatedUs) < 0);

        for (int bit = 0; bit < 8; bit++) {
            digitalWrite(_pin, (b & (1 << bit)) ? HIGH : LOW);
            accumulatedUs += bitUs;
            while ((int32_t)(micros() - (uint32_t)accumulatedUs) < 0);
        }

        digitalWrite(_pin, parity ? HIGH : LOW);
        accumulatedUs += bitUs;
        while ((int32_t)(micros() - (uint32_t)accumulatedUs) < 0);

        digitalWrite(_pin, HIGH);
        accumulatedUs += bitUs;
        while ((int32_t)(micros() - (uint32_t)accumulatedUs) < 0);
    }

    accumulatedUs += bitUs * 1.5f;
    while ((int32_t)(micros() - (uint32_t)accumulatedUs) < 0);

    setPinInput();
    interrupts();

    if (rxLen <= 0 || rxBuf == nullptr) return true;

    uint32_t guardStart = micros();
    while (digitalRead(_pin) == LOW && (micros() - guardStart < 1000));

    int bytesRead = 0;
    unsigned long startMs = millis();

    while (bytesRead < rxLen) {
        if (millis() - startMs > (unsigned long)_timeout) {
            if (_debug) {
                Serial.printf("[ICS RX Timeout] Read %d/%d bytes.\n", bytesRead, rxLen);
            }
            return false;
        }

        if (digitalRead(_pin) == HIGH) {
            continue;
        }

        noInterrupts();
        float rxClock = (float)micros();

        rxClock += bitUs * 0.5f;
        while ((int32_t)(micros() - (uint32_t)rxClock) < 0);

        if (digitalRead(_pin) != LOW) {
            interrupts();
            continue;
        }

        byte receivedByte = 0;
        for (int bit = 0; bit < 8; bit++) {
            rxClock += bitUs;
            while ((int32_t)(micros() - (uint32_t)rxClock) < 0);
            if (digitalRead(_pin) == HIGH) {
                receivedByte |= (1 << bit);
            }
        }

        rxClock += bitUs;
        while ((int32_t)(micros() - (uint32_t)rxClock) < 0);

        rxClock += bitUs;
        while ((int32_t)(micros() - (uint32_t)rxClock) < 0);

        interrupts();

        rxBuf[bytesRead++] = receivedByte;

        uint32_t bWait = micros();
        while (digitalRead(_pin) == LOW && (micros() - bWait < 500));
    }

    if (_debug) {
        Serial.print(F("[ICS RX] "));
        for (int i = 0; i < rxLen; i++) {
            Serial.printf("0x%02X ", rxBuf[i]);
        }
        Serial.println();
    }

    return (bytesRead == rxLen);
#endif
}

bool IcsPio::synchronize(const byte* txBuf, int txLen, byte* rxBuf, int rxLen) {
    return rawHalfDuplexTransfer(txBuf, txLen, rxBuf, rxLen);
}

// ----------------------------------------------------------------------------
// 公式ライブラリ (IcsHardSerialClass) 再現関数の実装
// ----------------------------------------------------------------------------

int IcsPio::setPos(byte id, int pos) {
    if (id > 31) return -1;

    byte txBuf[3];
    byte rxBuf[3];

    txBuf[0] = ICS_CMD_POS | (id & 0x1F);
    txBuf[1] = (pos >> 7) & 0x7F;
    txBuf[2] = pos & 0x7F;

    if (synchronize(txBuf, 3, rxBuf, 3)) {
        int retPos = ((int)(rxBuf[1] & 0x7F) << 7) | (rxBuf[2] & 0x7F);
        return retPos;
    }
    return -1;
}

int IcsPio::setFree(byte id) {
    return setPos(id, 0);
}

int IcsPio::getPos(byte id) {
    return setPos(id, 0);
}

int IcsPio::setStretch(byte id, int stretch) {
    if (id > 31 || stretch < 1 || stretch > 127) return -1;

    byte txBuf[3] = { (byte)(ICS_CMD_WRITE | (id & 0x1F)), ICS_SC_STRC, (byte)(stretch & 0x7F) };
    byte rxBuf[3];

    if (synchronize(txBuf, 3, rxBuf, 3)) {
        return rxBuf[2] & 0x7F;
    }
    return -1;
}

int IcsPio::getStretch(byte id) {
    if (id > 31) return -1;

    byte txBuf[2] = { (byte)(ICS_CMD_READ | (id & 0x1F)), ICS_SC_STRC };
    byte rxBuf[3];

    if (synchronize(txBuf, 2, rxBuf, 3)) {
        return rxBuf[2] & 0x7F;
    }
    return -1;
}

int IcsPio::setSpeed(byte id, int speed) {
    if (id > 31 || speed < 1 || speed > 127) return -1;

    byte txBuf[3] = { (byte)(ICS_CMD_WRITE | (id & 0x1F)), ICS_SC_SPD, (byte)(speed & 0x7F) };
    byte rxBuf[3];

    if (synchronize(txBuf, 3, rxBuf, 3)) {
        return rxBuf[2] & 0x7F;
    }
    return -1;
}

int IcsPio::getSpeed(byte id) {
    if (id > 31) return -1;

    byte txBuf[2] = { (byte)(ICS_CMD_READ | (id & 0x1F)), ICS_SC_SPD };
    byte rxBuf[3];

    if (synchronize(txBuf, 2, rxBuf, 3)) {
        return rxBuf[2] & 0x7F;
    }
    return -1;
}

int IcsPio::setDeadband(byte id, int deadband) {
    if (id > 31 || deadband < 0 || deadband > 10) return -1;

    byte txBuf[3] = { (byte)(ICS_CMD_WRITE | (id & 0x1F)), ICS_SC_DEADBAND, (byte)(deadband & 0x7F) };
    byte rxBuf[3];

    if (synchronize(txBuf, 3, rxBuf, 3)) {
        return rxBuf[2] & 0x7F;
    }
    return -1;
}

int IcsPio::getDeadband(byte id) {
    if (id > 31) return -1;

    byte txBuf[2] = { (byte)(ICS_CMD_READ | (id & 0x1F)), ICS_SC_DEADBAND };
    byte rxBuf[3];

    if (synchronize(txBuf, 2, rxBuf, 3)) {
        return rxBuf[2] & 0x7F;
    }
    return -1;
}

int IcsPio::getCur(byte id) {
    if (id > 31) return -1;

    byte txBuf[2] = { (byte)(ICS_CMD_READ | (id & 0x1F)), ICS_SC_CUR };
    byte rxBuf[3];

    if (synchronize(txBuf, 2, rxBuf, 3)) {
        return rxBuf[2] & 0x7F;
    }
    return -1;
}

int IcsPio::getTmp(byte id) {
    if (id > 31) return -1;

    byte txBuf[2] = { (byte)(ICS_CMD_READ | (id & 0x1F)), ICS_SC_TMP };
    byte rxBuf[3];

    if (synchronize(txBuf, 2, rxBuf, 3)) {
        return rxBuf[2] & 0x7F;
    }
    return -1;
}

int IcsPio::degPos(float deg) {
    if (deg < -135.0f) deg = -135.0f;
    if (deg > 135.0f) deg = 135.0f;

    int pos = (int)(7500.0f + (deg * 4000.0f / 135.0f) + 0.5f);
    return pos;
}

float IcsPio::posDeg(int pos) {
    if (pos < 3500) pos = 3500;
    if (pos > 11500) pos = 11500;

    float deg = (float)(pos - 7500) * 135.0f / 4000.0f;
    return deg;
}

// ----------------------------------------------------------------------------
// EEPROM 読み書き関数の実装 (全64B一括処理)
// ----------------------------------------------------------------------------

bool IcsPio::readEEPROM(byte id, byte* buffer64) {
    if (id > 31 || buffer64 == nullptr) return false;

    byte txBuf[2] = { (byte)(ICS_CMD_READ | (id & 0x1F)), ICS_SC_EEPROM };
    byte rxBuf[66];

    if (synchronize(txBuf, 2, rxBuf, 66)) {
        if (rxBuf[0] == txBuf[0] && rxBuf[1] == txBuf[1]) {
            memcpy(buffer64, &rxBuf[2], 64);
            return true;
        }
    }
    return false;
}

bool IcsPio::writeEEPROM(byte id, const byte* buffer64) {
    if (id > 31 || buffer64 == nullptr) return false;

    byte txBuf[66];
    byte rxBuf[66];

    txBuf[0] = ICS_CMD_WRITE | (id & 0x1F);
    txBuf[1] = ICS_SC_EEPROM;
    memcpy(&txBuf[2], buffer64, 64);

    if (synchronize(txBuf, 66, rxBuf, 66)) {
        return true;
    }
    return false;
}

void IcsPio::printEEPROM(byte id, Stream& serial) {
    byte eep[64];
    serial.println();
    serial.printf("=== Reading Servo ID %d EEPROM (64 Bytes) ===\n", id);

    if (!readEEPROM(id, eep)) {
        serial.println("Error: Failed to read EEPROM from servo.");
        return;
    }

    serial.println("Addr  00 01 02 03 04 05 06 07 08 09 0A 0B 0C 0D 0E 0F");
    serial.println("----  -----------------------------------------------");

    for (int i = 0; i < 64; i += 16) {
        serial.printf("0x%02X: ", i);
        for (int j = 0; j < 16; j++) {
            serial.printf("%02X ", eep[i + j]);
        }
        serial.println();
    }
    serial.println("=================================================\n");
}
