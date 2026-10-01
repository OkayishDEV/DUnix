#include <fs/devfs.h>
#include <fs/ramfs.h>
#include <mm/heap.h>
#include <arch/x86_64/drivers/vga.h>
#include <arch/x86_64/drivers/serial.h>
#include <arch/x86_64/drivers/console.h>
#include <arch/x86_64/drivers/bga.h>
#include <arch/x86_64/drivers/mouse.h>
#include <dunix/string.h>
#include <dunix/kprintf.h>

/* /dev/null ops */
static ssize_t dev_null_read(struct vfs_node *node, uint64_t offset, size_t size, void *buffer) {
    (void)node; (void)offset; (void)size; (void)buffer;
    return 0; /* EOF */
}

static ssize_t dev_null_write(struct vfs_node *node, uint64_t offset, size_t size, const void *buffer) {
    (void)node; (void)offset; (void)buffer;
    return (ssize_t)size; /* Discard */
}

/* /dev/zero ops */
static ssize_t dev_zero_read(struct vfs_node *node, uint64_t offset, size_t size, void *buffer) {
    (void)node; (void)offset;
    memset(buffer, 0, size);
    return (ssize_t)size;
}

static ssize_t dev_zero_write(struct vfs_node *node, uint64_t offset, size_t size, const void *buffer) {
    (void)node; (void)offset; (void)buffer;
    return (ssize_t)size;
}

/* /dev/console and /dev/tty ops */
static ssize_t dev_console_read(struct vfs_node *node, uint64_t offset, size_t size, void *buffer) {
    (void)node; (void)offset;
    return console_read(buffer, size);
}

static ssize_t dev_console_write(struct vfs_node *node, uint64_t offset, size_t size, const void *buffer) {
    (void)node; (void)offset;
    console_write((const char *)buffer, size);
    return (ssize_t)size;
}

static int dev_console_ioctl(struct vfs_node *node, unsigned long request, void *arg) {
    (void)node;
    return console_ioctl(request, arg);
}

/* /dev/urandom ops */
static uint32_t prng_state = 0x12345678;
static ssize_t dev_urandom_read(struct vfs_node *node, uint64_t offset, size_t size, void *buffer) {
    (void)node; (void)offset;
    uint8_t *buf = (uint8_t *)buffer;
    for (size_t i = 0; i < size; i++) {
        prng_state = prng_state * 1664525 + 1013904223;
        buf[i] = (uint8_t)(prng_state >> 24);
    }
    return (ssize_t)size;
}

#include <boot/boot_info.h>
#include <arch/x86_64/drivers/speaker.h>
#include <arch/x86_64/drivers/drm/drm.h>

/* /dev/kimg ops */
static ssize_t dev_kimg_read(struct vfs_node *node, uint64_t offset, size_t size, void *buffer) {
    (void)node;
    return kernel_read_clean_image(offset, size, buffer);
}

/* /dev/speaker ops */
static ssize_t dev_speaker_write(struct vfs_node *node, uint64_t offset, size_t size, const void *buffer) {
    (void)node; (void)offset;
    if (!buffer || size == 0) return 0;

    if (size == sizeof(struct speaker_note)) {
        const struct speaker_note *note = (const struct speaker_note *)buffer;
        speaker_beep(note->freq_hz, note->duration_ms);
        return (ssize_t)size;
    }

    /* Support text string input: "freq [duration_ms]\n" */
    const char *str = (const char *)buffer;
    uint32_t freq = 0;
    uint32_t ms = 200;
    size_t i = 0;
    while (i < size && (str[i] == ' ' || str[i] == '\t')) i++;
    while (i < size && str[i] >= '0' && str[i] <= '9') {
        freq = freq * 10 + (uint32_t)(str[i] - '0');
        i++;
    }
    while (i < size && (str[i] == ' ' || str[i] == '\t')) i++;
    if (i < size && str[i] >= '0' && str[i] <= '9') {
        ms = 0;
        while (i < size && str[i] >= '0' && str[i] <= '9') {
            ms = ms * 10 + (uint32_t)(str[i] - '0');
            i++;
        }
    }
    if (freq > 0) {
        speaker_beep(freq, ms);
    }
    return (ssize_t)size;
}

static struct vfs_ops null_ops;
static struct vfs_ops zero_ops;
static struct vfs_ops console_ops;
static struct vfs_ops urandom_ops;
static struct vfs_ops fb_ops;
static struct vfs_ops mouse_ops;
static struct vfs_ops kimg_ops;
static struct vfs_ops speaker_ops;

static struct vfs_node *register_device(struct vfs_node *dev_dir, const char *name, struct vfs_ops *ops) {
    struct vfs_node *dev = ramfs_create_file(dev_dir, name, NULL, 0, 0666);
    if (dev) {
        dev->flags = VFS_CHARDEVICE;
        dev->ops = ops;
    }
    return dev;
}

void devfs_init(struct vfs_node *dev_dir) {
    null_ops.read = dev_null_read;
    null_ops.write = dev_null_write;

    zero_ops.read = dev_zero_read;
    zero_ops.write = dev_zero_write;

    console_ops.read = dev_console_read;
    console_ops.write = dev_console_write;
    console_ops.ioctl = dev_console_ioctl;

    urandom_ops.read = dev_urandom_read;
    urandom_ops.write = dev_null_write;

    fb_ops.read = fb_device_read;
    fb_ops.write = fb_device_write;
    fb_ops.ioctl = fb_device_ioctl;

    mouse_ops.read = mouse_device_read;
    mouse_ops.write = dev_null_write;

    kimg_ops.read = dev_kimg_read;
    kimg_ops.write = dev_null_write;

    speaker_ops.read = dev_null_read;
    speaker_ops.write = dev_speaker_write;

    register_device(dev_dir, "null", &null_ops);
    register_device(dev_dir, "zero", &zero_ops);
    register_device(dev_dir, "console", &console_ops);
    register_device(dev_dir, "tty", &console_ops);
    register_device(dev_dir, "urandom", &urandom_ops);
    register_device(dev_dir, "fb0", &fb_ops);
    register_device(dev_dir, "mouse", &mouse_ops);
    register_device(dev_dir, "speaker", &speaker_ops);
    struct vfs_node *kimg_node = register_device(dev_dir, "kimg", &kimg_ops);
    if (kimg_node) {
        kimg_node->length = kernel_get_clean_image_size();
    }

    struct vfs_node *input_dir = ramfs_create_dir(dev_dir, "input", 0755);
    if (input_dir) {
        register_device(input_dir, "mice", &mouse_ops);
    }

    drm_init(dev_dir);

    klog(KLOG_INFO, "DevFS registered devices (/dev/null, /dev/zero, /dev/console, /dev/tty, /dev/urandom, /dev/fb0, /dev/mouse, /dev/kimg, /dev/speaker, /dev/dri/card0)\n");
}
