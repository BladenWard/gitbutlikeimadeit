#include <openssl/sha.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <zlib.h>

#include "hash-object.h"
#include "index.h"
#include "tree.h"

// TODO: Commands to add
// - ~cat-file~
// - add
// - commit
// - checkout
// - config
// - check-ignore
// - ~hash-object~
// - log
// - ~ls-files~
// - ~ls-tree~
// - rev-parse
// - rm
// - show-ref
// - status
// - tag

#define RED "\x1B[31m"
#define GRN "\x1B[32m"
#define YEL "\x1B[33m"
#define BLU "\x1B[34m"
#define MAG "\x1B[35m"
#define CYN "\x1B[36m"
#define WHT "\x1B[37m"
#define RESET "\x1B[0m"

int init(void) {
    mkdir(".gblimi", 0777);
    mkdir(".gblimi/objects", 0777);
    mkdir(".gblimi/refs", 0777);

    FILE *head = fopen(".gblimi/HEAD", "w");
    fprintf(head, "ref: refs/heads/master\n");
    fclose(head);

    printf("Initialized gblimi repository\n");

    return 0;
}

char *retrieve_object(char *hash, size_t *size) {
    char *object_path = malloc(strlen(".gblimi/objects/") + strlen(hash) + 2);

    char dir[3] = {hash[0], hash[1], '\0'};
    snprintf(object_path, strlen(".gblimi/objects/") + strlen(hash) + 2,
             ".gblimi/objects/%s/%s", dir, hash + 2);

    FILE *object = fopen(object_path, "r");
    fseek(object, 0, SEEK_END);
    *size = ftell(object);
    fseek(object, 0, SEEK_SET);
    char *data = malloc(*size);
    if (data)
        fread(data, 1, *size, object);
    fclose(object);

    size_t ucompSize = 8192;
    char *blob = malloc(ucompSize);
    uncompress((Bytef *)blob, (uLongf *)&ucompSize, (Bytef *)data, ucompSize);
    *size = ucompSize;

    free(object_path);
    free(data);

    return blob;
}

int ls_files(int argc, char **argv) {
    struct git_index_header header;
    struct git_index_entry *entries;
    read_index(&header, &entries);

    if (argc > 2 && strcmp(argv[2], "--log") == 0) {
        int longest = 0;
        for (size_t i = 0; i < header.entries; i++)
            if (strlen(entries[i].path) > (size_t)longest)
                longest = strlen(entries[i].path);

        for (size_t i = 0; i < header.entries; i++) {
            printf("%-*.*s %.6u %5uB ", longest, longest, entries[i].path,
                   entries[i].mode, entries[i].size);
            for (int j = 0; j < 20; j++)
                printf("%02x", entries[i].sha1[j]);
            printf("\n");
        }
    } else {
        for (size_t i = 0; i < header.entries; i++)
            printf("%s\n", entries[i].path);
    }

    free(entries);

    return 0;
}

// FIX: Long files are not being read correctly
// so Ill need to figure out how to read the file
// in chunks
int cat_file(int argc, char **argv) {
    if (argc < 3) {
        fprintf(stderr, "Usage: %s cat-file <hash>\n", argv[0]);
        return 1;
    }

    size_t ucompSize = 8192;
    char *blob = retrieve_object(argv[2], &ucompSize);

    printf("%s\n", blob + 10);

    free(blob);

    return 0;
}

int ls_tree(int argc, char **argv) {
    if (argc < 3) {
        fprintf(stderr, "Usage: %s ls-tree <hash>\n", argv[0]);
        return 1;
    }

    size_t ucompSize = 4096;
    char *tree = retrieve_object(argv[2], &ucompSize);

    // Skip the header
    int header_end = 0;
    while (tree[header_end] != '\0')
        header_end++;
    while (tree[header_end] == '\0')
        header_end++;

    // Count the number of entries by searching for the null terminator
    size_t num_entries = 0;
    for (size_t i = header_end; i < ucompSize - 1; i++)
        if (tree[i] == '\0')
            num_entries++;

    // Account for the double null on all but the last entry
    num_entries = (num_entries + 1) / 2;

    // Read the entries into the struct
    struct git_tree_entry entries[num_entries];
    size_t entry_size = 0;
    tree += header_end;
    for (size_t i = 0; i < num_entries; i++) {
        int entry_padding = 0;
        if (tree[entry_size] == '4')
            entry_padding = 1;

        entries[i].mode = atoi(tree + entry_size);

        while (tree[entry_size] != ' ')
            entry_size++;
        entries[i].path = tree + entry_size + 1;

        while (tree[entry_size] != '\0')
            entry_size++;
        strncpy(entries[i].sha1, tree + entry_size + 1, 40);
        entries[i].sha1[40] = '\0';

        entry_size += 42 + entry_padding;
    }

    for (size_t i = 0; i < num_entries; i++)
        printf("%.6d %s %.40s\t%s\n", entries[i].mode,
               entries[i].mode / 100000 == 0 ? "tree" : "blob", entries[i].sha1,
               entries[i].path);

    return 0;
}

int config(int argc, char **argv) {
    if (argc < 3) {
        fprintf(stderr, "Usage: %s config <key>\n", argv[0]);
        return 1;
    }

    if (strcmp(argv[2], "set") == 0) {
        printf("Config\n");
    }

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

    } else if (strcmp(cmd, "write-tree") == 0) {

        return write_tree();

    } else if (strcmp(cmd, "update-index") == 0) {

        return update_index(argc, argv);

    } else if (strcmp(cmd, "ls-files") == 0) {

        return ls_files(argc, argv);

    } else if (strcmp(cmd, "ls-tree") == 0) {

        return ls_tree(argc, argv);

    } else if (strcmp(cmd, "cat-file") == 0) {

        return cat_file(argc, argv);

    } else if (strcmp(cmd, "config") == 0) {

        return config(argc, argv);

    } else {

        fprintf(stderr, "Unknown command: %s\n", cmd);
        return 1;
    }

    return 0;
}
