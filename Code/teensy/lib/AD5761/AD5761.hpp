/**************************************************************************/
/*

AD5761 library

*/
/**************************************************************************/

#ifndef AD5761_H
#define AD5761_H

#include "Arduino.h"
#include <SPI.h>

/* Input Shift Register Commands */
#define CMD_NOP 0x0
#define CMD_WR_TO_INPUT_REG 0x1
#define CMD_UPDATE_DAC_REG 0x2
#define CMD_WR_UPDATE_DAC_REG 0x3
#define CMD_WR_CTRL_REG 0x4
#define CMD_NOP_ALT_1 0x5
#define CMD_NOP_ALT_2 0x6
#define CMD_SW_DATA_RESET 0x7
#define CMD_RESERVED 0x8
#define CMD_DIS_DAISY_CHAIN 0x9
#define CMD_RD_INPUT_REG 0xA
#define CMD_RD_DAC_REG 0xB
#define CMD_RD_CTRL_REG 0xC
#define CMD_NOP_ALT_3 0xD
#define CMD_NOP_ALT_4 0xE
#define CMD_SW_FULL_RESET 0xF

class AD5761
{

public:
  // mode
  // 0b0000000101000 -10V, +10V
  // 0b0000000101101 -3 to 3V
  AD5761(byte cs, uint16_t mode); // Constructor

  void write(uint8_t reg_addr_cmd, uint16_t reg_data);
  void write_volt(float voltage);
  void read(uint8_t reg_addr_cmd);

  void spi_init();
  void reset();

private:
  byte _cs;
  uint16_t _mode;
  SPISettings _spi_settings = SPISettings(40000000, MSBFIRST, SPI_MODE2);
  byte _spi_buffer[3];
};

#endif
