/* Live-VFO mini spectrum (Live Seek) - ported for xm-k5-firmware
 * =================================================================
 * Ported from uvk5cec firmware ceccommon.c by KD8CEC
 * (https://github.com/kd8cec, Copyright 2023, Apache License 2.0).
 * The upstream file marks this block "COPY BELOW TO OTHER FIRMWARE".
 *
 * xm-k5-firmware modifications (Apache-2.0):
 *   - Only the Live-VFO/live-seek feature set is ported; the rest of
 *     ceccommon.c (CW/memory helpers) is not used by xm.
 *   - CommBuff is now xm-local (128B .bss), guarded by CommBuffUsingType
 *     arbitration so menu/spectrum/seek users cannot collide.
 *   - UI_PrintStringSmallLeft not needed: uses base UI_PrintStringSmallNormal.
 *   - gFontBigDigits-based big frequency layout is not drawn here; xm main
 *     screen handles its own layout (see ui/main.c VfosIdentical work).
 */

#include <string.h>
#include <stdio.h>

#include "cecliveseek.h"
#include "app.h"
#include "audio.h"
#include "dcs.h"
#include "functions.h"
#include "misc.h"
#include "radio.h"
#include "settings.h"
#include "driver/bk4819.h"
#include "driver/eeprom.h"
#include "driver/st7565.h"
#include "driver/system.h"
#include "driver/systick.h"
#include "ui/helper.h"
#include "ui/main.h"
#include "ui/ui.h"
#include "font.h"

uint8_t  CommBuff[COMBUFF_LENGTH];
uint8_t  CommBuffUsingType = COMBUFF_USE_SEEK_NONE;
uint32_t CommBuffLastUseTime = 0;
uint8_t  CEC_LiveSeekMode = LIVESEEK_NONE;

static uint8_t  CommValue1 = 0;
static uint8_t  lastSeekDirection = 0;
static uint32_t rssiStartFreq = 0;
static uint32_t addRssiCount = 0;

// EEPROM slot for the Live-VFO mode byte (xm private area, spec section 8)
#define XM_EEPROM_LIVESEEK_ADDR 0x38003

// --------------------------------- RSSI ---------------------------------

static uint16_t CEC_GetRssi(void)
{
    int wait = 0;
    while (((BK4819_ReadRegister(0x63) & 0xFF) >= 255) && wait++ < 100)
        SYSTICK_DelayUs(100);
    return BK4819_GetRSSI();
}

// ------------------------------ drawing ---------------------------------

static void DrawFrequencySmall(uint32_t _frequency, int _startX, int _lineNumber)
{
    char strBuff[16];
    sprintf(strBuff, "%3u.%03u", (unsigned) (_frequency / 100000), (unsigned) ((_frequency / 100) % 1000));
    memset(gFrameBuffer[_lineNumber], 0, 128);
    UI_PrintStringSmall(strBuff, _startX, 127, _lineNumber);
}

void DrawCommBuffToSpectrum(void)
{
    int lowValue = 999;
    const int drawYPosition = 53;

    if (CEC_LiveSeekMode < LIVESEEK_RCV_SPECTRUM1)
        return;
    if (CommBuffUsingType != COMBUFF_USE_SEEK_RSSI)
        return;                 // xm guard: buffer owned by someone else
    if (addRssiCount < 3)
        return;

    for (int i = 0; i < COMBUFF_LENGTH; i++)
        if (CommBuff[i] > 0 && lowValue > CommBuff[i])
            lowValue = CommBuff[i];

    memset(gFrameBuffer[6], 0, 128);    // clear last line

    for (int i = 0; i < COMBUFF_LENGTH; i++)
    {
        const int x = (lastSeekDirection == 12 ? 127 - i : i);
        int v = CommBuff[i] - lowValue;
        if (v > 50) v = 50; else if (v < 0) v = 0;
        for (int y = drawYPosition - v; y <= drawYPosition; y++)
            UI_DrawPixelBuffer(gFrameBuffer, x, y, true);
    }

    uint32_t drawFreq = rssiStartFreq;
    int drawTextPosition;
    if (lastSeekDirection == 12)
    {
        drawTextPosition = 127 - addRssiCount;
        if (drawTextPosition > 73) drawTextPosition = 73;
        else if (drawTextPosition < 0)
        {
            drawTextPosition = 0;
            drawFreq = gTxVfo->freq_config_RX.Frequency - (gTxVfo->StepFrequency * 127);
        }
    }
    else
    {
        drawTextPosition = addRssiCount - 55;
        if (drawTextPosition < 0) drawTextPosition = 0;
        else if (drawTextPosition > 73)
        {
            drawTextPosition = 73;
            drawFreq = gTxVfo->freq_config_RX.Frequency + (gTxVfo->StepFrequency * 127);
        }
    }

    DrawFrequencySmall(drawFreq, drawTextPosition, 3);
    ST7565_BlitFullScreen();
}

// ------------------------- 500ms housekeeping ---------------------------

void CEC_TimeSlice500ms(void)
{
    if (CommBuffUsingType == COMBUFF_USE_SEEK_RSSI && (millis10() - CommBuffLastUseTime > 70))
    {
        CommBuffUsingType = COMBUFF_USE_SEEK_NONE;
        if (gScreenToDisplay == DISPLAY_MAIN)
        {
            gMonitor = false;
            if (addRssiCount > 3)
            {
                if (CommValue1 != FUNCTION_MONITOR)
                    RADIO_SetupRegisters(true);
                UI_DisplayMain();
            }
        }
    }
}

// ------------------------- frequency stepping ---------------------------

#define STOP_RSSI_LIMIT 50
#define STOP_RSSI_TIME  500

void CEC_ApplyChangeRXFreq(int _applyOption)
{
    if (CEC_LiveSeekMode == LIVESEEK_NONE)
        return;

    BK4819_SetFrequency(gTxVfo->freq_config_RX.Frequency);
    BK4819_RX_TurnOn();

    int32_t tmpRssi = CEC_GetRssi() / 3;
    if (tmpRssi < 0) tmpRssi = 0;

    if (_applyOption >= 10 && _applyOption <= 12)   // 10:dir -1, 12:dir +1
    {
        // xm guard: refuse if another subsystem owns the buffer right now
        if (CommBuffUsingType != COMBUFF_USE_SEEK_NONE &&
            CommBuffUsingType != COMBUFF_USE_SEEK_RSSI)
            return;

        CommBuffUsingType = COMBUFF_USE_SEEK_RSSI;
        if (lastSeekDirection != _applyOption || (millis10() - CommBuffLastUseTime > 70))
        {
            memset(CommBuff, 0, sizeof(CommBuff));
            rssiStartFreq = gTxVfo->freq_config_RX.Frequency;
            addRssiCount = 0;
            CommValue1 = gCurrentFunction;
        }

        addRssiCount++;
        lastSeekDirection = _applyOption;

        int insertIndex = COMBUFF_LENGTH - 1;
        if (addRssiCount < COMBUFF_LENGTH / 2)
            insertIndex = COMBUFF_LENGTH / 2 + addRssiCount;
        else
            memmove(&CommBuff[0], &CommBuff[1], sizeof(CommBuff) - 1);
        CommBuff[insertIndex] = tmpRssi > 255 ? 255 : tmpRssi;
        CommBuffLastUseTime = millis10();
    }

    if (gEeprom.SQUELCH_LEVEL == 0)
    {
        if (addRssiCount > 2)
            APP_StartListening(FUNCTION_MONITOR);
    }
    else if (tmpRssi > STOP_RSSI_LIMIT)
    {
        APP_StartListening(FUNCTION_MONITOR);
        SYSTEM_DelayMs(STOP_RSSI_TIME);
        RADIO_SetupRegisters(true);
    }
}

// ------------------------- persistence (xm) -----------------------------

// xm: declared here (the base has no scheduler.h); defined in scheduler.c
extern uint32_t SCHEDULER_GetTickCount10ms(void);

// 10ms tick count (xm): mirrors uvk5cec millis10(), reading the base
// scheduler tick counter (scheduler.c gGlobalSysTickCounter) via accessor
uint32_t millis10(void)
{
    return SCHEDULER_GetTickCount10ms();
}

void CEC_LiveSeek_Load(void)
{
    uint8_t v = 0;
    EEPROM_ReadBuffer(XM_EEPROM_LIVESEEK_ADDR, &v, 1);
    CEC_LiveSeekMode = (v <= LIVESEEK_RCV_SPECTRUM1) ? v : LIVESEEK_NONE;
}

void CEC_LiveSeek_Save(void)
{
    EEPROM_WriteBuffer(XM_EEPROM_LIVESEEK_ADDR, &CEC_LiveSeekMode, 1);
}
