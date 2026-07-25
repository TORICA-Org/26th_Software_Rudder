#include "IcsPio.h"

// ics.pio をコンパイルしたヘッダファイル（Arduino IDEのPicoコアが自動生成）
#if defined(ARDUINO_ARCH_RP2040)
#include "ics.pio.h"
#endif

// ----------------------------------------------------------------------------
// コンストラクタ / デストラクタ / 初期化
// ----------------------------------------------------------------------------

IcsPio::IcsPio(uint8_t pin, long baud, int timeout)
    : _pin(pin), _baud(baud), _timeout(timeout), _debug(false) {
}

IcsPio::~IcsPio() {
#if defined(ARDUINO_ARCH_RP2040)
    // PIO リソースの解放
    pio_sm_set_enabled(_pio, _tx_sm, false);
    pio_sm_set_enabled(_pio, _rx_sm, false);
    pio_sm_unclaim(_pio, _tx_sm);
    pio_sm_unclaim(_pio, _rx_sm);
    pio_remove_program(_pio, &ics_tx_program, _tx_offset);
    pio_remove_program(_pio, &ics_rx_program, _rx_offset);
#endif
}

bool IcsPio::begin() {
#if defined(ARDUINO_ARCH_RP2040)
    _pio = pio0; // PIO 0 を使用

    // TX / RX プログラムを PIO のメモリ領域へロード
    _tx_offset = pio_add_program(_pio, &ics_tx_program);
    _rx_offset = pio_add_program(_pio, &ics_rx_program);
    
    // 空いているステートマシンを取得
    _tx_sm = pio_claim_unused_sm(_pio, true);
    _rx_sm = pio_claim_unused_sm(_pio, true);
    
    // ヘッダに定義された初期化関数で設定を行う
    ics_tx_program_init(_pio, _tx_sm, _tx_offset, _pin, _baud);
    ics_rx_program_init(_pio, _rx_sm, _rx_offset, _pin, _baud);
#endif
    return true;
}

// 偶数パリティ計算 (データビット中の1の数が奇数のとき、パリティビットを1にする)
bool IcsPio::calcEvenParity(byte data) {
    byte count = 0;
    for (int i = 0; i < 8; i++) {
        if (data & (1 << i)) count++;
    }
    return (count % 2 != 0);
}

/**
 * @brief 自作PIOによる Half-Duplex 送受信 (ループバック回避・自動Hi-Z)
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

#if defined(ARDUINO_ARCH_RP2040)
    // 1. 送信前に RX SM を停止・FIFOクリアしておく (自身の送信を受信しないようにする)
    pio_sm_set_enabled(_pio, _rx_sm, false);
    pio_sm_clear_fifos(_pio, _rx_sm);

    // 2. TX SM を有効化し、送信開始
    pio_sm_set_enabled(_pio, _tx_sm, true);

    for (int i = 0; i < txLen; i++) {
        byte b = txBuf[i];
        bool parity = calcEvenParity(b);
        
        // PIOへ送る 10bit データ構成:
        // bit0~7: Data, bit8: Parity, bit9: Stop bit (1)
        uint32_t out_word = b | (parity ? (1 << 8) : 0) | (1 << 9);
        pio_sm_put_blocking(_pio, _tx_sm, out_word);
    }

    // 3. 全てのデータが TX FIFO から出力されるのを待つ
    while (!pio_sm_is_tx_fifo_empty(_pio, _tx_sm)) {
        tight_loop_contents();
    }
    
    // 最後のバイト (1文字 10bits + Start Bit 1bit = 11bits) がシフトアウトし終わるまで待機
    uint32_t bitUs = 1000000 / _baud;
    delayMicroseconds(bitUs * 11);

    // TX SM を停止 (しなくても待機状態でHi-Zになっていますが念のため)
    pio_sm_set_enabled(_pio, _tx_sm, false);

    if (rxLen <= 0 || rxBuf == nullptr) return true;

    // 4. RX SM の FIFOを再度クリアし、サーボからの返信受信を開始
    pio_sm_clear_fifos(_pio, _rx_sm);
    pio_sm_set_enabled(_pio, _rx_sm, true);

    int totalRead = 0;
    unsigned long startMs = millis();

    while (totalRead < rxLen) {
        if (millis() - startMs > (unsigned long)_timeout) {
            if (_debug) {
                Serial.printf("[ICS RX Timeout] Read %d/%d bytes.\n", totalRead, rxLen);
            }
            pio_sm_set_enabled(_pio, _rx_sm, false);
            return false;
        }

        if (!pio_sm_is_rx_fifo_empty(_pio, _rx_sm)) {
            uint32_t in_word = pio_sm_get(_pio, _rx_sm);
            
            // in_word の下位8ビットが受信データ
            byte b = in_word & 0xFF;
            
            // パリティチェックを行いたい場合は以下を参照
            // byte parity = (in_word >> 8) & 0x01;
            
            rxBuf[totalRead++] = b;
        }
    }
    
    // 5. 受信完了後、RX SM を停止
    pio_sm_set_enabled(_pio, _rx_sm, false);

    if (_debug) {
        Serial.print(F("[ICS RX Valid] "));
        for (int i = 0; i < rxLen; i++) {
            Serial.printf("0x%02X ", rxBuf[i]);
        }
        Serial.println();
    }

    return true;
#else
    Serial.println("Error: This library requires RP2040 Architecture.");
    return false;
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
