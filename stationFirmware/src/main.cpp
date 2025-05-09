#include <stdint.h>
#include <Arduino.h>
#include <RTClib.h> 
#include "config.h"
#include <SlimLoRa.h>
#include <Adafruit_HTU21DF.h>
#include <sps30.h>
#include <TimeLib.h> 
#include <avr/sleep.h>
#include <avr/wdt.h>
#include <avr/interrupt.h>

// Over-the-Air Configurable Settings - change FIRMWARE_CONFIG_VERSION before flashing!!!!!
uint16_t sendIntervalMinutes = SEND_INTERVAL_MINUTES;                         // define send interval in minutes !!!keep in multiples of 5 -> 5, 10, 30, 60 !!!
uint8_t spsCleanIntervalDays = SPS_CLEAN_INTERVAL_DAYS;                       // in days - cleaning interval for the fan
uint8_t spsStabilizationPreReadoutDelay = SPS_STABILIZATION_PREREADOUT_DELAY; // time in minutes for measurement to start, before data readout if sps30StopAfterReadout is true 
uint8_t spsStopAfterReadout = SPS_STOP_AFTER_READOUT;                         // Stop measurement after data readout. If false, it will start sps30StabilizationPreReadoutDelay minutes before the next sendIntervalMinutes slot
uint8_t realTimeResyncIntervalDays = REALTIME_RESYNC_INTERVAL_DAYS;           // in days - time resynchronisation interval for the real time
uint8_t overrideTimeSynchronization = OVERRIDE_TIME_SYNCHRONIZATION;          // 0 = send daty synchronized with time, 1 = send data based just on time interval 
uint8_t allowDeepSleep = USE_HW_RTC ? ALLOW_DEEP_SLEEP : 0 ;                  // IF there is no USE_HW_RTC, the deep sleep is not allowed.

#if USE_HW_RTC
  RTC_TYPE rtc;   // RTC object - define based on used module
#endif

#if DEBUG
uint32_t joinTime, RX2End;
#endif

// main payload variables
uint8_t payload[25];                  // payload array for data to be sent
uint8_t payload_length = sizeof(payload); 
uint8_t fport = 1;                   // fport for the data to be sent
uint32_t waitAfterJoin = 30;         // in seconds


        

//initialize LoRaWAN object - pin 8 is used for the RFM95 module
SlimLoRa lora = SlimLoRa(8);

//sensor variables
static Adafruit_HTU21DF htu = Adafruit_HTU21DF();
float temp = NAN;
float hum = NAN;
sps30_measurement sps30_data;
struct sps30_measurement m;
uint16_t data_ready;
int16_t ret;

//slot variables
uint32_t nextSlotEpoch = 0;   
uint32_t lastSyncEpoch = 0;
uint32_t lastSentSlot = 0;

//time resync intervals in minutes
uint16_t syncFailedResyncIntervalsInMinutes[8] = {0,5, 30, 60, 120, 300, 720, 1440}; 

//EEPROM Storage functions
void clearSessionEEPROM(); 
void loadConfigFromEEPROM();
void saveConfigToEEPROM();
void manageSessionKeyChange();

//OTA config and report functions
void processDownlink();
void reportSettingsByUplink();

//uplink formatters
void saveToPayload(float data, uint8_t *payload, int position);
void saveToPayload(sps30_measurement &data, uint8_t *payload, int position);
uint16_t f2sflt16(float f);

//time
void synchronizeTime();
uint32_t getTimeRequestTimestamp();
void printDateTime(int y, int mo, int d, int h, int mi, int s);
void waitUntilNextSlot();
void checkForTimeResync();
#if DEBUG
void printCurrentTime();
#endif

// deepSleep
volatile bool watchdogFired = false;
ISR(WDT_vect) {
  watchdogFired = true;
}
void deepSleepMillis(uint32_t milliseconds);
void setupWatchdog(uint8_t timeout);

void setup(){
  #if DEBUG
    Serial.begin(9600);
    while (!Serial)
      ; // don't start unless we have serial connection
    DBG_PRINTLN("Starting");
  #endif
    manageSessionKeyChange();

    loadConfigFromEEPROM();
    
    #if DEBUG
    pinMode(LED_BUILTIN, OUTPUT);
    #endif
   
    /*
    // turn on the LED - Just for testing to mark the station :)
    pinMode(LED_BUILTIN, OUTPUT);
    digitalWrite(LED_BUILTIN, HIGH);
    */

    htu.begin();
DBG_PRINTLN(F("HTU21D sensor initialized."));
    delay(1000);
    lora.Begin();
    lora.Join(); // Join the network;
    lora.SetPower(14);
    lora.SetDataRate(SF10BW125); //SF7BW125, SF9BW125, SF10BW125, SF11BW125, SF12BW125 when SF12BW125 have max range, but lowest data rate
    DBG_PRINTLN(lora.GetHasJoined() ? "OK" : "NO");
    uint8_t joinCounter = 0;
  #if LORAWAN_OTAA_ENABLED
  #if LORAWAN_KEEP_SESSION
    while (!lora.GetHasJoined())
    {
  #else
    while (!lora.HasJoined())
    {
  #endif // LORAWAN_KEEPSESSION
      DBG_PRINTLN(F("\nJoining..."));
      #if DEBUG
        joinTime = millis() / 1000;
      #endif
      joinCounter++;
      lora.Begin();
      lora.Join();
      if (joinCounter > 10){
        clearSessionEEPROM();
        joinCounter = 0;
      }
  #if DEBUG
      RX2End = millis() / 1000;
      DBG_PRINTLN(F("\nJoin effort finished."));
      DBG_PRINT(F("\nRx2End after: "));
      DBG_PRINT(RX2End - joinTime);
  #endif
      if (lora.HasJoined()){
        DBG_PRINTLN(F("\nJoined Sending packet in half minute."));
        delay(waitAfterJoin * 1000);
        break;
      }
      delay(5 * 1000);
    }
  #endif // LORAWAN_OTAA_ENABLED
  #if USE_HW_RTC
    if (!rtc.begin()){
      DBG_PRINTLN("Couldn't find RTC");
      while (1)
        ;
    }
    if (rtc.lostPower()){
      DBG_PRINTLN("RTC lost power, setting time...");
      #if SET_RTC_FROM_SERIAL
      DBG_PRINTLN("Enter date and time (YYYY MM DD HH mm ss): ");
      DBG_PRINTLN("Example: 2023 10 01 12 00 00");
      unsigned long startMillis = millis();
      while (Serial.available() < 12 && millis() - startMillis < 20000){

        digitalWrite(LED_BUILTIN, HIGH); // LED on
        delay(50);
        digitalWrite(LED_BUILTIN, LOW); // LED off
        delay(50);
      }

      if (Serial.available() >= 12){
        int year = Serial.parseInt();
        int month = Serial.parseInt();
        int day = Serial.parseInt();
        int hour = Serial.parseInt();
        int minute = Serial.parseInt();
        int second = Serial.parseInt();

        rtc.adjust(DateTime(year, month, day, hour, minute, second));
        DBG_PRINTLN("RTC was set!");
        digitalWrite(LED_BUILTIN, HIGH);
        delay(40000);
        digitalWrite(LED_BUILTIN, LOW);
      }
    #else
    synchronizeTime();
    #endif
    
  }

  #else
  #if TEST_RTC_VS_LORA_TIME
    DBG_PRINT_CURRENT_TIME();
    DBG_PRINT_RTC_TIME();
    synchronizeTime();
    DBG_PRINTLN(F("\nRTC time synchronized."));
    DBG_PRINT_CURRENT_TIME();
    DBG_PRINT_RTC_TIME();
  #endif
    synchronizeTime();
  #endif
  DBG_PRINT_CURRENT_TIME();
  sps30_start_measurement();
  sps30_set_fan_auto_cleaning_interval_days(spsCleanIntervalDays);
  DBG_PRINTLN(F("SPS30 fan auto cleaning interval set."));
  

}

  void loop() {

    waitUntilNextSlot(); // Wait until the next slot to send data


    if(spsStopAfterReadout == 1){ // Start measurement to szabilize sps if spsStopAfterReadout power save mode flag is set
      sps30_start_measurement();
      DBG_PRINT(F("SPS30 measurement started."));
    }
  
    #if USE_HW_RTC  
      uint32_t waitTime = millis() + ((nextSlotEpoch - rtc.now().unixtime()) * 1000);
    #else

      uint32_t waitTime = millis() +  ((nextSlotEpoch - now()) * 1000);
    #endif

    DBG_PRINT_CURRENT_TIME();
    DBG_PRINT(F("nextSlotEpoch: "));DBG_PRINTLN(nextSlotEpoch);
    DBG_PRINT(F("waitTime: "));DBG_PRINTLN(waitTime);


   while (millis() <= (waitTime - SENSORS_MEASUREMENT_DELAY)) 
   {
    delay(100);
   }
   
    temp = htu.readTemperature();
    hum = htu.readHumidity();
    
    do
        {
            ret = sps30_read_data_ready(&data_ready);
            if (ret < 0)
            {
              DBG_PRINTLN(F("SPS30 measure error"));
              DBG_PRINTLN(ret);
            }
            else if (data_ready) break;
            DBG_PRINT(F("SPS30 data not ready..."));
            delay(100);
        } while (1);

        ret = sps30_read_measurement(&m);

        saveToPayload(temp, payload, 0); // Save teperature to payload at [0] and [1]
        saveToPayload(hum, payload, 2);  // Save humidity to payload at [2] and [3]
        saveToPayload(m, payload, 4);    // Save SPS data to payload at [4] - [23]

    DBG_PRINT(("sending:"));DBG_PRINT_CURRENT_TIME();
    lora.SendData(fport, payload, payload_length); // Send data to LoRaWAN network
    DBG_PRINT(F("\nLoRaWAN packet send."));
    processDownlink();                            // Check and process downlink data
    if(!overrideTimeSynchronization){
      checkForTimeResync();
    }

  if(spsStopAfterReadout == 1){// Stop measurement to save power if flag is set
    sps30_stop_measurement(); 
    DBG_PRINT(F("SPS30 measurement stopped."));
   }

  }

  // save 16 bit float to 8 bit payload array 
  void saveToPayload(float data, uint8_t *payload, int position)
  {
    uint16_t payloadTmp = f2sflt16(data / 100);
    byte low = lowByte(payloadTmp);
    byte high = highByte(payloadTmp);

    payload[position] = low;
    payload[position + 1] = high;

  }
// assemble payload part for sps30 data
void saveToPayload(sps30_measurement &data, uint8_t *payload, int position)
{
    saveToPayload(data.mc_1p0, payload, position);
    saveToPayload(data.mc_2p5, payload, position + 2);
    saveToPayload(data.mc_4p0, payload, position + 4);
    saveToPayload(data.mc_10p0, payload, position + 6);
    saveToPayload(data.nc_0p5, payload, position + 8);
    saveToPayload(data.nc_1p0, payload, position + 10);
    saveToPayload(data.nc_2p5, payload, position + 12);
    saveToPayload(data.nc_4p0, payload, position + 14);
    saveToPayload(data.nc_10p0, payload, position + 16);
    saveToPayload(data.typical_particle_size, payload, position + 18);
}
//formatting float to 16 bit unsigned int for LoRaWAN payload
uint16_t f2sflt16(float f) {
  if (f <= -1.0f)
      return 0xFFFF;  // Overflow for negative values outside the range.
  else if (f >= 1.0f)
      return 0x7FFF;  // Overflow for positive values outside the range.
  else {
      int iExp;
      float normalValue;
      uint16_t sign = 0;
      // Normalization: frexpf returns the normalized value and the exponent.
      normalValue = frexpf(f, &iExp);
      // Check for negative sign
      if (normalValue < 0) {
          sign = 0x8000;  
          normalValue = -normalValue;
      }
      // Adjust the exponent to match the range expected by the format (biased by 15)
      iExp += 15;
      if (iExp < 0) iExp = 0;  
      // Compute the fractional part (mantissa) and handle rounding
      uint16_t outputFraction = (uint16_t)(ldexpf(normalValue, 11) + 0.5f);
      if (outputFraction >= (1 << 11)) {
          outputFraction = 1 << 10;  // If fraction exceeds 11 bits, saturate it
          iExp++;  // Increase the exponent to compensate
      }
      // Check for overflow of the exponent, saturate at max value (exponent=31)
      if (iExp > 15)
          return 0x7FFF | sign;
      // Combine sign, exponent, and mantissa into a 16-bit value
      return (uint16_t)(sign | (iExp << 11) | outputFraction);
  }
}

#if DEBUG
//just print time
void printCurrentTime() {
  #if USE_HW_RTC
    DateTime now = rtc.now();
    DBG_PRINT("RTC date/time: ");
    printDateTime(now.year(), now.month(), now.day(), now.hour(), now.minute(), now.second());
  #else 
  //if (currentUnixEpoch == 0) {
    if (now() == 0) {
      DBG_PRINTLN("GPS epoch not set yet."); // time n0t synchronized
      return;
    }
    //setTime(currentUnixEpoch); // Přepočítá podle TimeLib
    DBG_PRINT("GPS date/time: ");
    printDateTime(year(), month(), day(), hour(), minute(), second());
  #endif
}
// print time format
void printDateTime(int y, int mo, int d, int h, int mi, int s) {
  DBG_PRINT(y); DBG_PRINT("-");
  if (mo < 10) DBG_PRINT("0");
  DBG_PRINT(mo); DBG_PRINT("-");
  if (d < 10) DBG_PRINT("0");
  DBG_PRINT(d); DBG_PRINT(" ");
  if (h < 10) DBG_PRINT("0");
  DBG_PRINT(h); DBG_PRINT(":");
  if (mi < 10) DBG_PRINT("0");
  DBG_PRINT(mi); DBG_PRINT(":");
  if (s < 10) DBG_PRINT("0");
  DBG_PRINTLN(s);
}
#endif 
// Time synchronization - if it fails, it will try to resync in the specified intervals
void synchronizeTime() {
  DBG_PRINTLN("Synchronizing time...");
  uint32_t gpsEpoch = 0; // Request time synchronization from the network
  for (int i = 0; i < 8; i++) {
    delay(((unsigned long)syncFailedResyncIntervalsInMinutes[i]) * 60 * 1000); // Wait for the specified time interval
    gpsEpoch = getTimeRequestTimestamp(); // Retry time synchronization
    DBG_PRINT("GPS epoch: ");DBG_PRINTLN(gpsEpoch);
    if (gpsEpoch != 0) break;
  }
  if (gpsEpoch == 0) {
    DBG_PRINTLN("Failed to synchronize time.");
    overrideTimeSynchronization = 1; // Set flag to turn off time synchronization
    reportSettingsByUplink(); // Report settings to the network
    return;
  }   

  #if USE_HW_RTC
    DateTime now(gpsEpoch + GPS_TO_UNIX_OFFSET + (TIMEZONE_OFFSET_HOURS * 3600)); // lora epoch is GPS epoch -> has to be converted to unix epoch + timezone offset
    rtc.adjust(now);
    lastSyncEpoch = rtc.now().unixtime();
    DBG_PRINT_CURRENT_TIME();
    DBG_PRINT("GPS epoch: ");
    DBG_PRINTLN(gpsEpoch);
    DBG_PRINT("fracSecond ms: ");
    DBG_PRINTLN(lora.fracSecond);
    #if TEST_RTC_VS_LORA_TIME
      DBG_PRINT("RTC sync after: ");
      DBG_PRINT_RTC_TIME();
      DBG_PRINTLN(gpsEpoch);
      unsigned long startMillis = millis();
      while (Serial.available() < 12 && millis() - startMillis < 20000) {

      digitalWrite(LED_BUILTIN, HIGH);  // LED on
      delay(50);
      digitalWrite(LED_BUILTIN, LOW);   // LED off
      delay(50);
      }
      if (Serial.available() >= 12) {
        int year = Serial.parseInt();
        int month = Serial.parseInt();
        int day = Serial.parseInt();
        int hour = Serial.parseInt();
        int minute = Serial.parseInt();
        int second = Serial.parseInt();

        DBG_PRINT("Serial Time: ");
        printDateTime(year, month, day, hour, minute, second);
        DBG_PRINT_RTC_TIME();
      }
    #endif
  #else 
    setTime((gpsEpoch + GPS_TO_UNIX_OFFSET + (TIMEZONE_OFFSET_HOURS * 3600))); // Set the time 
    lastSyncEpoch = now(); 
  #endif

  return;
}
// proceeds MAC DeviceTimeReq and returns LoRaWAN timestamp in GPS epoch format
uint32_t getTimeRequestTimestamp() {
  uint32_t ret = 0;
  lora.epoch = 0;
  lora.LoRaWANreceived = 0;
  lora.TimeLinkCheck = 1; // Request time synchronization
  uint8_t emptyPayload[1] = {0}; // Empty payload for the request

  DBG_PRINTLN("Sending request...");

  lora.SendData(3, emptyPayload, 1); // Send the request port 3 has no formater setup 

  DBG_PRINT("Time request sent");
  DBG_PRINT("LoRaWAN flags: ");
  DBG_PRINTLN_HEX(lora.LoRaWANreceived);

  if ((lora.LoRaWANreceived & 0x40) == 0x40) {
      ret = lora.epoch;
  }

  DBG_PRINT("GPS epoch: ");
  DBG_PRINTLN(lora.epoch);
  DBG_PRINT_CURRENT_TIME();
  DBG_PRINT("RAW epoch from network: ");
  DBG_PRINTLN(lora.epoch);

  return ret;
}
// check if the time resync is schadduled for now - if the last sync was more than realTimeResyncIntervalDays days ago
void checkForTimeResync() {
  uint32_t currentEpoch;
  #if USE_HW_RTC
    DateTime now = rtc.now();
    currentEpoch = now.unixtime(); // aktuální čas v epoch
  #else
    currentEpoch = now(); 
  #endif
  if(lastSyncEpoch == 0){
    lastSyncEpoch = currentEpoch - (realTimeResyncIntervalDays * 86400UL)/3; // set last sync to 3 days ago if it is not set yet or was lost due to power off
  }
  if ((currentEpoch - lastSyncEpoch) >= (realTimeResyncIntervalDays * 86400UL)) {
    synchronizeTime(); // Synchronize time if the interval has passed
  }
}
// clear EEPROM SlimLoRa session data - for change of the session keys or for testing
void clearSessionEEPROM() {
  DBG_PRINTLN(F("Clearing session EEPROM..."));
  for (uint16_t addr = 0; addr < EEPROM_END; addr++) {
      EEPROM.write(addr, 0xFF);  // Nastaví všechny byty na 0xFF, což je vymazaný stav
  }
}
// Read configuration from EEPROM - save config settings to eeprom if config version changes 
void loadConfigFromEEPROM() {
  if (EEPROM.read(EEPROM_FIRMWARE_CONFIG_VERSION) != FIRMWARE_CONFIG_VERSION) { 
    saveConfigToEEPROM(); // backup configuration to EEPROM if the EEPROM is empty or the version is different
    return;
  }else{
    DBG_PRINTLN("EEPROM backup found, reading...");
    sendIntervalMinutes = EEPROM.read(EEPROM_SEND_INTERVAL) | (EEPROM.read(EEPROM_SEND_INTERVAL+1) << 8);
    spsCleanIntervalDays = EEPROM.read(EEPROM_SPS_CLEAN_INTERVAL); 
    spsStabilizationPreReadoutDelay = EEPROM.read(EEPROM_SPS_STABILIZATION_PRE_READOUT_DELAY);
    spsStopAfterReadout = EEPROM.read(EEPROM_SPS_STOP_AFTER_READOUT); 
    realTimeResyncIntervalDays = EEPROM.read(EEPROM_REAL_TIME_RESYNC_INTERVAL); 
    overrideTimeSynchronization = EEPROM.read(EEPROM_OVERRIDE_TIME_SYNCHRONIZATION); 
    allowDeepSleep = EEPROM.read(EEPROM_ALLOW_DEEP_SLEEP); 
    DBG_PRINT(F("EEPROM backup loaded"));
  }
  return;
}
// Save configuration to EEPROM - for keeping OTA configurable settings after power off
void saveConfigToEEPROM() {
    DBG_PRINTLN("Backing up configuration to EEPROM...");
    EEPROM.write(EEPROM_FIRMWARE_CONFIG_VERSION, FIRMWARE_CONFIG_VERSION); 
    EEPROM.write(EEPROM_SEND_INTERVAL, (sendIntervalMinutes & 0xFF)); 
    EEPROM.write(EEPROM_SEND_INTERVAL + 1, ((sendIntervalMinutes >> 8) & 0xFF)); 
    EEPROM.write(EEPROM_SPS_CLEAN_INTERVAL, spsCleanIntervalDays); 
    EEPROM.write(EEPROM_SPS_STABILIZATION_PRE_READOUT_DELAY, spsStabilizationPreReadoutDelay); 
    EEPROM.write(EEPROM_SPS_STOP_AFTER_READOUT, spsStopAfterReadout); 
    EEPROM.write(EEPROM_REAL_TIME_RESYNC_INTERVAL, realTimeResyncIntervalDays); 
    EEPROM.write(EEPROM_OVERRIDE_TIME_SYNCHRONIZATION, overrideTimeSynchronization); 
    EEPROM.write(EEPROM_ALLOW_DEEP_SLEEP, allowDeepSleep); 
  return;
}
void manageSessionKeyChange() {
  for (uint8_t i = 0; i < 8; i++)
  {
    DBG_PRINT(F(" config: ")); DBG_PRINT_HEX(DevEUI[i]);DBG_PRINT(F(" EEPROM: ")); 
    DBG_PRINT_HEX(EEPROM.read(EEPROM_DEVEUI + i)); 
    if (EEPROM.read(EEPROM_DEVEUI + i) != DevEUI[i]) {
      DBG_PRINTLN(F(" Session key changed, clearing EEPROM session."));
      clearSessionEEPROM(); // Clear EEPROM if the session keys are different
      for (uint8_t j = 0; j < 8; j++)
      {
        EEPROM.write(EEPROM_DEVEUI + j, DevEUI[j]); // Save the new DevEUI to EEPROM
      }
      return;
    }
    
  }
  DBG_PRINTLN(F(" Session keys checked."));
  return;
}
// control the time slotting - wait for the next time slot to send data
void waitUntilNextSlot() {
  uint32_t nowEpoch;
  if(overrideTimeSynchronization == 0){
    #if USE_HW_RTC
      DateTime now = rtc.now();
      nowEpoch = now.unixtime();
    #else
      if (now() == 0) {
        DBG_PRINTLN(F("Time not synchronized, resyncing..."));
        synchronizeTime();
      }
      nowEpoch = now(); 
    #endif

  
    uint32_t currentSlot = (nowEpoch / 60) / sendIntervalMinutes;
    nextSlotEpoch = (currentSlot * sendIntervalMinutes * 60) + (sendIntervalMinutes * 60);
    if (currentSlot == lastSentSlot) {
      currentSlot = currentSlot + sendIntervalMinutes * 60;
    }

    lastSentSlot = currentSlot; 

    uint8_t currentMinute = (nowEpoch / 60) % 60;
    uint8_t currentSecond = nowEpoch % 60;

    uint8_t remainder = currentMinute % sendIntervalMinutes;
    uint8_t waitMinutes = (remainder == 0 && currentSecond == 0) ? 0 : sendIntervalMinutes - remainder;
    uint16_t waitSeconds = (waitMinutes * 60) - currentSecond;
    if(spsStopAfterReadout == 1){
      if (waitSeconds >= spsStabilizationPreReadoutDelay * 60 ) { 
        waitSeconds = (waitSeconds - spsStabilizationPreReadoutDelay * 60); 
      }
      else{
        return; 
      }
    }
    DBG_PRINT(F("waiting for next slot: "));
    DBG_PRINT(waitSeconds);
    DBG_PRINTLN(F(" seconds."));

    if (waitSeconds > 0){
      if (allowDeepSleep == 1)
      {
        deepSleepMillis((waitSeconds * 1000UL));
      }else{
        delay(waitSeconds * 1000UL);
      }
    }
  }else{
    if (allowDeepSleep == 1)
    {
      deepSleepMillis(sendIntervalMinutes * 60 * 1000UL); // wait for the next slot if synchronisation by real time is overriden
    }else{
      delay((sendIntervalMinutes * 60 * 1000U) - SENSORS_MEASUREMENT_DELAY); // wait for the next slot if synchronisation by real time is overriden no deep sleep allowed
    }
      
  }
}
// Setup watchdog timer
void setupWatchdog(uint8_t timeout) {
  MCUSR &= ~(1 << WDRF); // Clear the watchdog reset flag
  WDTCSR = (1 << WDCE) | (1 << WDE);
  WDTCSR = (1 << WDIE) | timeout;
}
// Go to sleep for a specified number of milliseconds
void deepSleepMillis(uint32_t ms)
{
  
  const struct
  {
    uint16_t duration;
    uint8_t wdt_setting;
  } wdt_options[] = {
      {8000, WDTO_8S},
      {4000, WDTO_4S},
      {2000, WDTO_2S},
      {1000, WDTO_1S},
      {500, WDTO_500MS},
      {250, WDTO_250MS},
      {120, WDTO_120MS},
      {60, WDTO_60MS},
      {30, WDTO_30MS},
      {15, WDTO_15MS}};
  for (; ms > 0;)
  {
    for (uint8_t i = 0; i < sizeof(wdt_options) / sizeof(wdt_options[0]); i++)
    {
      if (ms >= wdt_options[i].duration)
      {
        MCUSR &= ~(1 << WDRF);
        WDTCSR |= (1 << WDCE) | (1 << WDE);
        WDTCSR = (1 << WDIE) | wdt_options[i].wdt_setting;

        set_sleep_mode(SLEEP_MODE_PWR_DOWN);
        cli();
        sleep_enable();
        sei();
        sleep_cpu(); // ZZZ...
        sleep_disable();

        ms -= wdt_options[i].duration;
        break;
      }
    }
  }
}
// process incoming data from the server - mainly used for OTA configuration changes
void processDownlink(){
  if ( lora.downlinkSize > 0 ) {
    uint16_t payload16b = 0;
    uint8_t payload = 0;
    switch (lora.downPort) {
      case 1: // SEND INTERvAL - send 2 bytes - uint16_t to port 1 - sets the interval for sending data in minutes !!!keep in multiples of 5 -> 5, 10, 30, 60 !!!
          payload16b = (lora.downlinkData[0] << 8) | lora.downlinkData[1];
          payload16b = (payload16b / 5) * 5; //keep in multiples of 5
          if (payload16b < 5) { 
              payload16b = 5;  // minimum 5 minutes
          }
          sendIntervalMinutes = payload16b;
          EEPROM.write(EEPROM_SEND_INTERVAL, (sendIntervalMinutes & 0xFF)); 
          EEPROM.write(EEPROM_SEND_INTERVAL + 1, ((sendIntervalMinutes >> 8) & 0xFF)); 
          reportSettingsByUplink(); // send the report back to the server to confirm the change
        break;    
      case 2: // SPS30 CLEANING INTERVAL - send 0-255 to port 2 - sets cleaning interval for the fan
          payload = lora.downlinkData[0];
          if (payload < 1) { 
              payload = 7;  // if less than 1, set to 7 days as default
          }
          spsCleanIntervalDays = payload;
          sps30_set_fan_auto_cleaning_interval_days(spsCleanIntervalDays); // set the cleaning interval for the fan
          EEPROM.write(EEPROM_SPS_CLEAN_INTERVAL, spsCleanIntervalDays); 
          reportSettingsByUplink(); // send the report back to the server to confirm the change 
        break;
      case 3: // SPS30 STABILIZATION PRE READOUT DELAY  - send 0-255 to port 3 - time in minutes for SPS30 to start, before data readout if sps30MeasurementStart is true
        payload = lora.downlinkData[0];
        if(payload > sendIntervalMinutes){
          payload = sendIntervalMinutes; // if more than sendIntervalMinutes, set to sendIntervalMinutes
        }  
        if (payload < 1 || payload == sendIntervalMinutes) { 
          spsStopAfterReadout = 0;  // if less than 1 or more than sendIntervalMinutes do not stop the SPS30
          sps30_start_measurement(); // start the measurement in case it is stopped
        }else{
          spsStopAfterReadout = 1; 
        } 
        spsStabilizationPreReadoutDelay = payload; 
        EEPROM.write(EEPROM_SPS_STABILIZATION_PRE_READOUT_DELAY, spsStabilizationPreReadoutDelay);
        reportSettingsByUplink(); // send the report back to the server to confirm the change 
        break;
      case 4: // SPS30 STOP AFTER READOUT - send 0/1 to port 4 - 1 stops measurement after data readout, starts sps30MeasurementLength minutes before the next sendIntervalMinutes slot. If false, it will run continuously.
          payload = lora.downlinkData[0];
          if(payload == 1){
            spsStopAfterReadout = 1;
            if (spsStabilizationPreReadoutDelay > sendIntervalMinutes) {
              spsStabilizationPreReadoutDelay = sendIntervalMinutes;
            }else if(spsStabilizationPreReadoutDelay < 1){
              spsStabilizationPreReadoutDelay = SPS30_MIN_STABILIZATION_TIME; 
            }
          }else if(payload == 0){
            spsStopAfterReadout = 0; // do not stop the SPS30
            sps30_start_measurement(); // start the measurement in case it is stopped
          }
          EEPROM.write(EEPROM_SPS_STOP_AFTER_READOUT, spsStopAfterReadout);
          reportSettingsByUplink(); // send the report back to the server to confirm the change 
        break;
      case 5: // REAL TIME RESYNcHRONISATION INTERVAL - 0-255 days send to port 5 to set the time resynchronisation interval for the real-time clock
          payload = lora.downlinkData[0];
          if (payload < 1) { 
              payload = 7;  // if less than 1, set to 7 days as default
          }
          realTimeResyncIntervalDays = payload; // set the resynchronisation interval for the real-time clock
          EEPROM.write(EEPROM_REAL_TIME_RESYNC_INTERVAL, realTimeResyncIntervalDays);
          reportSettingsByUplink(); // send the report back to the server to confirm the change
        break;
      case 6: // TIME SYNCHRONISATION OVERRIDE - send 0/1 to port 6 to set the time synchronisation override
          payload = lora.downlinkData[0];
          payload == 1 ? overrideTimeSynchronization = 1 : overrideTimeSynchronization = 0; // set the time synchronisation override
          EEPROM.write(EEPROM_OVERRIDE_TIME_SYNCHRONIZATION, overrideTimeSynchronization);
          reportSettingsByUplink(); // send the report back to the server to confirm the change
          break;
      case 7: // ALLOW DEEP SLEEP - send 0/1 to port 7 to set the deep sleep mode. IF there is no USE_HW_RTC, the deep sleep is not allowed.
          payload = lora.downlinkData[0];
          payload == 1 && USE_HW_RTC ? allowDeepSleep = 1 : allowDeepSleep = 0; // set the deep sleep mode
           EEPROM.write(EEPROM_ALLOW_DEEP_SLEEP, allowDeepSleep);
          reportSettingsByUplink(); // send the report back to the server to confirm the change
        break;
      case 8: // FORCE TIME RESYNCHRINISATION - just send 1 (01) to the port 6...
        if(lora.downlinkData[0] == 1){
          synchronizeTime();
        }
        break;
      case 9: // REQUEST CURRENT SETTINGS REPORT - just send 1 (01) to the port 7...
        if(lora.downlinkData[0] == 1){
          reportSettingsByUplink();
        }
        break;
      default:
          DBG_PRINT("\nUndefined Port\t: ");DBG_PRINT(lora.downPort);
          DBG_PRINT(F("\ndownlinkSize\t: "));DBG_PRINTLN(lora.downlinkSize);
      break;      
    }
  } else { 
    DBG_PRINTLN(F("No downlink data."));
  }
}
// Report settings to the server
void reportSettingsByUplink(){
  uint8_t reportPayload[12];                
  uint8_t fport = 4; // port 4 for settings report             
  #if USE_HW_RTC
    DateTime now = rtc.now();
    uint32_t timestamp = now.unixtime(); 
  #else
    uint32_t timestamp = now(); 
  #endif
  reportPayload[0] = (sendIntervalMinutes & 0xFF);           // sendIntervalMinutes Low byte
  reportPayload[1] = (sendIntervalMinutes >> 8) & 0xFF;      // sendIntervalMinutes High byte
  reportPayload[2] = spsCleanIntervalDays;                   // Cleaning interval (in days)
  reportPayload[3] = spsStabilizationPreReadoutDelay;        // Stabilization delay (in minutes)
  reportPayload[4] = spsStopAfterReadout;                    // Stop after readout flag (1 = stop, 0 = continue)
  reportPayload[5] = realTimeResyncIntervalDays;             // Real-time synchronization interval (in days)
  reportPayload[6] = overrideTimeSynchronization;            // Time synchronization override flag (1 = override, 0 = sync)
  reportPayload[7] = allowDeepSleep;                         // allow deep sleep
  
  // add timestamp (little-endian – LSB first)
  reportPayload[8]  = (timestamp >> 0) & 0xFF;
  reportPayload[9]  = (timestamp >> 8) & 0xFF;
  reportPayload[10] = (timestamp >> 16) & 0xFF;
  reportPayload[11] = (timestamp >> 24) & 0xFF;
  
  lora.SendData(fport, reportPayload, sizeof(reportPayload));
}