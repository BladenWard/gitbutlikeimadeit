#include <stdint.h>

struct git_index_header {
    char signature[4]; // Should be "DIRC"
    uint32_t version;  // Version number (usually 2)
    uint32_t entries;  // Number of entries
};

// Git index entry structure
struct git_index_entry {
    uint32_t ctime_sec;
    uint32_t ctime_nsec;
    uint32_t mtime_sec;
    uint32_t mtime_nsec;
    uint32_t dev;
    uint32_t ino;
    uint32_t mode;
    uint32_t uid;
    uint32_t gid;
    uint32_t size;
    unsigned char sha1[20];
    uint16_t flags;
    char path[4096];
};
