/* Live-VFO mini spectrum (Live Seek) - ported for xm-k5-firmware
 * from uvk5cec ceccommon.c by KD8CEC (Copyright 2023, Apache-2.0).
 * xm port: only the Live-VFO feature set; CommBuff ownership guarded by
 * CommBuffUsingType arbitration (menu / spectrum / seek must not collide).
 */
#ifndef CEC_LIVESEEK_H
#define CEC_LIVESEEK_H

#include <stdint.h>
#include <stdbool.h>

#define COMBUFF_USE_SEEK_RSSI   1
#define COMBUFF_USE_SEEK_NONE   99
#define COMBUFF_LENGTH          128

#define LIVESEEK_NONE           0
#define LIVESEEK_RCV            1
#define LIVESEEK_RCV_SPECTRUM1  2
#define LIVESEEK_RCV_SPECTRUM2  3

extern uint8_t  CommBuff[COMBUFF_LENGTH];
extern uint8_t  CommBuffUsingType;
extern uint32_t CommBuffLastUseTime;
extern uint8_t  CEC_LiveSeekMode;

uint32_t millis10(void);  // xm: 10ms tick count (scheduler)

void CEC_TimeSlice500ms(void);
void DrawCommBuffToSpectrum(void);
void CEC_ApplyChangeRXFreq(int _applyOption);
void CEC_LiveSeek_Load(void);
void CEC_LiveSeek_Save(void);

#endif
