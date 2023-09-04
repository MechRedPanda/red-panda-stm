#include "Arduino.h"
#include <SPI.h>
#include "EfficientStepper.hpp"

/**************************************************************************/
/*
    Constructor
*/
/**************************************************************************/

EfficientStepper::EfficientStepper(int number_of_steps, int motor_pin_1, int motor_pin_2,
                                   int motor_pin_3, int motor_pin_4) : _stepper_motor(Stepper(number_of_steps, motor_pin_1, motor_pin_2, motor_pin_3, motor_pin_4))
{
    _stepper_motor_pins[0] = motor_pin_1;
    _stepper_motor_pins[1] = motor_pin_2;
    _stepper_motor_pins[2] = motor_pin_3;
    _stepper_motor_pins[3] = motor_pin_4;
    _stepper_motor_enabled = true;
}

void EfficientStepper::_save_status()
{
    if (_stepper_motor_enabled)
    // If the motor is already disabled, then we should not try to read the current pins;
    {
        for (int i = 0; i < 4; ++i)
        {
            _stepper_motor_status[i] = digitalRead(_stepper_motor_pins[i]);
        }
    }
}

void EfficientStepper::enable()
{
    for (int i = 0; i < 4; ++i)
    {
        digitalWrite(_stepper_motor_pins[i], _stepper_motor_status[i]);
    }

    _stepper_motor_enabled = true;
}

void EfficientStepper::disable()
{
    _save_status();
    for (int i = 0; i < 4; ++i)
    {
        digitalWrite(_stepper_motor_pins[i], LOW);
    }
    _stepper_motor_enabled = false;
}

void EfficientStepper::step(int steps)
{
    if (!_stepper_motor_enabled)
    {
        enable();
    }
    _stepper_motor.step(steps);
    _total_steps = _total_steps + steps;
}

void EfficientStepper::setSpeed(long speed)
{
    _stepper_motor.setSpeed(speed);
}

int EfficientStepper::get_total_steps()
{
    return _total_steps;
}

void EfficientStepper::reset()
{
    _total_steps = 0;
}