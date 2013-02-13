





// This software is a copy of the original Rushduino-OSD project was written by Jean-Gabriel Maurice. http://code.google.com/p/rushduino-osd/
// For more information if you have a original Rushduino OSD <Multiwii forum>  http://www.multiwii.com/forum/viewtopic.php?f=8&t=922
// For more information if you have a Minim OSD <Multiwii forum>  http://www.multiwii.com/forum/viewtopic.php?f=8&t=2918
// For new code releases http://code.google.com/p/rush-osd-development/
// Thanks to all developers that coded this software before us, and all users that also help us to improve.
// This team wish you great flights.



              /***********************************************************************************************************************************************/
              /*                                                            KV_OSD_Team                                                                      */
              /*                                                                                                                                             */
              /*                                                                                                                                             */
              /*                                             This software is the result of a team work                                                      */
              /*                                                                                                                                             */
              /*                                     KATAVENTOS               ITAIN                    CARLONB                                               */
              /*                         POWER67                  LIAM2317             NEVERLANDED                                                           */
              /*                                                                                                                                             */
              /*                                                                                                                                             */
              /*                                                                                                                                             */
              /*                                                                                                                                             */
              /***********************************************************************************************************************************************/




              /************************************************************************************************************************************************/
              /*                         Created for Multiwii r1240 or higher and using the KV_OSD_Team_1.0.mcm Chararter map file.                            */
              /************************************************************************************************************************************************/


              // This software communicates using MSP via the serial port. Therefore Multiwii develop-dependent.
              // Changes the values of pid and rc-tuning, writes in eeprom of Multiwii FC.
              // In config mode, can do acc and mag calibration.
              // In addition, it works by collecting information analogue inputs. Such as voltage, amperage, rssi, temperature on the original hardware (RUSHDUINO).
              // At the end of the flight may be useful to look at the statistics.


              /***********************************************************************************************************************************************/
              /*                                                            KV_OSD_Team_2.2                                                                  */
              /*                                                                                                                                             */
              /*                                                                                                                                             */
              /***********************************************************************************************************************************************/


#include <avr/pgmspace.h>
#include <EEPROM.h> //Needed to access eeprom read/write functions
#include "symbols.h"
#include "Config.h"
#include "GlobalVariables.h"

// Screen is the Screen buffer between program an MAX7456 that will be writen to the screen at 10hz
char screen[480];
// ScreenBuffer is an intermietary buffer to created Strings to send to Screen buffer
char screenBuffer[20];

uint32_t modeMSPRequests;
uint32_t queuedMSPRequests;

//-------------- Timed Service Routine vars (No more needed Metro.h library)

// May be moved in GlobalVariables.h
unsigned long previous_millis_low=0;
unsigned long previous_millis_high =0;
int hi_speed_cycle = 50;
int lo_speed_cycle = 100;
//----------------


void setup()
{
  Serial.begin(SERIAL_SPEED);
//---- override UBRR with MWC settings
  uint8_t h = ((F_CPU  / 4 / (SERIAL_SPEED) -1) / 2) >> 8;
  uint8_t l = ((F_CPU  / 4 / (SERIAL_SPEED) -1) / 2);
  UCSR0A  |= (1<<U2X0); UBRR0H = h; UBRR0L = l; 
//---
  Serial.flush();
  pinMode(BST,OUTPUT);
  checkEEPROM();
  readEEPROM();
  MAX7456Setup();
  
  analogReference(INTERNAL);

  setMspRequests();

  blankserialRequest(MSP_IDENT);
}

void setMspRequests() {
  if(configMode) {
    modeMSPRequests = 
      REQ_MSP_IDENT|
      REQ_MSP_STATUS|
      REQ_MSP_RAW_GPS|
      REQ_MSP_ATTITUDE|
      REQ_MSP_ALTITUDE|
      REQ_MSP_RC_TUNING|
      REQ_MSP_PID|
      REQ_MSP_RC;
  }
  else {
    modeMSPRequests = 
      REQ_MSP_IDENT|
      REQ_MSP_STATUS|
      REQ_MSP_RAW_GPS|
      REQ_MSP_COMP_GPS|
      REQ_MSP_ATTITUDE|
      REQ_MSP_ALTITUDE;

    if(MwVersion == 0)
      modeMSPRequests |= REQ_MSP_IDENT;

    if(!armed || Settings[S_THROTTLEPOSITION])
      modeMSPRequests |= REQ_MSP_RC;

    if(mode_armed == 0) {
        modeMSPRequests |= REQ_MSP_BOX;
/*
#ifdef REQ_MSP_BOXNAMES
      if(msp_ids_failed)
        modeMSPRequests |= REQ_MSP_BOXNAMES;
      else
#endif
        modeMSPRequests |= REQ_MSP_BOXIDS;
*/
    }
  }
 
  if(Settings[S_MAINVOLTAGE_VBAT] ||
     Settings[S_VIDVOLTAGE_VBAT] ||
     Settings[S_MWRSSI])
    modeMSPRequests |= REQ_MSP_ANALOG;

  // so we do not send requests that are not needed.
  queuedMSPRequests &= modeMSPRequests;
}

void loop()
{
  // Process AI
  if (Settings[S_ENABLEADC]){
    temperature=(analogRead(temperaturePin)*1.1)/10.23;
    if (!Settings[S_MAINVOLTAGE_VBAT]){
      static uint8_t ind = 0;
      static uint16_t voltageRawArray[8];
      voltageRawArray[(ind++)%8] = analogRead(voltagePin);                  
      uint16_t voltageRaw = 0;
      for (uint8_t i=0;i<8;i++)
        voltageRaw += voltageRawArray[i];
      voltage = float(voltageRaw) * Settings[S_DIVIDERRATIO] * (1.1/102.3/4/8);  
    }
    if (!Settings[S_VIDVOLTAGE_VBAT]) {
      vidvoltage = float(analogRead(vidvoltagePin)) * Settings[S_VIDDIVIDERRATIO] * (1.1/102.3/4);
    }
    if (!Settings[S_MWRSSI]) {
      rssiADC = (analogRead(rssiPin)*1.1)/1023;
    }
    amperage = (AMPRERAGE_OFFSET - (analogRead(amperagePin)*AMPERAGE_CAL))/10.23;
  }
  if (Settings[S_MWRSSI]) {
      rssiADC = MwRssi;
  }

  // Blink Basic Sanity Test Led at 1hz
  if(tenthSec>10)
    BST_ON
  else
    BST_OFF

  //---------------  Start Timed Service Routines  ---------------------------------------
  unsigned long currentMillis = millis();

  if((currentMillis - previous_millis_low) >= lo_speed_cycle)  // 10 Hz (Executed every 100ms)
  {
    previous_millis_low = currentMillis;    
    if(!serialWait){
      blankserialRequest(MSP_ATTITUDE);
    }
  }  // End of slow Timed Service Routine (100ms loop)

  if((currentMillis - previous_millis_high) >= hi_speed_cycle)  // 20 Hz (Executed every 50ms)
  {
    previous_millis_high = currentMillis;   

    tenthSec++;
    halfSec++;
    Blink10hz=!Blink10hz;
    calculateTrip();
    if(Settings[S_DISPLAYRSSI])
      calculateRssi();

    if(!serialWait) {
      uint8_t MSPcmdsend;
      if(queuedMSPRequests == 0)
        queuedMSPRequests = modeMSPRequests;
      uint32_t req = queuedMSPRequests & -queuedMSPRequests;
      queuedMSPRequests &= ~req;
      switch(req) {
      case REQ_MSP_IDENT:
        MSPcmdsend = MSP_IDENT;
        break;
      case REQ_MSP_STATUS:
        MSPcmdsend = MSP_STATUS;
        break;
      case REQ_MSP_RAW_IMU:
        MSPcmdsend = MSP_RAW_IMU;
        break;
      case REQ_MSP_RC:
        MSPcmdsend = MSP_RC;
        break;
      case REQ_MSP_RAW_GPS:
        MSPcmdsend = MSP_RAW_GPS;
        break;
      case REQ_MSP_COMP_GPS:
        MSPcmdsend = MSP_COMP_GPS;
        break;
      case REQ_MSP_ATTITUDE:
        MSPcmdsend = MSP_ATTITUDE;
        break;
      case REQ_MSP_ALTITUDE:
        MSPcmdsend = MSP_ALTITUDE;
        break;
      case REQ_MSP_ANALOG:
        MSPcmdsend = MSP_ANALOG;
        break;
      case REQ_MSP_RC_TUNING:
        MSPcmdsend = MSP_RC_TUNING;
        break;
      case REQ_MSP_PID:
        MSPcmdsend = MSP_PID;
        break;
      case REQ_MSP_BOX:
#ifdef USE_BOXNAMES
        MSPcmdsend = MSP_BOXNAMES;
#else
        MSPcmdsend = MSP_BOXIDS;
#endif
        break;
      }
      blankserialRequest(MSPcmdsend);      
    } // End of serial wait

    MAX7456_DrawScreen();
    if( allSec < 9 )
      displayIntro();
    else
    {
      if(armed){
        previousarmedstatus=1;
      }
      if(previousarmedstatus && !armed){
        configPage=6;
        ROW=10;
        COL=1;
        configMode=1;
        setMspRequests();
      }
      if(configMode)
      {
        displayConfigScreen();
      }
      else
      {
//        CollectStatistics();

        if(Settings[S_DISPLAYVOLTAGE]&&((voltage>Settings[S_VOLTAGEMIN])||(Blink2hz))) displayVoltage();
        if(Settings[S_DISPLAYRSSI]&&((rssi>lowrssiAlarm)||(Blink2hz))) displayRSSI();

        displayTime();
        displayMode();

        if(Settings[S_DISPLAYTEMPERATURE]&&((temperature<Settings[S_TEMPERATUREMAX])||(Blink2hz))) displayTemperature();

        if(Settings[S_AMPERAGE]) displayAmperage();

        if(Settings[S_AMPER_HOUR])  displaypMeterSum();
        displayArmed();
        if (Settings[S_THROTTLEPOSITION])
          displayCurrentThrottle();

        if(MwSensorPresent&ACCELEROMETER)
           displayHorizon(MwAngle[0],MwAngle[1]);

        if(MwSensorPresent&MAGNETOMETER) {
          displayHeadingGraph();
          displayHeading();
        }

        if(MwSensorPresent&BAROMETER) {
          displayAltitude();
          displayClimbRate();
        }

        if(MwSensorPresent&GPSSENSOR) {
          displayNumberOfSat();
          displayDirectionToHome();
          displayDistanceToHome();
          displayAngleToHome();
          displayGPS_speed();

          if (Settings[S_DISPLAYGPS])
            displayGPSPosition();
        }
      }
    }
  }  // End of fast Timed Service Routine (20ms loop)

  if(halfSec >= 10) {
    halfSec = 0;
    Blink2hz =! Blink2hz;
  }

  if(tenthSec >= 20)     // this execute 1 time a second
  {
    onTime++;

    // XXX
    amperagesum += amperage / AMPDIVISION; //(mAh)

    tenthSec=0;

    if(!armed) {
      flyTime=0;
    }
    else {
      flyTime++;
      flyingTime++;
      configMode=0;
      setMspRequests();
    }
    allSec++;

    if((accCalibrationTimer==1)&&(configMode)) {
      blankserialRequest(MSP_ACC_CALIBRATION);
      accCalibrationTimer=0;
    }

    if((magCalibrationTimer==1)&&(configMode)) {
      blankserialRequest(MSP_MAG_CALIBRATION);
      magCalibrationTimer=0;
    }

    if((eepromWriteTimer==1)&&(configMode)) {
      blankserialRequest(MSP_EEPROM_WRITE);
      eepromWriteTimer=0;
    }

    if(accCalibrationTimer>0) accCalibrationTimer--;
    if(magCalibrationTimer>0) magCalibrationTimer--;
    if(eepromWriteTimer>0) eepromWriteTimer--;

    if((rssiTimer==1)&&(configMode)) {
      Settings[S_RSSIMIN]=rssiADC;
      rssiTimer=0;
    }
    if(rssiTimer>0) rssiTimer--;
  }

  serialMSPreceive();

}  // End of main loop
//---------------------  End of Timed Service Routine ---------------------------------------


  //void CollectStatistics() {
//  if(GPS_fix && GPS_speed > speedMAX)
//    speedMAX = GPS_speed;
//}

void calculateTrip(void)
{
//  if(GPS_fix && (GPS_speed>0))
  if(GPS_fix && armed && (GPS_speed>0)){
    if(!Settings[S_UNITSYSTEM]) trip += GPS_speed *0.0005;        //  50/(100*1000)=0.0005               cm/sec ---> mt/50msec (trip var is float)      
    if(Settings[S_UNITSYSTEM])  trip += GPS_speed *0.0016404;     //  50/(100*1000)*3.2808=0.0016404     cm/sec ---> ft/50msec
    }
}

void calculateRssi(void)
{
  float aa=0; 
  if (Settings[S_MWRSSI]) {
    aa =  MwRssi;    //Temporary for calculation tests only
  }
  else
  {
    aa =analogRead(rssiPin)/4; 
 }   
  aa = ((aa-Settings[S_RSSIMIN]) *101)/(Settings[S_RSSIMAX]-Settings[S_RSSIMIN]) ;
  rssi_Int += ( ( (signed int)((aa*rssiSample) - rssi_Int )) / rssiSample );
  rssi = rssi_Int / rssiSample ;
  if(rssi<0) rssi=0;
  if(rssi>100) rssi=100;
}

void writeEEPROM(void)
{
  for(int en=0;en<EEPROM_SETTINGS;en++){
    if (EEPROM.read(en) != Settings[en]) EEPROM.write(en,Settings[en]);
  } 
}

void readEEPROM(void)
{
  for(int en=0;en<EEPROM_SETTINGS;en++){
     Settings[en] = EEPROM.read(en);
  }
}


// for first run to ini
void checkEEPROM(void)
{
  uint8_t EEPROM_Loaded = EEPROM.read(0);
  if (!EEPROM_Loaded){
    for(uint8_t en=0;en<EEPROM_SETTINGS;en++){
      if (EEPROM.read(en) != EEPROM_DEFAULT[en])
        EEPROM.write(en,EEPROM_DEFAULT[en]);
    }
  }
}
