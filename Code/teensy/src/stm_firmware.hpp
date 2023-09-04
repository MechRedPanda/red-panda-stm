/**************************************************************************/
/*

STM Firmware for Teensy 4.1

*/
/**************************************************************************/

#ifndef STM_FIRMWARE_H
#define STM_FIRMWARE_H

#include <Arduino.h>
#include <SPI.h> // include the SPI library
#include "LTC2326_16.hpp"
#include <ArduinoJson.h>
#include "EfficientStepper.hpp"
#include "AD5761.hpp"
#include <logTable.hpp>

#define CS_ADC 38    // ADC chip select pin
#define ADC_MISO 39  // ADC MISO
#define CNV 19       // ADC CNV pin - initiates a conversion
#define BUSY 18      // ADC BUSY pin
#define SERIAL_LED 0 // Indicates serial data transmission
#define TUNNEL_LED 1 // Indicates tunneling

// DAC channel addresses: (Note they are not in order)
#define DAC_1 7  //
#define DAC_3 8  //
#define DAC_2 9  //
#define DAC_4 10 //

// DAC and ADC resolution:
#define DAC_BITS 16      // Actual DAC resolution
#define POSITION_BITS 20 // Sigma-delta resolution
#define ADC_BITS 16

const int MAX_DAC_OUT = (1 << (DAC_BITS - 1)) - 1; // DAC upper bound
const int MIN_DAC_OUT = -(1 << (DAC_BITS - 1));    // DAC lower bound

// ADC Settings
LTC2326_16 ltc2326 = LTC2326_16(CS_ADC, CNV, BUSY);

// initialize the stepper library
// ULN2003 Motor Driver Pins
#define IN1 33
#define IN2 34
#define IN3 35
#define IN4 36
#define STEPS_PER_REVOLUTION 2048

#define CMD_LENGTH 4

#define INIT_KP 2.0
#define INIT_KI 1.0
#define INIT_KD 1.0

#define MOVE_SPEED 1

class STMStatus
{
public:
    int bias = 0;
    int dac_z = 0;
    int dac_x = 0;
    int dac_y = 0;
    int adc = 0;
    int steps = 0;
    bool is_approaching = false;
    bool is_const_current = false;
    bool is_scanning = false;
    uint32_t time_millis = 0;

    void to_char(char *buffer)
    {
        sprintf(buffer, "%d,%d,%d,%d,%d,%d,%d,%d,%d,%lu", bias, dac_z, dac_x, dac_y, adc, steps, is_approaching, is_const_current, is_scanning, time_millis);
    }
};

struct Approach_Config
{
    int target_dac;
    int max_steps;
    int step_interval;
};

double clamp_value(double value, double min_value, double max_value)
{
    if (value > max_value)
    {
        return max_value;
    }
    if (value < min_value)
    {
        return min_value;
    }
    return value;
}

class STM
{       // The class
public: // Access specifier
    void move_motor(int steps)
    {
        stepper_motor.step(steps);
        stm_status.steps = stepper_motor.get_total_steps();
        stm_status.time_millis = millis();
    }
    void reset()
    {
        stepper_motor.setSpeed(2);
        stepper_motor.reset();
        dac_x.reset();
        dac_y.reset();
        dac_z.reset();
        dac_bias.reset();
        stm_status = STMStatus();
        ltc2326.convert();
    }
    // STM motors
    EfficientStepper stepper_motor = EfficientStepper(STEPS_PER_REVOLUTION, IN1, IN3, IN2, IN4);

    // DACs
    void set_dac_z(int value)
    {
        dac_z.write(CMD_WR_UPDATE_DAC_REG, value);
        stm_status.dac_z = value;
        stm_status.time_millis = millis();
    }
    void set_dac_x(int value)
    {
        dac_x.write(CMD_WR_UPDATE_DAC_REG, value);
        stm_status.dac_x = value;
        stm_status.time_millis = millis();
    }
    void set_dac_y(int value)
    {
        dac_y.write(CMD_WR_UPDATE_DAC_REG, value);
        stm_status.dac_y = value;
        stm_status.time_millis = millis();
    }
    void set_dac_bias(int value)
    {
        dac_bias.write(CMD_WR_UPDATE_DAC_REG, value);
        stm_status.bias = value;
        stm_status.time_millis = millis();
    }
    // ADC
    int read_adc_raw()
    {
        int start_time = millis();
        while (ltc2326.busy() && millis() - start_time <= 1)
        {
            continue;
        }
        int val = ltc2326.read();
        this->_add_adc_value(val);
        ltc2326.convert();
        return val;
    }
    int read_adc()
    {
        read_adc_raw();
        return _get_adc_avg();
    }
    void update()
    {
        int adc_val = read_adc_raw();
        stm_status.adc = adc_val;
        stm_status.time_millis = millis();
    }
    // Return the adc status.
    STMStatus get_status()
    {
        return stm_status;
    }
    Approach_Config approach_config = Approach_Config();
    void start_approach(int target_adc, int max_motor_steps, int step_interval)
    {

        approach_config.max_steps = stepper_motor.get_total_steps() + max_motor_steps;
        approach_config.step_interval = step_interval;
        approach_config.target_dac = target_adc;
        stm_status.is_approaching = true;
    }
    bool approach()
    {
        if (stm_status.is_approaching)
        {
            if (stepper_motor.get_total_steps() < approach_config.max_steps)
            {
                move_motor(approach_config.step_interval);
                delay(2);
                for (int z_value = 10000; z_value <= 50000; z_value = z_value + 100)
                {
                    set_dac_z(z_value);
                    delayMicroseconds(100);
                    update();
                    if (read_adc() > approach_config.target_dac)
                    {
                        Serial.println("Approached!");
                        Serial.println(stm_status.adc);
                        stm_status.is_approaching = false;
                        stepper_motor.disable();
                        return true;
                    }
                }
            }
            else
            {
                stm_status.is_approaching = false;
                return false;
            }
        }
        return false;
    }
    int iv_bias[1000];
    int iv_adc[1000];
    int iv_N;
    void generate_iv_curve(int bias_start, int bias_end, int bias_step)
    {
        int i = 0;
        int init_bias = stm_status.bias;
        for (int bias = bias_start; bias < bias_end; bias = bias + bias_step)
        {
            if (i >= 1000)
                break;
            set_dac_bias(bias);
            int adc = read_adc();
            iv_adc[i] = adc;
            iv_bias[i] = bias;
            i++;
        }
        iv_N = i;
        set_dac_bias(init_bias); // Set the bias value back to the starting point.
    }
    void send_iv_curve()
    {
        Serial.print("IV,");
        for (int i = 0; i < iv_N; ++i)
        {
            Serial.print(iv_bias[i]);
            Serial.print(",");
            Serial.print(iv_adc[i]);
            if (i < iv_N - 1)
                Serial.print(",");
        }
        Serial.print("\r\n");
    }
    int di_z[1000];
    int di_adc[1000];
    int di_N;
    void generate_di_curve(int z_start, int z_end, int z_step)
    {
        int i = 0;
        for (int z = z_start; z < z_end; z = z + z_step)
        {
            if (i >= 1000)
                break;
            set_dac_z(z);
            int adc = read_adc();
            di_adc[i] = adc;
            di_z[i] = z;
            i++;
        }
        di_N = i;
    }
    void send_di_curve()
    {
        for (int i = 0; i < di_N; ++i)
        {
            Serial.print(di_z[i]);
            Serial.print(",");
            Serial.print(di_adc[i]);
            if (i < di_N)
                Serial.print(",");
        }
        Serial.print("\r\n");
    }
    // Specify the links and initial tuning parameters
    // Constant current mode
    bool is_const_current = false;
    int adc_set_value;
    double adc_set_value_log, adc_real_value_log, dac_z_control_value;

    // PID current_pid = PID(&adc_real_value_log_log, &dac_z_control_value, &adc_set_value_log_log, INIT_KP, INIT_KI, INIT_KD, DIRECT);
    double Kp = 0.0, Ki = 0.0, Kd = 0.0;
    double pTerm, iTerm;
    void turn_on_const_current(int target_adc)
    {
        this->adc_set_value = target_adc;
        this->adc_set_value_log = static_cast<double>(logTable[abs(target_adc)]);
        this->dac_z_control_value = static_cast<double>(stm_status.dac_z);
        pTerm = 0.0;
        iTerm = 0.0;
        this->stm_status.is_const_current = true;
    }
    int control_current(int adc_value)
    {
        this->adc_real_value_log = static_cast<double>(logTable[abs(adc_value)]);
        double error = this->adc_set_value_log - this->adc_real_value_log;
        pTerm = Kp * error;
        iTerm += Ki * error;
        iTerm = clamp_value(iTerm, -32768, 32768);
        int z = static_cast<int>(pTerm + iTerm) + 32768;
        if (z > 50000)
        {
            z = 50000;
        }
        if (z < 10000)
        {
            z = 10000;
        }
        this->set_dac_z(z);
        return static_cast<int>(error);
    }
    void turn_off_const_current()
    {
        this->stm_status.is_const_current = false;
    }
    // Scan Control
    int scan_image_z[2048];
    int scan_image_adc[2048];

    void start_scan(int x_start, int x_end, int x_resolution, int y_start, int y_end, int y_resolution, int sample_per_pixel)
    {
        move_to(x_start, y_start);
        double x_step = 1.0f * (x_end - x_start) / x_resolution;
        double y_step = 1.0f * (y_end - y_start) / y_resolution / sample_per_pixel;
        for (int x_i = 0; x_i < x_resolution; ++x_i)
        {
            int x_now = static_cast<int>(x_start + x_i * x_step);
            set_dac_x(x_now);
            int sample_count = 0;
            int err_sum = 0;
            int dacz_sum = 0;
            for (int y_i = 0; y_i < y_resolution * sample_per_pixel; ++y_i)
            {
                int y_now = static_cast<int>(y_start + y_i * y_step);
                set_dac_y(y_now);
                int adc_value = read_adc_raw();
                if (this->stm_status.is_const_current)
                {

                    adc_value = control_current(adc_value);
                }
                err_sum += adc_value;
                dacz_sum += stm_status.dac_z;
                sample_count++;
                if (sample_count == sample_per_pixel)
                {
                    scan_image_adc[y_i / sample_per_pixel] = err_sum / sample_per_pixel;
                    scan_image_z[y_i / sample_per_pixel] = dacz_sum / sample_per_pixel;
                    sample_count = 0;
                    err_sum = 0;
                    dacz_sum = 0;
                }
            }
            send_scan_line("A", x_i, scan_image_adc, y_resolution);
            send_scan_line("Z", x_i, scan_image_z, y_resolution);
            for (int y_i = y_resolution * sample_per_pixel - 1; y_i >= 0; --y_i)
            {
                int y_now = static_cast<int>(y_start + y_i * y_step);
                set_dac_y(y_now);
                if (this->stm_status.is_const_current)
                {
                    control_current(read_adc_raw());
                }
            }
        }
        Serial.println("D");
    }
    void send_scan_line(String prefix, int x_i, int *data, int num_points)
    {
        Serial.print(prefix);
        Serial.printf(",%d,", x_i);
        for (int i = 0; i < num_points; ++i)
        {
            Serial.print(data[i]);
            if (i < num_points - 1)
                Serial.print(",");
        }
        Serial.print("\r\n");
    }
    void move_to(int target_x, int target_y)
    {
        while (target_x != stm_status.dac_x)
        {
            if (stm_status.is_const_current)
            {
                control_current(read_adc_raw());
            }
            if (abs(target_x - stm_status.dac_x) < MOVE_SPEED)
            {
                set_dac_x(target_x);
            }
            else
            {
                if (target_x > stm_status.dac_x)
                {
                    set_dac_x(stm_status.dac_x + MOVE_SPEED);
                }
                else
                {
                    set_dac_x(stm_status.dac_x - MOVE_SPEED);
                }
            }
        }
        while (target_y != stm_status.dac_y)
        {
            if (stm_status.is_const_current)
            {
                control_current(read_adc_raw());
            }
            if (abs(target_y - stm_status.dac_y) < MOVE_SPEED)
            {
                set_dac_y(target_y);
            }
            else
            {
                if (target_y > stm_status.dac_y)
                {
                    set_dac_y(stm_status.dac_y + MOVE_SPEED);
                }
                else
                {
                    set_dac_y(stm_status.dac_y - MOVE_SPEED);
                }
            }
        }
    }

    // Piezo
    void test_piezo()
    {
        for (int i = 0; i < 500; i++)
        {
            set_dac_z(50000);
            delayMicroseconds(500);
            set_dac_z(00000);
            delayMicroseconds(500);
        }
        delay(1000);
        for (int i = 0; i < 500; i++)
        {
            set_dac_x(50000);
            delayMicroseconds(500);
            set_dac_x(00000);
            delayMicroseconds(500);
        }
        delay(1000);
        for (int i = 0; i < 500; i++)
        {
            set_dac_y(50000);
            delayMicroseconds(500);
            set_dac_y(00000);
            delayMicroseconds(500);
        }
    }
    STMStatus stm_status = STMStatus();

private:
    // DAC Settings
    AD5761 dac_x = AD5761(DAC_1, 0b0000000000000101);    // Output range: -5 to 5V
    AD5761 dac_y = AD5761(DAC_2, 0b0000000000000101);    // Output range: -5 to 5V
    AD5761 dac_z = AD5761(DAC_3, 0b0000000000000000);    // Output range: -10 to 10V
    AD5761 dac_bias = AD5761(DAC_4, 0b0000000000000101); // Output range: -3 to 3V

    // ADC settings
    LTC2326_16 ltc2326 = LTC2326_16(CS_ADC, CNV, BUSY);

    int _adc_buffer[5];
    int _current_index = 0;
    int _adc_sum = 0;
    void _add_adc_value(int value)
    {
        _current_index += 1;
        _current_index = _current_index % 5;
        _adc_sum -= _adc_buffer[_current_index];
        _adc_buffer[_current_index] = value;
        _adc_sum += _adc_buffer[_current_index];
    }
    int _get_adc_avg()
    {
        return static_cast<int>(_adc_sum / 5.0);
    }
};

#endif // STM_FIRMWARE_H