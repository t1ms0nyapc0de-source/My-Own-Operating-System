#include "tar.h"
#include "string.h"

#define MAX_TAR_FILES 128

static fs_node_t tar_nodes[MAX_TAR_FILES];
static int tar_file_count = 0;
static fs_node_t tar_root;
static struct dirent dirent_buf;

extern void terminal_writestring(const char *);
extern void terminal_writedec(uint32_t);

static uint32_t get_octal(const char *in, int len) {
    uint32_t out = 0;
    int i = 0;
    while (i < len && in[i] != '\0') {
        if (in[i] >= '0' && in[i] <= '7') {
            out = out * 8 + (in[i] - '0');
        } else if (in[i] == ' ' || in[i] == '\0') {
            break;
        }
        i++;
    }
    return out;
}

static uint32_t tar_read(fs_node_t *node, uint32_t offset, uint32_t size, uint8_t *buffer) {
    if (offset >= node->length) return 0;
    if (offset + size > node->length) {
        size = node->length - offset;
    }
    memcpy(buffer, (const void *)(node->impl + offset), size);
    return size;
}

static int is_direct_child(const char *dir, const char *child) {
    int dir_len = strlen(dir);
    if (strcmp(dir, "/") == 0 || dir_len == 0) {
        int i = 0;
        while (child[i] != '\0') {
            if (child[i] == '/') {
                if (child[i + 1] == '\0') return 1; // Trailing slash is OK
                return 0; // Subdirectory
            }
            i++;
        }
        return 1;
    }

    if (strncmp(dir, child, dir_len) != 0) return 0;

    const char *rel = child + dir_len;
    int i = 0;
    while (rel[i] != '\0') {
        if (rel[i] == '/') {
            if (rel[i + 1] == '\0') return 1; // Trailing slash is OK
            return 0; // Subdirectory
        }
        i++;
    }
    return 1;
}

static int match_name(const char *rel_name, const char *search_name) {
    int len1 = strlen(rel_name);
    int len2 = strlen(search_name);
    if (len1 > 0 && rel_name[len1 - 1] == '/') {
        len1--;
    }
    if (len1 != len2) return 0;
    return strncmp(rel_name, search_name, len1) == 0;
}

static struct dirent *tar_readdir(fs_node_t *node, uint32_t index) {
    const char *dir_prefix = (node == &tar_root) ? "" : node->name;
    uint32_t count = 0;

    for (int i = 0; i < tar_file_count; i++) {
        if (strcmp(tar_nodes[i].name, dir_prefix) == 0) continue;

        if (is_direct_child(dir_prefix, tar_nodes[i].name)) {
            if (count == index) {
                const char *rel = tar_nodes[i].name + strlen(dir_prefix);
                int len = strlen(rel);
                if (len > 0 && rel[len - 1] == '/') {
                    len--;
                }
                if (len >= 128) len = 127;
                memcpy(dirent_buf.name, rel, len);
                dirent_buf.name[len] = '\0';
                dirent_buf.ino = i;
                return &dirent_buf;
            }
            count++;
        }
    }
    return NULL;
}

static fs_node_t *tar_finddir(fs_node_t *node, const char *name) {
    const char *dir_prefix = (node == &tar_root) ? "" : node->name;

    for (int i = 0; i < tar_file_count; i++) {
        if (strcmp(tar_nodes[i].name, dir_prefix) == 0) continue;

        if (is_direct_child(dir_prefix, tar_nodes[i].name)) {
            const char *rel = tar_nodes[i].name + strlen(dir_prefix);
            if (match_name(rel, name)) {
                return &tar_nodes[i];
            }
        }
    }
    return NULL;
}

fs_node_t *tar_init(uint32_t address, uint32_t size) {
    uint32_t ptr = address;
    tar_file_count = 0;

    terminal_writestring("[TAR] Initializing RAM disk filesystem at address ");
    terminal_writehex(address);
    terminal_writestring("...\n");

    while (ptr < address + size) {
        struct tar_header *header = (struct tar_header *)ptr;

        if (header->name[0] == '\0') {
            break; // EOF
        }

        // Parse magic to verify USTAR
        if (strncmp(header->magic, "ustar", 5) != 0) {
            // Some tar archives might not write "ustar" perfectly, so we just check header name
            // but normally it is correct.
        }

        uint32_t file_size = get_octal(header->size, 11);

        if (tar_file_count < MAX_TAR_FILES) {
            fs_node_t *node = &tar_nodes[tar_file_count++];
            memset(node, 0, sizeof(fs_node_t));
            strcpy(node->name, header->name);
            node->length = file_size;
            node->impl = ptr + 512;
            node->read = tar_read;

            if (header->typeflag == '5' || header->name[strlen(header->name) - 1] == '/') {
                node->flags = FS_DIRECTORY;
                node->readdir = tar_readdir;
                node->finddir = tar_finddir;
            } else {
                node->flags = FS_FILE;
            }

            terminal_writestring("  -> Found file: ");
            terminal_writestring(node->name);
            terminal_writestring(" (");
            terminal_writedec(node->length);
            terminal_writestring(" bytes)\n");
        }

        // Advance to the next header block (aligned to 512 bytes)
        ptr += 512 + ((file_size + 511) & ~511);
    }

    // Set up root directory
    memset(&tar_root, 0, sizeof(fs_node_t));
    strcpy(tar_root.name, "/");
    tar_root.flags = FS_DIRECTORY;
    tar_root.readdir = tar_readdir;
    tar_root.finddir = tar_finddir;

    fs_root = &tar_root;

    terminal_writestring("[TAR] RAM disk filesystem initialized successfully.\n");
    return &tar_root;
}
