#ifndef _X11_X_H
#define _X11_X_H

#include <stdint.h>

typedef uint32_t XID;
typedef XID Window;
typedef XID Drawable;
typedef XID Font;
typedef XID Pixmap;
typedef XID Cursor;
typedef XID Colormap;
typedef XID GContext;
typedef XID Atom;
typedef uint32_t Time;
typedef uint32_t KeySym;
typedef uint32_t KeyCode;
typedef uint32_t VisualID;

#define None 0L

/* Event definitions */
#define KeyPress         2
#define KeyRelease       3
#define ButtonPress      4
#define ButtonRelease    5
#define MotionNotify     6
#define EnterNotify      7
#define LeaveNotify      8
#define FocusIn          9
#define FocusOut         10
#define KeymapNotify     11
#define Expose           12
#define GraphicsExpose   13
#define NoExpose         14
#define VisibilityNotify 15
#define CreateNotify     16
#define DestroyNotify    17
#define UnmapNotify      18
#define MapNotify        19
#define MapRequest       20
#define ReparentNotify   21
#define ConfigureNotify  22
#define ConfigureRequest 23
#define GravityNotify    24
#define ResizeRequest    25
#define CirculateNotify  26
#define CirculateRequest 27
#define PropertyNotify   28
#define SelectionClear   29
#define SelectionRequest 30
#define SelectionNotify  31
#define ColormapNotify   32
#define ClientMessage    33
#define MappingNotify    34

/* Event masks */
#define NoEventMask              0L
#define KeyPressMask             (1L<<0)
#define KeyReleaseMask           (1L<<1)
#define ButtonPressMask          (1L<<2)
#define ButtonReleaseMask        (1L<<3)
#define EnterWindowMask          (1L<<4)
#define LeaveWindowMask          (1L<<5)
#define PointerMotionMask        (1L<<6)
#define PointerMotionHintMask    (1L<<7)
#define Button1MotionMask        (1L<<8)
#define Button2MotionMask        (1L<<9)
#define Button3MotionMask        (1L<<10)
#define Button4MotionMask        (1L<<11)
#define Button5MotionMask        (1L<<12)
#define ButtonMotionMask         (1L<<13)
#define KeymapStateMask          (1L<<14)
#define ExposureMask             (1L<<15)
#define VisibilityChangeMask     (1L<<16)
#define StructureNotifyMask      (1L<<17)
#define ResizeRedirectMask       (1L<<18)
#define SubstructureNotifyMask   (1L<<19)
#define SubstructureRedirectMask (1L<<20)
#define FocusChangeMask          (1L<<21)
#define PropertyChangeMask       (1L<<22)
#define ColormapChangeMask       (1L<<23)
#define OwnerGrabButtonMask      (1L<<24)

/* Window classes */
#define InputOutput 1
#define InputOnly   2

/* Window attributes masks */
#define CWBackPixmap       (1L<<0)
#define CWBackPixel        (1L<<1)
#define CWBorderPixmap     (1L<<2)
#define CWBorderPixel      (1L<<3)
#define CWBitGravity       (1L<<4)
#define CWWinGravity       (1L<<5)
#define CWBackingStore     (1L<<6)
#define CWBackingPlanes    (1L<<7)
#define CWBackingPixel     (1L<<8)
#define CWOverrideRedirect (1L<<9)
#define CWSaveUnder        (1L<<10)
#define CWEventMask        (1L<<11)
#define CWDontPropagate    (1L<<12)
#define CWColormap         (1L<<13)
#define CWCursor           (1L<<14)

/* Graphics Context masks */
#define GCFunction          (1L<<0)
#define GCPlaneMask         (1L<<1)
#define GCForeground        (1L<<2)
#define GCBackground        (1L<<3)
#define GCLineWidth         (1L<<4)
#define GCLineStyle         (1L<<5)
#define GCCapStyle          (1L<<6)
#define GCJoinStyle         (1L<<7)
#define GCFillStyle         (1L<<8)
#define GCFillRule          (1L<<9)
#define GCTile              (1L<<10)
#define GCStipple           (1L<<11)
#define GCTileStipXOrigin   (1L<<12)
#define GCTileStipYOrigin   (1L<<13)
#define GCFont              (1L<<14)
#define GCSubwindowMode     (1L<<15)
#define GCGraphicsExposures (1L<<16)
#define GCClipXOrigin       (1L<<17)
#define GCClipYOrigin       (1L<<18)
#define GCClipMask          (1L<<19)
#define GCDashOffset        (1L<<20)
#define GCDashList          (1L<<21)
#define GCArcMode           (1L<<22)

/* GC functions */
#define GXclear        0x0
#define GXand          0x1
#define GXandReverse   0x2
#define GXcopy         0x3
#define GXandInverted  0x4
#define GXnoop         0x5
#define GXxor          0x6
#define GXor           0x7
#define GXnor          0x8
#define GXequiv        0x9
#define GXinvert       0xa
#define GXorReverse    0xb
#define GXcopyInverted 0xc
#define GXorInverted   0xd
#define GXnand         0xe
#define GXset          0xf

/* Mouse buttons */
#define Button1 1
#define Button2 2
#define Button3 3
#define Button4 4
#define Button5 5

/* Grab modes */
#define GrabModeSync  0
#define GrabModeAsync 1

#endif /* _X11_X_H */
