#include <openssl/sha.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <zlib.h>

#include "index.h"
#include "tree.h"

struct git_tree_entry *prep_tree_entries(struct git_index_header header,
                                         struct git_index_entry *entries,
                                         size_t *size) {
    *size = 0;
    struct stat file_stat;
    for (size_t i = 0; i < header.entries; i++)
        if (stat(entries[i].path, &file_stat) == 0)
            *size += file_stat.st_size;

    struct git_tree_entry *tree_entries =
        malloc(header.entries * sizeof(struct git_tree_entry));

    for (size_t i = 0; i < header.entries; i++) {
        tree_entries[i].mode = entries[i].mode;
        tree_entries[i].path = entries[i].path;
        for (int j = 0; j < 20; j++)
            snprintf(tree_entries[i].sha1 + (j * 2), 3, "%02x",
                     entries[i].sha1[j]);
    }

    return tree_entries;
}

char *create_tree(size_t *size, struct git_tree_entry *tree_entries,
                  struct git_index_header header,
                  struct git_index_entry *entries) {
    int header_offset = 4 + 1 + 4 + 1;
    size_t ucompSize = 0;

    ucompSize += header_offset;
    for (size_t i = 0; i < header.entries; i++)
        ucompSize += 4 + 1 + entries[i].flags + 1 + 43;

    char *tree = malloc(ucompSize * sizeof(char));
    sprintf(tree, "tree %zu%c", *size, '\0');

    size_t entry_size = 0;
    for (size_t i = 0; i < header.entries; i++) {
        sprintf(tree + header_offset + (entry_size), "%u %s%c%s",
                tree_entries[i].mode, tree_entries[i].path, '\0',
                tree_entries[i].sha1);
        entry_size += 4 + 1 + entries[i].flags + 1 + 43;
    }
    *size = ucompSize;

    return tree;
}

char *create_object_store(char *hash) {
    char dir[3] = {hash[0], hash[1], '\0'};
    char *tree_path = malloc(16 + 2 + 1 + strlen(hash + 2) * sizeof(char));

    snprintf(tree_path,
             strlen(".gblimi/objects/") + strlen(dir) + 1 + strlen(hash + 2) +
                 1,
             ".gblimi/objects/%s/%s", dir, hash + 2);

    tree_path[18] = '\0';
    mkdir(tree_path, 0777);
    tree_path[18] = '/';

    return tree_path;
}

char *hash_tree(char *tree, size_t size) {
    unsigned char hash[20];
    SHA1((unsigned char *)tree, size, hash);
    char *hash_str = malloc(41);
    for (int i = 0; i < 20; i++)
        sprintf(hash_str + i * 2, "%02x", hash[i]);

    return hash_str;
}

char *compress_tree(char *tree, size_t size, size_t *compressed_size) {
    *compressed_size = compressBound(size);
    char *compressed = malloc(*compressed_size);
    compress((Bytef *)compressed, compressed_size, (Bytef *)tree, size);

    return compressed;
}

int write_tree(int argc, char **argv) {
    struct git_index_header header;
    struct git_index_entry *entries;
    read_index(&header, &entries);

    size_t content_size;
    struct git_tree_entry *tree_entries =
        prep_tree_entries(header, entries, &content_size);

    char *tree = create_tree(&content_size, tree_entries, header, entries);
    free(entries);

    char *hash = hash_tree(tree, content_size);

    size_t compressed_size;
    char *compressed = compress_tree(tree, content_size, &compressed_size);

    char *tree_path = create_object_store(hash);

    FILE *object = fopen(tree_path, "w");
    fwrite(compressed, 1, compressed_size, object);
    fclose(object);
    printf("%s\n", hash);

    free(compressed);
    free(tree);
    free(tree_path);

    return 0;
}
