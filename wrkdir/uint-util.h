#ifndef UINT_UTIL_H
#define UINT_UTIL_H

#include <stdint.h>
#include <stdio.h>

void read_uint16(FILE *fp, uint16_t *value);

void write_uint32(FILE *fp, uint32_t value);

void write_uint16(FILE *fp, uint16_t value);

void read_uint32(FILE *fp, uint32_t *value);

#endif
