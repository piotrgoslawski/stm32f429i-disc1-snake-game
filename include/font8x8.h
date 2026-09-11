#ifndef FONT8X8_H
#define FONT8X8_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* 8x8 bitmap font for printable ASCII (0x20-0x7E).
   Each char is 8 bytes; each byte is one row, LSB = leftmost pixel. */
extern const uint8_t font8x8[95][8];

#ifdef __cplusplus
}
#endif

#endif /* FONT8X8_H */
