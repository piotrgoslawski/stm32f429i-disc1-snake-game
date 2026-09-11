#ifndef L3GD20_H
#define L3GD20_H

#ifdef __cplusplus
extern "C" {
#endif

/* Initializes the L3GD20 MEMS gyroscope on the SPI bus and CS pin declared
   by the app-local `l3gd20` devicetree node (SPI5, shared with the LCD),
   and configures it for continuous 500 dps measurement. */
void L3GD20_Init(void);

/* Reads the current angular rate on all three axes, in degrees/second. */
void L3GD20_ReadDPS(float *x, float *y, float *z);

#ifdef __cplusplus
}
#endif

#endif /* L3GD20_H */
