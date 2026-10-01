#include <arch/x86_64/drivers/pit.h>
#include <arch/x86_64/cpu/idt.h>
#include <arch/x86_64/io.h>
#include <dunix/kprintf.h>

#define PIT_CHANNEL0_DATA 0x40
#define PIT_COMMAND_PORT  0x43

static volatile uint64_t timer_ticks = 0;
static uint32_t timer_frequency = PIT_TARGET_HZ;

#include <sched/sched.h>

static void pit_callback(struct interrupt_frame *frame) {
    timer_ticks++;
    sched_tick(frame);
}

void pit_init(uint32_t frequency) {
    timer_frequency = frequency ? frequency : PIT_TARGET_HZ;
    uint32_t divisor = PIT_BASE_FREQUENCY / timer_frequency;

    /* Register IRQ 0 handler (Vector 32) */
    register_interrupt_handler(32, pit_callback);

    /* Command byte: Channel 0, Lobyle/Hibyte, Mode 3 (Square wave generator) */
    outb(PIT_COMMAND_PORT, 0x36);

    uint8_t low  = (uint8_t)(divisor & 0xFF);
    uint8_t high = (uint8_t)((divisor >> 8) & 0xFF);

    outb(PIT_CHANNEL0_DATA, low);
    outb(PIT_CHANNEL0_DATA, high);

    klog(KLOG_INFO, "PIT timer initialized at %u Hz (divisor=%u)\n", timer_frequency, divisor);
}

uint64_t pit_get_ticks(void) {
    return timer_ticks;
}

uint64_t pit_get_uptime_ms(void) {
    return (timer_ticks * 1000) / timer_frequency;
}

void pit_sleep_ticks(uint64_t ticks) {
    uint64_t target = timer_ticks + ticks;
    while (timer_ticks < target) {
        hlt();
    }
}
