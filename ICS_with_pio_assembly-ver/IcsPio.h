#ifndef ICS_PIO_H
#define ICS_PIO_H

#include <Arduino.h>

#if defined(ARDUINO_ARCH_RP2040)
#include "hardware/pio.h"
#endif

/**
 * @file IcsPio.h
 * @brief ICS 3.5 / 3.6 通信用ライブラリ (RP2040 SDK PIO オープンドレイン実装)
 * 
 * RP2040 (Raspberry Pi Pico) の PIO を利用し、追加回路なしの単一GPIOピンのみで
 * 精度100%の ICS 8E1 ハーフデュプレックス通信を実現します。
 */

// ============================================================================
// ICS プロトコルコマンド定数
// ============================================================================
constexpr byte ICS_CMD_POS   = 0x80; // 位置指示 (1000 0000)
constexpr byte ICS_CMD_READ  = 0xA0; // パラメータ読み込み (1010 0000)
constexpr byte ICS_CMD_WRITE = 0xC0; // パラメータ書き込み (1100 0000)
constexpr byte ICS_CMD_ID    = 0xE0; // IDコマンド (1110 0000)

// ICS サブコマンド定数
constexpr byte ICS_SC_EEPROM   = 0x00; // EEPROMデータ (64B)
constexpr byte ICS_SC_STRC     = 0x01; // ストレッチ (保持力)
constexpr byte ICS_SC_SPD      = 0x02; // スピード
constexpr byte ICS_SC_CUR      = 0x03; // カレント (電流値)
constexpr byte ICS_SC_TMP      = 0x04; // テンパレチャ (温度)
constexpr byte ICS_SC_DEADBAND = 0x05; // デッドバンド (不感帯)

// ============================================================================
// EEPROM メモリアドレス定義 (ICS 3.5 / 3.6 仕様)
// ============================================================================
namespace IcsEepromAddr {
    constexpr byte ID          = 0x00;
    constexpr byte BAUD        = 0x01;
    constexpr byte STRETCH     = 0x02;
    constexpr byte SPEED       = 0x03;
    constexpr byte PUNCH       = 0x04;
    constexpr byte DEADBAND    = 0x05;
    constexpr byte DAMPING     = 0x06;
    constexpr byte FAILSAFE    = 0x07;
    constexpr byte POS_LIMIT_H = 0x0E;
    constexpr byte POS_LIMIT_L = 0x0F;
    constexpr byte OFFSET      = 0x14;
}

// ============================================================================
// IcsPio クラス定義
// ============================================================================
class IcsPio {
public:
    /**
     * @brief コンストラクタ
     * @param pin     ICS通信で使用する単一GPIOピン番号 (SIG直結)
     * @param baud    通信速度 (デフォルト 115200 bps)
     * @param timeout 通信タイムアウト時間 (ms)
     */
    IcsPio(uint8_t pin, long baud = 115200, int timeout = 100);
    ~IcsPio();

    /**
     * @brief PIOの初期化
     * @return 初期化成功時 true
     */
    bool begin();

    /**
     * @brief デバッグログ出力の有効/無効化
     */
    void setDebug(bool enable) { _debug = enable; }

    // ------------------------------------------------------------------------
    // 公式ライブラリ (IcsHardSerialClass) 互換メソッド
    // ------------------------------------------------------------------------
    int setPos(byte id, int pos);
    int setFree(byte id);
    int getPos(byte id);

    int setStretch(byte id, int stretch);
    int getStretch(byte id);

    int setSpeed(byte id, int speed);
    int getSpeed(byte id);

    int setDeadband(byte id, int deadband);
    int getDeadband(byte id);

    int getCur(byte id);
    int getTmp(byte id);

    static int degPos(float deg);
    static float posDeg(int pos);

    // ------------------------------------------------------------------------
    // EEPROM アクセス関数 (全64B一括処理)
    // ------------------------------------------------------------------------
    bool readEEPROM(byte id, byte* buffer64);
    bool writeEEPROM(byte id, const byte* buffer64);
    void printEEPROM(byte id, Stream& serial = Serial);

    bool synchronize(const byte* txBuf, int txLen, byte* rxBuf, int rxLen);

private:
    uint8_t _pin;
    long _baud;
    int _timeout;
    bool _debug;

#if defined(ARDUINO_ARCH_RP2040)
    PIO _pio;
    uint _tx_sm;
    uint _rx_sm;
    uint _tx_offset;
    uint _rx_offset;
#endif

    static bool calcEvenParity(byte data);
    bool rawHalfDuplexTransfer(const byte* txBuf, int txLen, byte* rxBuf, int rxLen);
};

#endif // ICS_PIO_H
