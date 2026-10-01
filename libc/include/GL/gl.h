#ifndef _GL_H
#define _GL_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef unsigned int   GLenum;
typedef unsigned char  GLboolean;
typedef unsigned int   GLbitfield;
typedef void           GLvoid;
typedef signed char    GLbyte;
typedef short          GLshort;
typedef int            GLint;
typedef unsigned char  GLubyte;
typedef unsigned short GLushort;
typedef unsigned int   GLuint;
typedef int            GLsizei;
typedef float          GLfloat;
typedef float          GLclampf;
typedef double         GLdouble;
typedef double         GLclampd;

#define GL_FALSE          0
#define GL_TRUE           1

/* Primitives */
#define GL_POINTS         0x0000
#define GL_LINES          0x0001
#define GL_LINE_LOOP      0x0002
#define GL_LINE_STRIP     0x0003
#define GL_TRIANGLES      0x0004
#define GL_TRIANGLE_STRIP 0x0005
#define GL_TRIANGLE_FAN   0x0006
#define GL_QUADS          0x0007
#define GL_QUAD_STRIP     0x0008
#define GL_POLYGON        0x0009

/* Matrix Modes */
#define GL_MODELVIEW      0x1700
#define GL_PROJECTION     0x1701
#define GL_TEXTURE        0x1702

/* Buffer Bits */
#define GL_COLOR_BUFFER_BIT 0x00004000
#define GL_DEPTH_BUFFER_BIT 0x00000100

/* Capabilities */
#define GL_CULL_FACE      0x0B44
#define GL_LIGHTING       0x0B50
#define GL_LIGHT0         0x4000
#define GL_LIGHT1         0x4001
#define GL_DEPTH_TEST     0x0B71
#define GL_BLEND          0x0BE2
#define GL_TEXTURE_2D     0x0DE1
#define GL_NORMALIZE      0x0BA1

/* Depth Functions */
#define GL_NEVER          0x0200
#define GL_LESS           0x0201
#define GL_EQUAL          0x0202
#define GL_LEQUAL         0x0203
#define GL_GREATER        0x0204
#define GL_NOTEQUAL       0x0205
#define GL_GEQUAL         0x0206
#define GL_ALWAYS         0x0207

/* Lighting & Materials */
#define GL_AMBIENT        0x1200
#define GL_DIFFUSE        0x1201
#define GL_SPECULAR       0x1202
#define GL_POSITION       0x1203
#define GL_SHININESS      0x1601
#define GL_FRONT          0x0404
#define GL_BACK           0x0405
#define GL_FRONT_AND_BACK 0x0408
#define GL_FLAT           0x1D00
#define GL_SMOOTH         0x1D01

/* Polygon Winding */
#define GL_CW             0x0900
#define GL_CCW            0x0901

/* Function Prototypes */
void glClear(GLbitfield mask);
void glClearColor(GLclampf red, GLclampf green, GLclampf blue, GLclampf alpha);
void glClearDepth(GLclampd depth);
void glViewport(GLint x, GLint y, GLsizei width, GLsizei height);

/* Matrix Stack */
void glMatrixMode(GLenum mode);
void glPushMatrix(void);
void glPopMatrix(void);
void glLoadIdentity(void);
void glLoadMatrixf(const GLfloat *m);
void glMultMatrixf(const GLfloat *m);
void glTranslatef(GLfloat x, GLfloat y, GLfloat z);
void glRotatef(GLfloat angle, GLfloat x, GLfloat y, GLfloat z);
void glScalef(GLfloat x, GLfloat y, GLfloat z);
void glOrtho(GLdouble left, GLdouble right, GLdouble bottom, GLdouble top, GLdouble near_val, GLdouble far_val);
void glFrustum(GLdouble left, GLdouble right, GLdouble bottom, GLdouble top, GLdouble near_val, GLdouble far_val);

/* State & Enables */
void glEnable(GLenum cap);
void glDisable(GLenum cap);
GLboolean glIsEnabled(GLenum cap);
void glDepthFunc(GLenum func);
void glDepthMask(GLboolean flag);
void glShadeModel(GLenum mode);
void glCullFace(GLenum mode);
void glFrontFace(GLenum mode);

/* Lighting */
void glLightfv(GLenum light, GLenum pname, const GLfloat *params);
void glLightf(GLenum light, GLenum pname, GLfloat param);
void glMaterialfv(GLenum face, GLenum pname, const GLfloat *params);
void glMaterialf(GLenum face, GLenum pname, GLfloat param);

/* Primitives */
void glBegin(GLenum mode);
void glEnd(void);
void glVertex2f(GLfloat x, GLfloat y);
void glVertex3f(GLfloat x, GLfloat y, GLfloat z);
void glVertex3fv(const GLfloat *v);
void glColor3f(GLfloat r, GLfloat g, GLfloat b);
void glColor3ub(GLubyte r, GLubyte g, GLubyte b);
void glColor4f(GLfloat r, GLfloat g, GLfloat b, GLfloat a);
void glColor4ub(GLubyte r, GLubyte g, GLubyte b, GLubyte a);
void glNormal3f(GLfloat nx, GLfloat ny, GLfloat nz);
void glNormal3fv(const GLfloat *v);
void glTexCoord2f(GLfloat s, GLfloat t);

void glFlush(void);
void glFinish(void);

#ifdef __cplusplus
}
#endif

#endif /* _GL_H */
