/* xm-k5-firmware menu helpers (zone badge)
 * Copyright 2025 xm-k5-firmware contributors
 * Licensed under the Apache License, Version 2.0
 */
#ifndef UI_XM_MENU_H
#define UI_XM_MENU_H

#include <stdint.h>
#include <stdbool.h>

// menu zone ids (spec section 5)
enum {
    XM_ZONE_RADIO = 1,   // 1 radio
    XM_ZONE_CHANNEL,     // 2 channel
    XM_ZONE_SIGNAL,      // 3 signal
    XM_ZONE_DTMF,        // 4 dtmf
    XM_ZONE_DISPLAY,     // 5 display/keys
    XM_ZONE_SYSTEM       // 6 system
};

uint8_t XM_MENU_ZoneOf(uint8_t menu_id);       // menu_id -> zone
const char *XM_MENU_ZoneBadge(uint8_t zone);   // "[1]".."[6]"

#endif
