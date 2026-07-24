#ifndef ICS_PIO_H
#define ICS_PIO_H

#include <Arduino.h>

/**
 * @file IcsPio.h
 * @brief ICS 3.5 / 3.6 通信用ライブラリ (RP2040 SerialPIO / Pico SDK ハードウェア対応)
 * 
 * RP2040 (Raspberry Pi Pico) の PIO ハードウェア (SerialPIO) を利用して
 * 単一GPIOピンのみで精度100%のICS 8E1 ハーフデュプレックス通信を実現します。
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
    constexpr byte ID          = 0x00; // サーボID (0~31)
    constexpr byte BAUD        = 0x01; // 通信速度設定 (0:115200, 1:625000, 2:1250000)
    constexpr byte STRETCH     = 0x02; // ストレッチ初期値 (1~127)
    constexpr byte SPEED       = 0x03; // スピード初期値 (1~127)
    constexpr byte PUNCH       = 0x04; // パンチ初期値 (1~10)
    constexpr byte DEADBAND    = 0x05; // デッドバンド初期値 (1~5 / 0~10)
    constexpr byte DAMPING     = 0x06; // ダンピング初期値 (1~255)
    constexpr byte FAILSAFE    = 0x07; // フェイルセーフ設定
    constexpr byte POS_LIMIT_H = 0x0E; // 最高位置制限 (上位)
    constexpr byte POS_LIMIT_L = 0x0F; // 最低位置制限 (下位)
    constexpr byte OFFSET      = 0x14; // オフセット調整値
}

// ============================================================================
// IcsPio クラス定義
// ============================================================================
class IcsPio {
public:
    /**
     * @brief コンストラクタ
     * @param pin     ICS通信で使用する単一GPIOピン番号 (TX/RX兼用, SIG直結)
     * @param baud    通信速度 (デフォルト 115200 bps)
     * @param timeout 通信タイムアウト時間 (ms)
     */
    IcsPio(uint8_t pin, long baud = 115200, int timeout = 100);
    ~IcsPio();

    /**
     * @brief 通信用ピンおよびハードウェアの初期化
     * @return 初期化成功時 true
     */
    bool begin();

    /**
     * @brief デバッグログ出力の有効/無効化
     */
    void setDebug(bool enable) { _debug = enable; }

    // ------------------------------------------------------------------------
    // 公式ライブラリ (IcsHardSerialClass) 再現および制御関数
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
    uint32_t _bitUs;
    bool _debug;
    void* _pioSerialPtr; // RP2040 SerialPIO インスタンスポインタ

    void setPinOutput();
    void setPinInput();
    static bool calcEvenParity(byte data);

    bool rawHalfDuplexTransfer(const byte* txBuf, int txLen, byte* rxBuf, int rxLen);
};

#endif // ICS_PIO_H
