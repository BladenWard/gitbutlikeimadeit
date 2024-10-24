#include <openssl/sha.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <zlib.h>

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

char *hash_file(char *data, long size);

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
void read_index(struct git_index_header *header,
                struct git_index_entry **entries) {
    char index_path[256];
    snprintf(index_path, 256, ".gblimi/index");
    FILE *fp = fopen(index_path, "rb");
    // TODO: return error if file doesn't exist
    // so we can create it
    if (!fp) {
        printf("Error opening index file\n");
        return;
    }

    // Read header
    if (fread(&header->signature, 4, 1, fp) != 1) {
        printf("Error reading header\n");
        fclose(fp);
        return;
    }
    // Verify signature
    if (memcmp(header->signature, "DIRC", 4) != 0) {
        printf("Invalid index file signature\n");
        fclose(fp);
        return;
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
        read_uint16(fp, &(*entries)[i].flags);
        char path[4096]; // Max path length
        int c, path_len = 0;
        while ((c = fgetc(fp)) != '\0' && c != EOF && path_len < 4095) {
            path[path_len++] = c;
        }
        path[path_len] = '\0';
        strncpy((*entries)[i].path, path, BUFSIZ - 1);
        while (fgetc(fp) != '\0')
            ;
    }

    for (int i = 0; i < header->entries; i++) {
        printf("Hex: ctime: %x mtime: %x dev: %x ino: %x mode: %x uid: %x "
               "gid: %x "
               "size: %x path: %s hash: ",
               (*entries)[i].ctime_sec, (*entries)[i].mtime_sec,
               (*entries)[i].dev, (*entries)[i].ino, (*entries)[i].mode,
               (*entries)[i].uid, (*entries)[i].gid, (*entries)[i].size,
               (*entries)[i].path);
        for (int j = 0; j < 20; j++)
            printf("%02x", (*entries)[i].sha1[j]);
        printf("\n");
    }

    fclose(fp);
}

void write_uint32(FILE *fp, uint32_t value) {
    unsigned char buf[4];
    buf[0] = (value >> 24) & 0xFF;
    buf[1] = (value >> 16) & 0xFF;
    buf[2] = (value >> 8) & 0xFF;
    buf[3] = value & 0xFF;
    fwrite(buf, 1, 4, fp);
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

int hash_object(int argc, char **argv) {
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
    // char *hash_str = hash_file(blob, ucompSize);

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

    return 0;
}

// void create_entry(FILE *index_fp, char *path, struct git_index_entry entry,
//                   struct stat file_stat) {
//     entry.ctime_sec = (uint32_t)file_stat.st_ctime;
//     entry.ctime_nsec = (uint32_t)file_stat.st_ctimespec.tv_nsec;
//     entry.mtime_sec = (uint32_t)file_stat.st_mtime;
//     entry.mtime_nsec = (uint32_t)file_stat.st_mtimespec.tv_nsec;
//     entry.dev = (uint32_t)file_stat.st_dev;
//     entry.ino = (uint32_t)file_stat.st_ino;
//     entry.mode = (uint32_t)file_stat.st_mode;
//     entry.uid = (uint32_t)file_stat.st_uid;
//     entry.gid = (uint32_t)file_stat.st_gid;
//     entry.size = (uint32_t)file_stat.st_size;
//
//     FILE *file = fopen(path, "r");
//     fseek(file, 0, SEEK_END);
//     long size = ftell(file);
//     fseek(file, 0, SEEK_SET);
//     char *data = malloc(size);
//     if (data)
//         fread(data, 1, size, file);
//     fclose(file);
//
//     // Create the blob
//     unsigned long ucompSize = size + 10;
//     char *blob = malloc(ucompSize);
//     sprintf(blob, "blob %ld\\0", size);
//     memcpy(blob + 10, data, size);
//     free(data);
//
//     // Hash the file
//     unsigned char hash[20];
//     SHA1((unsigned char *)blob, ucompSize, hash);
//     free(blob);
//     memcpy(entry.sha1, hash, 20);
//
//     // Set flags (path length etc.)
//     entry.flags = strlen(path);
//     strncpy(entry.path, path, BUFSIZ - 1);
//
//     index_fp = fopen(".gblimi/index", "wb+");
//     if (!index_fp) {
//         perror("Failed to open index file");
//         return 1;
//     }
//     printf("index signature: %s\n", header.signature);
//
//     // 12 byte header
//     fwrite(header.signature, 1, 4, index_fp);
//     write_uint32(index_fp, header.version);
//     write_uint32(index_fp, header.entries);
//
//     // Write entries
//     for (size_t i = 0; i < header.entries; i++) {
//         write_uint32(index_fp, entries[i].ctime_sec);
//         write_uint32(index_fp, entries[i].ctime_nsec);
//         write_uint32(index_fp, entries[i].mtime_sec);
//         write_uint32(index_fp, entries[i].mtime_nsec);
//         write_uint32(index_fp, entries[i].dev);
//         write_uint32(index_fp, entries[i].ino);
//         write_uint32(index_fp, entries[i].mode);
//         write_uint32(index_fp, entries[i].uid);
//         write_uint32(index_fp, entries[i].gid);
//         write_uint32(index_fp, entries[i].size);
//         fwrite(entries[i].sha1, 1, 20, index_fp);
//         fwrite(&entries[i].flags, 1, 2, index_fp);
//         fwrite(entries[i].path, 1, strlen(entries[0].path), index_fp);
//         printf("Hex: ctime: %x mtime: %x dev: %x ino: %x mode: %x uid: %x
//                "
//                "gid: %x "
//                "size: %x path: %s hash: ",
//                entries[i].ctime_sec, entries[i].mtime_sec, entries[i].dev,
//                entries[i].ino, entries[i].mode, entries[i].uid,
//                entries[i].gid, entries[i].size, entries[i].path);
//         for (int j = 0; j < 20; j++)
//             printf("%02x", entries[i].sha1[j]);
//         printf("\n");
//
//         // Add padding to ensure entry length is a multiple of 8
//         int padding = 8 - ((62 + entries[i].flags) % 8);
//         if (padding < 8) {
//             char null_bytes[8] = {0};
//             fwrite(null_bytes, 1, padding, index_fp);
//         }
//     }
//
//     // Add padding to ensure entry length is a multiple of 8
//     int padding = 8 - ((62 + entries[header.entries - 1].flags) % 8);
//     if (padding < 8) {
//         char null_bytes[8] = {0};
//         fwrite(null_bytes, 1, padding, index_fp);
//     }
//
//     fseek(index_fp, 0, SEEK_END);
//     long index_size = ftell(index_fp);
//     fseek(index_fp, 0, SEEK_SET);
//     char *index = malloc(index_size);
//     if (index)
//         fread(index, 1, index_size, index_fp);
//
//     // Find checksum
//     unsigned char checksum[20];
//     SHA1((unsigned char *)index, index_size, checksum);
//     printf("checksum: ");
//     for (int i = 0; i < 20; i++)
//         printf("%02x", checksum[i]);
//     // Write checksum
//     fwrite(checksum, 1, 20, index_fp);
//
//     free(index);
//     free(entries);
// }

#define FOUND 0x2
#define MODIFIED 0x4

int main(int argc, char **argv) {
    if (argc < 2) {
        printf("Usage: %s <command>\n", argv[0]);
        return 1;
    }

    char *cmd = argv[1];
    if (strcmp(cmd, "init") == 0) {
        return init();
    } else if (strcmp(cmd, "read-index") == 0) {
        // struct git_index_header header;
        // struct git_index_entry *entries;
        // size_t entries_size;
        // read_index(&header, &entries, &entries_size);
        //
        // printf("Index version: %u\n", header.version);
        // printf("Number of entries: %u\n\n", header.entries);
        //
        // for (size_t i = 0; i < entries_size; i++) {
        //     printf("Hex: ctime: %x mtime: %x dev: %x ino: %x mode: %x uid: %x
        //     "
        //            "gid: %x "
        //            "size: %x hash: ",
        //            entries[i].ctime_sec, entries[i].mtime_sec,
        //            entries[i].dev, entries[i].ino, entries[i].mode,
        //            entries[i].uid, entries[i].gid, entries[i].size);
        //     for (int j = 0; j < 20; j++)
        //         printf("%02x", entries[i].sha1[j]);
        //     printf("\n");
        // }
    } else if (strcmp(cmd, "hash-object") == 0) {
        // TODO: staging area
        return hash_object(argc, argv);
    } else if (strcmp(cmd, "read") == 0) {
        printf("Reading index\n");
        struct git_index_header header;
        struct git_index_entry *entries;
        read_index(&header, &entries);
        // printf("Index version: %u\n", header.version);
        // printf("Number of entries: %u\n\n", header.entries);
        // for (size_t i = 0; i < header.entries; i++) {
        //     printf("Hex: ctime: %x mtime: %x dev: %x ino: %x mode: %x uid: %x
        //     "
        //            "gid: %x "
        //            "size: %x path: %s hash: ",
        //            entries[i].ctime_sec, entries[i].mtime_sec,
        //            entries[i].dev, entries[i].ino, entries[i].mode,
        //            entries[i].uid, entries[i].gid, entries[i].size,
        //            entries[i].path);
        //     for (int j = 0; j < 20; j++)
        //         printf("%02x", entries[i].sha1[j]);
        //     printf("\n");
        // }
    } else if (strcmp(cmd, "update-index") == 0) {
        if (argc < 3) {
            fprintf(stderr, "Usage: %s update-index <path>\n", argv[0]);
            return 1;
        }
        FILE *index_fp;
        struct git_index_header header;
        struct git_index_entry *entries;

        read_index(&header, &entries);

        struct stat file_stat;
        if (stat(argv[2], &file_stat) != 0) {
            perror("Failed to get file stats");
            fclose(index_fp);
            return 1;
        }
        int found = 0;
        int modified = 0;
        for (int i = 0; i < header.entries; i++) {
            if (strcmp(entries[i].path, argv[2]) == 0) {
                found = i + 1;
                printf("Found: %d\n", found);
                if (entries[i].mtime_sec < file_stat.st_mtime)
                    modified = 1;
                printf("Modified: %d\n", modified);
            }
        }

        // Case 1: Entry found in index and modified since
        if (found && modified) {
            printf("Updating entry\n");

            entries[found].ctime_sec = (uint32_t)file_stat.st_ctime;
            entries[found].ctime_nsec =
                (uint32_t)file_stat.st_ctimespec.tv_nsec;
            entries[found].mtime_sec = (uint32_t)file_stat.st_mtime;
            entries[found].mtime_nsec =
                (uint32_t)file_stat.st_mtimespec.tv_nsec;
            entries[found].dev = (uint32_t)file_stat.st_dev;
            entries[found].ino = (uint32_t)file_stat.st_ino;
            entries[found].mode = (uint32_t)file_stat.st_mode;
            entries[found].uid = (uint32_t)file_stat.st_uid;
            entries[found].gid = (uint32_t)file_stat.st_gid;
            entries[found].size = (uint32_t)file_stat.st_size;

            FILE *file = fopen(argv[2], "r");
            fseek(file, 0, SEEK_END);
            long size = ftell(file);
            fseek(file, 0, SEEK_SET);
            char *data = malloc(size);
            if (data)
                fread(data, 1, size, file);
            fclose(file);

            // Create the blob
            unsigned long ucompSize = size + 10;
            char *blob = malloc(ucompSize);
            sprintf(blob, "blob %ld\\0", size);
            memcpy(blob + 10, data, size);
            free(data);

            // Hash the file
            unsigned char hash[20];
            SHA1((unsigned char *)blob, ucompSize, hash);
            free(blob);
            memcpy(entries[found].sha1, hash, 20);

            // Set flags (path length etc.)
            entries[found].flags = strlen(argv[2]);
            strncpy(entries[found].path, argv[2], BUFSIZ - 1);

            for (int i = 0; i < 20; i++)
                printf("%02x", entries[found].sha1[i]);

            index_fp = fopen(".gblimi/index", "wb+");
            if (!index_fp) {
                perror("Failed to open index file");
                return 1;
            }

            // 12 byte header
            fwrite(header.signature, 1, 4, index_fp);
            write_uint32(index_fp, header.version);
            write_uint32(index_fp, header.entries);

            // Write entries
            printf("Writing entries %d\n", header.entries);
            for (size_t i = 0; i < header.entries; i++) {
                write_uint32(index_fp, entries[found].ctime_sec);
                write_uint32(index_fp, entries[found].ctime_nsec);
                write_uint32(index_fp, entries[found].mtime_sec);
                write_uint32(index_fp, entries[found].mtime_nsec);
                write_uint32(index_fp, entries[found].dev);
                write_uint32(index_fp, entries[found].ino);
                write_uint32(index_fp, entries[found].mode);
                write_uint32(index_fp, entries[found].uid);
                write_uint32(index_fp, entries[found].gid);
                write_uint32(index_fp, entries[found].size);
                fwrite(entries[found].sha1, 1, 20, index_fp);
                fwrite(&entries[found].flags, 1, 2, index_fp);
                fwrite(entries[found].path, 1, strlen(entries[found].path),
                       index_fp);
                for (int i = 0; i < 20; i++)
                    printf("%02x", entries[found].sha1[i]);
                printf("\n");

                // Add padding to ensure entry length is a multiple of 8
                int padding = 8 - ((62 + entries[found].flags) % 8);
                if (padding < 8) {
                    char null_bytes[8] = {0};
                    fwrite(null_bytes, 1, padding, index_fp);
                }
            }

            // Add padding to ensure entry length is a multiple of 8
            // int padding = 8 - ((62 + entries[found].flags) % 8);
            // if (padding < 8) {
            //     char null_bytes[8] = {0};
            //     fwrite(null_bytes, 1, padding, index_fp);
            // }

            fseek(index_fp, 0, SEEK_END);
            long index_size = ftell(index_fp);
            fseek(index_fp, 0, SEEK_SET);
            char *index = malloc(index_size);
            if (index)
                fread(index, 1, index_size, index_fp);

            // Find checksum
            unsigned char checksum[20];
            SHA1((unsigned char *)index, index_size, checksum);
            printf("checksum: ");
            for (int i = 0; i < 20; i++)
                printf("%02x", checksum[i]);
            // Write checksum
            fwrite(checksum, 1, 20, index_fp);

            printf("Updated entry\n");

            free(index);
            free(entries);
            exit(0);
        } else if (!found) { // Case 2: Entry not found in index
            printf("Creating new entry\n");

            printf("Found: %d\n", found);
            printf("Modified: %d\n", modified);
            // Add entry to index
            entries = realloc(entries, ++header.entries *
                                           sizeof(struct git_index_entry));

            entries[header.entries - 1].ctime_sec =
                (uint32_t)file_stat.st_ctime;
            entries[header.entries - 1].ctime_nsec =
                (uint32_t)file_stat.st_ctimespec.tv_nsec;
            entries[header.entries - 1].mtime_sec =
                (uint32_t)file_stat.st_mtime;
            entries[header.entries - 1].mtime_nsec =
                (uint32_t)file_stat.st_mtimespec.tv_nsec;
            entries[header.entries - 1].dev = (uint32_t)file_stat.st_dev;
            entries[header.entries - 1].ino = (uint32_t)file_stat.st_ino;
            entries[header.entries - 1].mode = (uint32_t)file_stat.st_mode;
            entries[header.entries - 1].uid = (uint32_t)file_stat.st_uid;
            entries[header.entries - 1].gid = (uint32_t)file_stat.st_gid;
            entries[header.entries - 1].size = (uint32_t)file_stat.st_size;

            FILE *file = fopen(argv[2], "r");
            fseek(file, 0, SEEK_END);
            long size = ftell(file);
            fseek(file, 0, SEEK_SET);
            char *data = malloc(size);
            if (data)
                fread(data, 1, size, file);
            fclose(file);

            // Create the blob
            unsigned long ucompSize = size + 10;
            char *blob = malloc(ucompSize);
            sprintf(blob, "blob %ld\\0", size);
            memcpy(blob + 10, data, size);
            free(data);

            // Hash the file
            unsigned char hash[20];
            SHA1((unsigned char *)blob, ucompSize, hash);
            free(blob);
            memcpy(entries[header.entries - 1].sha1, hash, 20);

            // Set flags (path length etc.)
            entries[header.entries - 1].flags = strlen(argv[2]);
            strncpy(entries[header.entries - 1].path, argv[2], BUFSIZ - 1);

            // printf("New number of entries: %u\n\n", header.entries);
            // for (size_t i = 0; i < header.entries; i++) {
            //     printf("Hex: ctime: %x mtime: %x dev: %x ino: %x mode: %x uid
            //     "
            //            ": %x "
            //            "gid: %x "
            //            "size: %x path: %s hash: ",
            //            entries[i].ctime_sec, entries[i].mtime_sec,
            //            entries[i].dev, entries[i].ino, entries[i].mode,
            //            entries[i].uid, entries[i].gid, entries[i].size,
            //            entries[i].path);
            //     for (int j = 0; j < 20; j++)
            //         printf("%02x", entries[i].sha1[j]);
            //     printf("\n");
            // }
            index_fp = fopen(".gblimi/index", "wb+");
            if (!index_fp) {
                perror("Failed to open index file");
                return 1;
            }
            // printf("index signature: %s\n", header.signature);

            // 12 byte header
            fwrite(header.signature, 1, 4, index_fp);
            write_uint32(index_fp, header.version);
            write_uint32(index_fp, header.entries);

            // Write entries
            printf("Writing entries %d\n", header.entries);
            for (size_t i = 0; i < header.entries; i++) {
                write_uint32(index_fp, entries[i].ctime_sec);
                write_uint32(index_fp, entries[i].ctime_nsec);
                write_uint32(index_fp, entries[i].mtime_sec);
                write_uint32(index_fp, entries[i].mtime_nsec);
                write_uint32(index_fp, entries[i].dev);
                write_uint32(index_fp, entries[i].ino);
                write_uint32(index_fp, entries[i].mode);
                write_uint32(index_fp, entries[i].uid);
                write_uint32(index_fp, entries[i].gid);
                write_uint32(index_fp, entries[i].size);
                fwrite(entries[i].sha1, 1, 20, index_fp);
                fwrite(&entries[i].flags, 1, 2, index_fp);
                fwrite(entries[i].path, 1, strlen(entries[i].path), index_fp);
                printf("Hex: ctime: %x mtime: %x dev: %x ino: %x mode : %x "
                       "uid : %x "
                       "gid: %x "
                       "size: %x path: %s hash: ",
                       entries[i].ctime_sec, entries[i].mtime_sec,
                       entries[i].dev, entries[i].ino, entries[i].mode,
                       entries[i].uid, entries[i].gid, entries[i].size,
                       entries[i].path);
                for (int j = 0; j < 20; j++)
                    printf("%02x", entries[i].sha1[j]);
                printf("\n");

                // Add padding to ensure entry length is a multiple of 8
                int padding = 8 - ((62 + entries[i].flags) % 8);
                if (padding < 8) {
                    char null_bytes[8] = {0};
                    fwrite(null_bytes, 1, padding, index_fp);
                }
            }

            // Add padding to ensure entry length is a multiple of 8
            // int padding = 8 - ((62 + entries[header.entries - 1].flags) % 8);
            // if (padding < 8) {
            //     char null_bytes[8] = {0};
            //     fwrite(null_bytes, 1, padding, index_fp);
            // }

            fseek(index_fp, 0, SEEK_END);
            long index_size = ftell(index_fp);
            fseek(index_fp, 0, SEEK_SET);
            char *index = malloc(index_size);
            if (index)
                fread(index, 1, index_size, index_fp);

            // Find checksum
            unsigned char checksum[20];
            SHA1((unsigned char *)index, index_size, checksum);
            printf("checksum: ");
            for (int i = 0; i < 20; i++)
                printf("%02x", checksum[i]);
            printf("\n");
            // Write checksum
            fwrite(checksum, 1, 20, index_fp);

            free(index);
            free(entries);
        }

        fclose(index_fp);
    } else if (strcmp(cmd, "update-index2") == 0) {
        if (argc < 3) {
            fprintf(stderr, "Usage: %s update-index <path>\n", argv[0]);
            return 1;
        }
        FILE *index_fp;
        struct git_index_header header;
        struct git_index_entry entry;
        struct stat file_stat;

        // Open or create the index file
        index_fp = fopen(".gblimi/index", "wb+");
        if (!index_fp) {
            perror("Failed to open index file");
            return 1;
        }

        // 12 byte header
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
        entry.ctime_nsec = (uint32_t)file_stat.st_ctimespec.tv_nsec;
        entry.mtime_sec = (uint32_t)file_stat.st_mtime;
        entry.mtime_nsec = (uint32_t)file_stat.st_mtimespec.tv_nsec;
        entry.dev = (uint32_t)file_stat.st_dev;
        entry.ino = (uint32_t)file_stat.st_ino;
        entry.mode = (uint32_t)file_stat.st_mode;
        entry.uid = (uint32_t)file_stat.st_uid;
        entry.gid = (uint32_t)file_stat.st_gid;
        entry.size = (uint32_t)file_stat.st_size;

        FILE *file = fopen(argv[2], "r");
        fseek(file, 0, SEEK_END);
        long size = ftell(file);
        fseek(file, 0, SEEK_SET);
        char *data = malloc(size);
        if (data)
            fread(data, 1, size, file);
        fclose(file);

        // Create the blob
        unsigned long ucompSize = size + 10;
        char *blob = malloc(ucompSize);
        sprintf(blob, "blob %ld\\0", size);
        memcpy(blob + 10, data, size);
        free(data);

        // Hash the file
        unsigned char hash[20];
        SHA1((unsigned char *)blob, ucompSize, hash);
        free(blob);
        memcpy(entry.sha1, hash, 20);

        // Set flags (path length etc.)
        entry.flags = strlen(argv[2]);
        strncpy(entry.path, argv[2], BUFSIZ - 1);

        printf("Hex: ctime: %x mtime: %x dev: %x ino: %x mode: %x uid: %x "
               "gid: %x "
               "size: %x path: %s hash: ",
               entry.ctime_sec, entry.mtime_sec, entry.dev, entry.ino,
               entry.mode, entry.uid, entry.gid, entry.size, entry.path);
        for (int i = 0; i < 20; i++)
            printf("%02x", entry.sha1[i]);
        printf("\n");

        // Write entry
        // fwrite(&entry.ctime_sec, 1, 4, index_fp);
        write_uint32(index_fp, entry.ctime_sec);
        write_uint32(index_fp, entry.ctime_nsec);
        write_uint32(index_fp, entry.mtime_sec);
        write_uint32(index_fp, entry.mtime_nsec);
        write_uint32(index_fp, entry.dev);
        write_uint32(index_fp, entry.ino);
        write_uint32(index_fp, entry.mode);
        write_uint32(index_fp, entry.uid);
        write_uint32(index_fp, entry.gid);
        write_uint32(index_fp, entry.size);
        fwrite(entry.sha1, 1, 20, index_fp);
        fwrite(&entry.flags, 1, 2, index_fp);
        fwrite(entry.path, 1, entry.flags, index_fp);

        // Add padding to ensure entry length is a multiple of 8
        int padding = 8 - ((62 + entry.flags) % 8);
        if (padding < 8) {
            char null_bytes[8] = {0};
            fwrite(null_bytes, 1, padding, index_fp);
        }

        fseek(index_fp, 0, SEEK_END);
        long index_size = ftell(index_fp);
        fseek(index_fp, 0, SEEK_SET);
        char *index = malloc(index_size);
        if (index)
            fread(index, 1, index_size, index_fp);

        // Find checksum
        unsigned char checksum[20];
        SHA1((unsigned char *)index, index_size, checksum);
        // Write checksum
        fwrite(checksum, 1, 20, index_fp);

        // free(index);

        fclose(index_fp);
    } else if (strcmp(cmd, "ls-files") == 0) {
        // // Read the index
        // FILE *index = fopen(".gblimi/index", "r");
        // fseek(index, 0, SEEK_END);
        // long size = ftell(index);
        // fseek(index, 0, SEEK_SET);
        // char *data = malloc(size);
        // if (data)
        //     fread(data, 1, size, index);
        // fclose(index);
        //
        // // Parse the index
        // char *ptr = data;
        // while (ptr < data + size) {
        //     char *hash = ptr;
        //     ptr += 40;
        //     char *mode = ptr;
        //     ptr += 10;
        //     char *path = ptr;
        //     ptr += strlen(path) + 1;
        //
        //     printf("%s %s\n", hash, path);
        // }
        //
        // free(data);
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

char *hash_file(char *data, long size) {
    unsigned char hash[20];
    SHA1((unsigned char *)data, size, hash);
    char *hash_str = malloc(41);
    for (int i = 0; i < 20; i++)
        sprintf(hash_str + i * 2, "%02x", hash[i]);
    return hash_str;
}
