#ifndef TREE_H
#define TREE_H

#include <stdint.h>

#include "index.h"

struct git_tree_entry {
    uint32_t mode;
    char *path;
    char sha1[41];
};

struct git_tree_entry *prep_tree_entries(struct git_index_header header,
                                         struct git_index_entry *entries,
                                         size_t *size);

char *create_tree(size_t *size, struct git_tree_entry *tree_entries,
                  struct git_index_header header,
                  struct git_index_entry *entries);

char *hash_tree(char *tree, size_t size);

char *compress_tree(char *tree, size_t size, size_t *compressed_size);

int write_tree(int argc, char **argv);

char *create_object_store(char *hash);

#endif
