#include "uint-util.h"

void read_uint32(FILE *fp, uint32_t *value) {
    unsigned char buf[4];
    fread(buf, 1, 4, fp);
    *value = (buf[0] << 24) | (buf[1] << 16) | (buf[2] << 8) | buf[3];
}

void read_uint16(FILE *fp, uint16_t *value) {
    unsigned char buf[2];
    fread(buf, 1, 2, fp);
    *value = (buf[0] << 8) | buf[1];
}

void write_uint32(FILE *fp, uint32_t value) {
    unsigned char buf[4];
    buf[0] = (value >> 24) & 0xFF;
    buf[1] = (value >> 16) & 0xFF;
    buf[2] = (value >> 8) & 0xFF;
    buf[3] = value & 0xFF;
    fwrite(buf, 1, 4, fp);
}

void write_uint16(FILE *fp, uint16_t value) {
    unsigned char buf[2];
    buf[0] = (value >> 8) & 0xFF;
    buf[1] = value & 0xFF;
    fwrite(buf, 1, 2, fp);
}
