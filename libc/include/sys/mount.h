#ifndef _LIBC_SYS_MOUNT_H
#define _LIBC_SYS_MOUNT_H

int mount(const char *source, const char *target, const char *filesystemtype, unsigned long mountflags, const void *data);
int umount(const char *target);

#endif /* _LIBC_SYS_MOUNT_H */
