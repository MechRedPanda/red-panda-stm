/**************************************************************************/
/*
AD5761
*/
/**************************************************************************/

#include "Arduino.h"
#include "AD5761.hpp"
#include "SPI.h"

/**************************************************************************/
/*
    Constructor
*/
/**************************************************************************/

AD5761::AD5761(byte cs, uint16_t mode)
{
    pinMode(cs, OUTPUT);
    digitalWrite(cs, HIGH);
    _cs = cs;
    _mode = mode;
}

/**************************************************************************/
/*
    Set the output of a single DAC channel.
*/
/**************************************************************************/

void AD5761::spi_init()
{
    pinMode(_cs, OUTPUT); // Set the SS0 pin as an output
    SPI.begin();          // Begin SPI hardware
    delay(100);
    reset();
}

void AD5761::reset()
{
    // AD5761 software reset
    write(CMD_SW_FULL_RESET, 0);
    delay(100);
    // Set the Mode of AD5761
    write(CMD_WR_CTRL_REG, _mode);
}

// SPI Manputaion
void AD5761::write(uint8_t reg_addr_cmd, uint16_t reg_data)
{
    uint8_t data[3];
    SPI.beginTransaction(_spi_settings);
    digitalWrite(_cs, LOW);
    data[0] = reg_addr_cmd;
    data[1] = (reg_data & 0xFF00) >> 8;
    data[2] = (reg_data & 0x00FF) >> 0;
    for (int i = 0; i < 3; i++)
    {
        SPI.transfer(data[i]);
    }
    digitalWrite(_cs, HIGH);
}

void AD5761::write_volt(float voltage)
{
    int set_val = (int)((voltage / 2.5 + 4) / 8 * 65536);
    write(CMD_WR_UPDATE_DAC_REG, set_val);
}

void AD5761::read(uint8_t reg_addr_cmd)
{
    digitalWrite(_cs, LOW);
    delay(1);
    _spi_buffer[0] = SPI.transfer(reg_addr_cmd);
    _spi_buffer[1] = SPI.transfer(0xFF); // dummy
    _spi_buffer[2] = SPI.transfer(0xFF); // dummy
    digitalWrite(_cs, HIGH);
    delay(1);
}