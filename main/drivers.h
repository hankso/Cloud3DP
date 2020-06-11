/* 
 * File: drivers.h
 * Authors: Hank <hankso1106@gmail.com>
 * Create: 2020-05-06 19:53:43
 */

#ifndef _DRIVERS_H_
#define _DRIVERS_H_

#include "globals.h"

#include "esp_err.h"

#define _I2C_NUMBER(num) I2C_NUM_##num
#define I2C_NUMBER(num) _I2C_NUMBER(num)

#define _UART_NUMBER(num) UART_NUM_##num
#define UART_NUMBER(num) _UART_NUMBER(num)

#define _GPIO_NUMBER(num) GPIO_NUM_##num
#define GPIO_NUMBER(num) _GPIO_NUMBER(num)

#define NUM_RMT     RMT_CHANNEL_0
#define NUM_LED     CONFIG_LED_NUM
#define NUM_I2C     I2C_NUMBER(CONFIG_I2C_NUM)
#define NUM_UART    UART_NUMBER(CONFIG_UART_NUM)

#define PIN_LED     GPIO_NUMBER(CONFIG_GPIO_RGB_LED)
#define PIN_INT     GPIO_NUMBER(CONFIG_GPIO_ES_INT)

#define PIN_SDA     GPIO_NUMBER(CONFIG_GPIO_I2C_SDA)
#define PIN_SCL     GPIO_NUMBER(CONFIG_GPIO_I2C_SCL)

#define PIN_HMISO   GPIO_NUMBER(CONFIG_GPIO_HSPI_MISO)
#define PIN_HMOSI   GPIO_NUMBER(CONFIG_GPIO_HSPI_MOSI)
#define PIN_HSCLK   GPIO_NUMBER(CONFIG_GPIO_HSPI_SCLK)
#define PIN_HCS0    GPIO_NUMBER(CONFIG_GPIO_HSPI_CS0)
#define PIN_HCS1    GPIO_NUMBER(CONFIG_GPIO_HSPI_CS1)
#define PIN_HSDCD   GPIO_NUMBER(CONFIG_GPIO_HSPI_SDCD)


void driver_initialize();


// WS2812B LED Controller https://cdn-shop.adafruit.com/datasheets/WS2812B.pdf
// Color Order: G[7:0] R[7:0] B[7:0]
// Code timing:
//      0H 250 - 550 ns
//      0L 700 - 1000 ns
//      1H 650 - 950 ns
//      1L 300 - 600 ns
//      RESET LOW 50us+
// RMT Block size: 512 / 8(channel) * 32bits = 64 * 4bytes (per block)
// LED data size: 3(RGB) * 8(uint8_t) * 32bits = 24 * 4bytes (per LED)
// ESP32 RMT can drive up to 64 / 24 = 2.6 LEDs (per block and per chunk)
// That is 2.6 * 8 = 20.8 = 20 LEDs (8 blocks per chunk)

#define NUM_LED_MAX 20
#if NUM_LED > NUM_LED_MAX
#error "No space for data of more than 20 LEDs"
#endif

typedef struct {
    union {
        struct {
            uint8_t b, r, g, dummy; //ESP32 is little-endian (low byte first)
        };
        uint32_t color; // dummy[31:24] g[23:16] r[15:8] b[7:0]
    };
    bool disable;
} led_code_t;

void led_on(uint8_t idx = 0);   // enable LED (recover previous color)
void led_off(uint8_t idx = 0);  // disable LED (will render color 0x000000)
void led_blink(uint8_t idx = 0, uint32_t ms = 500, uint8_t n = 1);

void led_color(uint8_t idx, uint32_t color);
void led_color(uint8_t idx, uint8_t r, uint8_t g, uint8_t b);

uint8_t led_count();                    // get current LED numbers
bool led_count(uint8_t num = NUM_LED);  // change cascaded LED numbers
bool led_status(uint8_t idx = 0);       // get enabled of LED specified by `idx`
uint32_t led_color(uint8_t idx = 0);    // get color of LED specified by `idx`


// We use PCF8574 for IO expansion: Endstops | Temprature | Valves

typedef enum {
    PIN_I2C_MIN = 99,

    // endstops
    PIN_XMIN, PIN_XMAX, PIN_YMIN, PIN_YMAX,
    PIN_ZMIN, PIN_ZMAX, PIN_EVAL, PIN_PROB,

    // temprature
    PIN_BED,  PIN_NOZ1, PIN_NOZ2, PIN_NOZ3,
    PIN_FAN1, PIN_FAN2, PIN_FAN3, PIN_RSV,

    // valves
    PIN_VLV1, PIN_VLV2, PIN_VLV3, PIN_VLV4,
    PIN_VLV5, PIN_VLV6, PIN_VLV7, PIN_VLV8,

    PIN_I2C_MAX
} i2c_pin_num_t;

// Transfer data with PCF8574. idx indicates index of { endstops, temp, valves }
esp_err_t i2c_set_val(uint8_t idx);
esp_err_t i2c_get_val(uint8_t idx);

void i2c_detect();

esp_err_t i2c_gpio_set_level(i2c_pin_num_t pin, bool level);
uint8_t i2c_gpio_get_level(i2c_pin_num_t pin, bool sync = false);


// IO expansion by 74HC595 (SPI connection): Steppers
typedef enum {
    PIN_SPI_MIN = 199,

    // stepper direction & step (pulse)
    PIN_XDIR,  PIN_XSTEP,  PIN_YDIR,  PIN_YSTEP,  PIN_ZDIR,  PIN_ZSTEP,
    PIN_E1DIR, PIN_E1STEP, PIN_E2DIR, PIN_E2STEP, PIN_E3DIR, PIN_E3STEP,

    // enable | disable
    PIN_XYZEN, PIN_E1EN, PIN_E2EN,  PIN_E3EN,

    PIN_SPI_MAX
} spi_pin_num_t;

// Transfer data to 74HC595. Same like i2c_gpio_set_val
esp_err_t spi_gpio_flush();

esp_err_t spi_gpio_set_level(spi_pin_num_t pin, bool level);
uint8_t spi_gpio_get_level(spi_pin_num_t pin_num);

#endif // _DRIVERS_H_
