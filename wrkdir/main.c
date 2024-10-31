#include <openssl/sha.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <zlib.h>

#include "blob.h"
#include "hash-object.h"
#include "index.h"

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

#define MAX_PATH_LENGTH 4096
#define IDX_FND 10
#define IDX_MOD 01

void read_uint32(FILE *fp, uint32_t *value) {
    unsigned char buf[4];
    fread(buf, 1, 4, fp);
    *value = (buf[0] << 24) | (buf[1] << 16) | (buf[2] << 8) | buf[3];
}

void read_uint16(FILE *fp, uint16_t *value) {
    unsigned char buf[2];
    fread(buf, 1, 2, fp);
    *value = (buf[0] << 8) | buf[1];
}

// Function to read and parse the index file
int read_index(struct git_index_header *header,
               struct git_index_entry **entries) {
    char index_path[256];
    snprintf(index_path, 256, ".gblimi/index");
    FILE *fp = fopen(index_path, "rb");
    if (!fp) {
        fclose(fp);
        header->signature[0] = 'D';
        header->signature[1] = 'I';
        header->signature[2] = 'R';
        header->signature[3] = 'C';
        header->entries = 0;
        header->version = 2;

        *entries = malloc(sizeof(struct git_index_entry));
        return 2;
    }

    // Read header
    if (fread(&header->signature, 4, 1, fp) != 1) {
        fclose(fp);
        printf("Error reading header\n");
        return 1;
    }
    // Verify signature
    if (memcmp(header->signature, "DIRC", 4) != 0) {
        fclose(fp);
        printf("Invalid index file signature\n");
    }
    // Read rest of header
    read_uint32(fp, &header->version);
    read_uint32(fp, &header->entries);

    // Read entries
    *entries = malloc((header->entries) * sizeof(struct git_index_entry));
    for (size_t i = 0; i < header->entries; i++) {
        read_uint32(fp, &(*entries)[i].ctime_sec);
        read_uint32(fp, &(*entries)[i].ctime_nsec);
        read_uint32(fp, &(*entries)[i].mtime_sec);
        read_uint32(fp, &(*entries)[i].mtime_nsec);
        read_uint32(fp, &(*entries)[i].dev);
        read_uint32(fp, &(*entries)[i].ino);
        read_uint32(fp, &(*entries)[i].mode);
        read_uint32(fp, &(*entries)[i].uid);
        read_uint32(fp, &(*entries)[i].gid);
        read_uint32(fp, &(*entries)[i].size);
        fread((*entries)[i].sha1, 1, 20, fp);
        fread(&(*entries)[i].flags, 1, 2, fp);
        char path[4096]; // Max path length
        int c, path_len = 0;
        while ((c = fgetc(fp)) != '\0' && c != EOF && path_len < 4095) {
            path[path_len++] = c;
        }
        path[path_len] = '\0';
        strncpy((*entries)[i].path, path, path_len);
        int padding = (8 - ((62 + path_len) % 8)) - 1;
        if (padding > 0) {
            char null_bytes[8] = {0};
            fread(null_bytes, 1, padding, fp);
        }
    }

    fclose(fp);
    return 0;
}

void write_uint32(FILE *fp, uint32_t value) {
    unsigned char buf[4];
    buf[0] = (value >> 24) & 0xFF;
    buf[1] = (value >> 16) & 0xFF;
    buf[2] = (value >> 8) & 0xFF;
    buf[3] = value & 0xFF;
    fwrite(buf, 1, 4, fp);
}

void write_uint16(FILE *fp, uint16_t value) {
    unsigned char buf[2];
    buf[0] = (value >> 8) & 0xFF;
    buf[1] = value & 0xFF;
    fwrite(buf, 1, 2, fp);
}

int init() {
    // Create the nessecary dirs
    mkdir(".gblimi", 0777);
    mkdir(".gblimi/objects", 0777);
    mkdir(".gblimi/refs", 0777);

    // Create the HEAD file
    FILE *head = fopen(".gblimi/HEAD", "w");
    fprintf(head, "ref: refs/heads/master\n");
    fclose(head);

    printf("Initialized gblimi repository\n");

    return 0;
}

void prep_entry(struct git_index_entry *entry, struct stat *file_stat,
                char *path) {
    entry->ctime_sec = (uint32_t)file_stat->st_ctime;
    entry->ctime_nsec = (uint32_t)file_stat->st_ctimespec.tv_nsec;
    entry->mtime_sec = (uint32_t)file_stat->st_mtime;
    entry->mtime_nsec = (uint32_t)file_stat->st_mtimespec.tv_nsec;
    entry->dev = (uint32_t)file_stat->st_dev;
    entry->ino = (uint32_t)file_stat->st_ino;
    entry->mode = (uint32_t)file_stat->st_mode;
    entry->uid = (uint32_t)file_stat->st_uid;
    entry->gid = (uint32_t)file_stat->st_gid;
    entry->size = (uint32_t)file_stat->st_size;

    long size;
    unsigned char *hash = blob_and_hash_file(path, &size);
    memcpy(entry->sha1, hash, 20);

    // Set flags (path length etc.)
    entry->flags = strlen(path);
    strncpy(entry->path, path, entry->flags);
}

void write_index_header(FILE *fp, struct git_index_header *header) {
    fwrite(header->signature, 1, 4, fp);
    write_uint32(fp, header->version);
    write_uint32(fp, header->entries);
}

void write_index_entry(FILE *fp, struct git_index_entry *entry) {
    write_uint32(fp, entry->ctime_sec);
    write_uint32(fp, entry->ctime_nsec);
    write_uint32(fp, entry->mtime_sec);
    write_uint32(fp, entry->mtime_nsec);
    write_uint32(fp, entry->dev);
    write_uint32(fp, entry->ino);
    write_uint32(fp, entry->mode);
    write_uint32(fp, entry->uid);
    write_uint32(fp, entry->gid);
    write_uint32(fp, entry->size);
    fwrite(entry->sha1, 1, 20, fp);
    fwrite(&entry->flags, 1, 2, fp);
    fwrite(entry->path, 1, entry->flags, fp);

    int padding = 8 - ((62 + entry->flags) % 8);
    if (padding < 8) {
        char null_bytes[8] = {0};
        fwrite(null_bytes, 1, padding, fp);
    }
}

void write_index_checksum(FILE *fp) {
    fseek(fp, 0, SEEK_END);
    long index_size = ftell(fp);
    fseek(fp, 0, SEEK_SET);

    char *index = malloc(index_size);
    if (index)
        fread(index, 1, index_size, fp);

    unsigned char checksum[20];
    SHA1((unsigned char *)index, index_size, checksum);

    fwrite(checksum, 1, 20, fp);

    free(index);
}

void write_index(struct git_index_header header,
                 struct git_index_entry *entries) {
    FILE *fp = fopen(".gblimi/index", "wb+");
    if (!fp) {
        perror("Failed to open index file");
        return;
    }

    write_index_header(fp, &header);

    for (size_t i = 0; i < header.entries; i++)
        write_index_entry(fp, &entries[i]);

    write_index_checksum(fp);
}

int main(int argc, char **argv) {
    if (argc < 2) {
        printf("Usage: %s <command>\n", argv[0]);
        return 1;
    }

    char *cmd = argv[1];
    if (strcmp(cmd, "init") == 0) {

        return init();

    } else if (strcmp(cmd, "hash-object") == 0) {

        return hash_object(argc, argv);

        // } else if (strcmp(cmd, "read-index") == 0) {
        //
        //     struct git_index_header header;
        //     struct git_index_entry *entries;
        //     read_index(&header, &entries);
        //     for (size_t i = 0; i < header.entries; i++) {
        //         printf("Hex: ctime: %x mtime: %x dev: %x ino: %x mode: %x
        //         uid: %x "
        //                "gid: %x "
        //                "size: %x path: %s hash: ",
        //                entries[i].ctime_sec, entries[i].mtime_sec,
        //                entries[i].dev, entries[i].ino, entries[i].mode,
        //                entries[i].uid, entries[i].gid, entries[i].size,
        //                entries[i].path);
        //         for (int j = 0; j < 20; j++)
        //             printf("%02x", entries[i].sha1[j]);
        //         printf("\n");
        //     }
        //     free(entries);
        //
    } else if (strcmp(cmd, "update-index") == 0) {
        if (argc < 3) {
            fprintf(stderr, "Usage: %s update-index <path>\n", argv[0]);
            return 1;
        }

        struct git_index_header header;
        struct git_index_entry *entries;

        int file_code = read_index(&header, &entries);

        struct stat file_stat;
        if (stat(argv[2], &file_stat) != 0) {
            perror("Failed to get file stats");
            return 1;
        }

        int found = 0;
        int modified = 0;
        for (int i = 0; i < header.entries; i++) {
            if (strcmp(entries[i].path, argv[2]) == 0) {
                found = i + 1;
                if (entries[i].mtime_sec < file_stat.st_mtime)
                    modified = 1;
            }
        }

        if (found && modified) {
            prep_entry(&entries[--found], &file_stat, argv[2]);
        } else if (!found) {
            entries = realloc(entries, ++header.entries *
                                           sizeof(struct git_index_entry));
            prep_entry(&entries[header.entries - 1], &file_stat, argv[2]);
        }

        write_index(header, entries);

        free(entries);
    } else if (strcmp(cmd, "ls-files") == 0) {
        struct git_index_header header;
        struct git_index_entry *entries;
        read_index(&header, &entries);

        for (size_t i = 0; i < header.entries; i++)
            printf("%s\n", entries[i].path);

        free(entries);
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
