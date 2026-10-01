#ifndef _X11_XUTIL_H
#define _X11_XUTIL_H

#include <X11/X.h>

#define USPosition  (1L << 0)
#define USSize      (1L << 1)
#define PPosition   (1L << 2)
#define PSize       (1L << 3)
#define PMINSize    (1L << 4)
#define PMAXSize    (1L << 5)
#define PResizeInc  (1L << 6)
#define PAspect     (1L << 7)
#define PBaseSize   (1L << 8)
#define PWinGravity (1L << 9)

typedef struct {
    long flags;
    int x, y;
    int width, height;
    int min_width, min_height;
    int max_width, max_height;
    int width_inc, height_inc;
    struct {
        int x;
        int y;
    } min_aspect, max_aspect;
    int base_width, base_height;
    int win_gravity;
} XSizeHints;

typedef struct {
    long flags;
    int input;
    int initial_state;
    Pixmap icon_pixmap;
    Window icon_window;
    int icon_x, icon_y;
    Pixmap icon_mask;
    XID window_group;
} XWMHints;

#define WithdrawnState 0
#define NormalState    1
#define IconicState    3

#endif /* _X11_XUTIL_H */
