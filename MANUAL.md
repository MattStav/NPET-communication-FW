# NPET communication FW manual
The __NPET communication FW__ facilitates efficient communication with NPET devices,
i.e., exchange of commands and reading of measured data.
In this manual, you will find the instructions to use the firmware.
This document is automatically compiled into the executable
and can be accessed therein.

Call the executable with the `--help` or `-h` arguments to print program instructions.

### Important information prior to using the firmware:
The NPET needs to be configured to 115_200 Baud and
8N1 mode (eight databits, no parity, one stop bit) at the start of communication.
This is the default configuration when NPET is powered ON.
The program will fail when NPET isn't configured with these settings at the start of communication.
The program will fail to reach the `Main menu` if not connected to a running NPET.

## Table of contents:
- [COM Port](#1-com-port)
- [Internal NPET FW](#2-internal-npet-fw)
- [Pulse generation](#3-pulse-generation)
- [Measurements](#4-measurements)
- [NPET Data processor](#5-npet-data-processor)
- [Baud rate](#6-baud-rate)
- [Calibration constant](#7-calibration-constant)
- [Offline operation](#8-offline-operation)

## 1. COM port
When launching the program, it will attempt to automatically detect the NPET and connect to it.
In case it cannot distinguish the NPET or the initial connection attempts fail,
the user will be prompted to specify the COM port to which the NPET is physically connected.
The NPET needs to be connected to the computer via rs232 connection 
(recommended rs232 to USB adapter) and powered on before attempting connection via this FW.
The selected COM port cannot be changed during the firmware operation.

## 2. Internal NPET FW
This concerns the internal NPET firmware, i.e., the firmware running on the NPET itself!
At the time of this writing, there are two versions of internal NPET firmware:
- The original firmware - [documentation](docu/UARTCommunicationNPET.pdf)
- The revised firmware, which accommodates a change in the NPET HW - [documentation](docu/UARTCommunicationNPET_ADI_FWver_03012019.pdf)
- _(Offline NPET FW, which is used for mock NPET connection, i.e.,
without an actual NPET device, more on this topic [here](#8-offline-operation))_

When initializing, the program will read the internal NPET firmware version 
and automatically select the correct measurement readout protocol,
associated with that firmware version.
In case of failure, the original NPET firmware is assumed. 
The user is informed about this choice during the initialization.
The user can also select the internal NPET firmware version manually in the settings.

## 3. Pulse generation
The NPET can be used to generate pulses.
Pulse generation is initiated in the main menu.
The user is prompted to set the number of pulses to be generated.
Entering `-1` will generate an infinite number of pulses.
Entering `0` will stop the pulse generation.
After selecting the number of pulses, 
the user is prompted to select the frequency of pulse generation.
The value is entered in __Hz__.
The default value is 100 Hz.

## 4. Measurements
The NPET's main purpose is to take time epoch measurements, i.e., 
measure the pulse time of arrival.
The measurements can be initiated from the main menu.

Measurement options:
- The number of measurements. Entering `-1` starts an infinite number of measurements.
- The channel to be measured. The NPET has two channels, channel 1 and channel 2.
It is standard practice that PPS is connected to channel 2,
and the signal to be measured is connected to channel 1.
- Monitoring method in the console:
  - No monitoring: The measurements are not displayed in the console to achieve the best performance.
  - Basic monitoring: Only a progress bar is displayed in the console.
  - Advanced monitoring: Measurements are displayed in a scrolling buffer in format `hh:mm:ss fs [progress]`.
  The measurement status, frequency, number of corrupted measurements and current size of all in-app buffers are also shown.
  - Synchronization monitoring: Measurements are displayed only with 1-second precision.
  This is useful for synchronizing the NPET with an external clock, more on this topic [here](#7-calibration-constant).
- Enable saving measurements to file. The file is named after the channel and the datetime of the measurement.

The measurements can be stopped by pressing `Esc` at any time.
Windows sleep is disabled during the measurements to prevent disruptions.

## 5. NPET Data processor
An option to launch the data processor is available in the main menu.
__This is a separate program that needs to be installed on your device.__
The data processor allows processing of the measurements read by this firmware.
To acquire this program, please contact the maintainer of this firmware.

## 6. Baud rate
Baud rate sets the speed of the communication.
In 8N1 mode, baud rate represents a number of 10-bit packets (8 data bits) per second.
Baud rate can be changed in the settings menu.
Increasing the baud rate may be necessary to process higher frequency measurements.
This operation is complicated, and any errors encountered in its course 
may need to be resolved by switching the NPET off and on again and then restarting this FW.
Only three Baud rates are available:
- 115 200
- 230 400
- 576 000

## 7. Calibration constant
By default, the measurements record the pulse's time of arrival since NPET was switched on.
A calibration constant can be set to adjust the measured values, for example,
to synchronize the measurements with an external clock.
This program then adds the constant to all measured values.

The constant is stored in the NPET and remains there until the NPET is switched off.
It is imported from the NPET before each measurement.

The constant is in format `int_part fractional_part`, e.g., `123 0.456789`.
The fractional part is handled as a 128-bit floating-point number,
ensuring its precision for up to 34 decimal digits.

### Constant setting
The calibration constant can be set in the settings menu. 
Each time the constant is set,
ten quick measurements are read out at the end to show the result.
There are several ways to set the calibration constant:
1. Raw format \
This option allows manipulating the constant in its raw format.
Both the integer and fractional part can be directly set.
Additionally, it is possible to simply increment or decrement 
the integer part by a certain number of seconds.
2. Time format \
This option allows manipulating the constant in a time format `hh:mm:ss.ffffff`.
The PPS (pulse per second) signal MUST be connected to the NPET.
The user will be asked to specify the PPS channel and number of averaging measurements.
The measured fractional part from the measurements will be used as the fractional part of the constant.
Afterward, the user can select either system clock time (with or without NTP sync) or user-defined time, 
either of which will be used as the integer part of the constant. 
Selecting system time with NTP sync will force the Windows clock
to synchronize with an NTP server before grabbing the current time; 
this operation requires that this FW is run with administrator privileges.
3. Clear the constant \
This clears the NPET constant saved in NPET.


## 8. Offline operation
It is possible to emulate the NPET in offline mode using the `virtual_machine` command.
A virtual serial port driver, to virtually pair two COM ports,
is needed to operate the virtual machine. 
This one is free and works well: https://eterlogic.com/.
To launch the virtual machine, 
run this program from the command line with command `virtual_machine`.
You will be prompted to enter the COM port to which the virtual machine will be connected
and the frequency of the emulated measurement generation on channel 1.
Channel 2 always generates measurements at 1 Hz.

   