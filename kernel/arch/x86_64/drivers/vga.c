#include <arch/x86_64/drivers/vga.h>
#include <arch/x86_64/io.h>
#include <dunix/string.h>

#define VGA_BUFFER_VIRTUAL ((volatile uint16_t *)0xFFFFFFFF800B8000ULL)

static size_t vga_row;
static size_t vga_column;
static uint8_t vga_color_attr;
static volatile uint16_t *vga_buffer = VGA_BUFFER_VIRTUAL;

/* ANSI Escape Sequence State Machine */
enum ansi_state {
    ANSI_STATE_NORMAL = 0,
    ANSI_STATE_ESC,
    ANSI_STATE_CSI,
    ANSI_STATE_OSC
};

static enum ansi_state ansi_current_state = ANSI_STATE_NORMAL;
static int ansi_args[8];
static int ansi_arg_idx = 0;
static bool ansi_has_digit = false;
static bool ansi_bold = false;
static uint8_t current_fg = VGA_COLOR_LIGHT_GREY;
static uint8_t current_bg = VGA_COLOR_BLACK;

static void update_hardware_cursor(void) {
    uint16_t pos = (uint16_t)(vga_row * VGA_WIDTH + vga_column);
    outb(0x3D4, 0x0F);
    outb(0x3D5, (uint8_t)(pos & 0xFF));
    outb(0x3D4, 0x0E);
    outb(0x3D5, (uint8_t)((pos >> 8) & 0xFF));
}

void vga_init(void) {
    vga_row = 0;
    vga_column = 0;
    current_fg = VGA_COLOR_LIGHT_GREY;
    current_bg = VGA_COLOR_BLACK;
    ansi_bold = false;
    ansi_current_state = ANSI_STATE_NORMAL;
    vga_color_attr = vga_entry_color(current_fg, current_bg);
    vga_clear();
}

void vga_set_color(enum vga_color fg, enum vga_color bg) {
    current_fg = (uint8_t)fg;
    current_bg = (uint8_t)bg;
    vga_color_attr = vga_entry_color(fg, bg);
}

void vga_clear(void) {
    for (size_t y = 0; y < VGA_HEIGHT; y++) {
        for (size_t x = 0; x < VGA_WIDTH; x++) {
            const size_t index = y * VGA_WIDTH + x;
            vga_buffer[index] = vga_entry(' ', vga_color_attr);
        }
    }
    vga_row = 0;
    vga_column = 0;
    update_hardware_cursor();
}

static void vga_scroll(void) {
    for (size_t y = 0; y < VGA_HEIGHT - 1; y++) {
        for (size_t x = 0; x < VGA_WIDTH; x++) {
            vga_buffer[y * VGA_WIDTH + x] = vga_buffer[(y + 1) * VGA_WIDTH + x];
        }
    }
    for (size_t x = 0; x < VGA_WIDTH; x++) {
        vga_buffer[(VGA_HEIGHT - 1) * VGA_WIDTH + x] = vga_entry(' ', vga_color_attr);
    }
    vga_row = VGA_HEIGHT - 1;
}

static void vga_raw_putchar(char c) {
    if (c == '\n') {
        vga_column = 0;
        if (++vga_row == VGA_HEIGHT) {
            vga_scroll();
        }
    } else if (c == '\r') {
        vga_column = 0;
    } else if (c == '\t') {
        size_t tab_stop = (vga_column + 8) & ~7;
        while (vga_column < tab_stop && vga_column < VGA_WIDTH) {
            vga_raw_putchar(' ');
        }
    } else if (c == '\b') {
        if (vga_column > 0) {
            vga_column--;
            vga_buffer[vga_row * VGA_WIDTH + vga_column] = vga_entry(' ', vga_color_attr);
        }
    } else {
        const size_t index = vga_row * VGA_WIDTH + vga_column;
        vga_buffer[index] = vga_entry((unsigned char)c, vga_color_attr);
        if (++vga_column == VGA_WIDTH) {
            vga_column = 0;
            if (++vga_row == VGA_HEIGHT) {
                vga_scroll();
            }
        }
    }
    update_hardware_cursor();
}

static void vga_apply_sgr(int arg_count) {
    static const uint8_t ansi_to_vga[8] = {
        VGA_COLOR_BLACK,        /* 30/40: Black */
        VGA_COLOR_RED,          /* 31/41: Red */
        VGA_COLOR_GREEN,        /* 32/42: Green */
        VGA_COLOR_BROWN,        /* 33/43: Yellow / Brown */
        VGA_COLOR_BLUE,         /* 34/44: Blue */
        VGA_COLOR_MAGENTA,      /* 35/45: Magenta */
        VGA_COLOR_CYAN,         /* 36/46: Cyan */
        VGA_COLOR_LIGHT_GREY    /* 37/47: White / Light Grey */
    };

    static const uint8_t ansi_bright_to_vga[8] = {
        VGA_COLOR_DARK_GREY,    /* 90: Dark Grey (Bright Black) */
        VGA_COLOR_LIGHT_RED,    /* 91: Light Red */
        VGA_COLOR_LIGHT_GREEN,  /* 92: Light Green */
        VGA_COLOR_YELLOW,       /* 93: Yellow */
        VGA_COLOR_LIGHT_BLUE,   /* 94: Light Blue */
        VGA_COLOR_LIGHT_MAGENTA,/* 95: Light Magenta */
        VGA_COLOR_LIGHT_CYAN,   /* 96: Light Cyan */
        VGA_COLOR_WHITE         /* 97: Bright White */
    };

    if (arg_count == 0) {
        ansi_bold = false;
        current_fg = VGA_COLOR_LIGHT_GREY;
        current_bg = VGA_COLOR_BLACK;
    }

    for (int i = 0; i < arg_count; i++) {
        int code = ansi_args[i];
        if (code == 0) {
            ansi_bold = false;
            current_fg = VGA_COLOR_LIGHT_GREY;
            current_bg = VGA_COLOR_BLACK;
        } else if (code == 1) {
            ansi_bold = true;
        } else if (code == 22) {
            ansi_bold = false;
        } else if (code == 7) {
            uint8_t tmp = current_fg;
            current_fg = current_bg;
            current_bg = tmp;
        } else if (code >= 30 && code <= 37) {
            current_fg = ansi_to_vga[code - 30];
        } else if (code == 39) {
            current_fg = VGA_COLOR_LIGHT_GREY;
        } else if (code >= 40 && code <= 47) {
            current_bg = ansi_to_vga[code - 40];
        } else if (code == 49) {
            current_bg = VGA_COLOR_BLACK;
        } else if (code >= 90 && code <= 97) {
            current_fg = ansi_bright_to_vga[code - 90];
        } else if (code >= 100 && code <= 107) {
            current_bg = ansi_to_vga[code - 100];
        }
    }

    uint8_t effective_fg = current_fg;
    if (ansi_bold && effective_fg < 8) {
        effective_fg += 8;
    }
    vga_color_attr = vga_entry_color((enum vga_color)effective_fg, (enum vga_color)current_bg);
}

void vga_putchar(char c) {
    if (ansi_current_state == ANSI_STATE_NORMAL) {
        if (c == 0x1B) { /* ESC */
            ansi_current_state = ANSI_STATE_ESC;
            return;
        }
        vga_raw_putchar(c);
        return;
    }

    if (ansi_current_state == ANSI_STATE_ESC) {
        if (c == '[') {
            ansi_current_state = ANSI_STATE_CSI;
            ansi_arg_idx = 0;
            ansi_has_digit = false;
            for (int i = 0; i < 8; i++) ansi_args[i] = 0;
            return;
        } else if (c == ']') {
            ansi_current_state = ANSI_STATE_OSC;
            return;
        } else if (c == 'c') { /* Reset Terminal */
            vga_init();
            return;
        }
        ansi_current_state = ANSI_STATE_NORMAL;
        vga_raw_putchar(c);
        return;
    }

    if (ansi_current_state == ANSI_STATE_OSC) {
        if (c == 0x07 || c == '\n') {
            ansi_current_state = ANSI_STATE_NORMAL;
        } else if (c == 0x1B) {
            ansi_current_state = ANSI_STATE_ESC;
        }
        return;
    }

    if (ansi_current_state == ANSI_STATE_CSI) {
        if (c >= '0' && c <= '9') {
            ansi_args[ansi_arg_idx] = ansi_args[ansi_arg_idx] * 10 + (c - '0');
            ansi_has_digit = true;
            return;
        }

        if (c == ';') {
            if (ansi_arg_idx < 7) {
                ansi_arg_idx++;
            }
            ansi_has_digit = false;
            return;
        }

        if (c == '?' || c == '>') {
            return; /* Ignore private mode prefixes */
        }

        /* End of CSI sequence */
        ansi_current_state = ANSI_STATE_NORMAL;
        int arg_count = ansi_has_digit ? (ansi_arg_idx + 1) : ansi_arg_idx;

        if (c == 'm') {
            vga_apply_sgr(arg_count);
            return;
        }

        if (c == 'H' || c == 'f') {
            int row = (ansi_args[0] > 0) ? (ansi_args[0] - 1) : 0;
            int col = (arg_count > 1 && ansi_args[1] > 0) ? (ansi_args[1] - 1) : 0;
            vga_set_cursor((size_t)col, (size_t)row);
            return;
        }

        if (c == 'A') {
            int count = (ansi_args[0] > 0) ? ansi_args[0] : 1;
            if (vga_row >= (size_t)count) vga_row -= (size_t)count;
            else vga_row = 0;
            update_hardware_cursor();
            return;
        }

        if (c == 'B') {
            int count = (ansi_args[0] > 0) ? ansi_args[0] : 1;
            vga_row += (size_t)count;
            if (vga_row >= VGA_HEIGHT) vga_row = VGA_HEIGHT - 1;
            update_hardware_cursor();
            return;
        }

        if (c == 'C') {
            int count = (ansi_args[0] > 0) ? ansi_args[0] : 1;
            vga_column += (size_t)count;
            if (vga_column >= VGA_WIDTH) vga_column = VGA_WIDTH - 1;
            update_hardware_cursor();
            return;
        }

        if (c == 'D') {
            int count = (ansi_args[0] > 0) ? ansi_args[0] : 1;
            if (vga_column >= (size_t)count) vga_column -= (size_t)count;
            else vga_column = 0;
            update_hardware_cursor();
            return;
        }

        if (c == 'J') {
            if (ansi_args[0] == 2) {
                vga_clear();
            }
            return;
        }

        if (c == 'K') {
            if (ansi_args[0] == 0) {
                for (size_t x = vga_column; x < VGA_WIDTH; x++) {
                    vga_buffer[vga_row * VGA_WIDTH + x] = vga_entry(' ', vga_color_attr);
                }
            } else if (ansi_args[0] == 1) {
                for (size_t x = 0; x <= vga_column && x < VGA_WIDTH; x++) {
                    vga_buffer[vga_row * VGA_WIDTH + x] = vga_entry(' ', vga_color_attr);
                }
            } else if (ansi_args[0] == 2) {
                for (size_t x = 0; x < VGA_WIDTH; x++) {
                    vga_buffer[vga_row * VGA_WIDTH + x] = vga_entry(' ', vga_color_attr);
                }
            }
            return;
        }

        /* Default: ignore unknown CSI command */
        return;
    }
}

void vga_puts(const char *str) {
    while (*str) {
        vga_putchar(*str++);
    }
}

void vga_write(const char *data, size_t size) {
    for (size_t i = 0; i < size; i++) {
        vga_putchar(data[i]);
    }
}

void vga_set_cursor(size_t x, size_t y) {
    if (x < VGA_WIDTH && y < VGA_HEIGHT) {
        vga_column = x;
        vga_row = y;
        update_hardware_cursor();
    }
}
