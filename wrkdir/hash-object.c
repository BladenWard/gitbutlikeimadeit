#include <openssl/sha.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <zlib.h>

#include "blob.h"
#include "tree.h"

int hash_object(char *file, int write) {
    size_t ucompSize;
    char *blob = blob_file(file, &ucompSize);

    char *hash = create_object_hash(blob, ucompSize);

    if (write) {
        size_t compressed_size;
        char *compressed = compress_object(blob, ucompSize, &compressed_size);

        char *object_path = create_object_store(hash);

        FILE *object = fopen(object_path, "w");
        fwrite(compressed, 1, compressed_size, object);
        fclose(object);

        free(compressed);
        free(object_path);
    }

    printf("%s\n", hash);

    free(blob);
    free(hash);

    return 0;
}
