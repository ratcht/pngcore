/**
* @file pngcore_crc.h
* @brief CRC calculation functions for PNG files
*/

#ifndef PNGCORE_CRC_H
#define PNGCORE_CRC_H

#include <stddef.h>

/* CRC calculation functions */
void pngcore_make_crc_table(void);
unsigned long pngcore_update_crc(unsigned long crc, unsigned char *buf, int len);
unsigned long pngcore_crc(unsigned char *buf, int len);

#endif /* PNGCORE_CRC_H */