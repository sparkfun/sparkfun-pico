
/**
 * @file sfe_psram.c
 *
 * @brief This file contains a function that is used to detect and initialize PSRAM on
 *  SparkFun rp2350 boards.
 */

/*
The MIT License (MIT)

Copyright (c) 2024 SparkFun Electronics

Permission is hereby granted, free of charge, to any person obtaining a
copy of this software and associated documentation files (the "Software"),
to deal in the Software without restriction, including without limitation
the rights to use, copy, modify, merge, publish, distribute, sublicense,
and/or sell copies of the Software, and to permit persons to whom the
Software is furnished to do so, subject to the following conditions: The
above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software. THE SOFTWARE IS PROVIDED
"AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT
NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR
PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

*/
#include "hardware/address_mapped.h"
#include "hardware/clocks.h"
#include "hardware/gpio.h"
#include "hardware/regs/addressmap.h"
#include "hardware/spi.h"
#include "hardware/structs/qmi.h"
#include "hardware/structs/xip_ctrl.h"
#include "hardware/sync.h"
#include "pico/binary_info.h"
#include "pico/flash.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// DETAILS/
//
// SparkFun RP2350 boards use the following PSRAM IC:
//
//      apmemory APS6404L-3SQR-ZR
//      https://www.mouser.com/ProductDetail/AP-Memory/APS6404L-3SQR-ZR?qs=IS%252B4QmGtzzpDOdsCIglviw%3D%3D
//
// The origin of this logic is from the Circuit Python code that was downloaded from:
//     https://github.com/raspberrypi/pico-sdk-rp2350/issues/12#issuecomment-2055274428
//

// Details on the PSRAM IC that are used during setup/configuration of PSRAM on SparkFun RP2350 boards.

// max select pulse width = 8us
#define SFE_PSRAM_MAX_SELECT 0.000008f

// min deselect pulse width = 50ns
#define SFE_PSRAM_MIN_DESELECT 0.000000050f

// from psram datasheet - max Freq at 3.3v
#define SFE_PSRAM_MAX_SCK_HZ 109000000.f

// PSRAM SPI command codes
#define PSRAM_CMD_QUAD_END 0xF5
#define PSRAM_CMD_QUAD_ENABLE 0x35
#define PSRAM_CMD_READ_ID 0x9F
#define PSRAM_CMD_RSTEN 0x66
#define PSRAM_CMD_RST 0x99
#define PSRAM_CMD_QUAD_READ 0xEB
#define PSRAM_CMD_QUAD_WRITE 0x38
#define PSRAM_CMD_NOOP 0xFF

#define PSRAM_ID 0x5D

//-----------------------------------------------------------------------------
/// @brief Communicate directly with the PSRAM IC - validate it is present and return the size
///
/// @return size_t The size of the PSRAM
///
/// @note This function expects the CS pin set
static size_t __no_inline_not_in_flash_func(get_psram_size)(void)
{
    size_t psram_size = 0;
    uint32_t intr_stash = save_and_disable_interrupts();

    // Try and read the PSRAM ID via direct_csr.
    qmi_hw->direct_csr = 30 << QMI_DIRECT_CSR_CLKDIV_LSB | QMI_DIRECT_CSR_EN_BITS;

    // Need to poll for the cooldown on the last XIP transfer to expire
    // (via direct-mode BUSY flag) before it is safe to perform the first
    // direct-mode operation
    while ((qmi_hw->direct_csr & QMI_DIRECT_CSR_BUSY_BITS) != 0)
    {
    }

    // Exit out of QMI in case we've inited already
    qmi_hw->direct_csr |= QMI_DIRECT_CSR_ASSERT_CS1N_BITS;

    // Transmit the command to exit QPI quad mode - read ID as standard SPI
    qmi_hw->direct_tx =
        QMI_DIRECT_TX_OE_BITS | QMI_DIRECT_TX_IWIDTH_VALUE_Q << QMI_DIRECT_TX_IWIDTH_LSB | PSRAM_CMD_QUAD_END;

    while ((qmi_hw->direct_csr & QMI_DIRECT_CSR_BUSY_BITS) != 0)
    {
    }

    (void)qmi_hw->direct_rx;
    qmi_hw->direct_csr &= ~(QMI_DIRECT_CSR_ASSERT_CS1N_BITS);

    // Read the id
    qmi_hw->direct_csr |= QMI_DIRECT_CSR_ASSERT_CS1N_BITS;
    uint8_t kgd = 0;
    uint8_t eid = 0;
    for (size_t i = 0; i < 7; i++)
    {
        qmi_hw->direct_tx = (i == 0 ? PSRAM_CMD_READ_ID : PSRAM_CMD_NOOP);

        while ((qmi_hw->direct_csr & QMI_DIRECT_CSR_TXEMPTY_BITS) == 0)
        {
        }
        while ((qmi_hw->direct_csr & QMI_DIRECT_CSR_BUSY_BITS) != 0)
        {
        }
        if (i == 5)
            kgd = qmi_hw->direct_rx;
        else if (i == 6)
            eid = qmi_hw->direct_rx;
        else
            (void)qmi_hw->direct_rx; // just read and discard
    }

    // Disable direct csr.
    qmi_hw->direct_csr &= ~(QMI_DIRECT_CSR_ASSERT_CS1N_BITS | QMI_DIRECT_CSR_EN_BITS);

    // is this the PSRAM we're looking for obi-wan?
    if (kgd == PSRAM_ID)
    {
        // PSRAM size
        psram_size = 1024 * 1024; // 1 MiB
        uint8_t size_id = eid >> 5;
        if (eid == 0x26 || size_id == 2)
            psram_size *= 8;
        else if (size_id == 0)
            psram_size *= 2;
        else if (size_id == 1)
            psram_size *= 4;
    }
    restore_interrupts(intr_stash);
    return psram_size;
}
//-----------------------------------------------------------------------------
/// @brief Update the PSRAM timing configuration based on system clock
///
/// @note This function expects interrupts to be enabled on entry

static void __no_inline_not_in_flash_func(set_psram_timing)(void)
{
    // Get secs / cycle for the system clock - get before disabling interrupts
    float sysHz = clock_get_hz(clk_sys);
    float secsPerCycle = 1.0f / sysHz;

    volatile uint8_t clockDivider = (uint8_t)ceil(sysHz / SFE_PSRAM_MAX_SCK_HZ);

    uint32_t intr_stash = save_and_disable_interrupts();

    // the maxSelect value is defined in units of 64 clock cycles. = PSRAM MAX Select time/secsPerCycle / 64
    volatile uint8_t maxSelect = (uint8_t)round(SFE_PSRAM_MAX_SELECT / secsPerCycle / 64.0f);

    // minDeselect time - in system clock cycle
    volatile uint8_t minDeselect = (uint8_t)round(SFE_PSRAM_MIN_DESELECT / secsPerCycle);

    // printf("Max Select: %d, Min Deselect: %d, clock divider: %d\n", maxSelect, minDeselect, clockDivider);

    qmi_hw->m[1].timing = QMI_M1_TIMING_PAGEBREAK_VALUE_1024 << QMI_M1_TIMING_PAGEBREAK_LSB | // Break between pages.
                          3 << QMI_M1_TIMING_SELECT_HOLD_LSB | // Delay releasing CS for 3 extra system cycles.
                          1 << QMI_M1_TIMING_COOLDOWN_LSB | 1 << QMI_M1_TIMING_RXDELAY_LSB |
                          maxSelect << QMI_M1_TIMING_MAX_SELECT_LSB | minDeselect << QMI_M1_TIMING_MIN_DESELECT_LSB |
                          clockDivider << QMI_M1_TIMING_CLKDIV_LSB;

    restore_interrupts(intr_stash);
}
//-----------------------------------------------------------------------------
/// @brief The setup_psram function - note that this is not in flash
///
/// @param psram_cs_pin The pin that the PSRAM is connected to
/// @return size_t The size of the PSRAM
///
static size_t __no_inline_not_in_flash_func(setup_psram)(uint32_t psram_cs_pin)
{
    // Set the PSRAM CS pin in the SDK
    gpio_set_function(psram_cs_pin, GPIO_FUNC_XIP_CS1);

    // start with zero size
    size_t psram_size = get_psram_size();

    // No PSRAM - no dice
    if (psram_size == 0)
        return 0;

    uint32_t intr_stash = save_and_disable_interrupts();
    // Enable quad mode.
    qmi_hw->direct_csr = 30 << QMI_DIRECT_CSR_CLKDIV_LSB | QMI_DIRECT_CSR_EN_BITS;

    // Need to poll for the cooldown on the last XIP transfer to expire
    // (via direct-mode BUSY flag) before it is safe to perform the first
    // direct-mode operation
    while ((qmi_hw->direct_csr & QMI_DIRECT_CSR_BUSY_BITS) != 0)
    {
    }

    // RESETEN, RESET and quad enable
    for (uint8_t i = 0; i < 3; i++)
    {
        qmi_hw->direct_csr |= QMI_DIRECT_CSR_ASSERT_CS1N_BITS;
        if (i == 0)
            qmi_hw->direct_tx = PSRAM_CMD_RSTEN;
        else if (i == 1)
            qmi_hw->direct_tx = PSRAM_CMD_RST;
        else
            qmi_hw->direct_tx = PSRAM_CMD_QUAD_ENABLE;

        while ((qmi_hw->direct_csr & QMI_DIRECT_CSR_BUSY_BITS) != 0)
        {
        }
        qmi_hw->direct_csr &= ~(QMI_DIRECT_CSR_ASSERT_CS1N_BITS);
        for (size_t j = 0; j < 20; j++)
            asm("nop");

        (void)qmi_hw->direct_rx;
    }

    // Disable direct csr.
    qmi_hw->direct_csr &= ~(QMI_DIRECT_CSR_ASSERT_CS1N_BITS | QMI_DIRECT_CSR_EN_BITS);

    // check our interrupts and setup the timing
    restore_interrupts(intr_stash);
    set_psram_timing();

    // and now stash interrupts again
    intr_stash = save_and_disable_interrupts();

    qmi_hw->m[1].rfmt = (QMI_M1_RFMT_PREFIX_WIDTH_VALUE_Q << QMI_M1_RFMT_PREFIX_WIDTH_LSB |
                         QMI_M1_RFMT_ADDR_WIDTH_VALUE_Q << QMI_M1_RFMT_ADDR_WIDTH_LSB |
                         QMI_M1_RFMT_SUFFIX_WIDTH_VALUE_Q << QMI_M1_RFMT_SUFFIX_WIDTH_LSB |
                         QMI_M1_RFMT_DUMMY_WIDTH_VALUE_Q << QMI_M1_RFMT_DUMMY_WIDTH_LSB |
                         QMI_M1_RFMT_DUMMY_LEN_VALUE_24 << QMI_M1_RFMT_DUMMY_LEN_LSB |
                         QMI_M1_RFMT_DATA_WIDTH_VALUE_Q << QMI_M1_RFMT_DATA_WIDTH_LSB |
                         QMI_M1_RFMT_PREFIX_LEN_VALUE_8 << QMI_M1_RFMT_PREFIX_LEN_LSB |
                         QMI_M1_RFMT_SUFFIX_LEN_VALUE_NONE << QMI_M1_RFMT_SUFFIX_LEN_LSB);

    qmi_hw->m[1].rcmd = PSRAM_CMD_QUAD_READ << QMI_M1_RCMD_PREFIX_LSB | 0 << QMI_M1_RCMD_SUFFIX_LSB;

    qmi_hw->m[1].wfmt = (QMI_M1_WFMT_PREFIX_WIDTH_VALUE_Q << QMI_M1_WFMT_PREFIX_WIDTH_LSB |
                         QMI_M1_WFMT_ADDR_WIDTH_VALUE_Q << QMI_M1_WFMT_ADDR_WIDTH_LSB |
                         QMI_M1_WFMT_SUFFIX_WIDTH_VALUE_Q << QMI_M1_WFMT_SUFFIX_WIDTH_LSB |
                         QMI_M1_WFMT_DUMMY_WIDTH_VALUE_Q << QMI_M1_WFMT_DUMMY_WIDTH_LSB |
                         QMI_M1_WFMT_DUMMY_LEN_VALUE_NONE << QMI_M1_WFMT_DUMMY_LEN_LSB |
                         QMI_M1_WFMT_DATA_WIDTH_VALUE_Q << QMI_M1_WFMT_DATA_WIDTH_LSB |
                         QMI_M1_WFMT_PREFIX_LEN_VALUE_8 << QMI_M1_WFMT_PREFIX_LEN_LSB |
                         QMI_M1_WFMT_SUFFIX_LEN_VALUE_NONE << QMI_M1_WFMT_SUFFIX_LEN_LSB);

    qmi_hw->m[1].wcmd = PSRAM_CMD_QUAD_WRITE << QMI_M1_WCMD_PREFIX_LSB | 0 << QMI_M1_WCMD_SUFFIX_LSB;

    // Mark that we can write to PSRAM.
    xip_ctrl_hw->ctrl |= XIP_CTRL_WRITABLE_M1_BITS;

    restore_interrupts(intr_stash);

    return psram_size;
}

// public interface

// setup call
size_t sfe_setup_psram(uint32_t psram_cs_pin)
{
    return setup_psram(psram_cs_pin);
}

// update timing -- used if the system clock/timing was changed.
void sfe_psram_update_timing(void)
{
    set_psram_timing();
}