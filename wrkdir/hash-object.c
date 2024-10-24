#include <openssl/sha.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <zlib.h>

#include "blob.h"

int hash_object(int argc, char **argv) {
    // Blob the file
    size_t ucompSize;
    char *blob = blob_file(argv[argc > 3 ? 3 : 2], &ucompSize);

    // Hash the blob
    unsigned char hash[20];
    SHA1((unsigned char *)blob, ucompSize, hash);
    char *hash_str = malloc(41);
    for (int i = 0; i < 20; i++)
        sprintf(hash_str + i * 2, "%02x", hash[i]);

    if (argc > 3) {
        if (strcmp(argv[2], "-w") == 0) {
            // Compress the blob
            unsigned long compressed_size = compressBound(ucompSize);
            char *compressed = malloc(compressed_size);
            compress((Bytef *)compressed, &compressed_size, (Bytef *)blob,
                     ucompSize);

            // Create the object path
            char dir[3] = {hash_str[0], hash_str[1], '\0'};
            char *object_path =
                malloc(16 + 2 + 1 + strlen(hash_str + 2) * sizeof(char));

            snprintf(object_path,
                     strlen(".gblimi/objects/") + strlen(dir) + 1 +
                         strlen(hash_str + 2) + 1,
                     ".gblimi/objects/%s/%s", dir, hash_str + 2);

            object_path[18] = '\0';
            mkdir(object_path, 0777);

            // Write the object
            object_path[18] = '/';
            FILE *object = fopen(object_path, "w");
            fwrite(compressed, 1, compressed_size, object);
            printf("Created object path: %s\n", object_path);

            fclose(object);
            free(compressed);
            free(object_path);
        }
    }

    printf("%s\n", hash_str);

    free(blob);
    free(hash_str);

    return 0;
}
