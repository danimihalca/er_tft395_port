#ifndef NV3052_H_
#define NV3052_H_

#include <zephyr/device.h>

/* Bit-banged 9-bit SPI init of the panel's NV3052 controller (CS=14, SDI=12, SCK=13). */
int nv3052_init(void);

#endif
