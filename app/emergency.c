/* xm-k5-firmware three-layer emergency alarm
 * ==========================================
 * Copyright 2025 xm-k5-firmware contributors
 * Licensed under the Apache License, Version 2.0
 *
 * Sequencing (spec section 4):
 *   side key 1 long -> ACTION_Emergency()
 *     1. TX_freq_check(gTxVfo TX frequency)
 *          fail  -> L1 screamer still runs, screen shows 禁止发射
 *                   (RADIO_SetVfoState(VFO_STATE_TX_DISABLE)), no RF layers
 *          ok    -> continue
 *     2. prep tone 1kHz x3 (local speaker, no RF)
 *     3. per mode:
 *          L1 : base ALARM state machine (ALARM_STATE_TXALARM)
 *          L2 : MDC1200 op=0x00 emergency frame burst
 *          L3 : ZVEI five-tone 21414 (REGA_TransmitZvei)
 *     4. loop L1 wail until cancelled; MDC/ZVEI repeat each cycle while
 *        their layer is enabled.
 *
 * The whole cycle runs from XM_EMERGENCY_Tick500ms() which the main
 * loop calls on its 500ms timeslice (hook in app.c APP_TimeSlice500ms).
 */

#include <string.h>
#include <stdbool.h>
#include <stdio.h>

#include "emergency.h"
#include "app.h"
#include "action.h"
#include "audio.h"
#include "functions.h"
#include "misc.h"
#include "radio.h"
#include "settings.h"
#include "rega.h"
#include "app/mdc1200.h"
#include "driver/bk4819.h"
#include "driver/eeprom.h"
#include "driver/system.h"
#include "driver/st7565.h"
#include "frequencies.h"
#include "ui/ui.h"
#include "ui/inputbox.h"   // xm: gInputBoxIndex

#define XM_EEPROM_EMERGENCY_MODE_ADDR 0x38000   // spec section 8 private area

uint8_t gXmEmergencyMode = XM_EMG_LOCAL_REMOTE;  // factory default 本地+远程
bool    gXmEmergencyRunning = false;
bool    gXmEmergencyTxBlocked = false;

// sequencer state
static uint8_t  emgPhase;          // see enum below
static uint16_t emgPhaseTick;      // 500ms ticks inside current phase

enum {
    EMG_PHASE_IDLE = 0,
    EMG_PHASE_PRETONE,    // 1kHz x3 prep tones (local audio)
    EMG_PHASE_MDC,        // L2 MDC emergency frame (blocking burst)
    EMG_PHASE_ZVEI,       // L3 ZVEI five-tone (blocking burst)
    EMG_PHASE_WAIL,       // L1 screamer running (state machine in app.c)
};

// ------------------------------------------------------------------
// init / persistence
// ------------------------------------------------------------------

void XM_EMERGENCY_Init(void)
{
    uint8_t v = 0;
    EEPROM_ReadBuffer(XM_EEPROM_EMERGENCY_MODE_ADDR, &v, 1);
    gXmEmergencyMode = (v <= XM_EMG_LOCAL_REMOTE) ? v : XM_EMG_LOCAL_REMOTE;
}

void XM_EMERGENCY_Save(void)
{
    EEPROM_WriteBuffer(XM_EEPROM_EMERGENCY_MODE_ADDR, &gXmEmergencyMode, 1);
}

// ------------------------------------------------------------------
// action entry (side key 1 long press, ACTION_OPT_EMERGENCY)
// ------------------------------------------------------------------

void ACTION_Emergency(void)
{
    if (gXmEmergencyRunning)
    {
        XM_EMERGENCY_Cancel();
        return;
    }

    if (gXmEmergencyMode == XM_EMG_OFF)
        return;   // feature disabled in menu

    // -- step 1: TX whitelist check FIRST (spec: TX_freq_check 先行) ----
    gXmEmergencyTxBlocked = false;

    RADIO_SelectVfos();
    if (TX_freq_check(gTxVfo->freq_config_TX.Frequency) != 0)
    {
        // blocked: L1 still sounds (local alert), RF layers suppressed
        gXmEmergencyTxBlocked = true;
        RADIO_SetVfoState(VFO_STATE_TX_DISABLE);   // screen: 禁止发射
    }

    // -- step 2..4 handled by the 500ms sequencer -----------------------
    gXmEmergencyRunning   = true;
    emgPhase              = EMG_PHASE_PRETONE;
    emgPhaseTick          = 0;

    gInputBoxIndex        = 0;
    gRequestDisplayScreen = DISPLAY_MAIN;
}

void XM_EMERGENCY_Cancel(void)
{
    gXmEmergencyRunning = false;
    emgPhase            = EMG_PHASE_IDLE;
    emgPhaseTick        = 0;

    if (gAlarmState != ALARM_STATE_OFF)
    {
        gAlarmState = ALARM_STATE_OFF;
        BK4819_PlaySingleTone(0, 0, 0, false);
        BK4819_ExitSubAu();
        FUNCTION_Select(FUNCTION_FOREGROUND);
    }
    gFlagPrepareTX = false;

    gRequestDisplayScreen = DISPLAY_MAIN;
}

// ------------------------------------------------------------------
// helpers
// ------------------------------------------------------------------

static bool EmgWantsL1(void)
{
    return gXmEmergencyMode == XM_EMG_LOCAL_ONLY || gXmEmergencyMode == XM_EMG_LOCAL_REMOTE;
}

static bool EmgWantsRfLayers(void)
{
    // remote layers only when whitelist allows RF
    return !gXmEmergencyTxBlocked &&
           (gXmEmergencyMode == XM_EMG_REMOTE_ONLY || gXmEmergencyMode == XM_EMG_LOCAL_REMOTE);
}

static void StartL1Wail(void)
{
    // L1 only in modes that include the local layer (spec section 4), and
    // only when the whitelist allows RF - the stock state machine keys the
    // PA (even "site alarm" re-enables TX), so a blocked alert falls back
    // to our PA-off local-tone loop in EMG_PHASE_WAIL.
    if (gXmEmergencyTxBlocked || !EmgWantsL1())
        return;
    gAlarmRunningCounter = 0;
    gAlarmState = ALARM_STATE_TXALARM;
    gFlagPrepareTX = gAlarmState != ALARM_STATE_OFF;
}

// fire the L2 MDC emergency frame (op 0x00, arg 0x80 emergency)
static void FireMdcEmergency(void)
{
    // short blocking burst; TX hardware already spun up by FUNCTION_TRANSMIT
    BK4819_send_MDC1200(0x00, 0x80, gEeprom.MDC1200_ID, false);
}

// ------------------------------------------------------------------
// 500ms sequencer
// ------------------------------------------------------------------

void XM_EMERGENCY_Tick500ms(void)
{
    if (!gXmEmergencyRunning)
        return;

    emgPhaseTick++;

    switch (emgPhase)
    {
        case EMG_PHASE_PRETONE:
            // 1kHz x3 prep tones, 500ms apart, local audio only (no RF):
            // PlaySingleTone() enables the BK4819 PA gain stage, so the
            // external PA gate (GPIO1) is forced off for these beeps.
            if (emgPhaseTick <= 3)
            {
                BK4819_ToggleGpioOut(BK4819_GPIO1_PIN29_PA_ENABLE, false);
                BK4819_PlaySingleTone(1000, 400, 0, true);
                BK4819_ToggleGpioOut(BK4819_GPIO1_PIN29_PA_ENABLE, false);
            }
            if (emgPhaseTick >= 4)
            {
                emgPhase = EMG_PHASE_WAIL;
                emgPhaseTick = 0;

                // L1 owns the radio if we're going to TX the wail
                StartL1Wail();

                // remote layers fire once per start here (blocking, short)
                if (EmgWantsRfLayers())
                {
                    // order: ZVEI first (analog ears), then MDC (data)
                    REGA_TransmitZvei(rega_alarm_tones, "EMG");
                    FireMdcEmergency();
                }
                gRequestDisplayScreen = DISPLAY_MAIN;
            }
            break;

        case EMG_PHASE_WAIL:
            // blocked (whitelist refused): pure local screamer - the external
            // PA gate stays OFF so no RF leaves the radio despite PA_GAIN
            if (gXmEmergencyTxBlocked)
            {
                const uint16_t tone = 500 + ((emgPhaseTick % 20) * 50);  // 500..1450Hz sweep
                BK4819_ToggleGpioOut(BK4819_GPIO1_PIN29_PA_ENABLE, false);
                BK4819_PlaySingleTone(tone > 1450 ? 500 : tone, 400, 0, true);
                BK4819_ToggleGpioOut(BK4819_GPIO1_PIN29_PA_ENABLE, false);
                break;
            }

            // L1 wail runs in app.c state machine; RF layers repeat
            // every ~10s while active (8 x 1.25s ZVEI+MDC duty cycle)
            if (EmgWantsRfLayers() && emgPhaseTick >= 20)
            {
                emgPhaseTick = 0;
                REGA_TransmitZvei(rega_alarm_tones, "EMG");
                FireMdcEmergency();
            }
            // if user cancelled L1 via PTT/EXIT the state machine clears
            // gAlarmState; keep our flag until any key cancel too
            if (gAlarmState == ALARM_STATE_OFF && !EmgWantsRfLayers())
                XM_EMERGENCY_Cancel();
            break;

        default:
            emgPhase = EMG_PHASE_IDLE;
            break;
    }
}

// ------------------------------------------------------------------
// MDC RX integration (op 0x00 emergency received / op 0x20 ACK)
// ------------------------------------------------------------------

void XM_EMERGENCY_OnMdcRx(uint8_t op, uint8_t arg, uint16_t unit_id)
{
    (void)arg;
    (void)unit_id;   // id is shown by the stock MDC popup path (contact/ID line)

    if (op == 0x00)
    {
        // someone else's emergency: show popup + auto ACK with op 0x20
        // (popup is the existing MDC center-line, driven by
        //  mdc1200_rx_ready_tick_500ms set in the messenger rx path)
        mdc1200_rx_ready_tick_500ms = 2 * 5;
        gUpdateDisplay = true;

        // auto-ACK (L2 etiquette): emergency ack, only if we may TX
        RADIO_SelectVfos();
        if (gXmEmergencyMode != XM_EMG_OFF &&
            TX_freq_check(gTxVfo->freq_config_TX.Frequency) == 0)
        {
            BK4819_send_MDC1200(0x20, 0x00, gEeprom.MDC1200_ID, false);
        }
    }
    else if (op == 0x20)
    {
        // our emergency was acknowledged by unit_id - show it
        mdc1200_rx_ready_tick_500ms = 2 * 5;
        gUpdateDisplay = true;
    }
}
