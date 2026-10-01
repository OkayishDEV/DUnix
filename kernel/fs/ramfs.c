#include <fs/ramfs.h>
#include <mm/heap.h>
#include <dunix/string.h>
#include <dunix/kprintf.h>

struct ramfs_entry {
    char                name[128];
    struct vfs_node    *node;
    struct ramfs_entry *next;
};

struct ramfs_node_data {
    uint8_t            *buffer;
    size_t              capacity;
    struct ramfs_entry *children;
    uint32_t            child_count;
};

static struct vfs_ops ramfs_file_ops;
static struct vfs_ops ramfs_dir_ops;
static uint32_t next_inode = 1;

static ssize_t ramfs_read_func(struct vfs_node *node, uint64_t offset, size_t size, void *buffer) {
    struct ramfs_node_data *data = (struct ramfs_node_data *)node->device;
    if (!data || !data->buffer || offset >= node->length) {
        return 0;
    }

    if (offset + size > node->length) {
        size = (size_t)(node->length - offset);
    }

    memcpy(buffer, data->buffer + offset, size);
    return (ssize_t)size;
}

static ssize_t ramfs_write_func(struct vfs_node *node, uint64_t offset, size_t size, const void *buffer) {
    struct ramfs_node_data *data = (struct ramfs_node_data *)node->device;
    if (!data) return -1;

    size_t required = (size_t)(offset + size);
    if (required > data->capacity) {
        size_t new_cap = ALIGN_UP(required, 1024);
        data->buffer = (uint8_t *)krealloc(data->buffer, new_cap);
        data->capacity = new_cap;
    }

    memcpy(data->buffer + offset, buffer, size);
    if (offset + size > node->length) {
        node->length = (uint32_t)(offset + size);
    }

    return (ssize_t)size;
}

static struct dirent *ramfs_readdir_func(struct vfs_node *node, uint32_t index) {
    static struct dirent dir_entry;
    struct ramfs_node_data *data = (struct ramfs_node_data *)node->device;
    if (!data) return NULL;

    struct ramfs_entry *curr = data->children;
    uint32_t i = 0;
    while (curr && i < index) {
        curr = curr->next;
        i++;
    }

    if (!curr || !curr->node) return NULL;

    strncpy(dir_entry.d_name, curr->name, sizeof(dir_entry.d_name) - 1);
    dir_entry.d_ino = curr->node->inode;
    dir_entry.d_type = curr->node->flags;

    return &dir_entry;
}

static struct vfs_node *ramfs_finddir_func(struct vfs_node *node, const char *name) {
    struct ramfs_node_data *data = (struct ramfs_node_data *)node->device;
    if (!data) return NULL;

    struct ramfs_entry *curr = data->children;
    while (curr) {
        if (strcmp(curr->name, name) == 0) {
            return curr->node;
        }
        curr = curr->next;
    }

    return NULL;
}

static int ramfs_mkdir_func(struct vfs_node *node, const char *name, mode_t mode) {
    if (ramfs_finddir_func(node, name) != NULL) {
        return -17; /* -EEXIST */
    }

    struct vfs_node *dir = ramfs_create_dir(node, name, mode);
    return dir ? 0 : -12;
}

static int ramfs_create_func(struct vfs_node *node, const char *name, mode_t mode) {
    if (ramfs_finddir_func(node, name) != NULL) {
        return -17; /* -EEXIST */
    }

    struct vfs_node *file = ramfs_create_file(node, name, NULL, 0, mode);
    return file ? 0 : -12;
}

static int ramfs_unlink_func(struct vfs_node *node, const char *name) {
    struct ramfs_node_data *data = (struct ramfs_node_data *)node->device;
    if (!data) return -1;

    struct ramfs_entry *prev = NULL;
    struct ramfs_entry *curr = data->children;

    while (curr) {
        if (strcmp(curr->name, name) == 0) {
            if (prev) {
                prev->next = curr->next;
            } else {
                data->children = curr->next;
            }
            data->child_count--;

            /* Free child node */
            if (curr->node) {
                struct ramfs_node_data *cdata = (struct ramfs_node_data *)curr->node->device;
                if (cdata) {
                    if (cdata->buffer) kfree(cdata->buffer);
                    kfree(cdata);
                }
                kfree(curr->node);
            }
            kfree(curr);
            return 0;
        }
        prev = curr;
        curr = curr->next;
    }

    return -2; /* -ENOENT */
}

int ramfs_rename(struct vfs_node *old_parent, const char *old_name, struct vfs_node *new_parent, const char *new_name) {
    if (!old_parent || !old_name || !new_parent || !new_name) return -14;

    struct ramfs_node_data *old_data = (struct ramfs_node_data *)old_parent->device;
    struct ramfs_node_data *new_data = (struct ramfs_node_data *)new_parent->device;
    if (!old_data || !new_data) return -1;

    /* If target already exists in new_parent, unlink it first */
    ramfs_unlink_func(new_parent, new_name);

    /* Find entry in old_parent */
    struct ramfs_entry *prev = NULL;
    struct ramfs_entry *curr = old_data->children;
    while (curr) {
        if (strcmp(curr->name, old_name) == 0) {
            break;
        }
        prev = curr;
        curr = curr->next;
    }
    if (!curr) return -2; /* -ENOENT */

    /* Detach from old_parent */
    if (prev) {
        prev->next = curr->next;
    } else {
        old_data->children = curr->next;
    }
    old_data->child_count--;

    /* Update entry name */
    strncpy(curr->name, new_name, sizeof(curr->name) - 1);
    curr->name[sizeof(curr->name) - 1] = '\0';
    if (curr->node) {
        strncpy(curr->node->name, new_name, sizeof(curr->node->name) - 1);
        curr->node->name[sizeof(curr->node->name) - 1] = '\0';
    }

    /* Attach to new_parent */
    curr->next = new_data->children;
    new_data->children = curr;
    new_data->child_count++;

    return 0;
}

struct vfs_node *ramfs_create_root(void) {
    /* Setup ops */
    ramfs_file_ops.read = ramfs_read_func;
    ramfs_file_ops.write = ramfs_write_func;

    ramfs_dir_ops.readdir = ramfs_readdir_func;
    ramfs_dir_ops.finddir = ramfs_finddir_func;
    ramfs_dir_ops.mkdir   = ramfs_mkdir_func;
    ramfs_dir_ops.create  = ramfs_create_func;
    ramfs_dir_ops.unlink  = ramfs_unlink_func;

    struct vfs_node *root = (struct vfs_node *)kzalloc(sizeof(struct vfs_node));
    struct ramfs_node_data *data = (struct ramfs_node_data *)kzalloc(sizeof(struct ramfs_node_data));

    strcpy(root->name, "/");
    root->flags = VFS_DIRECTORY;
    root->inode = next_inode++;
    root->mask = 0755;
    root->ops = &ramfs_dir_ops;
    root->device = data;

    return root;
}

struct vfs_node *ramfs_create_dir(struct vfs_node *parent, const char *name, mode_t mode) {
    struct vfs_node *dir = (struct vfs_node *)kzalloc(sizeof(struct vfs_node));
    struct ramfs_node_data *data = (struct ramfs_node_data *)kzalloc(sizeof(struct ramfs_node_data));

    strncpy(dir->name, name, sizeof(dir->name) - 1);
    dir->flags = VFS_DIRECTORY;
    dir->inode = next_inode++;
    dir->mask = mode ? mode : 0755;
    dir->ops = &ramfs_dir_ops;
    dir->device = data;

    if (parent) {
        struct ramfs_node_data *pdata = (struct ramfs_node_data *)parent->device;
        struct ramfs_entry *entry = (struct ramfs_entry *)kzalloc(sizeof(struct ramfs_entry));
        strncpy(entry->name, name, sizeof(entry->name) - 1);
        entry->node = dir;
        entry->next = pdata->children;
        pdata->children = entry;
        pdata->child_count++;
    }

    return dir;
}

struct vfs_node *ramfs_create_file(struct vfs_node *parent, const char *name, const void *data, size_t size, mode_t mode) {
    struct vfs_node *file = (struct vfs_node *)kzalloc(sizeof(struct vfs_node));
    struct ramfs_node_data *fdata = (struct ramfs_node_data *)kzalloc(sizeof(struct ramfs_node_data));

    strncpy(file->name, name, sizeof(file->name) - 1);
    file->flags = VFS_FILE;
    file->inode = next_inode++;
    file->mask = mode ? mode : 0644;
    file->length = (uint32_t)size;
    file->ops = &ramfs_file_ops;

    if (size > 0) {
        fdata->buffer = (uint8_t *)kmalloc(size);
        if (data) {
            memcpy(fdata->buffer, data, size);
        }
        fdata->capacity = size;
    }
    file->device = fdata;

    if (parent) {
        struct ramfs_node_data *pdata = (struct ramfs_node_data *)parent->device;
        struct ramfs_entry *entry = (struct ramfs_entry *)kzalloc(sizeof(struct ramfs_entry));
        strncpy(entry->name, name, sizeof(entry->name) - 1);
        entry->node = file;
        entry->next = pdata->children;
        pdata->children = entry;
        pdata->child_count++;
    }

    return file;
}
