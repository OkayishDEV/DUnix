#include <GL/glu.h>
#include <math.h>

void gluPerspective(GLdouble fovy, GLdouble aspect, GLdouble zNear, GLdouble zFar) {
    GLdouble ymax = zNear * tan(fovy * 3.14159265358979323846 / 360.0);
    GLdouble ymin = -ymax;
    GLdouble xmin = ymin * aspect;
    GLdouble xmax = ymax * aspect;
    glFrustum(xmin, xmax, ymin, ymax, zNear, zFar);
}

void gluLookAt(GLdouble eyex, GLdouble eyey, GLdouble eyez,
               GLdouble centerx, GLdouble centery, GLdouble centerz,
               GLdouble upx, GLdouble upy, GLdouble upz) {
    float f[3], up[3], s[3], u[3];

    f[0] = (float)(centerx - eyex);
    f[1] = (float)(centery - eyey);
    f[2] = (float)(centerz - eyez);
    float flen = sqrtf(f[0]*f[0] + f[1]*f[1] + f[2]*f[2]);
    if (flen > 1e-6f) { f[0] /= flen; f[1] /= flen; f[2] /= flen; }

    up[0] = (float)upx; up[1] = (float)upy; up[2] = (float)upz;
    float uplen = sqrtf(up[0]*up[0] + up[1]*up[1] + up[2]*up[2]);
    if (uplen > 1e-6f) { up[0] /= uplen; up[1] /= uplen; up[2] /= uplen; }

    /* s = f x up */
    s[0] = f[1]*up[2] - f[2]*up[1];
    s[1] = f[2]*up[0] - f[0]*up[2];
    s[2] = f[0]*up[1] - f[1]*up[0];
    float slen = sqrtf(s[0]*s[0] + s[1]*s[1] + s[2]*s[2]);
    if (slen > 1e-6f) { s[0] /= slen; s[1] /= slen; s[2] /= slen; }

    /* u = s x f */
    u[0] = s[1]*f[2] - s[2]*f[1];
    u[1] = s[2]*f[0] - s[0]*f[2];
    u[2] = s[0]*f[1] - s[1]*f[0];

    float m[16];
    m[0] = s[0];  m[1] = s[1];  m[2] = s[2];  m[3] = 0.0f;
    m[4] = u[0];  m[5] = u[1];  m[6] = u[2];  m[7] = 0.0f;
    m[8] = -f[0]; m[9] = -f[1]; m[10] = -f[2]; m[11] = 0.0f;
    m[12] = 0.0f; m[13] = 0.0f; m[14] = 0.0f;  m[15] = 1.0f;

    glMultMatrixf(m);
    glTranslatef((GLfloat)-eyex, (GLfloat)-eyey, (GLfloat)-eyez);
}

void gluOrtho2D(GLdouble left, GLdouble right, GLdouble bottom, GLdouble top) {
    glOrtho(left, right, bottom, top, -1.0, 1.0);
}
