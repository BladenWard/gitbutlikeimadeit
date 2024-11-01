#include "index.h"
#include "blob.h"
#include "uint-util.h"

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

void prep_index_entry(struct git_index_entry *entry, struct stat *file_stat,
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

void write_index(FILE *fp, struct git_index_header header,
                 struct git_index_entry *entries) {
    // FILE *fp = fopen(".gblimi/index", "wb+");
    // if (!fp) {
    //     perror("Failed to open index file");
    //     return;
    // }

    write_index_header(fp, &header);

    for (size_t i = 0; i < header.entries; i++)
        write_index_entry(fp, &entries[i]);

    write_index_checksum(fp);
}
