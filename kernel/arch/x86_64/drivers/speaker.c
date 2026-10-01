#include <arch/x86_64/drivers/speaker.h>
#include <arch/x86_64/io.h>
#include <sched/sched.h>
#include <dunix/kprintf.h>

void speaker_init(void) {
    speaker_off();
    klog(KLOG_INFO, "PC Speaker audio driver initialized (PIT Channel 2 online)\n");
}

void speaker_tone(uint32_t freq_hz) {
    if (freq_hz == 0) {
        speaker_off();
        return;
    }

    uint32_t div = PIT_BASE_FREQUENCY / freq_hz;
    if (div > 65535) div = 65535;
    if (div < 1) div = 1;

    /* Set Channel 2, lo/hi byte access, mode 3 (square wave generator) */
    outb(PIT_COMMAND_PORT, 0xB6);
    outb(PIT_CH2_DATA_PORT, (uint8_t)(div & 0xFF));
    outb(PIT_CH2_DATA_PORT, (uint8_t)((div >> 8) & 0xFF));

    /* Enable gate to speaker and timer output on Port 0x61 */
    uint8_t ctrl = inb(SYSTEM_CTRL_PORT_B);
    if ((ctrl & 0x03) != 0x03) {
        outb(SYSTEM_CTRL_PORT_B, (uint8_t)(ctrl | 0x03));
    }
}

void speaker_off(void) {
    uint8_t ctrl = inb(SYSTEM_CTRL_PORT_B);
    outb(SYSTEM_CTRL_PORT_B, (uint8_t)(ctrl & 0xFC));
}

void speaker_beep(uint32_t freq_hz, uint32_t duration_ms) {
    if (freq_hz > 0) {
        speaker_tone(freq_hz);
    }
    if (duration_ms > 0) {
        sched_sleep(duration_ms);
    }
    speaker_off();
}
