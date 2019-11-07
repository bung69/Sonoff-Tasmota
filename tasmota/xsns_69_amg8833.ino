/*
  xsns_55_amg8833.ino - amg8833 thermopile sensor for Tasmota

  Copyright (C) 2019  Matt Howes

  insperation taken from drivers by:
      Nick Poole @ SparkFun and 
      Dean Miller for Adafruit Industries
      Xose Pérez  (espurna )

  This program is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/


/*
  TODO: 
    * problem solve why not compile outside of Screen env:display
    * add support for auto deteccting which / multiple sensor address
    * Web interface readouts
    * 10hz refesh rate with averaging, interpolation, threashold triggering, blob counting for presence detection FUNC_EVERY_100_MSECOND 
    * cordinate output of N number of detected blobs for targeted / location based home automation
    * posibly granular control of above features via defines
    * aproimate flash, ram and processor usage
    * write wiki
    * PR
*/


#ifdef USE_SPI
#ifdef USE_AMG

#define XSNS_69          69

#warning ****  XSNS_69 is being compiled ****  // compile Debug,  not debugging out side of env:display

// AMG8833 Status and configuration registers
#define AMG8833_PCTL     0x00
#define AMG8833_RST      0x01
#define AMG8833_FPSC     0x02
#define AMG8833_STAT     0x04  // status register
#define AMG8833_SCLR     0x05  // status clear register
#define AMG8833_TTHL     0x0E  // thermister register low
#define AMG8833_DATA01L  0x80  // first pixel low byte 

//#define AMG8833_ADDRESS_1  0x68  // 0x68 when ADO = LOW, 0x69 when ADO = HIGH
#define AMG8833_ADDRESS_2  0x69  // 0x68 when ADO = LOW, 0x69 when ADO = HIGH


#define amg8833_PIXEL_ARRAY_SIZE 64

int16_t thermistorRawData;
int16_t pixelRawData[amg8833_PIXEL_ARRAY_SIZE];
uint8_t amg8833_detected;

struct AMGSTRUCT {
  float thermistorTemp;
  float pixelTemps[amg8833_PIXEL_ARRAY_SIZE];
} AMG;

void amg8833_Detect(void) 
{

  uint8_t buffer;

  if (I2cValidRead8(&buffer, AMG8833_ADDRESS_1, AMG8833_PCTL)) {
    I2cWrite8(AMG8833_ADDRESS_1, AMG8833_PCTL, 0x00); // set operating mode (NORMAL_MODE = 0x00, SLEEP_MODE = 0x01, STANDBY_MODE_60SEC = 0x20, STANDBY_MODE_10SEC = 0x21)
    if (I2cValidRead8(&buffer, AMG8833_ADDRESS_1, AMG8833_PCTL)) {
      if (0x00 == buffer){
        amg8833_detected =1;
        AddLog_P2(LOG_LEVEL_DEBUG, S_LOG_I2C_FOUND_AT, "amg8833", AMG8833_ADDRESS_1);

      }else{
        amg8833_detected =0;
        return;
      }
    }
  }  
}

void amg8833_Reset(void)
{
  if (!amg8833_detected) { return; }
    I2cWrite8(AMG8833_ADDRESS_1, AMG8833_PCTL, 0x00); // set operating mode (NORMAL_MODE = 0x00, SLEEP_MODE = 0x01, STANDBY_MODE_60SEC = 0x20, STANDBY_MODE_10SEC = 0x21)
    I2cWrite8(AMG8833_ADDRESS_1, AMG8833_RST, 0x3F);  // software a reset
    I2cWrite8(AMG8833_ADDRESS_1, AMG8833_FPSC, 0x01); // sample rate (0x00 = 10 fps or 0x01 = 1 fps)
}

void amg8833_Read(void) 
{
  if (!amg8833_detected) { return; }

  uint8_t size = amg8833_PIXEL_ARRAY_SIZE;
  uint8_t buffer;

// read thermister
  if (I2cValidRead8(&buffer, AMG8833_ADDRESS_1, AMG8833_TTHL)) {
    thermistorRawData = I2cRead16LE(AMG8833_ADDRESS_1, AMG8833_TTHL);
    thermistorRawData = thermistorRawData << 4 ;
    thermistorRawData = thermistorRawData >> 4 ;
    AMG.thermistorTemp = ConvertTemp(thermistorRawData * 0.0625f);
  }else{ 
    amg8833_detected =0;
    return;
  }
  
 // read pixels
  for (uint8_t start=0; start < size; start++) {
    if (I2cValidRead8(&buffer, AMG8833_ADDRESS_1, AMG8833_DATA01L + start * 2)) {
      pixelRawData[start] = I2cRead16LE(AMG8833_ADDRESS_1, AMG8833_DATA01L + start * 2 );
      pixelRawData[start] = pixelRawData[start] << 4 ;
      pixelRawData[start] = pixelRawData[start] >> 4 ;
      AMG.pixelTemps[start] = (float) (pixelRawData[start]) * 0.25f;
    }else{ 
      amg8833_detected =0;
      return;
    }
  }
}

void amg8833toJSON(void)  // convert pixel temperature floats to json string
{
  char *comma = (char*)"";
  for (unsigned int i = 0; i < 64; i++) {
   char temperature[33];
   dtostrfd(AMG.pixelTemps[i], Settings.flag2.temperature_resolution, temperature);
    ResponseAppend_P(PSTR("%s%s"), comma, temperature);
    comma = (char*)",";
  }
  ResponseJsonEnd();
}


// void amg8833toString(void)  // TODO - convert pixel temperature floats to string for web
// {
//   char label[15];
//   snprintf_P(label, sizeof(label), "amg_T(%02x)");
//   WSContentSend_PD(HTTP_SNS_TEMP, label, AMG.thermistorTemp);
//   snprintf_P(label, sizeof(label), "amg_P(%02x)");

//   for (unsigned int i = 0; i < 64; i++) {
//     WSContentSend_PD(HTTP_SNS_TEMP, label, i, AMG.pixelTemps[i]);
//   }
// }

void amg8833_Show(bool json)
{
  if (!amg8833_detected) {return; }

  char temperature[33];
  dtostrfd(AMG.thermistorTemp, Settings.flag2.temperature_resolution, temperature);

  if (json) {

    ResponseAppend_P(PSTR(",\"amg_T\":"));
    ResponseAppend_P(PSTR("%s"), temperature);
    ResponseAppend_P(PSTR(",\"amg_P\":["));
    amg8833toJSON();
    ResponseAppend_P(PSTR("]"));
  }

#ifdef USE_WEBSERVER
      else {
        // amg8833toString();
      }
#endif  // USE_WEBSERVER
  
}

/*********************************************************************************************\
 * Interface
\*********************************************************************************************/

bool Xsns69(uint8_t function)
{
  bool result = false;
  if(i2c_flg) {
    switch (function) {
      case FUNC_INIT:
        amg8833_Detect();
        amg8833_Reset();
        break;

      case FUNC_EVERY_SECOND:
        amg8833_Detect();  
        amg8833_Read();
        break;

      case FUNC_JSON_APPEND:
        amg8833_Show(1);
        break;

#ifdef USE_WEBSERVER
      case FUNC_WEB_SENSOR:
        //amg8833_Show(0);
        break;
#endif  // USE_WEBSERVER
    }
  }
  return result;
}

#endif  // USE_amg8833
#endif  // USE_I2C
