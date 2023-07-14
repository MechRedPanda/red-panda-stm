#include <Arduino.h>
#include <SPI.h> // include the SPI library
#include <Stepper.h>
#include "LTC2326_16.hpp"
#include "AD5761.hpp"

#define CS_ADC 38    // ADC chip select pin
#define ADC_MISO 39  // ADC MISO
#define CNV 19       // ADC CNV pin - initiates a conversion
#define BUSY 18      // ADC BUSY pin
#define SERIAL_LED 0 // Indicates serial data transmission
#define TUNNEL_LED 1 // Indicates tunneling

// DAC channel addresses: (Note they are not sorted)
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

// DAC Settings
AD5761 dac_1 = AD5761(DAC_1);
AD5761 dac_2 = AD5761(DAC_2);
AD5761 dac_3 = AD5761(DAC_3);
AD5761 dac_4 = AD5761(DAC_4);

// ADC Settings
LTC2326_16 ltc2326 = LTC2326_16(CS_ADC, CNV, BUSY);

// initialize the stepper library
// ULN2003 Motor Driver Pins
#define IN1 33
#define IN2 34
#define IN3 35
#define IN4 36
const int stepsPerRevolution = 2048;
Stepper myStepper(stepsPerRevolution, IN1, IN2, IN3, IN4);
bool step_motor_status[4];
bool step_motor_enabled = false;
void _getStepperMotorStatus()
{
  if (step_motor_enabled)
  {
    step_motor_status[0] = digitalRead(IN1);
    step_motor_status[1] = digitalRead(IN2);
    step_motor_status[2] = digitalRead(IN3);
    step_motor_status[3] = digitalRead(IN4);
  }
}
void _restoreStepperMotorStatus()
{
  digitalWrite(IN1, step_motor_status[0]);
  digitalWrite(IN2, step_motor_status[1]);
  digitalWrite(IN3, step_motor_status[2]);
  digitalWrite(IN4, step_motor_status[3]);
  step_motor_enabled = true;
}
void _disableStepperMotor()
{
  _getStepperMotorStatus();
  digitalWrite(IN1, LOW);
  digitalWrite(IN2, LOW);
  digitalWrite(IN3, LOW);
  digitalWrite(IN4, LOW);
  step_motor_enabled = false;
}

void setup()
{
  // initialize the serial port
  Serial.begin(115200);
  // Set the speed at 1 rpm
  myStepper.setSpeed(1);
  // initialize SPI:
  digitalWrite(CS_ADC, HIGH);
  digitalWrite(DAC_4, HIGH);
  SPI.begin();
  // Set Up SPI1
  SPI1.setSCK(27);
  SPI1.setCS(38);
  SPI1.setMISO(39);
  SPI1.begin();
  dac_1.reset();
  dac_2.reset();
  dac_3.reset();
  dac_4.reset();
}

void loop()
{
  // int N = 100;
  // float v_min = -2.0, v_max = 2.0;
  // for (int i = 0; i < N; ++i)
  // {
  //   Serial.println("-----------");
  //   Serial.println(i);
  //   float voltage = 1.0 * i / N * (v_max - v_min) + v_min;
  //   dac_1.write_volt(voltage * 0.5);
  //   dac_2.write_volt(voltage * 0.5);
  //   dac_3.write_volt(-1.0);
  //   dac_4.write_volt(voltage);
  //   delay(1);
  //   Serial.print("read adc: ");
  //   ltc2326.convert();
  //   delay(2);
  //   int val = ltc2326.read();
  //   Serial.println(val);
  //   Serial.print("read volt: ");
  //   Serial.println(val / 32768.0 * 10.24);
  //   myStepper.step(1000);
  //   myStepper.step(-1000);
  //   delay(100);
  // }
  myStepper.step(500);
  delay(1000);
  myStepper.step(-500);
  delay(1000);
}
