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
    // TODO: return error if file doesn't exist
    // so we can create it
    if (!fp) {
        // printf("No index file found\n");
        fclose(fp);
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
        // while (fgetc(fp) == '\0')
        //     ;
        //
        // printf(
        //     "Hex: ctime: %x mtime: %x dev: %x ino: %x mode: %x uid: %x gid:
        //     %x " "size: %x path: %s hash: ",
        //     (*entries)[i].ctime_sec, (*entries)[i].mtime_sec,
        //     (*entries)[i].dev,
        //     (*entries)[i].ino, (*entries)[i].mode, (*entries)[i].uid,
        //     (*entries)[i].gid, (*entries)[i].size, (*entries)[i].path);
        // for (int j = 0; j < 20; j++)
        //     printf("%02x", (*entries)[i].sha1[j]);
        // printf("\n");
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
    } else if (strcmp(cmd, "read-index") == 0) {
        struct git_index_header header;
        struct git_index_entry *entries;
        read_index(&header, &entries);
        for (size_t i = 0; i < header.entries; i++) {
            printf("Hex: ctime: %x mtime: %x dev: %x ino: %x mode: %x uid: %x "
                   "gid: %x "
                   "size: %x path: %s hash: ",
                   entries[i].ctime_sec, entries[i].mtime_sec, entries[i].dev,
                   entries[i].ino, entries[i].mode, entries[i].uid,
                   entries[i].gid, entries[i].size, entries[i].path);
            for (int j = 0; j < 20; j++)
                printf("%02x", entries[i].sha1[j]);
            printf("\n");
        }
        free(entries);
    } else if (strcmp(cmd, "update-index") == 0) {
        if (argc < 3) {
            fprintf(stderr, "Usage: %s update-index <path>\n", argv[0]);
            return 1;
        }

        FILE *index_fp;
        struct git_index_header header;
        struct git_index_entry *entries;

        int file_code = read_index(&header, &entries);

        if (file_code == 2) {
            index_fp = fopen(".gblimi/index", "wb+");
            if (!index_fp) {
                perror("Failed to open index file");
                return 1;
            }

            header.signature[0] = 'D';
            header.signature[1] = 'I';
            header.signature[2] = 'R';
            header.signature[3] = 'C';
            header.entries = 0;
            header.version = 2;

            // 12 byte header
            fwrite(header.signature, 1, 4, index_fp);
            write_uint32(index_fp, header.version);
            write_uint32(index_fp, header.entries);

            fseek(index_fp, 0, SEEK_END);
            long index_size = ftell(index_fp);
            fseek(index_fp, 0, SEEK_SET);
            char *index = malloc(index_size);
            if (index)
                fread(index, 1, index_size, index_fp);

            // Find checksum
            unsigned char checksum[20];
            SHA1((unsigned char *)index, index_size, checksum);
            // for (int i = 0; i < 20; i++)
            //     printf("%02x", checksum[i]);
            // Write checksum
            fwrite(checksum, 1, 20, index_fp);
        }

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
                if (entries[i].mtime_sec < file_stat.st_mtime)
                    modified = 1;
            }
        }

        printf("Found: %d Modified: %d\n", found, modified);

        printf("Entries:\n");
        for (int i = 0; i < header.entries; i++) {
            printf("Path: %s Hash: ", entries[i].path);
            for (int j = 0; j < 20; j++)
                printf("%02x", entries[i].sha1[j]);
            printf("\n");
        }

        // Case 1: Entry found in index and modified since
        if (found && modified) {
            printf("Updating entry\n");

            found--;

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

            long size;
            unsigned char *hash = blob_and_hash_file(argv[2], &size);
            memcpy(entries[found].sha1, hash, 20);

            // Set flags (path length etc.)
            entries[found].flags = strlen(argv[2]);
            strncpy(entries[found].path, argv[2], entries[found].flags);

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
                fwrite(entries[i].path, 1, entries[i].flags, index_fp);

                // Add padding to ensure entry length is a multiple of 8
                int padding = 8 - ((62 + entries[i].flags) % 8);
                if (padding < 8) {
                    char null_bytes[8] = {0};
                    fwrite(null_bytes, 1, padding, index_fp);
                }
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
            for (int i = 0; i < 20; i++)
                printf("%02x", checksum[i]);
            // Write checksum
            fwrite(checksum, 1, 20, index_fp);

            free(index);
            free(entries);
            // free(hash);
            exit(0);
        } else if (!found) { // Case 2: Entry not found in index
            printf("Creating new entry\n");

            // Make space for new entry
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

            long size;
            unsigned char *hash = blob_and_hash_file(argv[2], &size);
            memcpy(entries[header.entries - 1].sha1, hash, 20);

            // Set flags (path length etc.)
            entries[header.entries - 1].flags = strlen(argv[2]);
            strncpy(entries[header.entries - 1].path, argv[2],
                    entries[header.entries - 1].flags);

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
                fwrite(entries[i].path, 1, entries[i].flags, index_fp);

                // Add padding to ensure entry length is a multiple of 8
                int padding = 8 - ((62 + entries[i].flags) % 8);
                if (padding < 8) {
                    char null_bytes[8] = {0};
                    fwrite(null_bytes, 1, padding, index_fp);
                }

                // printf("Path: %s Hash: ", entries[i].path);
                // for (int j = 0; j < 20; j++)
                //     printf("%02x", entries[i].sha1[j]);
                // printf("\n");
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
            // for (int i = 0; i < 20; i++)
            //     printf("%02x", checksum[i]);
            fwrite(checksum, 1, 20, index_fp);

            free(index);
            free(entries);
            // free(hash);
        }

        fclose(index_fp);
        // NOTE: This is the old update-index command
        //  It is helpful for debugging and staging purposes
        // } else if (strcmp(cmd, "update-index2") == 0) {
        //     if (argc < 3) {
        //         fprintf(stderr, "Usage: %s update-index <path>\n", argv[0]);
        //         return 1;
        //     }
        //     FILE *index_fp;
        //     struct git_index_header header;
        //     struct git_index_entry entry;
        //     struct stat file_stat;
        //
        //     // Open or create the index file
        //     index_fp = fopen(".gblimi/index", "wb+");
        //     if (!index_fp) {
        //         perror("Failed to open index file");
        //         return 1;
        //     }
        //
        //     // 12 byte header
        //     memcpy(header.signature, "DIRC", 4);
        //     header.version = 2;
        //     header.entries = 1;
        //
        //     fwrite(header.signature, 1, 4, index_fp);
        //     write_uint32(index_fp, header.version);
        //     write_uint32(index_fp, header.entries);
        //
        //     // Get file stats
        //     if (stat(argv[2], &file_stat) != 0) {
        //         perror("Failed to get file stats");
        //         fclose(index_fp);
        //         return 1;
        //     }
        //
        //     // Prepare entry
        //     memset(&entry, 0, sizeof(entry));
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
        //     size_t size;
        //     unsigned char *hash = blob_and_hash_file(argv[2], (long *)&size);
        //     memcpy(entry.sha1, hash, 20);
        //
        //     // Set flags (path length etc.)
        //     entry.flags = strlen(argv[2]);
        //     strncpy(entry.path, argv[2], BUFSIZ - 1);
        //
        //     printf("Hex: ctime: %x mtime: %x dev: %x ino: %x mode: %x uid: %x
        //     "
        //            "gid: %x "
        //            "size: %x path: %s hash: ",
        //            entry.ctime_sec, entry.mtime_sec, entry.dev, entry.ino,
        //            entry.mode, entry.uid, entry.gid, entry.size, entry.path);
        //     for (int i = 0; i < 20; i++)
        //         printf("%02x", entry.sha1[i]);
        //     printf("\n");
        //
        //     // Write entry
        //     // fwrite(&entry.ctime_sec, 1, 4, index_fp);
        //     write_uint32(index_fp, entry.ctime_sec);
        //     write_uint32(index_fp, entry.ctime_nsec);
        //     write_uint32(index_fp, entry.mtime_sec);
        //     write_uint32(index_fp, entry.mtime_nsec);
        //     write_uint32(index_fp, entry.dev);
        //     write_uint32(index_fp, entry.ino);
        //     write_uint32(index_fp, entry.mode);
        //     write_uint32(index_fp, entry.uid);
        //     write_uint32(index_fp, entry.gid);
        //     write_uint32(index_fp, entry.size);
        //     fwrite(entry.sha1, 1, 20, index_fp);
        //     fwrite(&entry.flags, 1, 2, index_fp);
        //     fwrite(entry.path, 1, entry.flags, index_fp);
        //
        //     // Add padding to ensure entry length is a multiple of 8
        //     int padding = 8 - ((62 + entry.flags) % 8);
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
        //     // Write checksum
        //     fwrite(checksum, 1, 20, index_fp);
        //
        //     // free(index);
        //
        //     fclose(index_fp);
    } else if (strcmp(cmd, "ls-files") == 0) {
        // I hate this and dont really get it anymore so ill figure something
        // different out for this
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
