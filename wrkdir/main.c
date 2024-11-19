#include <openssl/sha.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <zlib.h>

#include "hash-object.h"
#include "index.h"

// TODO: Commands to add
// - ~cat-file~
// - add
// - commit
// - checkout
// - check-ignore
// - ~hash-object~
// - log
// - ~ls-files~
// - ls-tree
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

struct git_tree_entry {
    uint32_t mode;
    char *path;
    char sha1[41];
};

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

int write_tree(int argc, char **argv) {
    struct git_index_header header;
    struct git_index_entry *entries;
    read_index(&header, &entries);

    size_t content_size = 0;
    struct stat file_stat;
    for (size_t i = 0; i < header.entries; i++)
        if (stat(entries[i].path, &file_stat) == 0)
            content_size += file_stat.st_size;

    struct git_tree_entry tree_entries[header.entries];

    for (size_t i = 0; i < header.entries; i++) {
        tree_entries[i].mode = entries[i].mode;
        tree_entries[i].path = entries[i].path;
        for (int j = 0; j < 20; j++)
            snprintf(tree_entries[i].sha1 + (j * 2), 3, "%02x",
                     entries[i].sha1[j]);
    }

    int header_offset = 4 + 1 + 4 + 1;
    size_t ucompSize = 0;

    ucompSize += header_offset;
    for (size_t i = 0; i < header.entries; i++)
        ucompSize += 4 + 1 + entries[i].flags + 1 + 43;

    char *tree = malloc(ucompSize * sizeof(char));
    sprintf(tree, "tree %zu%c", content_size, '\0');

    size_t entry_size = 0;
    for (size_t i = 0; i < header.entries; i++) {
        sprintf(tree + header_offset + (entry_size), "%u %s%c%s",
                tree_entries[i].mode, tree_entries[i].path, '\0',
                tree_entries[i].sha1);
        entry_size += 4 + 1 + entries[i].flags + 1 + 43;
    }

    // Hash the tree
    // TODO: Move stuff like this to a function
    unsigned char hash[20];
    SHA1((unsigned char *)tree, ucompSize, hash);
    char *hash_str = malloc(41);
    for (int i = 0; i < 20; i++)
        sprintf(hash_str + i * 2, "%02x", hash[i]);

    // Create the tree path
    char dir[3] = {hash_str[0], hash_str[1], '\0'};
    char *tree_path = malloc(16 + 2 + 1 + strlen(hash_str + 2) * sizeof(char));

    snprintf(tree_path,
             strlen(".gblimi/objects/") + strlen(dir) + 1 +
                 strlen(hash_str + 2) + 1,
             ".gblimi/objects/%s/%s", dir, hash_str + 2);

    tree_path[18] = '\0';
    mkdir(tree_path, 0777);

    // Compress the blob
    unsigned long compressed_size = compressBound(ucompSize);
    char *compressed = malloc(compressed_size);
    compress((Bytef *)compressed, &compressed_size, (Bytef *)tree, ucompSize);

    // Write the tree
    tree_path[18] = '/';
    FILE *object = fopen(tree_path, "w");
    fwrite(compressed, 1, compressed_size, object);
    printf("%s\n", hash_str);

    free(compressed);
    free(tree);
    free(tree_path);
    free(entries);

    return 0;
}

int search_index(struct git_index_header header,
                 struct git_index_entry *entries, struct stat *file_stat,
                 char *path, int *found) {
    if (stat(path, file_stat) != 0) {
        perror("Failed to get file stats");
        return 1;
    }

    int modified = 0;
    for (int i = 0; i < header.entries; i++) {
        if (strcmp(entries[i].path, path) == 0) {
            *found = i + 1;
            if (entries[i].mtime_sec < file_stat->st_mtime)
                return 1;
        }
    }

    return 0;
}

// BUG: Dirs only work if they are added twice
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

    int file_code = read_index(&header, &entries);

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

    FILE *fp = fopen(".gblimi/index", "wb+");
    if (!fp)
        perror("Failed to open index file");

    write_index(fp, header, entries);

    free(entries);

    return 0;
}

int ls_files(int argc, char **argv) {
    struct git_index_header header;
    struct git_index_entry *entries;
    read_index(&header, &entries);

    if (argc > 2 && strcmp(argv[2], "--log") == 0) {
        for (size_t i = 0; i < header.entries; i++) {
            printf(YEL "%s " RESET "mode: %u "
                       "size: %u "
                       "sha1: " GRN,
                   entries[i].path, entries[i].mode, entries[i].size);
            for (int j = 0; j < 20; j++)
                printf("%02x", entries[i].sha1[j]);
            printf(RESET "\n");
        }
    } else {
        for (size_t i = 0; i < header.entries; i++)
            printf(YEL "%s\n" RESET, entries[i].path);
    }

    free(entries);

    return 0;
}

int cat_file(int argc, char **argv) {
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

    return 0;
}

int ls_tree(int argc, char **argv) {
    if (argc < 3) {
        fprintf(stderr, "Usage: %s ls-tree <hash>\n", argv[0]);
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
    char *tree = malloc(BUFSIZ);
    size_t ucompSize = BUFSIZ;
    uncompress((Bytef *)tree, (uLongf *)&ucompSize, (Bytef *)data, BUFSIZ);

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

    free(data);

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

        return write_tree(argc, argv);

    } else if (strcmp(cmd, "update-index") == 0) {

        return update_index(argc, argv);

    } else if (strcmp(cmd, "ls-files") == 0) {

        return ls_files(argc, argv);

    } else if (strcmp(cmd, "ls-tree") == 0) {

        return ls_tree(argc, argv);

    } else if (strcmp(cmd, "cat-file") == 0) {

        return cat_file(argc, argv);

    } else {

        fprintf(stderr, "Unknown command: %s\n", cmd);
        return 1;
    }

    return 0;
}
