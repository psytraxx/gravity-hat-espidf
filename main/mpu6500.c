#include "mpu6500.h"

#include <string.h>

#include "esp_check.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "driver/i2c_master.h"
#include "driver/spi_master.h"
#include "sdkconfig.h"

static const char *TAG = "mpu6500";

/* Registers */
#define REG_SMPLRT_DIV    0x19
#define REG_CONFIG        0x1A
#define REG_GYRO_CONFIG   0x1B
#define REG_ACCEL_CONFIG  0x1C
#define REG_ACCEL_CONFIG2 0x1D
#define REG_FIFO_EN       0x23
#define REG_INT_STATUS    0x3A
#define REG_ACCEL_XOUT_H  0x3B
#define REG_SIGNAL_RESET  0x68
#define REG_USER_CTRL     0x6A
#define REG_PWR_MGMT_1    0x6B
#define REG_PWR_MGMT_2    0x6C
#define REG_FIFO_COUNT_H  0x72
#define REG_FIFO_R_W      0x74
#define REG_WHO_AM_I      0x75

#define WHO_AM_I_MPU6500 0x70

#define PWR_RESET       0x80
#define PWR_CLK_AUTO    0x01 /* gyro PLL when ready, else internal oscillator */
#define USER_FIFO_EN    0x40
#define USER_I2C_IF_DIS 0x10 /* keep the I2C interface off the shared pins */
#define USER_FIFO_RST   0x04
#define FIFO_ACCEL_GYRO 0x78 /* gyro X, Y, Z and accel; no temperature */
#define INT_FIFO_OFLOW  0x10

/* FIFO_MODE: stop writing when full, so records stay aligned; a full FIFO
 * is reset instead. DLPF_CFG 2: gyro 92 Hz bandwidth, 1 kHz internal rate. */
#define CONFIG_FIFO_STOP_DLPF_92HZ 0x42
/* 1 kHz / (1 + 3) = 250 Hz. */
#define SMPLRT_DIV_250HZ 3
#define GYRO_1000DPS     0x10
#define ACCEL_4G         0x08
/* Accel low-pass at 99 Hz bandwidth. */
#define ACCEL_DLPF_99HZ 0x02

#define FIFO_SIZE 512

/* Gyro readings averaged at startup to measure its zero-rate offset. */
#define CALIBRATION_SAMPLES 50

static vec3_t s_gyro_bias;

/* The board's turn about the hat's axis, undone on every sample: LED 0's
 * side lies CONFIG_HAT_IMU_YAW_DEG counter-clockwise (seen from above)
 * of the board's X arrow, and every reading turns back by as much. */
#define YAW_RAD ((float)CONFIG_HAT_IMU_YAW_DEG * 3.14159265f / 180.0f)

/* All registers are good for 1 MHz; only the sensor registers take 20. */
#define SPI_HZ 1000000

static spi_device_handle_t s_spi;
/* Command byte plus a FIFO's worth of data. */
static uint8_t s_tx[1 + FIFO_SIZE] __attribute__((aligned(4)));
static uint8_t s_rx[1 + FIFO_SIZE] __attribute__((aligned(4)));

static esp_err_t bus_init(void)
{
    const spi_bus_config_t bus = {
        .mosi_io_num = CONFIG_HAT_IMU_SPI_MOSI,
        .miso_io_num = CONFIG_HAT_IMU_SPI_MISO,
        .sclk_io_num = CONFIG_HAT_IMU_SPI_SCLK,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = sizeof s_tx,
    };
    ESP_RETURN_ON_ERROR(spi_bus_initialize(SPI3_HOST, &bus, SPI_DMA_CH_AUTO), TAG, "spi bus");
    const spi_device_interface_config_t dev = {
        .clock_speed_hz = SPI_HZ,
        .mode = 3,
        .spics_io_num = CONFIG_HAT_IMU_SPI_CS,
        .queue_size = 1,
    };
    return spi_bus_add_device(SPI3_HOST, &dev, &s_spi);
}

static esp_err_t read_regs(uint8_t reg, uint8_t *buf, size_t len)
{
    memset(s_tx, 0, len + 1);
    s_tx[0] = 0x80 | reg;
    spi_transaction_t t = {.length = (len + 1) * 8, .tx_buffer = s_tx, .rx_buffer = s_rx};
    const esp_err_t err = spi_device_polling_transmit(s_spi, &t);
    if (err == ESP_OK) {
        memcpy(buf, s_rx + 1, len);
    }
    return err;
}

static esp_err_t write_reg(uint8_t reg, uint8_t value)
{
    const uint8_t tx[2] = {reg & 0x7F, value};
    spi_transaction_t t = {.length = 16, .tx_buffer = tx};
    return spi_device_polling_transmit(s_spi, &t);
}

/* One accel and gyro reading straight from the data registers, as a FIFO
 * record (temperature dropped). */
static esp_err_t read_direct(imu_sample_t *s)
{
    uint8_t raw[14];
    ESP_RETURN_ON_ERROR(read_regs(REG_ACCEL_XOUT_H, raw, sizeof raw), TAG, "read");
    uint8_t rec[MPU6500_SAMPLE_BYTES];
    memcpy(rec, raw, 6);
    memcpy(rec + 6, raw + 8, 6);
    *s = mpu6500_decode(rec);
    return ESP_OK;
}

static esp_err_t fifo_reset(void)
{
    ESP_RETURN_ON_ERROR(write_reg(REG_USER_CTRL, USER_I2C_IF_DIS | USER_FIFO_RST), TAG, "fifo");
    return write_reg(REG_USER_CTRL, USER_I2C_IF_DIS | USER_FIFO_EN);
}

#if CONFIG_HAT_IMU_I2C_SCAN
/* Log every address that acknowledges, on the SPI pins used as I2C. */
static void i2c_scan(void)
{
    /* High before it is driven, so NCS never dips low on the way. */
    gpio_set_level(CONFIG_HAT_IMU_SPI_CS, 1);
    gpio_set_direction(CONFIG_HAT_IMU_SPI_CS, GPIO_MODE_OUTPUT);

    const i2c_master_bus_config_t cfg = {
        .i2c_port = -1,
        .sda_io_num = CONFIG_HAT_IMU_SPI_MOSI,
        .scl_io_num = CONFIG_HAT_IMU_SPI_SCLK,
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup = true,
    };
    i2c_master_bus_handle_t bus;
    if (i2c_new_master_bus(&cfg, &bus) != ESP_OK) {
        ESP_LOGE(TAG, "I2C scan: bus setup failed");
        return;
    }
    int found = 0;
    for (uint16_t addr = 0x08; addr < 0x78; addr++) {
        if (i2c_master_probe(bus, addr, 20) == ESP_OK) {
            ESP_LOGI(TAG, "I2C scan: device at 0x%02x", addr);
            found++;
        }
    }
    ESP_LOGI(TAG, "I2C scan: %d device(s) on SDA %d, SCL %d", found, CONFIG_HAT_IMU_SPI_MOSI,
             CONFIG_HAT_IMU_SPI_SCLK);
    i2c_del_master_bus(bus);
}
#endif

esp_err_t mpu6500_init(void)
{
#if CONFIG_HAT_IMU_I2C_SCAN
    i2c_scan();
#endif
    ESP_RETURN_ON_ERROR(bus_init(), TAG, "bus");

    ESP_RETURN_ON_ERROR(write_reg(REG_PWR_MGMT_1, PWR_RESET), TAG, "no answer");
    vTaskDelay(pdMS_TO_TICKS(100));
    ESP_RETURN_ON_ERROR(write_reg(REG_USER_CTRL, USER_I2C_IF_DIS), TAG, "user ctrl");
    ESP_RETURN_ON_ERROR(write_reg(REG_SIGNAL_RESET, 0x07), TAG, "signal reset");
    vTaskDelay(pdMS_TO_TICKS(100));

    uint8_t who = 0;
    ESP_RETURN_ON_ERROR(read_regs(REG_WHO_AM_I, &who, 1), TAG, "who am i");
    if (who == 0x00 || who == 0xFF) {
        ESP_LOGE(TAG, "WHO_AM_I 0x%02X: nothing on the bus", who);
        return ESP_ERR_NOT_FOUND;
    }
    if (who != WHO_AM_I_MPU6500) {
        /* MPU-9250 (0x71) and many clones share the register map. */
        ESP_LOGW(TAG, "WHO_AM_I 0x%02X, expected 0x%02X; carrying on", who, WHO_AM_I_MPU6500);
    }

    ESP_RETURN_ON_ERROR(write_reg(REG_PWR_MGMT_1, PWR_CLK_AUTO), TAG, "pwr");
    ESP_RETURN_ON_ERROR(write_reg(REG_PWR_MGMT_2, 0x00), TAG, "pwr2");
    ESP_RETURN_ON_ERROR(write_reg(REG_CONFIG, CONFIG_FIFO_STOP_DLPF_92HZ), TAG, "config");
    ESP_RETURN_ON_ERROR(write_reg(REG_SMPLRT_DIV, SMPLRT_DIV_250HZ), TAG, "rate");
    ESP_RETURN_ON_ERROR(write_reg(REG_GYRO_CONFIG, GYRO_1000DPS), TAG, "gyro");
    ESP_RETURN_ON_ERROR(write_reg(REG_ACCEL_CONFIG, ACCEL_4G), TAG, "accel");
    ESP_RETURN_ON_ERROR(write_reg(REG_ACCEL_CONFIG2, ACCEL_DLPF_99HZ), TAG, "accel2");
    vTaskDelay(pdMS_TO_TICKS(50));

    vec3_t sum = v3(0.0f, 0.0f, 0.0f);
    imu_sample_t s;
    for (int i = 0; i < CALIBRATION_SAMPLES; i++) {
        ESP_RETURN_ON_ERROR(read_direct(&s), TAG, "calibrate");
        sum = v3_add(sum, s.gyro);
        vTaskDelay(pdMS_TO_TICKS(5));
    }
    s_gyro_bias = v3_scale(sum, 1.0f / CALIBRATION_SAMPLES);
    ESP_LOGI(TAG,
             "WHO_AM_I 0x%02X, accel (%.2f, %.2f, %.2f) g, gyro offset (%.3f, %.3f, %.3f) rad/s",
             who, s.accel.x, s.accel.y, s.accel.z, s_gyro_bias.x, s_gyro_bias.y, s_gyro_bias.z);

    ESP_RETURN_ON_ERROR(write_reg(REG_FIFO_EN, FIFO_ACCEL_GYRO), TAG, "fifo en");
    return fifo_reset();
}

int mpu6500_read(imu_sample_t *out, int max)
{
    static uint8_t buf[FIFO_SIZE];
    uint8_t cnt[2], status;
    if (read_regs(REG_INT_STATUS, &status, 1) != ESP_OK ||
        read_regs(REG_FIFO_COUNT_H, cnt, 2) != ESP_OK) {
        return -1;
    }
    const int count = (cnt[0] & 0x1F) << 8 | cnt[1];
    if ((status & INT_FIFO_OFLOW) || count > FIFO_SIZE - MPU6500_SAMPLE_BYTES) {
        ESP_LOGW(TAG, "FIFO overflow, reset");
        return fifo_reset() == ESP_OK ? 0 : -1;
    }

    int n = count / MPU6500_SAMPLE_BYTES;
    if (n > max) {
        n = max;
    }
    if (n == 0) {
        return 0;
    }
    if (read_regs(REG_FIFO_R_W, buf, (size_t)n * MPU6500_SAMPLE_BYTES) != ESP_OK) {
        return -1;
    }
    for (int i = 0; i < n; i++) {
        const imu_sample_t s = mpu6500_decode(&buf[i * MPU6500_SAMPLE_BYTES]);
        const vec3_t undo_yaw = v3(0.0f, 0.0f, -YAW_RAD);
        out[i].accel = v3_rotate(s.accel, undo_yaw);
        out[i].gyro = v3_rotate(v3_sub(s.gyro, s_gyro_bias), undo_yaw);
    }
    return n;
}
