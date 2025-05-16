# OSU LoRa Station Firmware

This repository contains new firmware developed as part of my diploma thesis, intended to extend the functionality of the original measurement stations. The original firmware is available [here](https://github.com/torar9/OSU-LoRa-Station).

This firmware uses the [SlimLoRa](https://github.com/clavisound/SlimLoRa) library, enabling additional and extended features due to its minimal memory footprint.
This choice addresses the memory limitations of the Adafruit Feather 32u4 board used in the stations.
The firmware now supports time synchronization with LoRaWAN network. The introduction of this key feature solves the issue of unsynchronized measurements between stations.

## Key Features

* **Measured Parameters**: The station is equipped to measure temperature and humidity using the HTU21D sensor. It also measures various particulate matter concentrations (PM1.0, PM2.5, PM4.0, PM10.0 mass concentration, and PN0.5, PN1.0, PN2.5, PN4.0, PN10.0 number concentration, along with typical particle size) using the SPS30 sensor.
* **Data Transmission via LoRaWAN**: Supports OTAA (Over-The-Air Activation) and ABP (Activation By Personalization) modes using the SlimLoRa library. The recommended and primarily used mode is OTAA for higher security and flexibility.
* **Timekeeping**: Ability to use hardware RTC (supported chips DS1307, DS3231, PCF8523, PCF8563) for accurate timekeeping even after power loss, or software timekeeping using the MCU.
* **Time Synchronization**: Remote time synchronization using time timestamps from the TTN (The Things Network) network via DeviceTimeReq MAC frames. Periodic resynchronization is configurable.
* **Regular Data Transmission Intervals**: Data is sent at configurable intervals, with a minimum of 5 minutes due to radio spectrum limitations. Option to synchronize transmission with real time.
* **Power Saving**: Ability to enter deep sleep mode between data transmission cycles (requires hardware RTC). Option to stop the SPS30 sensor fan to reduce power consumption.
* **Remote Configuration (Over-The-Air)**: Modification of operational parameters via downlink messages from the LoRaWAN network. Configuration is stored in EEPROM for persistence.

## Configuration

All necessary configuration is in the `config.h` file. This includes LoRaWAN settings (including activation mode, keys and transmission data rate), timing (intervals and synchronization), hardware configuration (RTC and sensors), debug options, and OTA configurable parameters that can be changed remotely via downlink messages.

**Important Settings in `config.h`:**

* `FIRMWARE_CONFIG_VERSION`: Firmware configuration version. Change this number every time you want the new configuration from `config.h` to be saved to EEPROM and overwrite any remotely set configuration.
* `DEBUG`: Enable/disable debug output on the serial port.
* `LORAWAN_OTAA_ENABLED`: Enable OTAA mode. Set to `1` for OTAA, `0` for ABP.
* `LORAWAN_KEEP_SESSION`: Store session data to EEPROM when using OTAA.
* `DATA_RATE`: Sets the LoRa data rate (spreading factor and bandwidth combination). Possible values from the SlimLoRa library (v0.7.5): {`SF7BW250` (only for 868.3 MHz), `SF7BW125`, `SF9BW125`, `SF10BW125`, `SF11BW125`, `SF12BW125`} Higher [spreading factors](https://www.thethingsnetwork.org/docs/lorawan/spreading-factors) (SF) provide longer range but lower data rates.
* `DevEUI`, `JoinEUI`, `AppKey`: Keys for OTAA.
* `NwkSKey`, `AppSKey`, `DevAddr`: Keys and address for ABP (only if `LORAWAN_OTAA_ENABLED` is `0`).
* `TIMEZONE_OFFSET_HOURS`: Time offset from UTC in hours.
* `USE_HW_RTC`: Enable the use of hardware RTC. Set to `1` to use an external RTC module, `0` for software timekeeping by MCU.
* `RTC_TYPE`: Type of RTC chip used (e.g., `RTC_DS3231`). (Used only if `USE_HW_RTC` is `1`).
* `SEND_INTERVAL_MINUTES`: Data transmission interval in minutes. Must be a multiple of 5 (5, 10, 15, ...).
* `SPS_CLEAN_INTERVAL_DAYS`: Automatic cleaning interval for the SPS30 sensor in days.
* `SPS_STABILIZATION_PREREADOUT_DELAY`: Time in minutes before scheduled data readout for the SPS30 measurement to start if stopping is enabled.
* `SPS_STOP_AFTER_READOUT`: Stop measurement on the SPS30 sensor after data readout. `1` to stop, `0` for continuous measurement.
* `REALTIME_RESYNC_INTERVAL_DAYS`: Real-time resynchronization interval in days.
* `OVERRIDE_TIME_SYNCHRONIZATION`: Override time synchronization. `0` for time synchronization, `1` for sending data based only on the interval.
* `ALLOW_DEEP_SLEEP`: Allow deep sleep mode. `1` to allow, `0` to disallow (deep sleep requires hardware RTC).

**Note on `FIRMWARE_CONFIG_VERSION`:**

To ensure the correct loading of the configuration from `config.h` to EEPROM after a firmware update, it is essential to change the value of `FIRMWARE_CONFIG_VERSION`. If the value in EEPROM is different from the value in the newly uploaded firmware, the configuration from `config.h` will be saved to EEPROM.

## LoRaWAN Activation (OTAA vs ABP)

The firmware supports both main LoRaWAN device activation modes.

* **OTAA (Over-The-Air Activation)**: Recommended mode. The device dynamically connects to the network and obtains a network address and session keys upon each connection. For activation are needed keys Device EUI, Join EUI and Application Key.
* **ABP (Activation By Personalization)**: The device has a fixed network address and session keys. This mode is less secure and flexible.

The mode setting is done in `config.h` using `#define LORAWAN_OTAA_ENABLED`.

## Timekeeping and Synchronization

The firmware offers two options for timekeeping:

* **Hardware RTC**: Utilizes an external RTC module for accurate timekeeping even after restart or power loss. The type of RTC chip is configured in `config.h`.
* **MCU**: Time is maintained by the microcontroller's internal timer. This method is less accurate, and time is lost after a restart.

The choice between hardwarového and softwarového RTC is made in `config.h` using `#define USE_HW_RTC`.

Time synchronization occurs remotely using time timestamps from TTN via DeviceTimeReq MAC frames. Station periodically requests resynchronization in interval configured in `config.h` (`REALTIME_RESYNC_INTERVAL_DAYS`) or using remote configuration.

### Time Synchronization Behavior

If time synchronization fails, the device will retry synchronization at increasing intervals defined in the `syncFailedResyncIntervalsInMinutes` array. The intervals are: 0, 5, 30, 60, 120, 300, 720, and 1440 minutes. If synchronization continues to fail, the device will set the `overrideTimeSynchronization` flag to `1` and operate without real-time synchronization.

## Remote Configuration (OTA)

The device allows changing some operational parameters using downlink messages from the TTN server. Each configuration setting is assigned a specific port (fport).

The modified configuration is saved to EEPROM and loaded upon each device startup.

**Configurable Parameters and their Ports:**

* **Port 1**: Data transmission interval (in minutes). Expects 2 bytes (uint16\_t).
* **Port 2**: Automatic cleaning interval for the SPS30 sensor (in days). Expects 1 byte (uint8\_t).
* **Port 3**: Stabilization interval for SPS30 sensor measurement before readout (in minutes). Expects 1 byte (uint8\_t).
* **Port 4**: Enable/disable stopping SPS30 measurement after readout. Expects 1 byte (0 or 1).
* **Port 5**: Real-time resynchronization interval (in days). Expects 1 byte (uint8\_t).
* **Port 6**: Disable/enable time synchronization. Expects 1 byte (0 or 1).
* **Port 7**: Enable/disable deep sleep mode. Expects 1 byte (0 or 1).
* **Port 8**: Force time synchronization. Expects a specific message (e.g., a byte with value 1).
* **Port 9**: Request current settings report. Expects a specific message (e.g., a byte with value 1).

Upon successful application of a configuration setting (except for forced time synchronization and request for report), the device automatically sends a confirmation message (uplink) to the server with the current configuration status.

### Data Format for OTA Configuration (Downlink)

Data for remote configuration is sent as a downlink message to the corresponding FPort.

* **Port 1 (Send Interval):** 2 bytes, uint16\_t, little-endian (LSB first). Value in minutes, rounded down to the nearest multiple of 5. Minimum is 5 minutes.
  * Example: To set 60 minutes, send `0x3C 0x00`.
  * **Note:** When entering the value in TTN Console, provide it in **big-endian format** (e.g., `0x00 0x3C` for 60 minutes). The device will receive it correctly. Make sure to include the first byte!
* **Port 2 (SPS30 Cleaning Interval):** 1 byte, uint8\_t. Value in days. Minimum is 1 day (if the value is less, it will be set to 7).
  * Example: To set 7 days, send `0x07`.
* **Port 3 (SPS30 Stabilization Delay):** 1 byte, `uint8_t`. Value in minutes. This defines how long before the scheduled data transmission the SPS30 sensor should start measuring to stabilize.
  * If the value is greater than `sendIntervalMinutes`, it will be set to `sendIntervalMinutes`.
  * If the value is less than 1, stopping the SPS30 after measurement (`spsStopAfterReadout`) will be disabled.
  * Example: To set a stabilization delay of 5 minutes, send `0x05`.
* **Port 4 (Stop SPS30 after Readout):** 1 byte, `uint8_t`. Values 0 or 1. This controls whether the SPS30 sensor stops after data readout.
  * `0x01` enables stopping the SPS30. If `spsStabilizationPreReadoutDelay` is greater than `sendIntervalMinutes` or less than 1, `spsStopAfterReadout` will automatically be set to `0`. If the stabilization delay is less than the minimum required time (`SPS30_DEFAILT_STABILIZATION_TIME`), it will be set to the minimum stabilization time.
  * `0x00` disables stopping the SPS30, allowing it to run continuously.
  * Example: To enable stopping the SPS30 after data readout, send `0x01`.
* **Port 5 (Real Time Resynchronization Interval):** 1 byte, uint8\_t. Value in days. Minimum is 1 day (if the value is less, it will be set to 7).
  * Example: To set 7 days, send `0x07`.
* **Port 6 (Time Synchronization Override):** 1 byte, uint8\_t. Values 0 or 1.
  * `0x01` overrides time synchronization (sending data without real-time synchronization).
  * `0x00` uses time synchronization.
* **Port 7 (Allow Deep Sleep):** 1 byte, uint8\_t. Values 0 or 1. Allowed only if `USE_HW_RTC` is set to 1 in `config.h`.
  * `0x01` allows deep sleep.
  * `0x00` disallows deep sleep.
* **Port 8 (Force Time Resynchronization):** 1 byte, of value (`0x01`). Triggers immediate time synchronization.
* **Port 9 (Request Current Settings Report):** 1 byte, of value (`0x01`). The device will send an uplink message with the current settings on FPort 4.

### Data Format of Settings Report (Uplink on Port 4)

The settings report sent on port 4 has a length of 12 bytes and contains the following parameters:

* Bytes 0-1: `sendIntervalMinutes` (uint16\_t)
* Byte 2: `spsCleanIntervalDays` (uint8\_t)
* Byte 3: `spsStabilizationPreReadoutDelay` (uint8\_t)
* Byte 4: `spsStopAfterReadout` (uint8\_t)
* Byte 5: `realTimeResyncIntervalDays` (uint8\_t)
* Byte 6: `overrideTimeSynchronization` (uint8\_t)
* Byte 7: `allowDeepSleep` (uint8\_t)
* Bytes 8-11: Current timestamp (uint32_t, Unix epoch format, little-endian - LSB first)

This report allows monitoring and confirming the configuration changes made on individual stations.

## Power Saving

The firmware implements several mechanisms for power saving:

* **Deep Sleep**: Between measurement and transmission cycles, the device can enter deep sleep mode, which significantly reduces consumption. This feature is only available when using a hardware RTC. It can be enabled/disabled in `config.h` or remotely.
* **SPS30 Fan Stop**: The SPS30 sensor fan has relatively high power consumption. The firmware allows stopping the fan after data readout and starting it only before the next scheduled measurement (considering the stabilization interval). This function can be configured in `config.h` or remotely.

### Deep Sleep Requirements

Deep sleep mode is only available if a hardware RTC is used (`USE_HW_RTC` set to `1` in `config.h`). If no hardware RTC is present, deep sleep will be automatically disabled, even if enabled via OTA configuration.

## Deployment recommendation

Based on a study published in my diploma thesis, I do not recommend using the MCU's internal clock for timekeeping due to significant drift over time. Instead, I suggest utilizing the DS3231 Real-Time Clock (RTC) module.

**Corresponding `config.h` Settings:**

* `USE_HW_RTC`: Enable the use of hardware RTC. Set to `1` to use an external RTC module, `0` for software timekeeping by MCU.
* `RTC_TYPE`: Type of RTC chip used (e.g., `RTC_DS3231`). (Used only if `USE_HW_RTC` is `1`).

The use of an RTC module ensures stable timekeeping and allows us to set a long `REALTIME_RESYNC_INTERVAL_DAYS` re-synchronization interval, minimizing excessive network usage for this purpose. You should keep downlink messages to a minimum, as uplinks can [impact network performance](https://www.thethingsnetwork.org/docs/lorawan/limitations/).

## Tools

The repository includes several tools to assist with data processing and RTC synchronization via serial. These tools can be helpful for further development and debugging.

Below is an overview of the available tools:

### 1. **CSV to Excel Converter**

* **File:** [tools/convertData/CSVtoXLS.py](https://github.com/Vit-Kolar/New-OSU-LoRa-Station/blob/master/tools/convertData/CSVtoXLS.py)

* **Description:** Converts a CSV file with uplinks from the DB server to an Excel file (`.xlsx`), while removing the `raw_json` column.

* **Usage:**
  1. Place your CSV file in the same directory as the script and name it `data.csv` (or modify the script to use a different file name).
  2. Run the script using Python:
  
     ```bash
     python CSVtoXLS.py
     ```

  3. The output Excel file (`data.xlsx`) will be created in the same directory (or modify the script to change the output file name).

* **Dependencies:** Requires the [pandas](https://pypi.org/project/pandas/) and [openpyxl](https://pypi.org/project/openpyxl/) Python libraries.

### 2. **Set RTC Time via Serial**

* **File:** [tools/SetTimeSerial/SetTimeSerial.ino](https://github.com/Vit-Kolar/New-OSU-LoRa-Station/blob/master/tools/SetTimeSerial/SetTimeSerial.ino)

* **Description:** Arduino sketch for setting the time on an RTC module (e.g., DS3231) via serial input.

* **Usage:**
  1. Upload the sketch to an Arduino board connected to the RTC module.
  2. Open the Serial Monitor in the Arduino IDE and input the time in the following format:
  
     ```bash
     YYYY MM DD HH MM SS
     ```

  3. The RTC will be adjusted to the provided time, and the current time will be printed to the Serial Monitor.

* **Purpose:** Useful for initial setup or manual adjustment of the RTC time.

### **Firmware Integration**

The station firmware includes this functionality for manually synchronizing the RTC (Real-Time Clock) via serial input. This feature can be helpful during the initial setup, when network-based time synchronization is unavailable, for testing and development purposes.

* **Usage:**
  1. Open the `config.h` file in the firmware source code.
  2. Make sure `USE_HW_RTC` and `DEBUG` are set to `1`.
  3. Enable one or both of the following options by setting them to `1`: `SET_RTC_FROM_SERIAL` and `TEST_RTC_VS_LORA_TIME`.
  4. The controller will prompt you to enter the time via serial by blinking an LED for a period of 20 seconds.  
     For this purpose, use tool **3. Synchronize RTC with PC Time**.

### 3. **Synchronize RTC with PC Time**

* **File:** [tools/RTCsync/syncFromPC.py](https://github.com/Vit-Kolar/New-OSU-LoRa-Station/blob/master/tools/RTCsync/syncFromPC.py)

* **Description:** Python script that sends the current PC time to the microcontroller in the same format expected by the tool **2. Set RTC Time via Serial**. This allows for automatic synchronization of the RTC module over a serial connection.

* **Usage:**
  1. Connect your RTC module to a microcontroller and ensure it is ready to receive time data via serial.
  2. Update the `SERIAL_PORT` variable in the script to match your serial port (e.g., `COM4`).
  3. Run the script using Python:
  
     ```bash
     python syncFromPC.py
     ```

  4. The script will send the current PC time to the controller in the format `YYYY MM DD HH MM SS` and display the serial output in the terminal.

* **Dependencies:** Requires the [pyserial](https://pypi.org/project/pyserial/) Python library.

* **Purpose:** Automates the process of setting the RTC to the current PC time, ensuring accurate synchronization.
