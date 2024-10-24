#ifndef BLOB_H
#define BLOB_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <zlib.h>

unsigned char *blob_and_hash_file(char *filepath, long *size);
char *blob_file(char *filepath, size_t *size);

#endif
