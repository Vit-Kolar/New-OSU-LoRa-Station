#ifndef CONFIG_H
#define CONFIG_H

#define FIRMWARE_CONFIG_VERSION 1 //go back to 0 if more than 254 (255 -> 0xff is cleared state) change every time you want new configuration to be used and saved to EEPROM!!!
#define DEBUG 0

//IMPORTANT!!! Enable lora.LoRaWANreceived parameter for lora.timeLinkCheck 
//for enabling uncoment #define SLIM_DEBUG_VARS in SlimLoRa.h or use buildflag -DSLIM_DEBUG_VARS
//for enabling uncoment #define EPOCH_RX2_WINDOW_OFFSET in SlimLoRa.h or use buildflag -DEPOCH_RX2_WINDOW_OFFSET

// Enable LoRaWAN Over-The-Air Activation
#define LORAWAN_OTAA_ENABLED    1
#define LORAWAN_KEEP_SESSION    1 // Store the session data to EEPROM

// Set data rate (LoRa SF + BW combination) – maps to SlimLoRa.h macro
#define DATA_RATE SF10BW125  //SF7BW125, SF9BW125, SF10BW125, SF11BW125, SF12BW125 when SF12BW125 have max range, but lowest data rate

//Timekeeping
const uint32_t GPS_TO_UNIX_OFFSET = 315964800UL;
#define TIMEZONE_OFFSET_HOURS   2  // UTC+2 central european time
#define USE_HW_RTC 1 // 1 = use hardware RTC, 0 = use software RTC 


#if USE_HW_RTC
  #define RTC_TYPE RTC_DS3231
  #if DEBUG == 1
    #define SET_RTC_FROM_SERIAL 0 // 1 = set RTC from serial input
    #define TEST_RTC_VS_LORA_TIME 0 // 1 = test RTC vs LoRa time
  #endif
#endif

// Over-the-Air Configurable Settings - change FIRMWARE_CONFIG_VERSION before flashing!!!!!
#define SEND_INTERVAL_MINUTES               60 // define send interval in minutes !!!keep in multiples of 5 -> 5, 10, 30, 60 !!!
#define SPS_CLEAN_INTERVAL_DAYS             7  // in days - cleaning interval for the fan
#define SPS_STABILIZATION_PREREADOUT_DELAY  5  // time in minutes for measurement to start, before data readout if sps30StopAfterReadout is true 
#define SPS_STOP_AFTER_READOUT              1  // Stop measurement after data readout. If false, it will start sps30StabilizationPreReadoutDelay minutes before the next sendIntervalMinutes slot
#define REALTIME_RESYNC_INTERVAL_DAYS       7  // in days - time resynchronisation interval for the real time
#define OVERRIDE_TIME_SYNCHRONIZATION       0  // 0 = send data synchronized with time, 1 = send data based just on time interval 
#define ALLOW_DEEP_SLEEP                    0  // 1 = allow deep sleep, 0 = do not allow deep sleep


// Over-the-Air Configurable Settings BACKUP EEPROM ADDRESS
#define EEPROM_FIRMWARE_CONFIG_VERSION 200 // EEPROM backup block starting from 200 (because SlimLoRa uses 0-151 v0.7.5 so to safly avoid collision)) end at 208 (8 bytes for backup + firmware version)
#define EEPROM_SEND_INTERVAL 201 // sendIntervalMinutes backup it is unit16_t, so 2 bytes!
#define EEPROM_SPS_CLEAN_INTERVAL 203 // spsCleanIntervalDays backup
#define EEPROM_SPS_STABILIZATION_PRE_READOUT_DELAY 204 // spsStabilizationPreReadoutDelay backup
#define EEPROM_SPS_STOP_AFTER_READOUT 205 // spsStopAfterReadout backup
#define EEPROM_REAL_TIME_RESYNC_INTERVAL 206 // realTimeResyncIntervalDays backup
#define EEPROM_OVERRIDE_TIME_SYNCHRONIZATION 207 // overrideTimeSynchronization backup
#define EEPROM_ALLOW_DEEP_SLEEP 208 // allowDeepSleep backup
#define EEPROM_DEVEUI   209 // DevEUI storage to know if the session is changed - 8 bytes!



#define SPS30_DEFAULT_STABILIZATION_TIME 3 // in minutes - time for the SPS30 to stabilize before data readout
#define SENSORS_MEASUREMENT_DELAY 135 // about a time to readout all used sensors in milliseconds (SPS30, HTU21D)

// LoRaWAN settings - set the keys registred for the device 
#if LORAWAN_OTAA_ENABLED
extern const uint8_t DevEUI[8] = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
extern const uint8_t JoinEUI[8] = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };
extern const uint8_t AppKey[16] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
#else
const uint8_t NwkSKey[16] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
const uint8_t AppSKey[16] = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
const uint8_t DevAddr[4] = { 0x00, 0x00, 0x00, 0x00 };
#endif 

#endif

#if DEBUG == 1
  #define DBG_SERIAL_BEGIN(x) Serial.begin(x);
  #define DBG_PRINTF(x, y) Serial.printf(x, y);
  #define DBG_PRINT(x) Serial.print(x);
  #define DBG_PRINT_DEC(x) Serial.print(x, DEC);
  #define DBG_PRINTLN_DEC(x) Serial.println(x, DEC);
  #define DBG_PRINT_HEX(x) Serial.print(x, HEX);
  #define DBG_PRINTLN_HEX(x) Serial.println(x, HEX);
  #define DBG_PRINTLN(x) Serial.println(x);
  #define DBG_FLUSH() Serial.flush();
  #define DBG_PRINT_CURRENT_TIME() printCurrentTime();
  #define DBG_PRINT_RTC_TIME() printRTCTime();
#else
  #define DBG_SERIAL_BEGIN(x)
  #define DBG_PRINTF(X, Y)
  #define DBG_PRINT(x)
  #define DBG_PRINT_DEC(x)
  #define DBG_PRINTLN_DEC(x)
  #define DBG_PRINT_HEX(x)
  #define DBG_PRINTLN_HEX(x)
  #define DBG_PRINTLN(x)
  #define DBG_FLUSH()
  #define DBG_PRINT_CURRENT_TIME()
  #define DBG_PRINT_RTC_TIME()
#endif