import serial

import numpy as np
from dataclasses import dataclass
from collections import deque
import time


@dataclass
class STM_Status:
    bias: int = 0
    dac_z: int = 0
    dac_x: int = 0
    dac_y: int = 0
    adc: int = 0
    steps: int = 0
    is_approaching: bool = False
    is_const_current: bool = False
    is_scanning: bool = False
    time_millis: int = 0

    @staticmethod
    def from_list(values):
        return STM_Status(bias=values[0],
                          dac_z=values[1],
                          dac_x=values[2],
                          dac_y=values[3],
                          adc=values[4],
                          steps=values[5],
                          is_approaching=bool(values[6]),
                          is_const_current=bool(values[7]),
                          is_scanning=bool(values[8]),
                          time_millis=values[9])

    @staticmethod
    def adc_to_amp(adc: int):
        return 1.0 * adc / 32768 * 10.24 / 100e6

    @staticmethod
    def dac_to_dacz_volts(dac: int):
        return 1.0 * (dac - 32768) / 32768 * 10.0 / 2.0

    @staticmethod
    def dac_to_dacx_volts(dac: int):
        return 1.0 * (dac - 32768) / 32768 * 10.0 / 2.0

    @staticmethod
    def dac_to_dacy_volts(dac: int):
        return 1.0 * (dac - 32768) / 32768 * 10.0 / 2.0

    @staticmethod
    def dac_to_bias_volts(dac: int):
        return 1.0 * (dac - 32768) / 32768 * 3.0

    def to_string(self):
        return """STM Status:
Bias: {} 
Z: {} 
X: {} 
Y: {} 
ADC: {} 
STEPS: {}
Appoaching: {} 
ConstCurrent: {} 
Scan: {}  
Time: {}""".format(self.bias, self.dac_z, self.dac_x, self.dac_y, self.adc, self.steps, self.is_approaching,  self.is_const_current, self.is_scanning, self.time_millis)


class STM(object):
    def __init__(self, device=None):
        self.is_opened = False
        self.busy = False
        if device:
            self.open(device)

        self.status = STM_Status()
        self.hist_length = 1000
        self.history = deque()
        self.scan_adc = None
        self.scan_dacz = None

        self.scan_config = [0, 100, 10, 0, 100, 10]
        self.scan_adc = np.ones([512, 512], dtype=np.float32)
        self.scan_dacz = np.ones([512, 512], dtype=np.float32)

    def open(self, device):
        self.stm_serial = serial.Serial(device, 115200, timeout=1)
        self.stm_serial.set_buffer_size(rx_size=128000, tx_size=128000)
        self.is_opened = True

    def get_status(self):
        if self.busy:
            return
        if self.is_opened:
            if self.busy:
                print('busy')
                return self.history[-1]
            try:
                self.send_cmd('GSTS')
                status_str = self.stm_serial.readline().decode()
                status_value = status_str.split(',')
                status_value = [int(x) for x in status_value]
                self.status = STM_Status.from_list(status_value)
            except:
                print('no response')
                return self.history[-1]
        else:
            self.status = STM_Status()
        self.history.append(self.status)
        if len(self.history) > self.hist_length:
            self.history.popleft()

        return self.status

    def reset(self):
        self.send_cmd('RSET')
        self.clear()

    def clear(self):
        self.history = deque()

    def send_cmd(self, cmd):
        if self.is_opened:
            self.stm_serial.write(cmd.encode())

    def move_motor(self, steps):
        self.send_cmd('MTMV {steps}')

    def approach(self, target_dac, steps):
        self.send_cmd(f'APRH {target_dac} {steps}')

    def stop(self):
        self.send_cmd('STOP')

    def measure_iv_curve(self, dac_start, dac_end, dac_step):
        self.send_cmd(f'IVME {dac_start} {dac_end} {dac_step}')
        # Wait for 0.1s for the STM to response
        time.sleep(2)
        return self.get_iv_curve()

    def get_iv_curve(self):
        iv_curve_values = [0, 0]
        if self.is_opened:
            self.busy = True
            time.sleep(1)
            self.send_cmd('IVGE')
            data_str = self.stm_serial.readline().decode()
            data = data_str.split(',')
            if data[0] == "IV":
                iv_curve_values = [int(x) for x in data[1:]]
        self.busy = False
        print(iv_curve_values)
        return iv_curve_values

    def set_bias(self, value):
        self.send_cmd(f"BIAS {value}")

    def set_dacz(self, value):
        self.send_cmd(f"DACZ {value}")

    def set_dacx(self, value):
        self.send_cmd(f"DACX {value}")

    def set_dacy(self, value):
        self.send_cmd(f"DACY {value}")

    def turn_on_const_current(self, target_adc):
        self.send_cmd(f"CCON {target_adc}")

    def turn_off_const_current(self):
        self.send_cmd(f"CCOF")

    def set_pid(self, Kp, Ki, Kd):
        self.send_cmd(f"PIDS {Kp} {Ki} {Kd}")

    def start_scan(self, x_start, x_end, x_resolution, y_start, y_end, y_resolution, sample_number):
        self.busy = True
        self.scan_config = [x_start, x_end,
                            x_resolution, y_start, y_end, y_resolution]
        self.send_cmd(
            f"SCST {x_start} {x_end} {x_resolution} {y_start} {y_end} {y_resolution} {sample_number}")

        self.scan_adc = np.ones([x_resolution, y_resolution], dtype=np.float32)
        self.scan_dacz = np.ones(
            [x_resolution, y_resolution], dtype=np.float32)

        current_line = ''

        def _process_full_line(full_line):
            # print(full_line)
            data = full_line.split(',')
            data_type = data[0]
            if data_type == "A":
                x_i = int(data[1])
                data_content = data[2:]
                data_content = [int(x) for x in data_content]
                self.scan_adc[x_i, :] = data_content
            if data_type == "Z":
                x_i = int(data[1])
                data_content = data[2:]
                data_content = [int(x) for x in data_content]
                self.scan_dacz[x_i, :] = data_content
            if data_type == "D":
                return True
            return False

        while (True):
            read_number = self.stm_serial.inWaiting()
            if (read_number == 0):
                continue
            read_str = self.stm_serial.read(read_number).decode()
            if "\n" in read_str:
                split_lines = read_str.split("\n")
                for data_line in split_lines:
                    if len(data_line) == 0:
                        continue
                    current_line += data_line
                    if current_line[-1] == "\r":  # We have a full line
                        _process_full_line(current_line)
                        current_line = ''
            else:
                current_line += read_str
            # We have a full line
            if current_line and current_line[-1] == "\r":
                _process_full_line(data_line)
                current_line = ''
            if "D" in read_str:
                break
        self.busy = False
        return
