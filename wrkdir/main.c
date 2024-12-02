#include <getopt.h>
#include <openssl/sha.h>
#include <regex.h>
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
// - commit-tree
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

struct config {
    char name[30];
    char email[40];
};

void read_config(struct config *config) {
    FILE *config_file = fopen(".gblimi/config", "r");
    char *config_str = malloc(1000);
    fread(config_str, 1, 1000, config_file);

    int i = 0;
    while (config_str[i] != '\0') {
        if (config_str[i] == '\n')
            config_str[i] = '\0';
        i++;
    }

    char *name = config_str + 7;
    char *email = config_str + 8 + strlen(name) + 8;
    // config->name = name;
    // config->email = email;
    strncpy(config->name, name, 30);
    strncpy(config->email, email, 40);

    fclose(config_file);
    free(config_str);
}

void write_config(struct config *config) {
    FILE *config_file = fopen(".gblimi/config", "w");
    fprintf(config_file, "name = %s\nemail = %s", config->name, config->email);
    fclose(config_file);
}

int config(int argc, char **argv) {
    if (argc < 3) {
        fprintf(stderr, "Usage: %s config <key>\n", argv[0]);
        return 1;
    }

    if (strcmp(argv[2], "set") == 0) {
        if (argc < 4) {
            fprintf(stderr, "Usage: %s %s %s <name> <value>\n", argv[0],
                    argv[1], argv[2]);
            return 1;
        }

        struct config *config;
        config = malloc(sizeof(struct config));
        read_config(config);

        if (!strcmp(argv[3], "name")) {
            strncpy(config->name, argv[4], 30);
        } else if (!strcmp(argv[3], "email")) {
            strncpy(config->email, argv[4], 40);
        } else {
            fprintf(stderr, "Cannot set %s\n", config->name);
            return 1;
        }

        write_config(config);

        free(config);

        return 0;

    } else if (strcmp(argv[2], "get") == 0) {

        if (argc < 3) {
            fprintf(stderr, "Usage: %s get <name>\n", argv[0]);
            return 1;
        }

        struct config config;
        read_config(&config);

        if (!strcmp(argv[3], "name")) {
            printf("%s\n", config.name);
        } else if (!strcmp(argv[3], "email")) {
            printf("%s\n", config.email);
        } else {
            fprintf(stderr, "Cannot get %s\n", argv[3]);
            return 1;
        }

        return 0;

    } else {
        fprintf(stderr, "Unknown command: %s\n", argv[2]);
        return 1;
    }
}

int commit_tree(int argc, char **argv) {
    if (argc < 3) {
        fprintf(stderr, "Usage: %s commit-tree <hash>\n", argv[0]);
        return 1;
    }

    printf("commit-tree %s\n", argv[2]);

    return 0;
}

static int helpflag;
struct option options[] = {
    {"help", no_argument, &helpflag, 1},
};

int main(int argc, char **argv) {
    if (argc < 2) {
        printf("Usage: %s <command>\n", argv[0]);
        return 1;
    }

    char *cmd = argv[1];

    if (strcmp(cmd, "init") == 0) {

        return init();

    } else if (strcmp(cmd, "hash-object") == 0) {

        if (argc < 3) {
            fprintf(stderr, "Usage: %s hash-object <file>\n", argv[0]);
            return 1;
        }

        int hash_object_write_flag;
        struct option hash_options[] = {
            {"write", no_argument, NULL, 'w'},
            {NULL, 0, NULL, 0},
        };
        int hash_option_index = 0;

        int c;
        while ((c = getopt_long(argc, argv, "w", hash_options,
                                &hash_option_index)) != -1) {
            if (c == -1)
                break;

            switch (c) {
            case 'w':
                hash_object_write_flag = 1;
                break;
            default:
                break;
            }
        }

        return hash_object(argv[optind + 1], hash_object_write_flag);

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

    } else if (strcmp(cmd, "commit-tree") == 0) {

        return commit_tree(argc, argv);

    } else if (strcmp(cmd, "config") == 0) {

        return config(argc, argv);

    } else {

        fprintf(stderr, "Unknown command: %s\n", cmd);
        return 1;
    }

    return 0;
}
