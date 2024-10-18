#include <openssl/sha.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <zlib.h>

// TODO: Commands to add
// - cat-file
// - add
// - commit
// - checkout
// - check-ignore
// - ~hash-object~
// - log
// - ls-files
// - ls-tree
// - rev-parse
// - rm
// - show-ref
// - status
// - tag

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
    char path[1024];
};

void write_uint32(FILE *fp, uint32_t value) {
    unsigned char buf[4];
    buf[0] = (value >> 24) & 0xFF;
    buf[1] = (value >> 16) & 0xFF;
    buf[2] = (value >> 8) & 0xFF;
    buf[3] = value & 0xFF;
    fwrite(buf, 1, 4, fp);
}

int main(int argc, char **argv) {
    if (argc < 2) {
        printf("Usage: %s <command>\n", argv[0]);
        return 1;
    }

    char *cmd = argv[1];
    if (strcmp(cmd, "init") == 0) {
        // Create the nessecary dirs
        mkdir(".gblimi", 0777);
        mkdir(".gblimi/objects", 0777);
        mkdir(".gblimi/refs", 0777);

        // Create the HEAD file
        FILE *head = fopen(".gblimi/HEAD", "w");
        fprintf(head, "ref: refs/heads/master\n");
        fclose(head);

        printf("Initialized gblimi repository\n");
    } else if (strcmp(cmd, "hash-object") == 0) {
        // Read the file
        FILE *file = fopen(argv[argc > 3 ? 3 : 2], "r");
        fseek(file, 0, SEEK_END);
        long size = ftell(file);
        fseek(file, 0, SEEK_SET);
        char *data = malloc(size);
        if (data)
            fread(data, 1, size, file);
        fclose(file);

        // Make the blob
        unsigned long ucompSize = size + 10;
        char *blob = malloc(ucompSize);
        sprintf(blob, "blob %ld\\0", size);
        memcpy(blob + 10, data, size);

        // Hash the blob
        unsigned char hash[20];
        SHA1((unsigned char *)blob, ucompSize, hash);
        char *hash_str = malloc(41);
        for (int i = 0; i < 20; i++)
            sprintf(hash_str + i * 2, "%02x", hash[i]);

        if (argc > 3) {
            if (strcmp(argv[2], "-w") == 0) {
                // Compress the blob
                unsigned long compressed_size = compressBound(ucompSize);
                char *compressed = malloc(compressed_size);
                compress((Bytef *)compressed, &compressed_size, (Bytef *)blob,
                         ucompSize);

                // Create the object path
                char dir[3] = {hash_str[0], hash_str[1], '\0'};
                char *object_path =
                    malloc(16 + 2 + 1 + strlen(hash_str + 2) * sizeof(char));

                snprintf(object_path,
                         strlen(".gblimi/objects/") + strlen(dir) + 1 +
                             strlen(hash_str + 2) + 1,
                         ".gblimi/objects/%s/%s", dir, hash_str + 2);

                object_path[18] = '\0';
                mkdir(object_path, 0777);

                // Write the object
                object_path[18] = '/';
                FILE *object = fopen(object_path, "w");
                fwrite(compressed, 1, compressed_size, object);
                printf("Created object path: %s\n", object_path);

                fclose(object);
                free(compressed);
                free(object_path);
            }
        }

        printf("%s\n", hash_str);

        free(blob);
        free(hash_str);
        free(data);
    } else if (strcmp(cmd, "update-index") == 0) {
        if (argc < 3) {
            fprintf(stderr, "Usage: %s update-index <path>\n", argv[0]);
            return 1;
        }
        FILE *index_fp;
        struct git_index_header header;
        struct git_index_entry entry;
        struct stat file_stat;

        // Open or create the index file
        index_fp = fopen(".git/index", "wb");
        if (!index_fp) {
            perror("Failed to open index file");
            return 1;
        }

        // Prepare header
        memcpy(header.signature, "DIRC", 4);
        header.version = 2;
        header.entries = 1;

        fwrite(header.signature, 1, 4, index_fp);
        write_uint32(index_fp, header.version);
        write_uint32(index_fp, header.entries);

        // Get file stats
        if (stat(argv[2], &file_stat) != 0) {
            perror("Failed to get file stats");
            fclose(index_fp);
            return 1;
        }

        // Prepare entry
        memset(&entry, 0, sizeof(entry));
        entry.ctime_sec = (uint32_t)file_stat.st_ctime;
        entry.ctime_nsec = 0;
        entry.mtime_sec = (uint32_t)file_stat.st_mtime;
        entry.mtime_nsec = 0;
        entry.dev = (uint32_t)file_stat.st_dev;
        entry.ino = (uint32_t)file_stat.st_ino;
        entry.mode = (uint32_t)file_stat.st_mode;
        entry.uid = (uint32_t)file_stat.st_uid;
        entry.gid = (uint32_t)file_stat.st_gid;
        entry.size = (uint32_t)file_stat.st_size;

    } else if (strcmp(cmd, "ls-files") == 0) {
        // Read the index
        FILE *index = fopen(".gblimi/index", "r");
        fseek(index, 0, SEEK_END);
        long size = ftell(index);
        fseek(index, 0, SEEK_SET);
        char *data = malloc(size);
        if (data)
            fread(data, 1, size, index);
        fclose(index);

        // Parse the index
        char *ptr = data;
        while (ptr < data + size) {
            char *hash = ptr;
            ptr += 40;
            char *mode = ptr;
            ptr += 10;
            char *path = ptr;
            ptr += strlen(path) + 1;

            printf("%s %s\n", hash, path);
        }

        free(data);
    } else if (strcmp(cmd, "cat-file") == 0) {
        if (argc < 3) {
            fprintf(stderr, "Usage: %s cat-file <hash>\n", argv[0]);
            return 1;
        }

        // Make the object path
        char *object_path =
            malloc(strlen(".gblimi/objects/") + strlen(argv[2]) + 2);

        char dir[3] = {argv[2][0], argv[2][1], '\0'};
        snprintf(object_path, strlen(".gblimi/objects/") + strlen(argv[2]) + 2,
                 ".gblimi/objects/%s/%s", dir, argv[2] + 2);

        // Read the object
        FILE *object = fopen(object_path, "r");
        fseek(object, 0, SEEK_END);
        long size = ftell(object);
        fseek(object, 0, SEEK_SET);
        char *data = malloc(size);
        if (data)
            fread(data, 1, size, object);
        fclose(object);

        // Decompress the object
        char *blob = malloc(size);
        uncompress((Bytef *)blob, (uLongf *)&size, (Bytef *)data, size);
        // Print the blob
        printf("%s\n", blob + 10);

        free(object_path);
        free(blob);
        free(data);
    } else {
        fprintf(stderr, "Unknown command: %s\n", cmd);
        return 1;
    }
    return 0;
}
