/**************************************************************************/
/*

A lib for contolling stepper motor and cut off current to lower the temperature.

*/
/**************************************************************************/

#ifndef STEPPER_H
#define STEPPER_H

#include <Arduino.h>
#include <Stepper.h>

class EfficientStepper
{

public:
    EfficientStepper(int number_of_steps, int motor_pin_1, int motor_pin_2,
                     int motor_pin_3, int motor_pin_4);
    void step(int steps);
    void enable();
    void disable();
    void setSpeed(long speed);
    int get_total_steps();
    void reset();

private:
    bool _stepper_motor_status[4] = {false, false, false, false};
    bool _stepper_motor_enabled = false;
    int _stepper_motor_pins[4];
    Stepper _stepper_motor;
    void _save_status();
    int _total_steps = 0;
};

#endif // STEPPER_H
