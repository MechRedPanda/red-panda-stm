/**************************************************************************/
/*
LTC2326-16 library for Teensy 4.1
*/
/**************************************************************************/

#include "Arduino.h"
#include <SPI.h>
#include "LTC2326_16.hpp"

/**************************************************************************/
/*
    Constructor
*/
/**************************************************************************/

LTC2326_16::LTC2326_16(byte cs, byte cnv, byte busy)
{
    pinMode(cs, OUTPUT);
    pinMode(cnv, OUTPUT);
    pinMode(busy, INPUT);
    digitalWrite(cs, HIGH);
    digitalWrite(cnv, LOW);
    _cs = cs;
    _cnv = cnv;
    _busy = busy;
}

/**************************************************************************/
/*
    Initiate a conversion.
*/
/**************************************************************************/

void LTC2326_16::convert()
{
    digitalWrite(_cnv, HIGH);
}

/**************************************************************************/
/*
    Check whether ADC is busy doing a conversion. Returns true if conversion
    is in progress (BUSY pin is HIGH), false otherwise.
*/
/**************************************************************************/

bool LTC2326_16::busy()
{
    bool status;
    status = (bool)digitalRead(_busy);
    return status;
}

/**************************************************************************/
/*
    Read the ADC data register.
*/
/**************************************************************************/

int16_t LTC2326_16::read()
{
    int16_t val;

    digitalWrite(_cnv, LOW); // Reset CNV for another conversion later on
    SPI1.beginTransaction(_spi_settings);
    digitalWrite(_cs, HIGH);
    val = SPI1.transfer16(0x00);
    digitalWrite(_cs, LOW);
    return val;
}

float LTC2326_16::read_volts()
{
    int16_t val = read();
    return val * _ref_buffer_volts;
}