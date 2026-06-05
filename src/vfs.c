#include "vfs.h"
#include "string.h"

fs_node_t *fs_root = NULL;

uint32_t read_fs(fs_node_t *node, uint32_t offset, uint32_t size, uint8_t *buffer) {
    if (node && node->read) {
        return node->read(node, offset, size, buffer);
    }
    return 0;
}

uint32_t write_fs(fs_node_t *node, uint32_t offset, uint32_t size, uint8_t *buffer) {
    if (node && node->write) {
        return node->write(node, offset, size, buffer);
    }
    return 0;
}

void open_fs(fs_node_t *node) {
    if (node && node->open) {
        node->open(node);
    }
}

void close_fs(fs_node_t *node) {
    if (node && node->close) {
        node->close(node);
    }
}

struct dirent *readdir_fs(fs_node_t *node, uint32_t index) {
    if (node && (node->flags & 0x07) == FS_DIRECTORY && node->readdir) {
        return node->readdir(node, index);
    }
    return NULL;
}

fs_node_t *finddir_fs(fs_node_t *node, const char *name) {
    if (node && (node->flags & 0x07) == FS_DIRECTORY && node->finddir) {
        return node->finddir(node, name);
    }
    return NULL;
}

fs_node_t *find_node_by_path(const char *path) {
    if (!path || path[0] != '/') return NULL;
    if (path[1] == '\0') return fs_root;

    fs_node_t *curr = fs_root;
    char token[128];
    int i = 1;
    while (path[i] != '\0') {
        int j = 0;
        while (path[i] != '/' && path[i] != '\0') {
            if (j < 127) {
                token[j++] = path[i];
            }
            i++;
        }
        token[j] = '\0';
        if (path[i] == '/') i++;

        if (j > 0) {
            fs_node_t *next = finddir_fs(curr, token);
            if (!next) return NULL;
            curr = next;
        }
    }
    return curr;
}
