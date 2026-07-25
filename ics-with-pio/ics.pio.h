// ----------------------------------------------------------------------------
// ics.pio.h
// Arduino IDE環境用（pioasmによる自動生成が動作しない環境のための直接定義ヘッダー）
// ----------------------------------------------------------------------------

#ifndef _ICS_PIO_H_
#define _ICS_PIO_H_

#if defined(ARDUINO_ARCH_RP2040)

#include "pico/stdlib.h"
#include "hardware/pio.h"
#include "hardware/clocks.h"

// ============================================================================
// ics_tx プログラム定義 (8E1 送信用PIOプログラム)
// ============================================================================

static const uint16_t ics_tx_program_instructions[] = {
    0x9020, // 0: pull block          side 0
    0xff29, // 1: set x, 9            side 1 [7]
    0x66e1, // 2: out pindirs, 1         [6]
    0x0022, // 3: jmp x-- 2
};

static const struct pio_program ics_tx_program = {
    .instructions = ics_tx_program_instructions,
    .length = 4,
    .origin = -1,
};

static inline pio_sm_config ics_tx_program_get_default_config(uint offset) {
    pio_sm_config c = pio_get_default_sm_config();
    sm_config_set_wrap(&c, offset + 0, offset + 3);
    sm_config_set_sideset(&c, 2, true, true); // 1-bit side-set + 1-bit opt
    return c;
}

static inline void ics_tx_program_init(PIO pio, uint sm, uint offset, uint pin, uint baud) {
    pio_sm_config c = ics_tx_program_get_default_config(offset);
    
    // ピンの初期設定 (オープンドレイン制御のため常に 0 出力)
    pio_gpio_init(pio, pin);
    gpio_put(pin, 0);

    sm_config_set_out_pins(&c, pin, 1);
    sm_config_set_sideset_pins(&c, pin);
    
    // 右シフト (LSB first), Auto-pull無効
    sm_config_set_out_shift(&c, true, false, 32);

    // 1bit = 8 cycles に設定
    float div = (float)clock_get_hz(clk_sys) / (baud * 8.0f);
    sm_config_set_clkdiv(&c, div);

    pio_sm_init(pio, sm, offset, &c);
}

// ============================================================================
// ics_rx プログラム定義 (8E1 受信用PIOプログラム)
// ============================================================================

static const uint16_t ics_rx_program_instructions[] = {
    0x2000, // 0: wait 0 pin 0
    0xea29, // 1: set x, 9               [10]
    0x4601, // 2: in pins, 1             [6]
    0x0022, // 3: jmp x-- 2
    0x8000, // 4: push noblock
};

static const struct pio_program ics_rx_program = {
    .instructions = ics_rx_program_instructions,
    .length = 5,
    .origin = -1,
};

static inline pio_sm_config ics_rx_program_get_default_config(uint offset) {
    pio_sm_config c = pio_get_default_sm_config();
    sm_config_set_wrap(&c, offset + 0, offset + 4);
    return c;
}

static inline void ics_rx_program_init(PIO pio, uint sm, uint offset, uint pin, uint baud) {
    pio_sm_config c = ics_rx_program_get_default_config(offset);
    
    pio_gpio_init(pio, pin);

    sm_config_set_in_pins(&c, pin);
    sm_config_set_jmp_pin(&c, pin);

    // 右シフト (LSB first), Auto-push無効
    sm_config_set_in_shift(&c, true, false, 32);

    // 1bit = 8 cycles に設定
    float div = (float)clock_get_hz(clk_sys) / (baud * 8.0f);
    sm_config_set_clkdiv(&c, div);

    pio_sm_init(pio, sm, offset, &c);
}

#endif // ARDUINO_ARCH_RP2040

#endif // _ICS_PIO_H_
