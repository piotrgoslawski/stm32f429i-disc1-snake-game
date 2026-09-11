#include "l3gd20.h"

#include <stdbool.h>
#include <string.h>

#include <zephyr/device.h>
#include <zephyr/drivers/spi.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(l3gd20, LOG_LEVEL_INF);

/* SPI5 bus shared with the LCD (SCK=PF7, MISO=PF8, MOSI=PF9); CS=PC1 is
   driven by the SPI driver itself via the `l3gd20` node's entry in
   `&spi5 { cs-gpios = ... }` (see app.overlay) -- no manual GPIO here. */
static const struct spi_dt_spec gyro_spi =
    SPI_DT_SPEC_GET(DT_NODELABEL(l3gd20), SPI_WORD_SET(8) | SPI_OP_MODE_MASTER);

/* L3GD20 register map (subset actually used) */
#define L3GD20_WHO_AM_I_ADDR   0x0F
#define L3GD20_CTRL_REG1_ADDR  0x20
#define L3GD20_CTRL_REG4_ADDR  0x23
#define L3GD20_OUT_X_L_ADDR    0x28

#define L3GD20_I_AM_L3GD20     0xD4
#define L3GD20_I_AM_L3GD20_TR  0xD5
/* Board rev E01+ ships the register-compatible I3G4250D instead (same
   CTRL_REG1/CTRL_REG4 values and +-500 dps sensitivity used below). Seen
   on the actual board in use: WHO_AM_I=0xD3. */
#define L3GD20_I_AM_I3G4250D   0xD3

/* Address-byte control bits (SPI read/write protocol, see datasheet) */
#define L3GD20_READ_CMD   0x80
#define L3GD20_MULTI_CMD  0x40

/* CTRL_REG1: normal/active mode, ODR1, bandwidth 4, X/Y/Z enabled */
#define L3GD20_CTRL_REG1_VALUE  0x3F
/* CTRL_REG4: continuous update, LSB-first, +-500 dps full scale */
#define L3GD20_CTRL_REG4_VALUE  0x10

/* Sensitivity for the +-500 dps full-scale setting above, in mdps/LSB */
#define L3GD20_SENSITIVITY_500DPS  17.50f

/* Largest read used is the 6-byte X/Y/Z burst; +1 for the address byte. */
#define L3GD20_MAX_READ_LEN  6

static void gyro_write_reg(uint8_t reg, uint8_t value)
{
    uint8_t tx[2] = { reg, value };
    const struct spi_buf tx_buf = { .buf = tx, .len = sizeof(tx) };
    const struct spi_buf_set tx_set = { .buffers = &tx_buf, .count = 1 };

    int ret = spi_write_dt(&gyro_spi, &tx_set);
    if (ret != 0) {
        LOG_ERR("L3GD20: spi_write_dt failed (reg=0x%02X): %d", reg, ret);
    }
}

/* One spi_transceive_dt call covering the address byte and `len` data
   bytes, so CS stays asserted across the whole transaction (the L3GD20
   auto-increments its register pointer on each clocked byte while CS is
   low; a separate write-then-read would deassert CS in between and break
   that). rx[0] is a garbage byte clocked out while the address byte is
   being shifted in and is discarded -- only rx[1..len] is real data. */
static void gyro_read(uint8_t start_reg, uint8_t *out, size_t len, bool multi)
{
    uint8_t tx[1 + L3GD20_MAX_READ_LEN] = { 0 };
    uint8_t rx[1 + L3GD20_MAX_READ_LEN] = { 0 };

    tx[0] = start_reg | L3GD20_READ_CMD | (multi ? L3GD20_MULTI_CMD : 0);

    const struct spi_buf tx_buf = { .buf = tx, .len = len + 1 };
    const struct spi_buf rx_buf = { .buf = rx, .len = len + 1 };
    const struct spi_buf_set tx_set = { .buffers = &tx_buf, .count = 1 };
    const struct spi_buf_set rx_set = { .buffers = &rx_buf, .count = 1 };

    int ret = spi_transceive_dt(&gyro_spi, &tx_set, &rx_set);
    if (ret != 0) {
        LOG_ERR("L3GD20: spi_transceive_dt failed (reg=0x%02X): %d", start_reg, ret);
        memset(out, 0, len);
        return;
    }

    memcpy(out, &rx[1], len);
}

void L3GD20_Init(void)
{
    LOG_DBG("L3GD20: init start");

    uint8_t id = 0;
    gyro_read(L3GD20_WHO_AM_I_ADDR, &id, 1, false);
    if (id != L3GD20_I_AM_L3GD20 && id != L3GD20_I_AM_L3GD20_TR &&
        id != L3GD20_I_AM_I3G4250D) {
        LOG_ERR("L3GD20: unexpected WHO_AM_I=0x%02X (gyroscope not detected)", id);
    } else {
        LOG_INF("L3GD20: WHO_AM_I=0x%02X (%s)", id,
                id == L3GD20_I_AM_I3G4250D ? "I3G4250D" : "L3GD20");
    }

    gyro_write_reg(L3GD20_CTRL_REG1_ADDR, L3GD20_CTRL_REG1_VALUE);
    gyro_write_reg(L3GD20_CTRL_REG4_ADDR, L3GD20_CTRL_REG4_VALUE);

    /* Datasheet: allow ~100 ms turn-on time after leaving power-down before
       trusting the output. */
    k_msleep(100);

    LOG_DBG("L3GD20: init done");
}

void L3GD20_ReadDPS(float *x, float *y, float *z)
{
    uint8_t raw[6];
    gyro_read(L3GD20_OUT_X_L_ADDR, raw, sizeof(raw), true);

    int16_t rx = (int16_t)((raw[1] << 8) | raw[0]);
    int16_t ry = (int16_t)((raw[3] << 8) | raw[2]);
    int16_t rz = (int16_t)((raw[5] << 8) | raw[4]);

    *x = (float)rx * L3GD20_SENSITIVITY_500DPS / 1000.0f;
    *y = (float)ry * L3GD20_SENSITIVITY_500DPS / 1000.0f;
    *z = (float)rz * L3GD20_SENSITIVITY_500DPS / 1000.0f;
}
