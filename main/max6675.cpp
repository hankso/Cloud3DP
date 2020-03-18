/*************************************************************************
File: max6675.cpp
Author: Hankso
Webpage: http://github.com/hankso
Time: Mon 27 May 2019 15:47:10 CST
************************************************************************/

#include "max6675.h"

#include "soc/spi_pins.h"
#include "driver/spi_common.h"
#include "driver/spi_master.h"

const uint8_t HSPI_IOMUX_PIN_NUM_CSn[3] = {15, 25, 32};
const uint8_t VSPI_IOMUX_PIN_NUM_CSn[3] = {5, 17, 23};

uint16_t temprature[6]; // in Celcius

spi_bus_config_t hspi_buscfg = {
    .mosi_io_num = -1,
    .miso_io_num = HSPI_IOMUX_PIN_NUM_MISO,
    .sclk_io_num = HSPI_IOMUX_PIN_NUM_CLK,
    .quadwp_io_num = -1,
    .quadhd_io_num = -1,
    .max_transfer_sz = 0,
    .flags = SPICOMMON_BUSFLAG_MASTER,
};

spi_bus_config_t vspi_buscfg = {
    .mosi_io_num = -1,
    .miso_io_num = VSPI_IOMUX_PIN_NUM_MISO,
    .sclk_io_num = VSPI_IOMUX_PIN_NUM_CLK,
    .quadwp_io_num = -1,
    .quadhd_io_num = -1,
    .max_transfer_sz = 0,
    .flags = SPICOMMON_BUSFLAG_MASTER,
};

spi_device_interface_config_t devcfg = {
    .command_bits = 0,
    .address_bits = 0,
    .dummy_bits = 0,
    .mode = 0,  // SPI Mode: CPOL=0, CPHA=0
    .duty_cycle_pos = 128,
    .cs_ena_pretrans = 0,
    .cs_ena_posttrans = 0,
    .clock_speed_hz = 4000000,  // SPI Freq: 4MHz
    .input_delay_ns = 0,
    .spics_io_num = 0,  // not allocated yet
    .flags = 0,
    .queue_size = 7,  // max: 6 devices + 1
    .pre_cb = NULL,
    .post_cb = NULL,
};

spi_device_handle_t _hdlrs[6];
spi_transaction_t _trans;

void max6675_init() {
    spi_bus_initialize(HSPI_HOST, &hspi_buscfg, 1);
    spi_bus_initialize(VSPI_HOST, &vspi_buscfg, 2);
    for (int i = 0; i < 3; i++) {
        devcfg.spics_io_num = HSPI_IOMUX_PIN_NUM_CSn[i];
        spi_bus_add_device(HSPI_HOST, &devcfg, &_hdlrs[i]);
        devcfg.spics_io_num = VSPI_IOMUX_PIN_NUM_CSn[i];
        spi_bus_add_device(VSPI_HOST, &devcfg, &_hdlrs[i + 3]);
    }
    // memset(&_trans, 0, sizeof(_trans));
    _trans.flags = SPI_TRANS_USE_RXDATA;
    _trans.length = 16;  // sizeof(recv_buff) * sizeof(recv_buff[0]);
}

void max6675_read() {
    uint16_t tmp;
    for (int i = 0; i < 6; i++) {
        spi_device_transmit(_hdlrs[i], &_trans);
        tmp = ((uint16_t)_trans.rx_data[0] << 5) | (_trans.rx_data[1] >> 3);
        temprature[i] = (tmp & 0xfff) / 4;
    }
}

size_t max6675_json(char *buffer) {
    uint16_t len = 0, maxlen = sizeof(buffer);
    snprintf(buffer, maxlen, "[");
    for (int i = 0; i < 6; i++) {
        len += snprintf(buffer, maxlen, "%s%d", i ? "," : "", temprature[i]);
        if (len > (maxlen - 1)) return 0;
    }
    sprintf(buffer, "]");
    return len + 2;
}
