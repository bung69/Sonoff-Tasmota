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
    * add support for auto deteccting which / multiple sensor address
    * Web interface readouts
    * 10hz refesh rate with averaging, interpolation, threashold triggering, blob counting for presence detection FUNC_EVERY_100_MSECOND 
    * cordinate output of N number of detected blobs for targeted / location based home automation
    * posibly granular control of above features via defines
    * aproimate flash, ram and processor usage
    * write wiki
    * PR
*/


#ifdef USE_I2C
#ifdef USE_AMG

#define XSNS_69          69

// AMG8833 Status and configuration registers
#define AMG_PCTL             0x00           // operating mode
#define AMG_RST              0x01           // reset
#define AMG_FPSC             0x02           // frame rate
#define AMG_SCLR             0x05           // status clear register
#define AMG_TTHL             0x0E           // thermister register low
#define AMG_DATA01L          0x80           // first pixel low byte 
#define AMG_SIZE               64            // number of pixels
#define AMG_ADDR_1           0x68           // 0x68 when ADO = LOW, 0x69 when ADO = HIGH
#define AMG_ADDR_2           0x69           // 0x68 when ADO = LOW, 0x69 when ADO = HIGH
#define AMG_MAX_SENSORS         2


typedef struct {
  uint8_t bmp_address; 
  char bmp_name[7];      
  float thermistorTemp;
  float pixelTemps[AMG_SIZE];
} AMG_sensors_t;

int16_t thermistorRawData;
int16_t pixelRawData[AMG_SIZE];
uint8_t amg_detected = 0;
uint8_t amg_addresses[] = { BMP_ADDR1, BMP_ADDR2 };

amg_sensors_t *amg_sensors =nullptr;


void amg_Detect(void) 
{
  uint8_t buffer;

  if (I2cValidRead8(&buffer, AMG_PCTL)) {
    I2cWrite, AMG_PCTL, 0x00); // set operating mode (NORMAL_MODE = 0x00, SLEEP_MODE = 0x01, STANDBY_MODE_60SEC = 0x20, STANDBY_MODE_10SEC = 0x21)
    if (I2cValidRead8(&buffer, AMG_PCTL)) {
      if (0x00 == buffer){
        amg_detected =1;
        AddLog_P2(LOG_LEVEL_DEBUG, S_LOG_I2C_FOUND_AT, "amg");

      }else{
        amg_detected =0;
        return;
      }
    }
  }  
}

void amg_Reset(void)
{
  if (!amg_detected) { return; }
    I2cWrite, AMG_PCTL, 0x00); // set operating mode (NORMAL_MODE = 0x00, SLEEP_MODE = 0x01, STANDBY_MODE_60SEC = 0x20, STANDBY_MODE_10SEC = 0x21)
    I2cWrite, AMG_RST, 0x3F);  // software a reset
    I2cWrite, AMG_FPSC, 0x01); // sample rate (0x00 = 10 fps or 0x01 = 1 fps)
}

void amg_Read(void) 
{
  if (!amg_detected) { return; }

  uint8_t buffer;

// read thermister
  if (I2cValidRead8(&buffer, AMG_TTHL)) {
    thermistorRawData = I2cRead16L, AMG_TTHL);
    thermistorRawData = thermistorRawData << 4 ;
    thermistorRawData = thermistorRawData >> 4 ;
    AMG.thermistorTemp = ConvertTemp(thermistorRawData * 0.0625f);
  }else{ 
    amg_detected =0;
    return;
  }
  
 // read pixels
  for (uint32_t start=0; start < AMG_SIZE; start++) {
    if (I2cValidRead8(&buffer, AMG_DATA01L + start * 2)) {
      pixelRawData[start] = I2cRead16L, AMG_DATA01L + start * 2 );
      pixelRawData[start] = pixelRawData[start] << 4 ;
      pixelRawData[start] = pixelRawData[start] >> 4 ;
      AMG.pixelTemps[start] = (float) (pixelRawData[start]) * 0.25f;
    }else{ 
      amg_detected =0;
      return;
    }
  }
}

void amgtoJSON(void)  // convert pixel temperature floats to json string
{
  char *comma = (char*)"";
  for (uint32_t i = 0; i < AMG_SIZE; i++) {
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

void amg_Show(bool json)
{
  if (!amg_detected) {return; }

  char temperature[33];
  dtostrfd(AMG.thermistorTemp, Settings.flag2.temperature_resolution, temperature);

  if (json) {

    ResponseAppend_P(PSTR(",\"amg_T\":"));
    ResponseAppend_P(PSTR("%s"), temperature);
    ResponseAppend_P(PSTR(",\"amg_P\":["));
    amgtoJSON();
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
        amg_Detect();
        amg_Reset();
        break;

      case FUNC_EVERY_SECOND:
        amg_Detect();  
        amg_Read();
        break;

      case FUNC_JSON_APPEND:
        amg_Show(1);
        break;

#ifdef USE_WEBSERVER
      case FUNC_WEB_SENSOR:
        //amg_Show(0);
        break;
#endif  // USE_WEBSERVER
    }
  }
  return result;
}

#endif  // USE_amg8833
#endif  // USE_I2C
