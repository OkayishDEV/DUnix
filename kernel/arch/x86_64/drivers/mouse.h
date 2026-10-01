#ifndef _DRIVERS_MOUSE_H
#define _DRIVERS_MOUSE_H

#include <dunix/types.h>
#include <fs/vfs.h>

#define MOUSE_BTN_LEFT   (1 << 0)
#define MOUSE_BTN_RIGHT  (1 << 1)
#define MOUSE_BTN_MIDDLE (1 << 2)

struct mouse_event {
    uint8_t flags;
    int32_t dx;
    int32_t dy;
    int32_t x;
    int32_t y;
    uint8_t buttons;
};

void mouse_init(void);
void mouse_set_bounds(int width, int height);
void mouse_get_state(int *x, int *y, int *buttons);
bool mouse_has_event(void);

ssize_t mouse_device_read(struct vfs_node *node, uint64_t offset, size_t size, void *buffer);

#endif /* _DRIVERS_MOUSE_H */
