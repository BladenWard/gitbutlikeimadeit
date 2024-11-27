#include "index.h"
#include "blob.h"
#include "uint-util.h"

int search_index(struct git_index_header header,
                 struct git_index_entry *entries, struct stat *file_stat,
                 char *path, int *found) {
    if (stat(path, file_stat) != 0) {
        perror("Failed to get file stats");
        return 1;
    }

    for (uint32_t i = 0; i < header.entries; i++) {
        if (strcmp(entries[i].path, path) == 0) {
            *found = i + 1;
            if (entries[i].mtime_sec < file_stat->st_mtime)
                return 1;
        }
    }

    return 0;
}

void sort_entries(struct git_index_entry *entries, size_t num_entries) {
    // This is a bubble sort, but it's fine since the number of entries is
    // probably small
    for (size_t i = 0; i < num_entries; i++) {
        for (size_t j = i + 1; j < num_entries; j++) {
            if (strcmp(entries[i].path, entries[j].path) > 0) {
                struct git_index_entry temp = entries[i];
                entries[i] = entries[j];
                entries[j] = temp;
            }
        }
    }
}

// TODO: Orgainze these function into separate files and factor out the reused
// parts
int update_index(int argc, char **argv) {
    if (argc < 4) {
        fprintf(stderr, "Usage: %s update-index [--add | --remove] <path>\n",
                argv[0]);
        return 1;
    }

    size_t path_len = strlen(argv[3]);
    if (argv[3][path_len - 1] == '/')
        argv[3][path_len - 1] = '\0';

    struct git_index_header header;
    struct git_index_entry *entries;

    read_index(&header, &entries);

    struct stat file_stat;
    int found;
    int modified = search_index(header, entries, &file_stat, argv[3], &found);

    if (strcmp(argv[2], "--add") == 0) {
        if (found && modified) {
            prep_index_entry(&entries[--found], &file_stat, argv[3]);
        } else if (!found) {
            entries = realloc(entries, ++header.entries *
                                           sizeof(struct git_index_entry));
            prep_index_entry(&entries[header.entries - 1], &file_stat, argv[3]);
        }
    } else if (strcmp(argv[2], "--remove") == 0) {
        if (found--) {
            entries[found] = entries[--header.entries];
            entries = realloc(entries,
                              header.entries * sizeof(struct git_index_entry));
        }
    }

    sort_entries(entries, header.entries);

    FILE *fp = fopen(".gblimi/index", "wb+");
    if (!fp)
        perror("Failed to open index file");

    write_index(fp, header, entries);
    fclose(fp);

    free(entries);

    return 0;
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

void prep_index_entry(struct git_index_entry *entry, struct stat *file_stat,
                      char *path) {
    entry->ctime_sec = (uint32_t)file_stat->st_ctime;
    entry->ctime_nsec = (uint32_t)file_stat->st_ctimespec.tv_nsec;
    entry->mtime_sec = (uint32_t)file_stat->st_mtime;
    entry->mtime_nsec = (uint32_t)file_stat->st_mtimespec.tv_nsec;
    entry->dev = (uint32_t)file_stat->st_dev;
    entry->ino = (uint32_t)file_stat->st_ino;
    if (S_ISDIR(file_stat->st_mode)) {
        entry->mode = 40000;
    } else if (S_ISREG(file_stat->st_mode)) {
        entry->mode = 100644;
    } else if (S_ISLNK(file_stat->st_mode)) {
        entry->mode = 120000;
    }
    // entry->mode = (uint32_t)file_stat->st_mode;
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

    if (entry->mode == 40000)
        write_uint32(fp, 40000);
    else if (entry->mode == 100644)
        write_uint32(fp, 100644);
    else if (entry->mode == 120000)
        write_uint32(fp, 120000);

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
