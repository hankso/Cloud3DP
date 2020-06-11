/* 
 * File: drivers.cpp
 * Authors: Hank <hankso1106@gmail.com>
 * Create: 2020-05-06 19:54:28
 */

#include "drivers.h"

#include "esp_log.h"
#include "esp_attr.h"
#include "esp_vfs_dev.h"
#include "esp_intr_alloc.h"
#include "soc/soc.h"
#include "sys/param.h"
#include "driver/rmt.h"
#include "driver/i2c.h"
#include "driver/gpio.h"
#include "driver/uart.h"
#include "driver/spi_common.h"
#include "driver/spi_master.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define RMT_CLK_DIV 2       // TICK = 1 / (80MHz / RMT_CLK_DIV) = 25ns
#define RMT_LED_0H  16      // 400ns / 25ns
#define RMT_LED_0L  34      // 850ns / 25ns
#define RMT_LED_1H  32      // 800ns / 25ns
#define RMT_LED_1L  18      // 450ns / 25ns
#define RMT_LED_RST 2000    // 50us / 25ns

static const char *NAME = "Drivers";

static uint8_t led_number = 0;
static uint8_t led_indexs[NUM_LED_MAX]; // index array used by RMT translation
static led_code_t *led_list;

// mapping RGB value to WS2812B code transfer time
void IRAM_ATTR rmt_convert_code(const void* src, rmt_item32_t* dest,
                                size_t s_src, size_t n_block,
                                size_t *s_trans, size_t *n_items) {
    if (src == NULL || dest == NULL || led_list == NULL) {
        *s_trans = *n_items = 0;
        return;
    }

    static const rmt_item32_t c0 = {{{ RMT_LED_0H, 1, RMT_LED_0L, 0 }}};
    static const rmt_item32_t c1 = {{{ RMT_LED_1H, 1, RMT_LED_1L, 0 }}};
    size_t trans = 0, items = 0;

    while (trans < s_src && items < n_block) {
        led_code_t led = led_list[*((uint8_t *)src + trans++)];
        uint32_t color = led.disable ? 0 : led.color;
        for (uint8_t i = 24; i > 0; i--) {
            (dest + items++)->val = bitRead(color, i - 1) ? c1.val : c0.val;
        }
    }
    *s_trans = trans;
    *n_items = items;
}

void rmt_initialize() {
    rmt_config_t rmt_conf = {
        .rmt_mode = RMT_MODE_TX,
        .channel = NUM_RMT,
        .clk_div = RMT_CLK_DIV,
        .gpio_num = PIN_LED,
        .mem_block_num = 8,
    };
    rmt_conf.tx_config = {
        .loop_en = false,
        .carrier_freq_hz = 0,
        .carrier_duty_percent = 0,
        .carrier_level = RMT_CARRIER_LEVEL_LOW,
        .carrier_en = false,
        .idle_level = RMT_IDLE_LEVEL_LOW,
        .idle_output_en = true,
    };
    ESP_ERROR_CHECK( rmt_config(&rmt_conf) );
    ESP_ERROR_CHECK( rmt_driver_install(NUM_RMT, 0, 0) );
    ESP_ERROR_CHECK( rmt_translator_init(NUM_RMT, rmt_convert_code) );
    for (uint8_t i = 0; i < NUM_LED_MAX; i++) led_indexs[i] = i;
    assert(led_count(NUM_LED) && "Failed to allocate space for LED values");
}

static esp_err_t led_flush(uint8_t maxidx = -2) {
    size_t src_size = MIN(maxidx + 1, led_number);
    return rmt_write_sample(NUM_RMT, led_indexs, src_size, false);
}

static led_code_t * led_get_code(uint8_t idx) {
    return (led_list != NULL && idx < led_number) ? &led_list[idx] : NULL;
}

void led_color(uint8_t idx, uint32_t color) {
    led_color(idx, (color >> 16) & 0xff, (color >> 8) & 0xff, color & 0xff);
}

void led_color(uint8_t idx, uint8_t r, uint8_t g, uint8_t b) {
    led_code_t *led = led_get_code(idx); if (!led) return;
    led->r = r; led->g = g; led->b = b;
    led_flush(idx);
}

uint32_t led_color(uint8_t idx) {
    led_code_t *led = led_get_code(idx); if (!led) return 0;
    return (led->r << 16) | (led->g << 8) | led->b; // led->color order: G-R-B
}

void led_on(uint8_t idx) {
    led_code_t *led = led_get_code(idx); if (!led) return;
    led->disable = false;
    if (!led->color) led->color = 0xffffff;
    led_flush(idx);
}

void led_off(uint8_t idx) {
    led_code_t *led = led_get_code(idx); if (!led) return;
    led->disable = true;
    led_flush(idx);
}

bool led_status(uint8_t idx) {
    led_code_t *led = led_get_code(idx);
    return led ? !led->disable : false;
}

void led_blink(uint8_t idx, uint32_t ms, uint8_t n) {
    while (n--) {
        led_on(idx);  vTaskDelay(pdMS_TO_TICKS(ms));
        led_off(idx); vTaskDelay(pdMS_TO_TICKS(ms));
    }
}

uint8_t led_count() { return led_number; }

bool led_count(uint8_t n) {
    if (n > NUM_LED_MAX) return false;
    if (n == led_number) return true;
    if (led_list == NULL) {
        led_list = (led_code_t *)calloc(n, sizeof(led_code_t));
        if (led_list == NULL) return false;
    } else {
        led_code_t *tmp = (led_code_t *)realloc(led_list, n*sizeof(led_code_t));
        if (tmp == NULL) return false;
        if (n > led_number) {
            size_t s = sizeof(led_code_t), extra = (n - led_number) * s;
            memset(tmp + led_number * s, 0, extra); // set newly allocated to 0
        }
        led_list = tmp;
    }
    led_number = n;
    return true;
}

// I2C GPIO Expander

static uint8_t i2c_pin_data[3] = { 0, 0, 0 };
static uint8_t i2c_pin_addr[3] = { 0b0100000, 0b0100001, 0b0100010 };

void i2c_initialize() {
    i2c_config_t master_conf = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = PIN_SDA,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_io_num = PIN_SCL,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
    };
    master_conf.master.clk_speed = 50 * 1000;
    ESP_ERROR_CHECK( i2c_param_config(NUM_I2C, &master_conf) );
    ESP_ERROR_CHECK( i2c_driver_install(NUM_I2C, master_conf.mode, 0, 0, 0) );
}

esp_err_t i2c_master_transfer(
    uint8_t addr, uint8_t rw, uint8_t *data,
    size_t size = 1, uint16_t timeout = 50)
{
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (addr << 1) | rw, true);
    if (rw == I2C_MASTER_WRITE) {
        if (size > 1) {
            i2c_master_write(cmd, data, size, true);
        } else if (size && data != NULL) {
            i2c_master_write_byte(cmd, *data, true);
        }
    } else if (rw == I2C_MASTER_READ) {
        if (size > 1) {
            i2c_master_read(cmd, data, size, I2C_MASTER_LAST_NACK);
        } else if (size && data != NULL) {
            i2c_master_read_byte(cmd, data, I2C_MASTER_NACK);
        }
    }
    i2c_master_stop(cmd);
    esp_err_t err = i2c_master_cmd_begin(NUM_I2C, cmd, pdMS_TO_TICKS(timeout));
    i2c_cmd_link_delete(cmd);
    return err;
}

esp_err_t i2c_gpio_set_level(i2c_pin_num_t pin_num, bool level) {
    uint8_t
        pin = pin_num - PIN_I2C_MIN - 1,
        idx = pin >> 3, bit = pin & 0x7,
        addr = i2c_pin_addr[idx],
        *data = i2c_pin_data + idx;
    bitWrite(*data, bit, level);
    return i2c_master_transfer(addr, I2C_MASTER_WRITE, data);
}

uint8_t i2c_gpio_get_level(i2c_pin_num_t pin_num, bool sync) {
    uint8_t
        pin = pin_num - PIN_I2C_MIN - 1,
        idx = pin >> 3, bit = pin & 0x7,
        *data = i2c_pin_data + idx;
    if (sync) i2c_master_transfer(i2c_pin_addr[idx], I2C_MASTER_READ, data);
    return bitRead(*data, bit);
}

void i2c_detect() {
    for (uint8_t i = 0; i < 0x10; i++) {
        if (!i) printf("  ");
        printf("  %c", i < 10 ? (i + '0') : (i - 10 + 'A'));
    }
    for (uint8_t addr = 0; addr < 0x80; addr++) {
        if (addr % 0x10 == 0) printf("\n%02X", addr);
        esp_err_t ret = i2c_master_transfer(addr, I2C_MASTER_WRITE, NULL, 0);
        switch (ret) {
        case ESP_OK:
            printf(" %02X", addr); break;
        case ESP_ERR_TIMEOUT:
            printf(" UU"); break;
        default:
            printf(" --");
        }
    }
}

static void IRAM_ATTR gpio_isr_endstop(void *arg) {
    i2c_master_transfer(i2c_pin_addr[0], I2C_MASTER_READ, i2c_pin_data + 0);
    static char buf[9]; itoa(i2c_pin_data[0], buf, 2);
    printf("Endstops value: 0b%s", buf);
}

void gpio_initialize() {
    gpio_config_t inp_conf = {
        .pin_bit_mask = BIT64(PIN_INT),
        .mode         = GPIO_MODE_INPUT,
        .pull_up_en   = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type    = GPIO_INTR_NEGEDGE,
    };
    ESP_ERROR_CHECK( gpio_config(&inp_conf) );
    ESP_ERROR_CHECK( gpio_install_isr_service(0) );
    ESP_ERROR_CHECK( gpio_isr_handler_add(PIN_INT, gpio_isr_endstop, NULL) );
}

// If transmitted data is 32bits or less, spi_transaction_t can use tx_data.
// Here we have no more than 4 chips, thus SPI_TRANS_USE_TXDATA.
static spi_device_handle_t spi_pin_hdlr;
static spi_transaction_t spi_pin_trans;
static uint8_t spi_pin_data[2] = { 0, 0 };

void spi_initialize() {
    spi_bus_config_t hspi_busconf = {
        .mosi_io_num = PIN_HMOSI,
        .miso_io_num = PIN_HMISO,
        .sclk_io_num = PIN_HSCLK,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = 0,
        .flags = SPICOMMON_BUSFLAG_MASTER,
        .intr_flags = 0
    };
    spi_device_interface_config_t devconf = {
        .command_bits = 0,
        .address_bits = 0,
        .dummy_bits = 0,
        .mode = 0b10, // CPOL = 1, CPHA = 0
        .duty_cycle_pos = 128, // 128/255 = 50% (Tlow equals to Thigh)
        .cs_ena_pretrans = 0,
        .cs_ena_posttrans = 0,
        .clock_speed_hz = 5 * 1000 * 1000, // 5MHz
        .input_delay_ns = 0,
        .spics_io_num = PIN_HCS1,
        .flags = 0,
        .queue_size = 1, // only one transaction exists in the queue
        .pre_cb = NULL,
        .post_cb = NULL
    };
    esp_err_t err = spi_bus_initialize(HSPI_HOST, &hspi_busconf, 1);
    assert((!err || err == ESP_ERR_INVALID_STATE) && "SPI init failed");
    ESP_ERROR_CHECK( spi_bus_add_device(HSPI_HOST, &devconf, &spi_pin_hdlr) );
    uint8_t spi_pin_data_len = sizeof(spi_pin_data) / sizeof(spi_pin_data[0]);
    spi_pin_trans.length = spi_pin_data_len * 8; // in bits;
    // if (spi_pin_data_len <= 4) {
    //     spi_pin_data = spi_pin_trans.tx_data;
    //     for (uint16_t i = 0; i < spi_pin_data_len; i++) spi_pin_data[i] = 0;
    //     spi_pin_trans.flags = SPI_TRANS_USE_TXDATA;
    // } else {
        spi_pin_trans.tx_buffer = spi_pin_data;
    // }
}

esp_err_t spi_gpio_flush() {
    return spi_device_polling_transmit(spi_pin_hdlr, &spi_pin_trans);
}

esp_err_t spi_gpio_set_level(spi_pin_num_t pin_num, bool level) {
    uint8_t pin = pin_num - PIN_SPI_MIN - 1, idx = pin >> 3, bit = pin & 0x7;
    bitWrite(spi_pin_data[idx], bit, level);
    return spi_gpio_flush();
}

uint8_t spi_gpio_get_level(spi_pin_num_t pin_num) {
    uint8_t pin = pin_num - PIN_SPI_MIN - 1, idx = pin >> 3, bit = pin & 0x7;
    return bitRead(spi_pin_data[idx], bit);
}

// Others

void uart_initialize() {
    fflush(stdout); fflush(stderr);
    vTaskDelay(pdMS_TO_TICKS(10));
    // UART driver configuration
    uart_config_t uart_conf = {
        .baud_rate = 115200,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .rx_flow_ctrl_thresh = 0,
        .use_ref_tick = true
    };
    ESP_ERROR_CHECK( uart_param_config(NUM_UART, &uart_conf) );
    ESP_ERROR_CHECK( uart_driver_install(NUM_UART, 256, 0, 0, NULL, 0) );
    // Register UART to VFS and configure
    /* esp_vfs_dev_uart_register(); */
    esp_vfs_dev_uart_use_driver(NUM_UART);
    esp_vfs_dev_uart_set_rx_line_endings(ESP_LINE_ENDINGS_CR);
    esp_vfs_dev_uart_set_tx_line_endings(ESP_LINE_ENDINGS_CRLF);
}

void driver_initialize() {
    uart_initialize();
    rmt_initialize();
    i2c_initialize();
    gpio_initialize();
    spi_initialize();
}
