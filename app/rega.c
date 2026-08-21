/*
 * REGA ZVEI five-tone emergency sequence - ported for xm-k5-firmware
 * ================================================================
 * Ported from Dondji firmware (Dondji/App/app/rega.c) by Markus Baertschi
 * (https://github.com/markusb), Copyright 2025, Apache License 2.0.
 *
 * xm-k5-firmware is licensed under Apache License 2.0.
 * This file is part of xm-k5-firmware, a custom firmware for the
 * Quansheng UV-K5 based on the LOSEHU (uv-k5-firmware-custom) code base.
 *
 * Port notes:
 *   - Include paths aligned to LOSEHU base (bsp/dp32g030/gpio.h etc.)
 *   - Tone tables and ZVEI timing identical to upstream Dondji.
 *   - UI_DisplayREGA() and DISPLAY_REGA screen are NOT ported; the xm
 *     emergency layer (app/emergency.c) owns the display during alarm.
 *   - REGA_TransmitZvei() exported for xm emergency L3 (ZVEI layer).
 */

#include <string.h>
#include <stdio.h>
#include <stdbool.h>

#include "rega.h"
#include "radio.h"
#include "action.h"
#include "misc.h"
#include "settings.h"
#include "functions.h"
#include "driver/bk4819.h"
#include "driver/st7565.h"
#include "driver/system.h"
#include "driver/gpio.h"
#include "bsp/dp32g030/gpio.h"
#include "ui/helper.h"
#include "ui/ui.h"

// ZVEI tone sequence for alarm: 21414 (xm: only the alarm sequence ships;
// the upstream 21301 test sequence is trimmed for the 60KB flash budget)
const uint16_t rega_alarm_tones[5] = {
    1160,  // 2 1160Hz
    1060,  // 1 1060Hz
    1400,  // 4 1400Hz
    1060,  // 1 1060Hz
    1400,  // 4 1400Hz
};

// Transmit the ZVEI alarm tone sequence (21414)

// Transmit a ZVEI tone sequence on the current VFO frequency
// tones: array of 5 ZVEI tones
// message: message shown on screen while transmitting
void REGA_TransmitZvei(const uint16_t tones[], const char message[])
{
    (void)message; // xm emergency layer owns the screen

    // xm: decorative LED flashes removed for size; the radio switches to
    // transmit immediately below (PA status is visible on the TX screen)

    // Set the radio to transmit mode (frequency/modulation are prepared
    // by the xm emergency layer before calling this function)
    RADIO_PrepareCssTX();
    FUNCTION_Select(FUNCTION_TRANSMIT);

    // Wait to allow the radio to switch to transmit mode and stabilize the transmitter
    SYSTEM_DelayMs(ZVEI_PRE_LENGTH_MS);

    // Send out the ZVEI2 tone sequence
    for (int i = 0; i < ZVEI_NUM_TONES; i++)
    {
        BK4819_PlaySingleTone(tones[i], ZVEI_TONE_LENGTH_MS, 100, true);
        SYSTEM_DelayMs(ZVEI_PAUSE_LENGTH_MS);
    }

    // Wait to allow the radio to finish transmitting
    SYSTEM_DelayMs(ZVEI_POST_LENGTH_MS);

    // Return to the normal screen
    gRequestDisplayScreen = DISPLAY_MAIN;
}
