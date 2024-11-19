#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <zlib.h>

#include "blob.h"
#include "openssl/sha.h"

unsigned char *blob_and_hash_file(char *filepath, long *size) {
    // // Read the file
    // FILE *file = fopen(filepath, "r");
    // fseek(file, 0, SEEK_END);
    // *size = ftell(file);
    // fseek(file, 0, SEEK_SET);
    // char *data = malloc(*size);
    // if (data)
    //     fread(data, 1, *size, file);
    // fclose(file);
    //
    // // Make the blob
    // unsigned long ucompSize = *size + 10;
    // char *blob = malloc(ucompSize);
    // sprintf(blob, "blob %ld\\0", *size);
    // memcpy(blob + 10, data, *size);
    // free(data);
    char *blob = blob_file(filepath, (size_t *)size);

    // *size = ucompSize;

    // Hash the blob
    unsigned char hash[20];
    return SHA1((unsigned char *)blob, *size, hash);
}

// TODO: these are almost the same function, refactor
char *blob_file(char *filepath, size_t *size) {
    // Read the file
    FILE *file = fopen(filepath, "r");
    fseek(file, 0, SEEK_END);
    *size = ftell(file);
    fseek(file, 0, SEEK_SET);
    char *data = malloc(*size);
    if (data)
        fread(data, 1, *size, file);
    fclose(file);

    // Make the blob
    unsigned long ucompSize = *size + 10;
    char *blob = malloc(ucompSize);
    sprintf(blob, "blob %ld\\0", *size);
    memcpy(blob + 10, data, *size);
    free(data);

    *size = ucompSize;

    return blob;
}
