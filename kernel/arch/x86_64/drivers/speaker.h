#ifndef _ARCH_X86_64_DRIVERS_SPEAKER_H
#define _ARCH_X86_64_DRIVERS_SPEAKER_H

#include <dunix/types.h>

/* PC Speaker PIT / System Control Ports */
#define PIT_CH2_DATA_PORT   0x42
#define PIT_COMMAND_PORT    0x43
#define SYSTEM_CTRL_PORT_B  0x61

/* Standard PC PIT Base Frequency: 1.193182 MHz */
#define PIT_BASE_FREQUENCY  1193182

struct speaker_note {
    uint16_t freq_hz;
    uint16_t duration_ms;
};

void speaker_init(void);
void speaker_tone(uint32_t freq_hz);
void speaker_off(void);
void speaker_beep(uint32_t freq_hz, uint32_t duration_ms);

#endif /* _ARCH_X86_64_DRIVERS_SPEAKER_H */
