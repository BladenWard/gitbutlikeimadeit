#ifndef HASH_OBJECT_H
#define HASH_OBJECT_H

#include <openssl/sha.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <zlib.h>

int hash_object(char *file, int write);

#endif
