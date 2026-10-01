#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>

#ifndef KIOCSOUND
#define KIOCSOUND 0x4B2F
#endif
#ifndef KDMKTONE
#define KDMKTONE  0x4B30
#endif

struct speaker_note {
    uint16_t freq_hz;
    uint16_t duration_ms;
};

static void play_note(int spk_fd, uint32_t freq, uint32_t ms) {
    if (spk_fd >= 0) {
        struct speaker_note note;
        note.freq_hz = (uint16_t)freq;
        note.duration_ms = (uint16_t)ms;
        write(spk_fd, &note, sizeof(note));
    } else {
        /* Fallback to console tone or bell */
        if (freq > 0) {
            unsigned long arg = ((ms / 10) << 16) | (1193182 / freq);
            if (ioctl(STDOUT_FILENO, KDMKTONE, (void *)arg) < 0) {
                putchar('\a');
                fflush(stdout);
            }
        }
        usleep(ms * 1000);
    }
}

static void play_melody(int spk_fd, const char *name) {
    if (strcmp(name, "success") == 0 || strcmp(name, "s") == 0) {
        play_note(spk_fd, 523, 80);  /* C5 */
        play_note(spk_fd, 659, 80);  /* E5 */
        play_note(spk_fd, 784, 180); /* G5 */
    } else if (strcmp(name, "alert") == 0 || strcmp(name, "a") == 0) {
        play_note(spk_fd, 880, 120);
        usleep(40000);
        play_note(spk_fd, 880, 120);
    } else if (strcmp(name, "mario") == 0) {
        play_note(spk_fd, 659, 90);
        usleep(30000);
        play_note(spk_fd, 659, 90);
        usleep(90000);
        play_note(spk_fd, 659, 90);
        usleep(90000);
        play_note(spk_fd, 523, 90);
        play_note(spk_fd, 659, 90);
        play_note(spk_fd, 784, 180);
    } else if (strcmp(name, "fanfare") == 0) {
        play_note(spk_fd, 440, 120); /* A4 */
        play_note(spk_fd, 440, 120);
        play_note(spk_fd, 440, 120);
        play_note(spk_fd, 587, 300); /* D5 */
    } else if (strcmp(name, "scale") == 0) {
        uint32_t notes[] = { 262, 294, 330, 349, 392, 440, 494, 523 };
        for (int i = 0; i < 8; i++) {
            play_note(spk_fd, notes[i], 100);
        }
    } else {
        fprintf(stderr, "beep: unknown melody '%s'\n", name);
    }
}

static void print_usage(const char *prog) {
    fprintf(stderr, "Usage: %s [options]\n", prog);
    fprintf(stderr, "Options:\n");
    fprintf(stderr, "  -f FREQ    Frequency in Hz (default: 750)\n");
    fprintf(stderr, "  -l LEN     Duration in milliseconds (default: 200)\n");
    fprintf(stderr, "  -r REPS    Number of repetitions (default: 1)\n");
    fprintf(stderr, "  -d DELAY   Delay between repetitions in ms (default: 100)\n");
    fprintf(stderr, "  -s         Play success chime\n");
    fprintf(stderr, "  -a         Play alert tone\n");
    fprintf(stderr, "  -m NAME    Play built-in melody (mario, fanfare, scale, success, alert)\n");
    fprintf(stderr, "  -h         Display this help message\n");
}

int main(int argc, char **argv) {
    uint32_t freq = 750;
    uint32_t len = 200;
    int reps = 1;
    uint32_t delay = 100;
    const char *melody = NULL;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-f") == 0 && i + 1 < argc) {
            freq = (uint32_t)atoi(argv[++i]);
        } else if (strcmp(argv[i], "-l") == 0 && i + 1 < argc) {
            len = (uint32_t)atoi(argv[++i]);
        } else if (strcmp(argv[i], "-r") == 0 && i + 1 < argc) {
            reps = atoi(argv[++i]);
        } else if (strcmp(argv[i], "-d") == 0 && i + 1 < argc) {
            delay = (uint32_t)atoi(argv[++i]);
        } else if (strcmp(argv[i], "-s") == 0 || strcmp(argv[i], "--success") == 0) {
            melody = "success";
        } else if (strcmp(argv[i], "-a") == 0 || strcmp(argv[i], "--alert") == 0) {
            melody = "alert";
        } else if (strcmp(argv[i], "-m") == 0 && i + 1 < argc) {
            melody = argv[++i];
        } else if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
            print_usage(argv[0]);
            return 0;
        } else {
            fprintf(stderr, "%s: unrecognized option '%s'\n", argv[0], argv[i]);
            print_usage(argv[0]);
            return 1;
        }
    }

    int spk_fd = open("/dev/speaker", O_WRONLY);

    if (melody) {
        for (int r = 0; r < reps; r++) {
            play_melody(spk_fd, melody);
            if (r + 1 < reps && delay > 0) {
                usleep(delay * 1000);
            }
        }
    } else {
        for (int r = 0; r < reps; r++) {
            play_note(spk_fd, freq, len);
            if (r + 1 < reps && delay > 0) {
                usleep(delay * 1000);
            }
        }
    }

    if (spk_fd >= 0) {
        close(spk_fd);
    }

    return 0;
}
