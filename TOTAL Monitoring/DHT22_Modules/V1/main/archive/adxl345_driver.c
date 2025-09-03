/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "adxl345_driver.h"
#include "esp_log.h"
#include "esp_check.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>

static const char *TAG = "ADXL345_DRIVER";

/* Internal device structure */
struct adxl345_device_t {
    adxl345_interface_t interface;
    adxl345_range_t range;
    bool full_resolution;
    union {
        struct {
            spi_device_handle_t handle;
            spi_host_device_t host_id;
        } spi;
        struct {
            i2c_port_t port;
            uint8_t device_address;
        } i2c;
    } comm;
};

/* SPI communication functions */
static esp_err_t adxl345_spi_write_reg(adxl345_handle_t handle, uint8_t reg, uint8_t data)
{
    if (handle == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    spi_transaction_t trans = {
        .length = 16,                           /* 2 bytes: register + data */
        .tx_data = {reg, data},
        .flags = SPI_TRANS_USE_TXDATA
    };

    esp_err_t ret = spi_device_transmit(handle->comm.spi.handle, &trans);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "SPI write failed: %s", esp_err_to_name(ret));
    }

    return ret;
}

static esp_err_t adxl345_spi_read_reg(adxl345_handle_t handle, uint8_t reg, uint8_t *data)
{
    if (handle == NULL || data == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    spi_transaction_t trans = {
        .length = 16,                           /* 2 bytes: register + dummy */
        .rxlength = 8,                          /* 1 byte response */
        .tx_data = {reg | ADXL345_SPI_READ_BIT, 0x00},
        .flags = SPI_TRANS_USE_TXDATA | SPI_TRANS_USE_RXDATA
    };

    esp_err_t ret = spi_device_transmit(handle->comm.spi.handle, &trans);
    if (ret == ESP_OK) {
        *data = trans.rx_data[1];
    } else {
        ESP_LOGE(TAG, "SPI read failed: %s", esp_err_to_name(ret));
    }

    return ret;
}

static esp_err_t adxl345_spi_read_multiple(adxl345_handle_t handle, uint8_t reg, 
                                          uint8_t *data, size_t len)
{
    if (handle == NULL || data == NULL || len == 0U) {
        return ESP_ERR_INVALID_ARG;
    }

    uint8_t *tx_buffer = calloc(len + 1U, sizeof(uint8_t));
    uint8_t *rx_buffer = calloc(len + 1U, sizeof(uint8_t));
    
    if (tx_buffer == NULL || rx_buffer == NULL) {
        free(tx_buffer);
        free(rx_buffer);
        return ESP_ERR_NO_MEM;
    }

    tx_buffer[0] = reg | ADXL345_SPI_READ_BIT | ADXL345_SPI_MULTI_BYTE;

    spi_transaction_t trans = {
        .length = (len + 1U) * 8U,
        .tx_buffer = tx_buffer,
        .rx_buffer = rx_buffer
    };

    esp_err_t ret = spi_device_transmit(handle->comm.spi.handle, &trans);
    if (ret == ESP_OK) {
        memcpy(data, &rx_buffer[1], len);
    } else {
        ESP_LOGE(TAG, "SPI multi-read failed: %s", esp_err_to_name(ret));
    }

    free(tx_buffer);
    free(rx_buffer);
    return ret;
}

/* I2C communication functions */
static esp_err_t adxl345_i2c_write_reg(adxl345_handle_t handle, uint8_t reg, uint8_t data)
{
    if (handle == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    if (cmd == NULL) {
        return ESP_ERR_NO_MEM;
    }

    esp_err_t ret = ESP_OK;
    
    ESP_GOTO_ON_ERROR(i2c_master_start(cmd), cleanup, TAG, "I2C start failed");
    ESP_GOTO_ON_ERROR(i2c_master_write_byte(cmd, (handle->comm.i2c.device_address << 1) | I2C_MASTER_WRITE, true), 
                     cleanup, TAG, "I2C write address failed");
    ESP_GOTO_ON_ERROR(i2c_master_write_byte(cmd, reg, true), 
                     cleanup, TAG, "I2C write register failed");
    ESP_GOTO_ON_ERROR(i2c_master_write_byte(cmd, data, true), 
                     cleanup, TAG, "I2C write data failed");
    ESP_GOTO_ON_ERROR(i2c_master_stop(cmd), cleanup, TAG, "I2C stop failed");
    
    ret = i2c_master_cmd_begin(handle->comm.i2c.port, cmd, pdMS_TO_TICKS(100));

cleanup:
    i2c_cmd_link_delete(cmd);
    return ret;
}

static esp_err_t adxl345_i2c_read_reg(adxl345_handle_t handle, uint8_t reg, uint8_t *data)
{
    if (handle == NULL || data == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    if (cmd == NULL) {
        return ESP_ERR_NO_MEM;
    }

    esp_err_t ret = ESP_OK;
    
    /* Write register address */
    ESP_GOTO_ON_ERROR(i2c_master_start(cmd), cleanup, TAG, "I2C start failed");
    ESP_GOTO_ON_ERROR(i2c_master_write_byte(cmd, (handle->comm.i2c.device_address << 1) | I2C_MASTER_WRITE, true), 
                     cleanup, TAG, "I2C write address failed");
    ESP_GOTO_ON_ERROR(i2c_master_write_byte(cmd, reg, true), 
                     cleanup, TAG, "I2C write register failed");
    
    /* Read data */
    ESP_GOTO_ON_ERROR(i2c_master_start(cmd), cleanup, TAG, "I2C restart failed");
    ESP_GOTO_ON_ERROR(i2c_master_write_byte(cmd, (handle->comm.i2c.device_address << 1) | I2C_MASTER_READ, true), 
                     cleanup, TAG, "I2C read address failed");
    ESP_GOTO_ON_ERROR(i2c_master_read_byte(cmd, data, I2C_MASTER_NACK), 
                     cleanup, TAG, "I2C read data failed");
    ESP_GOTO_ON_ERROR(i2c_master_stop(cmd), cleanup, TAG, "I2C stop failed");
    
    ret = i2c_master_cmd_begin(handle->comm.i2c.port, cmd, pdMS_TO_TICKS(100));

cleanup:
    i2c_cmd_link_delete(cmd);
    return ret;
}

static esp_err_t adxl345_i2c_read_multiple(adxl345_handle_t handle, uint8_t reg, 
                                          uint8_t *data, size_t len)
{
    if (handle == NULL || data == NULL || len == 0U) {
        return ESP_ERR_INVALID_ARG;
    }

    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    if (cmd == NULL) {
        return ESP_ERR_NO_MEM;
    }

    esp_err_t ret = ESP_OK;
    
    /* Write register address */
    ESP_GOTO_ON_ERROR(i2c_master_start(cmd), cleanup, TAG, "I2C start failed");
    ESP_GOTO_ON_ERROR(i2c_master_write_byte(cmd, (handle->comm.i2c.device_address << 1) | I2C_MASTER_WRITE, true), 
                     cleanup, TAG, "I2C write address failed");
    ESP_GOTO_ON_ERROR(i2c_master_write_byte(cmd, reg, true), 
                     cleanup, TAG, "I2C write register failed");
    
    /* Read data */
    ESP_GOTO_ON_ERROR(i2c_master_start(cmd), cleanup, TAG, "I2C restart failed");
    ESP_GOTO_ON_ERROR(i2c_master_write_byte(cmd, (handle->comm.i2c.device_address << 1) | I2C_MASTER_READ, true), 
                     cleanup, TAG, "I2C read address failed");
    
    for (size_t i = 0U; i < len; i++) {
        i2c_ack_type_t ack = (i == (len - 1U)) ? I2C_MASTER_NACK : I2C_MASTER_ACK;
        ESP_GOTO_ON_ERROR(i2c_master_read_byte(cmd, &data[i], ack), 
                         cleanup, TAG, "I2C read data failed");
    }
    
    ESP_GOTO_ON_ERROR(i2c_master_stop(cmd), cleanup, TAG, "I2C stop failed");
    
    ret = i2c_master_cmd_begin(handle->comm.i2c.port, cmd, pdMS_TO_TICKS(100));

cleanup:
    i2c_cmd_link_delete(cmd);
    return ret;
}

/* Generic register access functions */
static esp_err_t adxl345_write_reg(adxl345_handle_t handle, uint8_t reg, uint8_t data)
{
    if (handle == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    if (handle->interface == ADXL345_INTERFACE_SPI) {
        return adxl345_spi_write_reg(handle, reg, data);
    } else {
        return adxl345_i2c_write_reg(handle, reg, data);
    }
}

static esp_err_t adxl345_read_reg(adxl345_handle_t handle, uint8_t reg, uint8_t *data)
{
    if (handle == NULL || data == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    if (handle->interface == ADXL345_INTERFACE_SPI) {
        return adxl345_spi_read_reg(handle, reg, data);
    } else {
        return adxl345_i2c_read_reg(handle, reg, data);
    }
}

static esp_err_t adxl345_read_multiple(adxl345_handle_t handle, uint8_t reg, 
                                      uint8_t *data, size_t len)
{
    if (handle == NULL || data == NULL || len == 0U) {
        return ESP_ERR_INVALID_ARG;
    }

    if (handle->interface == ADXL345_INTERFACE_SPI) {
        return adxl345_spi_read_multiple(handle, reg, data, len);
    } else {
        return adxl345_i2c_read_multiple(handle, reg, data, len);
    }
}

/* Public API implementation */
esp_err_t adxl345_init(const adxl345_config_t *config, adxl345_handle_t *handle)
{
    ESP_RETURN_ON_FALSE(config != NULL, ESP_ERR_INVALID_ARG, TAG, "config is NULL");
    ESP_RETURN_ON_FALSE(handle != NULL, ESP_ERR_INVALID_ARG, TAG, "handle is NULL");

    /* Allocate device structure */
    adxl345_handle_t dev = calloc(1, sizeof(struct adxl345_device_t));
    ESP_RETURN_ON_FALSE(dev != NULL, ESP_ERR_NO_MEM, TAG, "Failed to allocate device structure");

    dev->interface = config->interface;
    dev->range = config->range;
    dev->full_resolution = config->full_resolution;

    esp_err_t ret = ESP_OK;

    /* Initialize communication interface */
    if (config->interface == ADXL345_INTERFACE_SPI) {
        /* Configure SPI bus */
        spi_bus_config_t bus_cfg = {
            .miso_io_num = config->comm_config.spi.miso_gpio,
            .mosi_io_num = config->comm_config.spi.mosi_gpio,
            .sclk_io_num = config->comm_config.spi.sclk_gpio,
            .quadwp_io_num = -1,
            .quadhd_io_num = -1,
            .max_transfer_sz = 32
        };

        ret = spi_bus_initialize(config->comm_config.spi.host_id, &bus_cfg, SPI_DMA_CH_AUTO);
        if (ret != ESP_OK && ret != ESP_ERR_INVALID_STATE) {
            ESP_LOGE(TAG, "Failed to initialize SPI bus: %s", esp_err_to_name(ret));
            free(dev);
            return ret;
        }

        /* Configure SPI device */
        spi_device_interface_config_t dev_cfg = {
            .command_bits = 0,
            .address_bits = 0,
            .dummy_bits = 0,
            .mode = 3,                                          /* CPOL=1, CPHA=1 */
            .duty_cycle_pos = 0,
            .cs_ena_pretrans = 0,
            .cs_ena_posttrans = 0,
            .clock_speed_hz = config->comm_config.spi.clock_speed_hz,
            .spics_io_num = config->comm_config.spi.cs_gpio,
            .flags = 0,
            .queue_size = 1,
            .pre_cb = NULL,
            .post_cb = NULL
        };

        ret = spi_bus_add_device(config->comm_config.spi.host_id, &dev_cfg, &dev->comm.spi.handle);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Failed to add SPI device: %s", esp_err_to_name(ret));
            free(dev);
            return ret;
        }

        dev->comm.spi.host_id = config->comm_config.spi.host_id;

    } else {
        /* Configure I2C */
        i2c_config_t i2c_cfg = {
            .mode = I2C_MODE_MASTER,
            .sda_io_num = config->comm_config.i2c.sda_gpio,
            .scl_io_num = config->comm_config.i2c.scl_gpio,
            .sda_pullup_en = GPIO_PULLUP_ENABLE,
            .scl_pullup_en = GPIO_PULLUP_ENABLE,
            .master.clk_speed = config->comm_config.i2c.clock_speed_hz
        };

        ret = i2c_param_config(config->comm_config.i2c.port, &i2c_cfg);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Failed to configure I2C: %s", esp_err_to_name(ret));
            free(dev);
            return ret;
        }

        ret = i2c_driver_install(config->comm_config.i2c.port, I2C_MODE_MASTER, 0, 0, 0);
        if (ret != ESP_OK && ret != ESP_ERR_INVALID_STATE) {
            ESP_LOGE(TAG, "Failed to install I2C driver: %s", esp_err_to_name(ret));
            free(dev);
            return ret;
        }

        dev->comm.i2c.port = config->comm_config.i2c.port;
        dev->comm.i2c.device_address = config->comm_config.i2c.device_address;
    }

    /* Configure device registers */
    
    /* Set data format */
    uint8_t data_format = 0;
    if (config->full_resolution) {
        data_format |= ADXL345_DATA_FORMAT_FULL_RES;
    }
    data_format |= (uint8_t)config->range;

    ret = adxl345_write_reg(dev, ADXL345_REG_DATA_FORMAT, data_format);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to set data format: %s", esp_err_to_name(ret));
        adxl345_deinit(dev);
        return ret;
    }

    /* Set data rate */
    ret = adxl345_write_reg(dev, ADXL345_REG_BW_RATE, (uint8_t)config->datarate);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to set data rate: %s", esp_err_to_name(ret));
        adxl345_deinit(dev);
        return ret;
    }

    *handle = dev;
    ESP_LOGI(TAG, "ADXL345 initialized successfully");
    return ESP_OK;
}

esp_err_t adxl345_deinit(adxl345_handle_t handle)
{
    ESP_RETURN_ON_FALSE(handle != NULL, ESP_ERR_INVALID_ARG, TAG, "handle is NULL");

    /* Stop measurement if active */
    adxl345_stop_measurement(handle);

    /* Cleanup communication interface */
    if (handle->interface == ADXL345_INTERFACE_SPI) {
        spi_bus_remove_device(handle->comm.spi.handle);
        /* Note: We don't free the SPI bus as it might be used by other devices */
    } else {
        /* Note: We don't uninstall I2C driver as it might be used by other devices */
    }

    free(handle);
    ESP_LOGI(TAG, "ADXL345 deinitialized");
    return ESP_OK;
}

esp_err_t adxl345_read_device_id(adxl345_handle_t handle, uint8_t *device_id)
{
    ESP_RETURN_ON_FALSE(handle != NULL, ESP_ERR_INVALID_ARG, TAG, "handle is NULL");
    ESP_RETURN_ON_FALSE(device_id != NULL, ESP_ERR_INVALID_ARG, TAG, "device_id is NULL");

    return adxl345_read_reg(handle, ADXL345_REG_DEVID, device_id);
}

esp_err_t adxl345_start_measurement(adxl345_handle_t handle)
{
    ESP_RETURN_ON_FALSE(handle != NULL, ESP_ERR_INVALID_ARG, TAG, "handle is NULL");

    return adxl345_write_reg(handle, ADXL345_REG_POWER_CTL, ADXL345_POWER_CTL_MEASURE);
}

esp_err_t adxl345_stop_measurement(adxl345_handle_t handle)
{
    ESP_RETURN_ON_FALSE(handle != NULL, ESP_ERR_INVALID_ARG, TAG, "handle is NULL");

    return adxl345_write_reg(handle, ADXL345_REG_POWER_CTL, 0);
}

esp_err_t adxl345_read_accel(adxl345_handle_t handle, adxl345_accel_data_t *data)
{
    ESP_RETURN_ON_FALSE(handle != NULL, ESP_ERR_INVALID_ARG, TAG, "handle is NULL");
    ESP_RETURN_ON_FALSE(data != NULL, ESP_ERR_INVALID_ARG, TAG, "data is NULL");

    uint8_t raw_data[6];
    esp_err_t ret = adxl345_read_multiple(handle, ADXL345_REG_DATAX0, raw_data, 6);
    if (ret != ESP_OK) {
        return ret;
    }

    /* Convert raw data to signed 16-bit values */
    data->x = (int16_t)((uint16_t)raw_data[1] << 8U | raw_data[0]);
    data->y = (int16_t)((uint16_t)raw_data[3] << 8U | raw_data[2]);
    data->z = (int16_t)((uint16_t)raw_data[5] << 8U | raw_data[4]);

    return ESP_OK;
}

esp_err_t adxl345_read_accel_multiple(adxl345_handle_t handle, 
                                      adxl345_accel_data_t *data, 
                                      size_t count, 
                                      size_t *read_count)
{
    ESP_RETURN_ON_FALSE(handle != NULL, ESP_ERR_INVALID_ARG, TAG, "handle is NULL");
    ESP_RETURN_ON_FALSE(data != NULL, ESP_ERR_INVALID_ARG, TAG, "data is NULL");
    ESP_RETURN_ON_FALSE(read_count != NULL, ESP_ERR_INVALID_ARG, TAG, "read_count is NULL");
    ESP_RETURN_ON_FALSE(count > 0U, ESP_ERR_INVALID_ARG, TAG, "count must be > 0");

    *read_count = 0;

    /* Check FIFO count first */
    uint8_t fifo_count = 0;
    esp_err_t ret = adxl345_get_fifo_count(handle, &fifo_count);
    if (ret != ESP_OK) {
        /* If FIFO is not available, read single sample */
        if (count >= 1U) {
            ret = adxl345_read_accel(handle, &data[0]);
            if (ret == ESP_OK) {
                *read_count = 1;
            }
        }
        return ret;
    }

    /* Read available samples from FIFO */
    size_t samples_to_read = (size_t)fifo_count;
    if (samples_to_read > count) {
        samples_to_read = count;
    }

    for (size_t i = 0; i < samples_to_read; i++) {
        ret = adxl345_read_accel(handle, &data[i]);
        if (ret != ESP_OK) {
            break;
        }
        (*read_count)++;
    }

    return ret;
}

esp_err_t adxl345_get_fifo_count(adxl345_handle_t handle, uint8_t *count)
{
    ESP_RETURN_ON_FALSE(handle != NULL, ESP_ERR_INVALID_ARG, TAG, "handle is NULL");
    ESP_RETURN_ON_FALSE(count != NULL, ESP_ERR_INVALID_ARG, TAG, "count is NULL");

    uint8_t fifo_status = 0;
    esp_err_t ret = adxl345_read_reg(handle, ADXL345_REG_FIFO_STATUS, &fifo_status);
    if (ret == ESP_OK) {
        *count = fifo_status & 0x3FU;  /* Lower 6 bits contain the count */
    }

    return ret;
}

esp_err_t adxl345_configure_fifo(adxl345_handle_t handle, uint8_t mode, uint8_t samples)
{
    ESP_RETURN_ON_FALSE(handle != NULL, ESP_ERR_INVALID_ARG, TAG, "handle is NULL");
    ESP_RETURN_ON_FALSE(samples <= 31U, ESP_ERR_INVALID_ARG, TAG, "samples must be <= 31");

    uint8_t fifo_ctl = mode | (samples & 0x1FU);
    return adxl345_write_reg(handle, ADXL345_REG_FIFO_CTL, fifo_ctl);
}

esp_err_t adxl345_set_data_rate(adxl345_handle_t handle, adxl345_datarate_t datarate)
{
    ESP_RETURN_ON_FALSE(handle != NULL, ESP_ERR_INVALID_ARG, TAG, "handle is NULL");

    return adxl345_write_reg(handle, ADXL345_REG_BW_RATE, (uint8_t)datarate);
}

esp_err_t adxl345_set_range(adxl345_handle_t handle, adxl345_range_t range)
{
    ESP_RETURN_ON_FALSE(handle != NULL, ESP_ERR_INVALID_ARG, TAG, "handle is NULL");

    /* Read current data format */
    uint8_t data_format = 0;
    esp_err_t ret = adxl345_read_reg(handle, ADXL345_REG_DATA_FORMAT, &data_format);
    if (ret != ESP_OK) {
        return ret;
    }

    /* Update range bits only */
    data_format = (data_format & 0xFCU) | (uint8_t)range;
    ret = adxl345_write_reg(handle, ADXL345_REG_DATA_FORMAT, data_format);
    if (ret == ESP_OK) {
        handle->range = range;
    }

    return ret;
}

float adxl345_convert_to_mg(int16_t raw_data, adxl345_range_t range, bool full_resolution)
{
    float scale_factor;

    if (full_resolution) {
        /* In full resolution mode, sensitivity is always 4 mg/LSB */
        scale_factor = 4.0f;
    } else {
        /* In 10-bit mode, sensitivity depends on range */
        switch (range) {
            case ADXL345_RANGE_2G:
                scale_factor = (float)ADXL345_SCALE_FACTOR_2G / 1000.0f;  /* Convert to mg */
                break;
            case ADXL345_RANGE_4G:
                scale_factor = (float)ADXL345_SCALE_FACTOR_4G / 1000.0f;
                break;
            case ADXL345_RANGE_8G:
                scale_factor = (float)ADXL345_SCALE_FACTOR_8G / 1000.0f;
                break;
            case ADXL345_RANGE_16G:
                scale_factor = (float)ADXL345_SCALE_FACTOR_16G / 1000.0f;
                break;
            default:
                scale_factor = 4.0f;  /* Default to full resolution scale */
                break;
        }
    }

    return (float)raw_data * scale_factor;
}