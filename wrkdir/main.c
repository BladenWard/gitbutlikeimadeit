#include <openssl/sha.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <zlib.h>

// TODO: Commands to add
// - cat-file
// - add
// - commit
// - checkout
// - check-ignore
// - hash-object
// - log
// - ls-files
// - ls-tree
// - rev-parse
// - rm
// - show-ref
// - status
// - tag

int main(int argc, char **argv) {
    if (argc < 2) {
        printf("Usage: %s <command>\n", argv[0]);
        return 1;
    }

    char *cmd = argv[1];
    if (strcmp(cmd, "init") == 0) {
        // Create the nessecary dirs
        mkdir(".gblimi", 0777);
        mkdir(".gblimi/objects", 0777);
        mkdir(".gblimi/refs", 0777);

        // Create the HEAD file
        FILE *head = fopen(".gblimi/HEAD", "w");
        fprintf(head, "ref: refs/heads/master\n");
        fclose(head);

        printf("Initialized gblimi repository\n");
    } else if (strcmp(cmd, "hash-object") == 0) {
        // Read the file
        FILE *file = fopen(argv[2], "r");
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

        // Compress the blob
        unsigned long compressed_size = compressBound(ucompSize);
        char *compressed = malloc(compressed_size);
        compress((Bytef *)compressed, &compressed_size, (Bytef *)blob,
                 ucompSize);

        printf("%s\n", hash_str);
        char dir[3] = {hash_str[0], hash_str[1], '\0'};
        printf("%s\n", dir);

    } else {
        fprintf(stderr, "Unknown command: %s\n", cmd);
        return 1;
    }
    return 0;
}
