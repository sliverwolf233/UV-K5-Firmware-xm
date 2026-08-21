/* xm-k5-firmware three-layer emergency alarm
 * ==========================================
 * Copyright 2025 xm-k5-firmware contributors
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Layer design (spec section 4):
 *   L1 local  - base ALARM screamer (app.c 500Hz sweep state machine)
 *   L2 MDC    - MDC1200 op=0x00 emergency frame + op=0x20 auto ACK
 *   L3 ZVEI    - REGA five-tone 21414 via app/rega.c (ported Dondji)
 *
 * Trigger order: TX_freq_check first (on fail L1 still sounds, radio
 * shows "TX DISABLE" via RADIO_SetVfoState(VFO_STATE_TX_DISABLE)),
 * then 1kHz x3 attention tones, then the enabled layers.
 */

#ifndef APP_EMERGENCY_H
#define APP_EMERGENCY_H

#include <stdint.h>
#include <stdbool.h>

// four-position mode stored at EEPROM 0x38000 (spec section 8)
typedef enum {
    XM_EMG_OFF = 0,       // off
    XM_EMG_LOCAL_ONLY,    // local only (L1)
    XM_EMG_REMOTE_ONLY,   // remote only (L2 + L3)
    XM_EMG_LOCAL_REMOTE   // local + remote (L1 + L2 + L3) - default
} XmEmergencyMode_t;

extern uint8_t gXmEmergencyMode;        // XmEmergencyMode_t
extern bool    gXmEmergencyRunning;     // an emergency cycle is active
extern bool    gXmEmergencyTxBlocked;   // whitelist refused RF this cycle

void XM_EMERGENCY_Init(void);           // load mode from EEPROM
void XM_EMERGENCY_Save(void);           // persist mode byte
void ACTION_Emergency(void);            // side-key action entry (M3.1)
void XM_EMERGENCY_Tick500ms(void);      // layer sequencer (from app tick)
void XM_EMERGENCY_Cancel(void);         // any-key cancel

// MDC integration (op 0x00 emergency / 0x20 ACK) - called from messenger rx path
void XM_EMERGENCY_OnMdcRx(uint8_t op, uint8_t arg, uint16_t unit_id);

#endif
