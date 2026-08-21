/*
 * REGA ZVEI five-tone emergency sequence - ported for xm-k5-firmware
 * ================================================================
 * Ported from Dondji firmware (Dondji/App/app/rega.c) by Markus Baertschi
 * (https://github.com/markusb), Copyright 2025, Apache License 2.0.
 * Original upstream header preserved below.
 *
 * xm-k5-firmware port notes:
 *   - Include paths adjusted to LOSEHU (uv-k5-firmware-custom) base tree.
 *   - ZVEI tone tables (alarm 21414 / test 21301) and timing constants
 *     are byte-identical to upstream Dondji.
 *   - Frequency/CTCSS forcing removed here; xm integrates REGA as the L3
 *     layer of its own three-layer emergency alarm (see app/emergency.c),
 *     which performs TX whitelist checks before any RF is emitted.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef REGA_H
#define REGA_H

#include <stdint.h>

#define ZVEI_NUM_TONES 5
#define ZVEI_TONE_LENGTH_MS 70
#define ZVEI_PAUSE_LENGTH_MS 10
#define ZVEI_PRE_LENGTH_MS 300
#define ZVEI_POST_LENGTH_MS 100
#define REGA_CTCSS_FREQ_INDEX 18 // dcs.c: CTCSS_Options[18] = 1230
#define REGA_FREQUENCY 16130000

void REGA_TransmitZvei(const uint16_t[], const char[]);

// xm: tone table (used by app/emergency.c L3 layer)
extern const uint16_t rega_alarm_tones[5];

#endif
