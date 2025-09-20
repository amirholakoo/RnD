/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"
#include "driver/spi_master.h"
#include "driver/i2c.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ADXL345 Register Addresses */
#define ADXL345_REG_DEVID           0x00U   /* Device ID */
#define ADXL345_REG_THRESH_TAP      0x1DU   /* Tap threshold */
#define ADXL345_REG_OFSX            0x1EU   /* X-axis offset */
#define ADXL345_REG_OFSY            0x1FU   /* Y-axis offset */
#define ADXL345_REG_OFSZ            0x20U   /* Z-axis offset */
#define ADXL345_REG_DUR             0x21U   /* Tap duration */
#define ADXL345_REG_LATENT          0x22U   /* Tap latency */
#define ADXL345_REG_WINDOW          0x23U   /* Tap window */
#define ADXL345_REG_THRESH_ACT      0x24U   /* Activity threshold */
#define ADXL345_REG_THRESH_INACT    0x25U   /* Inactivity threshold */
#define ADXL345_REG_TIME_INACT      0x26U   /* Inactivity time */
#define ADXL345_REG_ACT_INACT_CTL   0x27U   /* Axis enable control for activity and inactivity detection */
#define ADXL345_REG_THRESH_FF       0x28U   /* Free-fall threshold */
#define ADXL345_REG_TIME_FF         0x29U   /* Free-fall time */
#define ADXL345_REG_TAP_AXES        0x2AU   /* Axis control for single tap/double tap */
#define ADXL345_REG_ACT_TAP_STATUS  0x2BU   /* Source of single tap/double tap */
#define ADXL345_REG_BW_RATE         0x2CU   /* Data rate and power mode control */
#define ADXL345_REG_POWER_CTL       0x2DU   /* Power-saving features control */
#define ADXL345_REG_INT_ENABLE      0x2EU   /* Interrupt enable control */
#define ADXL345_REG_INT_MAP         0x2FU   /* Interrupt mapping control */
#define ADXL345_REG_INT_SOURCE      0x30U   /* Source of interrupts */
#define ADXL345_REG_DATA_FORMAT     0x31U   /* Data format control */
#define ADXL345_REG_DATAX0          0x32U   /* X-Axis Data 0 */
#define ADXL345_REG_DATAX1          0x33U   /* X-Axis Data 1 */
#define ADXL345_REG_DATAY0          0x34U   /* Y-Axis Data 0 */
#define ADXL345_REG_DATAY1          0x35U   /* Y-Axis Data 1 */
#define ADXL345_REG_DATAZ0          0x36U   /* Z-Axis Data 0 */
#define ADXL345_REG_DATAZ1          0x37U   /* Z-Axis Data 1 */
#define ADXL345_REG_FIFO_CTL        0x38U   /* FIFO control */
#define ADXL345_REG_FIFO_STATUS     0x39U   /* FIFO status */

/* ADXL345 Constants */
#define ADXL345_DEVICE_ID           0xE5U   /* Expected device ID */
#define ADXL345_SPI_READ_BIT        0x80U   /* SPI read bit */
#define ADXL345_SPI_MULTI_BYTE      0x40U   /* SPI multi-byte read */

/* I2C Addresses */
#define ADXL345_I2C_ADDR_ALT_LOW    0x53U   /* I2C address when ALT pin is low */
#define ADXL345_I2C_ADDR_ALT_HIGH   0x1DU   /* I2C address when ALT pin is high */

/* Data Rate Codes (BW_RATE register) */
#define ADXL345_RATE_3200HZ         0x0FU   /* 3200 Hz - Maximum sample rate */
#define ADXL345_RATE_1600HZ         0x0EU   /* 1600 Hz */
#define ADXL345_RATE_800HZ          0x0DU   /* 800 Hz */
#define ADXL345_RATE_400HZ          0x0CU   /* 400 Hz */
#define ADXL345_RATE_200HZ          0x0BU   /* 200 Hz */
#define ADXL345_RATE_100HZ          0x0AU   /* 100 Hz */
#define ADXL345_RATE_50HZ           0x09U   /* 50 Hz */
#define ADXL345_RATE_25HZ           0x08U   /* 25 Hz */

/* Power Control Bits */
#define ADXL345_POWER_CTL_MEASURE   0x08U   /* Measurement mode */
#define ADXL345_POWER_CTL_SLEEP     0x04U   /* Sleep mode */
#define ADXL345_POWER_CTL_WAKEUP_8  0x00U   /* 8 Hz wake-up frequency */
#define ADXL345_POWER_CTL_WAKEUP_4  0x01U   /* 4 Hz wake-up frequency */
#define ADXL345_POWER_CTL_WAKEUP_2  0x02U   /* 2 Hz wake-up frequency */
#define ADXL345_POWER_CTL_WAKEUP_1  0x03U   /* 1 Hz wake-up frequency */

/* Data Format Bits */
#define ADXL345_DATA_FORMAT_RANGE_2G    0x00U   /* +/- 2g range */
#define ADXL345_DATA_FORMAT_RANGE_4G    0x01U   /* +/- 4g range */
#define ADXL345_DATA_FORMAT_RANGE_8G    0x02U   /* +/- 8g range */
#define ADXL345_DATA_FORMAT_RANGE_16G   0x03U   /* +/- 16g range */
#define ADXL345_DATA_FORMAT_FULL_RES    0x08U   /* Full resolution mode */
#define ADXL345_DATA_FORMAT_JUSTIFY     0x04U   /* Left-justified mode */

/* FIFO Control Bits */
#define ADXL345_FIFO_MODE_BYPASS    0x00U   /* Bypass mode */
#define ADXL345_FIFO_MODE_FIFO      0x40U   /* FIFO mode */
#define ADXL345_FIFO_MODE_STREAM    0x80U   /* Stream mode */
#define ADXL345_FIFO_MODE_TRIGGER   0xC0U   /* Trigger mode */

/* Maximum values for bounds checking */
#define ADXL345_MAX_FIFO_SAMPLES    32U     /* Maximum FIFO samples */
#define ADXL345_SCALE_FACTOR_2G     256     /* Scale factor for 2g range */
#define ADXL345_SCALE_FACTOR_4G     128     /* Scale factor for 4g range */
#define ADXL345_SCALE_FACTOR_8G     64      /* Scale factor for 8g range */
#define ADXL345_SCALE_FACTOR_16G    32      /* Scale factor for 16g range */

/**
 * @brief ADXL345 communication interface type
 */
typedef enum {
    ADXL345_INTERFACE_SPI = 0U,    /**< SPI interface */
    ADXL345_INTERFACE_I2C = 1U     /**< I2C interface */
} adxl345_interface_t;

/**
 * @brief ADXL345 data range configuration
 */
typedef enum {
    ADXL345_RANGE_2G = 0U,         /**< +/- 2g range */
    ADXL345_RANGE_4G = 1U,         /**< +/- 4g range */
    ADXL345_RANGE_8G = 2U,         /**< +/- 8g range */
    ADXL345_RANGE_16G = 3U         /**< +/- 16g range */
} adxl345_range_t;

/**
 * @brief ADXL345 data rate configuration
 */
typedef enum {
    ADXL345_DATARATE_25HZ = 0x08U,     /**< 25 Hz */
    ADXL345_DATARATE_50HZ = 0x09U,     /**< 50 Hz */
    ADXL345_DATARATE_100HZ = 0x0AU,    /**< 100 Hz */
    ADXL345_DATARATE_200HZ = 0x0BU,    /**< 200 Hz */
    ADXL345_DATARATE_400HZ = 0x0CU,    /**< 400 Hz */
    ADXL345_DATARATE_800HZ = 0x0DU,    /**< 800 Hz */
    ADXL345_DATARATE_1600HZ = 0x0EU,   /**< 1600 Hz */
    ADXL345_DATARATE_3200HZ = 0x0FU    /**< 3200 Hz - Maximum */
} adxl345_datarate_t;

/**
 * @brief ADXL345 acceleration data structure
 */
typedef struct {
    int16_t x;                     /**< X-axis acceleration */
    int16_t y;                     /**< Y-axis acceleration */
    int16_t z;                     /**< Z-axis acceleration */
} adxl345_accel_data_t;

/**
 * @brief ADXL345 SPI configuration
 */
typedef struct {
    spi_host_device_t host_id;     /**< SPI host */
    int cs_gpio;                   /**< Chip select GPIO */
    int sclk_gpio;                 /**< SPI clock GPIO */
    int mosi_gpio;                 /**< SPI MOSI GPIO */
    int miso_gpio;                 /**< SPI MISO GPIO */
    uint32_t clock_speed_hz;       /**< SPI clock frequency */
} adxl345_spi_config_t;

/**
 * @brief ADXL345 I2C configuration
 */
typedef struct {
    i2c_port_t port;               /**< I2C port number */
    int sda_gpio;                  /**< I2C SDA GPIO */
    int scl_gpio;                  /**< I2C SCL GPIO */
    uint32_t clock_speed_hz;       /**< I2C clock frequency */
    uint8_t device_address;        /**< I2C device address */
} adxl345_i2c_config_t;

/**
 * @brief ADXL345 device configuration
 */
typedef struct {
    adxl345_interface_t interface; /**< Communication interface */
    adxl345_range_t range;         /**< Measurement range */
    adxl345_datarate_t datarate;   /**< Data rate */
    bool full_resolution;          /**< Full resolution mode */
    union {
        adxl345_spi_config_t spi;  /**< SPI configuration */
        adxl345_i2c_config_t i2c;  /**< I2C configuration */
    } comm_config;
} adxl345_config_t;

/**
 * @brief ADXL345 device handle
 */
typedef struct adxl345_device_t* adxl345_handle_t;

/**
 * @brief Initialize ADXL345 device
 *
 * @param[in] config Pointer to device configuration
 * @param[out] handle Pointer to store device handle
 * @return
 *      - ESP_OK: Success
 *      - ESP_ERR_INVALID_ARG: Invalid arguments
 *      - ESP_ERR_NO_MEM: Out of memory
 *      - ESP_FAIL: Initialization failed
 */
esp_err_t adxl345_init(const adxl345_config_t *config, adxl345_handle_t *handle);

/**
 * @brief Deinitialize ADXL345 device
 *
 * @param[in] handle Device handle
 * @return
 *      - ESP_OK: Success
 *      - ESP_ERR_INVALID_ARG: Invalid handle
 */
esp_err_t adxl345_deinit(adxl345_handle_t handle);

/**
 * @brief Read device ID from ADXL345
 *
 * @param[in] handle Device handle
 * @param[out] device_id Pointer to store device ID
 * @return
 *      - ESP_OK: Success
 *      - ESP_ERR_INVALID_ARG: Invalid arguments
 *      - ESP_FAIL: Communication failed
 */
esp_err_t adxl345_read_device_id(adxl345_handle_t handle, uint8_t *device_id);

/**
 * @brief Start measurement mode
 *
 * @param[in] handle Device handle
 * @return
 *      - ESP_OK: Success
 *      - ESP_ERR_INVALID_ARG: Invalid handle
 *      - ESP_FAIL: Communication failed
 */
esp_err_t adxl345_start_measurement(adxl345_handle_t handle);

/**
 * @brief Stop measurement mode
 *
 * @param[in] handle Device handle
 * @return
 *      - ESP_OK: Success
 *      - ESP_ERR_INVALID_ARG: Invalid handle
 *      - ESP_FAIL: Communication failed
 */
esp_err_t adxl345_stop_measurement(adxl345_handle_t handle);

/**
 * @brief Read single acceleration sample
 *
 * @param[in] handle Device handle
 * @param[out] data Pointer to store acceleration data
 * @return
 *      - ESP_OK: Success
 *      - ESP_ERR_INVALID_ARG: Invalid arguments
 *      - ESP_FAIL: Communication failed
 */
esp_err_t adxl345_read_accel(adxl345_handle_t handle, adxl345_accel_data_t *data);

/**
 * @brief Read multiple acceleration samples
 *
 * @param[in] handle Device handle
 * @param[out] data Pointer to array to store acceleration data
 * @param[in] count Number of samples to read
 * @param[out] read_count Pointer to store actual number of samples read
 * @return
 *      - ESP_OK: Success
 *      - ESP_ERR_INVALID_ARG: Invalid arguments
 *      - ESP_FAIL: Communication failed
 */
esp_err_t adxl345_read_accel_multiple(adxl345_handle_t handle, 
                                      adxl345_accel_data_t *data, 
                                      size_t count, 
                                      size_t *read_count);

/**
 * @brief Get FIFO sample count
 *
 * @param[in] handle Device handle
 * @param[out] count Pointer to store FIFO sample count
 * @return
 *      - ESP_OK: Success
 *      - ESP_ERR_INVALID_ARG: Invalid arguments
 *      - ESP_FAIL: Communication failed
 */
esp_err_t adxl345_get_fifo_count(adxl345_handle_t handle, uint8_t *count);

/**
 * @brief Configure FIFO mode
 *
 * @param[in] handle Device handle
 * @param[in] mode FIFO mode (bypass, FIFO, stream, trigger)
 * @param[in] samples Number of samples for trigger mode (0-31)
 * @return
 *      - ESP_OK: Success
 *      - ESP_ERR_INVALID_ARG: Invalid arguments
 *      - ESP_FAIL: Communication failed
 */
esp_err_t adxl345_configure_fifo(adxl345_handle_t handle, uint8_t mode, uint8_t samples);

/**
 * @brief Set data rate
 *
 * @param[in] handle Device handle
 * @param[in] datarate Data rate configuration
 * @return
 *      - ESP_OK: Success
 *      - ESP_ERR_INVALID_ARG: Invalid arguments
 *      - ESP_FAIL: Communication failed
 */
esp_err_t adxl345_set_data_rate(adxl345_handle_t handle, adxl345_datarate_t datarate);

/**
 * @brief Set measurement range
 *
 * @param[in] handle Device handle
 * @param[in] range Measurement range
 * @return
 *      - ESP_OK: Success
 *      - ESP_ERR_INVALID_ARG: Invalid arguments
 *      - ESP_FAIL: Communication failed
 */
esp_err_t adxl345_set_range(adxl345_handle_t handle, adxl345_range_t range);

/**
 * @brief Convert raw acceleration data to mg (milli-g)
 *
 * @param[in] raw_data Raw acceleration data
 * @param[in] range Current measurement range
 * @param[in] full_resolution Full resolution mode enabled
 * @return Acceleration value in mg
 */
float adxl345_convert_to_mg(int16_t raw_data, adxl345_range_t range, bool full_resolution);

#ifdef __cplusplus
}
#endif