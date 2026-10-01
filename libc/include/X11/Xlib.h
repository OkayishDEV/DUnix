#ifndef _X11_XLIB_H
#define _X11_XLIB_H

#include <X11/X.h>
#include <X11/Xutil.h>
#include <X11/Xatom.h>
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#define Bool int
#define Status int
#define True 1
#define False 0

typedef struct _XDisplay Display;
typedef struct _XScreen Screen;
typedef struct _XVisual Visual;
typedef struct _XGC *GC;

struct _XScreen {
    int width;
    int height;
    Window root;
    uint32_t white_pixel;
    uint32_t black_pixel;
    int root_depth;
};

struct _XDisplay {
    int fd;
    int default_screen;
    struct _XScreen screens[1];
    uint32_t resource_id_base;
    uint32_t next_resource_id;
    char display_name[64];
    Window root_window;
};

typedef struct {
    int function;
    unsigned long plane_mask;
    unsigned long foreground;
    unsigned long background;
    int line_width;
    int line_style;
    int cap_style;
    int join_style;
    int fill_style;
    int fill_rule;
    int arc_mode;
    Pixmap tile;
    Pixmap stipple;
    int ts_x_origin;
    int ts_y_origin;
    Font font;
    int subwindow_mode;
    Bool graphics_exposures;
    int clip_x_origin;
    int clip_y_origin;
    Pixmap clip_mask;
    int dash_offset;
    char dashes;
} XGCValues;

struct _XGC {
    Display *display;
    XID gid;
    XGCValues values;
};

typedef struct {
    int type;
    unsigned long serial;
    Bool send_event;
    Display *display;
    Window window;
} XAnyEvent;

typedef struct {
    int type;
    unsigned long serial;
    Bool send_event;
    Display *display;
    Window window;
    Window root;
    Window subwindow;
    Time time;
    int x, y;
    int x_root, y_root;
    unsigned int state;
    unsigned int keycode;
    Bool same_screen;
} XKeyEvent;

typedef XKeyEvent XKeyPressedEvent;
typedef XKeyEvent XKeyReleasedEvent;

typedef struct {
    int type;
    unsigned long serial;
    Bool send_event;
    Display *display;
    Window window;
    Window root;
    Window subwindow;
    Time time;
    int x, y;
    int x_root, y_root;
    unsigned int state;
    unsigned int button;
    Bool same_screen;
} XButtonEvent;

typedef XButtonEvent XButtonPressedEvent;
typedef XButtonEvent XButtonReleasedEvent;

typedef struct {
    int type;
    unsigned long serial;
    Bool send_event;
    Display *display;
    Window window;
    Window root;
    Window subwindow;
    Time time;
    int x, y;
    int x_root, y_root;
    unsigned int state;
    char is_hint;
    Bool same_screen;
} XMotionEvent;

typedef XMotionEvent XPointerMovedEvent;

typedef struct {
    int type;
    unsigned long serial;
    Bool send_event;
    Display *display;
    Window window;
    int x, y;
    int width, height;
    int count;
} XExposeEvent;

typedef struct {
    int type;
    unsigned long serial;
    Bool send_event;
    Display *display;
    Window event;
    Window window;
    int x, y;
    int width, height;
    int border_width;
    Window above;
    Bool override_redirect;
} XConfigureEvent;

typedef struct {
    int type;
    unsigned long serial;
    Bool send_event;
    Display *display;
    Window window;
    Atom message_type;
    int format;
    union {
        char b[20];
        short s[10];
        long l[5];
    } data;
} XClientMessageEvent;

typedef struct {
    int type;
    unsigned long serial;
    Bool send_event;
    Display *display;
    Window event;
    Window window;
} XDestroyWindowEvent;

typedef union _XEvent {
    int type;
    XAnyEvent xany;
    XKeyEvent xkey;
    XButtonEvent xbutton;
    XMotionEvent xmotion;
    XExposeEvent xexpose;
    XConfigureEvent xconfigure;
    XClientMessageEvent xclient;
    XDestroyWindowEvent xdestroywindow;
    long pad[24];
} XEvent;

typedef struct {
    int x, y;
    int width, height;
    int border_width;
    int depth;
    Visual *visual;
    Window root;
    int class;
    int bit_gravity;
    int win_gravity;
    int backing_store;
    unsigned long backing_planes;
    unsigned long backing_pixel;
    Bool save_under;
    Colormap colormap;
    Bool map_installed;
    int map_state;
    long all_event_masks;
    long your_event_mask;
    long do_not_propagate_mask;
    Bool override_redirect;
    Screen *screen;
} XWindowAttributes;

typedef struct {
    Pixmap background_pixmap;
    unsigned long background_pixel;
    Pixmap border_pixmap;
    unsigned long border_pixel;
    int bit_gravity;
    int win_gravity;
    int backing_store;
    unsigned long backing_planes;
    unsigned long backing_pixel;
    Bool save_under;
    long event_mask;
    long do_not_propagate_mask;
    Bool override_redirect;
    Colormap colormap;
    Cursor cursor;
} XSetWindowAttributes;

typedef struct {
    short x, y;
} XPoint;

typedef struct {
    short x, y;
    unsigned short width, height;
} XRectangle;

typedef struct {
    short x1, y1, x2, y2;
} XSegment;

typedef struct {
    short x, y;
    unsigned short width, height;
    short angle1, angle2;
} XArc;

/* Core Xlib API Functions */
Display *XOpenDisplay(const char *display_name);
int XCloseDisplay(Display *display);
int XFlush(Display *display);
int XSync(Display *display, Bool discard);

Window XDefaultRootWindow(Display *display);
int XDefaultScreen(Display *display);
int XDisplayWidth(Display *display, int screen_number);
int XDisplayHeight(Display *display, int screen_number);
unsigned long XBlackPixel(Display *display, int screen_number);
unsigned long XWhitePixel(Display *display, int screen_number);

Window XCreateSimpleWindow(Display *display, Window parent, int x, int y,
                           unsigned int width, unsigned int height,
                           unsigned int border_width, unsigned long border,
                           unsigned long background);

Window XCreateWindow(Display *display, Window parent, int x, int y,
                     unsigned int width, unsigned int height,
                     unsigned int border_width, int depth, unsigned int class,
                     Visual *visual, unsigned long valuemask,
                     XSetWindowAttributes *attributes);

int XDestroyWindow(Display *display, Window w);
int XMapWindow(Display *display, Window w);
int XMapRaised(Display *display, Window w);
int XUnmapWindow(Display *display, Window w);

int XMoveWindow(Display *display, Window w, int x, int y);
int XResizeWindow(Display *display, Window w, unsigned int width, unsigned int height);
int XMoveResizeWindow(Display *display, Window w, int x, int y, unsigned int width, unsigned int height);
int XSetWindowBorderWidth(Display *display, Window w, unsigned int width);
int XSetWindowBackground(Display *display, Window w, unsigned long background_pixel);

GC XCreateGC(Display *display, Drawable d, unsigned long valuemask, XGCValues *values);
int XFreeGC(Display *display, GC gc);
int XSetForeground(Display *display, GC gc, unsigned long foreground);
int XSetBackground(Display *display, GC gc, unsigned long background);
int XSetLineAttributes(Display *display, GC gc, unsigned int line_width,
                       int line_style, int cap_style, int join_style);

int XDrawPoint(Display *display, Drawable d, GC gc, int x, int y);
int XDrawLine(Display *display, Drawable d, GC gc, int x1, int y1, int x2, int y2);
int XDrawRectangle(Display *display, Drawable d, GC gc, int x, int y, unsigned int width, unsigned int height);
int XFillRectangle(Display *display, Drawable d, GC gc, int x, int y, unsigned int width, unsigned int height);
int XDrawArc(Display *display, Drawable d, GC gc, int x, int y, unsigned int width, unsigned int height, int angle1, int angle2);
int XFillArc(Display *display, Drawable d, GC gc, int x, int y, unsigned int width, unsigned int height, int angle1, int angle2);
int XDrawString(Display *display, Drawable d, GC gc, int x, int y, const char *string, int length);
int XDrawImageString(Display *display, Drawable d, GC gc, int x, int y, const char *string, int length);
int XClearWindow(Display *display, Window w);
int XClearArea(Display *display, Window w, int x, int y, unsigned int width, unsigned int height, Bool exposures);

int XSelectInput(Display *display, Window w, long event_mask);
int XNextEvent(Display *display, XEvent *event_return);
int XPending(Display *display);
Bool XCheckWindowEvent(Display *display, Window w, long event_mask, XEvent *event_return);
Bool XCheckTypedEvent(Display *display, int event_type, XEvent *event_return);
Status XSendEvent(Display *display, Window w, Bool propagate, long event_mask, XEvent *event_send);

int XStoreName(Display *display, Window w, const char *window_name);
int XFetchName(Display *display, Window w, char **window_name_return);
Atom XInternAtom(Display *display, const char *atom_name, Bool only_if_exists);
Status XSetWMProtocols(Display *display, Window w, Atom *protocols, int count);
void XSetWMNormalHints(Display *display, Window w, XSizeHints *hints);

int XGrabButton(Display *display, unsigned int button, unsigned int modifiers,
                Window grab_window, Bool owner_events, unsigned int event_mask,
                int pointer_mode, int keyboard_mode, Window confine_to, Cursor cursor);
int XUngrabButton(Display *display, unsigned int button, unsigned int modifiers, Window grab_window);
int XGrabPointer(Display *display, Window grab_window, Bool owner_events,
                 unsigned int event_mask, int pointer_mode, int keyboard_mode,
                 Window confine_to, Cursor cursor, Time time);
int XUngrabPointer(Display *display, Time time);

Bool XQueryPointer(Display *display, Window w, Window *root_return, Window *child_return,
                   int *root_x_return, int *root_y_return, int *win_x_return, int *win_y_return,
                   unsigned int *mask_return);
Status XGetWindowAttributes(Display *display, Window w, XWindowAttributes *window_attributes_return);
Status XGetGeometry(Display *display, Drawable d, Window *root_return,
                    int *x_return, int *y_return, unsigned int *width_return,
                    unsigned int *height_return, unsigned int *border_width_return,
                    unsigned int *depth_return);
Status XQueryTree(Display *display, Window w, Window *root_return, Window *parent_return,
                  Window **children_return, unsigned int *nchildren_return);

int XLookupString(XKeyEvent *event_struct, char *buffer_return, int bytes_buffer,
                  KeySym *keysym_return, void *status_in_out);

#endif /* _X11_XLIB_H */
