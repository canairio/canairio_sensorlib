#include "Sensors.hpp"

// Humidity sensor
Adafruit_AM2320 am2320 = Adafruit_AM2320();

/***********************************************************************************
 *  P U B L I C   M E T H O D S
 * *********************************************************************************/

/**
 * Main sensors loop.
 * All sensors are read here, please call it on main loop.
 */
void Sensors::loop() {
    static uint_fast64_t pmLoopTimeStamp = 0;                 // timestamp for sensor loop check data
    if ((millis() - pmLoopTimeStamp > sample_time * 1000)) {  // sample time for each capture
        dataReady = false;
        pmLoopTimeStamp = millis();
        am2320Read();
        if(pmSensorRead()) {           
            if(_onDataCb) _onDataCb();
            dataReady = true;            // only if the main sensor is ready
        }else{
            if(_onErrorCb)_onErrorCb("-->[W][SENSORS] PM sensor not configured!");
            dataReady = false;
        }
        printValues();
    }
}

/**
 * All sensors init.
 * Particle meter sensor (PMS) and AM2320 sensor init.
 * 
 * @param pms_type PMS type, please see DEVICE_TYPE enum.
 * @param pms_rx PMS RX pin.
 * @param pms_tx PMS TX pin.
 * @param debug enable PMS log output.
 */
void Sensors::init(int pms_type, int pms_rx, int pms_tx) {

    // override with debug INFO level (>=3)
    #ifdef CORE_DEBUG_LEVEL
    if (CORE_DEBUG_LEVEL>=3) devmode = true;  
    #endif
    if (devmode) Serial.println("-->[SENSORS] debug is enable.");

    DEBUG("-->[SENSORS] sample time set to: ",String(sample_time).c_str());
    
    pmSensorInit(pms_type, pms_rx, pms_tx);

    // TODO: enable/disable via flag
    DEBUG("-->[AM2320] starting AM2320 sensor..");
    am2320Init();
}

/// set loop time interval for each sensor sample
void Sensors::setSampleTime(int seconds){
    sample_time = seconds;
}

void Sensors::restart(){
    _serial->flush();
    init();
    delay(100);
}

void Sensors::setOnDataCallBack(voidCbFn cb){
    _onDataCb = cb;
}

void Sensors::setOnErrorCallBack(errorCbFn cb){
    _onErrorCb = cb;
}

void Sensors::setDebugMode(bool enable){
    devmode = enable;
}

bool Sensors::isDataReady() {
    return dataReady;
}

uint16_t Sensors::getPM1() {
    return pm1;
}

String Sensors::getStringPM1() {
    char output[5];
    sprintf(output, "%03d", getPM1());
    return String(output);
}

uint16_t Sensors::getPM25() {
    return pm25;
}

String Sensors::getStringPM25() {
    char output[5];
    sprintf(output, "%03d", getPM25());
    return String(output);
}

uint16_t Sensors::getPM10() {
    return pm10;
}

String Sensors::getStringPM10() {
    char output[5];
    sprintf(output, "%03d", getPM10());
    return String(output);
}

float Sensors::getHumidity() {
    return humi;
}

float Sensors::getTemperature() {
    return temp;
}

float Sensors::getGas() {
    return gas;
}

float Sensors::getAltitude() {
    return alt;
}

float Sensors::getPressure() {
    return pres;
}

bool Sensors::isPmSensorConfigured(){
    return device_type>=0;
}

String Sensors::getPmDeviceSelected(){
    return device_selected;
}

int Sensors::getPmDeviceTypeSelected(){
    return device_type;
}

/******************************************************************************
*   S E N S O R   P R I V A T E   M E T H O D S
******************************************************************************/

/**
 *  @brief PMS sensor generic read. Supported: Honeywell & Plantower sensors
 *  @return true if header and sensor data is right
 */
bool Sensors::pmGenericRead() {
    String txtMsg = hwSerialRead();
    if (txtMsg[0] == 66) {
        if (txtMsg[1] == 77) {
            DEBUG("-->[HPMA] read > done!");
            pm25 = txtMsg[6] * 256 + byte(txtMsg[7]);
            pm10 = txtMsg[8] * 256 + byte(txtMsg[9]);
            if (pm25 > 1000 && pm10 > 1000) {
                onPmSensorError("-->[E][PMSENSOR] out of range pm25 > 1000");
            }
            else
                return true;
        } else {
            onPmSensorError("-->[E][PMSENSOR] invalid Generic sensor header!");
        }
    }
    return false;
} 

/**
 *  @brief Panasonic SNGC particulate meter sensor read.
 *  @return true if header and sensor data is right
 */
bool Sensors::pmPanasonicRead() {
    String txtMsg = hwSerialRead();
    if (txtMsg[0] == 02) {
        DEBUG("-->[PANASONIC] read > done!");
        pm25 = txtMsg[6] * 256 + byte(txtMsg[5]);
        pm10 = txtMsg[10] * 256 + byte(txtMsg[9]);
        if (pm25 > 2000 && pm10 > 2000) {
            onPmSensorError("-->[E][PMSENSOR] out of range pm25 > 2000");
        }
        else
            return true;
    } else {
        onPmSensorError("-->[E][PMSENSOR] invalid Panasonic sensor header!");
    }
    return false;
}

/**
 * @brief PMSensor Serial read to basic string
 * 
 * @param SENSOR_RETRY attempts before failure
 * @return String buffer
 **/
String Sensors::hwSerialRead() {
    int try_sensor_read = 0;
    String txtMsg = "";
    while (txtMsg.length() < 32 && try_sensor_read++ < SENSOR_RETRY) {
        while (_serial->available() > 0) {
            char inChar = _serial->read();
            txtMsg += inChar;
        }
    }
    if (try_sensor_read > SENSOR_RETRY) {
        onPmSensorError("-->[E][PMSENSOR] sensor read fail!");
    }
    return txtMsg;
}

/**
 *  @brief Sensirion SPS30 particulate meter sensor read.
 *  @return true if reads succes
 */
bool Sensors::pmSensirionRead() {
    uint8_t ret, error_cnt = 0;
    delay(35);  //Delay for sincronization
    do {
        ret = sps30.GetValues(&val);
        if (ret == ERR_DATALENGTH) {
            if (error_cnt++ > 3) {
                DEBUG("-->[E][SPS30] Error during reading values: ", String(ret).c_str());
                return false;
            }
            delay(1000);
        } else if (ret != ERR_OK) {
            pmSensirionErrtoMess((char *)"-->[W][SPS30] Error during reading values: ", ret);
            return false;
        }
    } while (ret != ERR_OK);

    DEBUG("-->[SPS30] read > done!");

    pm25 = round(val.MassPM2);
    pm10 = round(val.MassPM10);

    if (pm25 > 1000 && pm10 > 1000) {
        onPmSensorError("-->[E][SPS30] Sensirion out of range pm25 > 1000");
        return false;
    }

    return true;
}

/**
 * @brief read sensor data. Sensor selected.
 * @return true if data is loaded from sensor
 */
bool Sensors::pmSensorRead() {
    switch (device_type) {
        case Honeywell:
            return pmGenericRead();
            break;

        case Panasonic:
            return pmPanasonicRead();
            break;

        case Sensirion:
            return pmSensirionRead();
            break;

        default:
            return false;
            break;
    }
}

void Sensors::am2320Read() {
    humi = am2320.readHumidity();
    temp = am2320.readTemperature();
    if (isnan(humi)) humi = 0.0;
    if (isnan(temp)) temp = 0.0;
}

void Sensors::onPmSensorError(const char *msg) {
    DEBUG(msg);
    if(_onErrorCb)_onErrorCb(msg);
}

void Sensors::pmSensirionErrtoMess(char *mess, uint8_t r) {
    char buf[80];
    sps30.GetErrDescription(r, buf, 80);
    DEBUG("-->[E][SENSIRION]",buf);
}

void Sensors::pmSensirionErrorloop(char *mess, uint8_t r) {
    if (r) pmSensirionErrtoMess(mess, r);
    else DEBUG(mess);
}

/**
 * Particule meter sensor (PMS) init.
 * 
 * Hardware serial init for multiple PM sensors, like
 * Honeywell, Plantower, Panasonic, Sensirion, etc.
 * 
 * @param pms_type PMS type, please see DEVICE_TYPE enum.
 * @param pms_rx PMS RX pin.
 * @param pms_tx PMS TX pin.
 **/
bool Sensors::pmSensorInit(int pms_type, int pms_rx, int pms_tx) {
    // set UART for autodetection sensors (Honeywell, Plantower, Panasonic)
    if (pms_type <= 1) {
        DEBUG("-->[PMSENSOR] detecting PM sensor..");
        Serial2.begin(9600, SERIAL_8N1, pms_rx, pms_tx);
    }
    // set UART for autodetection Sensirion sensor
    else if (pms_type == Sensirion) {
        DEBUG("-->[PMSENSOR] detecting Sensirion sensor..");
        Serial2.begin(115200);
    }

    // starting auto detection loop 
    _serial = &Serial2;
    int try_sensor_init = 0;
    while (!pmSensorAutoDetect(pms_type) && try_sensor_init++ < 2);

    // get device selected..
    if (device_type >= 0) {
        DEBUG("-->[PMSENSOR] detected: ",device_selected.c_str());
        return true;
    } else {
        DEBUG("-->[E][PMSENSOR] detection failed!");
        if(_onErrorCb)_onErrorCb("-->[E][PMSENSOR] detection failed!");
        return false;
    }
}
/**
 * @brief Generic PM sensor auto detection. 
 * 
 * In order UART config, this method looking up for
 * special header on Serial stream
 **/
bool Sensors::pmSensorAutoDetect(int pms_type) {

    delay(1000);  // sync serial

    if (pms_type == Sensirion) {
        if (pmSensirionInit()) {
            device_selected = "SENSIRION";
            device_type = Sensirion;
            return true;
        }
    } else {
        DEBUG("-->[PMSENSOR] detecting Honeywell/Plantower sensor..");
        if (pmGenericRead()) {
            device_selected = "HONEYWELL";
            device_type = Honeywell;
            return true;
        }
        DEBUG("-->[PMSENSOR] detecting Panasonic sensor..");
        if (pmPanasonicRead()) {
            device_selected = "PANASONIC";
            device_type = Panasonic;
            return true;
        }
    }

    return false;
}

bool Sensors::pmSensirionInit() {
    // Begin communication channel
    DEBUG("-->[SPS30] starting SPS30 sensor..");
    if(!devmode) sps30.EnableDebugging(0);
    // Begin communication channel;
    if (!sps30.begin(SP30_COMMS))
        pmSensirionErrorloop((char *)"-->[E][SPS30] could not initialize communication channel.", 0);
    // check for SPS30 connection
    if (!sps30.probe())
        pmSensirionErrorloop((char *)"-->[E][SPS30] could not probe / connect with SPS30.", 0);
    else {
        DEBUG("-->[SPS30] Detected SPS30.");
        getSensirionDeviceInfo();
    }
    // reset SPS30 connection
    if (!sps30.reset())
        pmSensirionErrorloop((char *)"-->[E][SPS30] could not reset.", 0);

    // start measurement
    if (sps30.start()==true) {
        DEBUG("-->[SPS30] Measurement OK");
        return true;
    } else
        pmSensirionErrorloop((char *)"-->[E][SPS30] Could NOT start measurement", 0);

    if (SP30_COMMS == I2C_COMMS) {
        if (sps30.I2C_expect() == 4)
            DEBUG("-->[E][SPS30] Due to I2C buffersize only PM values  \n");
    }
    return false;
}
/**
 * @brief : read and display Sensirion device info
 */
void Sensors::getSensirionDeviceInfo() { 
  char buf[32];
  uint8_t ret;
  SPS30_version v;

  //try to read serial number
  ret = sps30.GetSerialNumber(buf, 32);
  if (ret == ERR_OK) {
    if(strlen(buf) > 0) DEBUG("-->[SPS30] Serial number : ",buf);
    else DEBUG("not available");
  }
  else
    DEBUG("[SPS30] could not get serial number");

  // try to get product name
  ret = sps30.GetProductName(buf, 32);
  if (ret == ERR_OK)  {
    if(strlen(buf) > 0) DEBUG("-->[SPS30] Product name  : ",buf);
    else DEBUG("not available");
  }
  else
    DEBUG("[SPS30] could not get product name.");

  // try to get version info
  ret = sps30.GetVersion(&v);
  if (ret != ERR_OK) {
    DEBUG("[SPS30] Can not read version info");
    return;
  }
  sprintf(buf,"%d.%d",v.major,v.minor);
  DEBUG("-->[SPS30] Firmware level: ", buf);

  if (SP30_COMMS != I2C_COMMS) {
    sprintf(buf,"%d.%d",v.SHDLC_major,v.SHDLC_minor);
    DEBUG("-->[SPS30] Hardware level: ",String(v.HW_version).c_str());
    DEBUG("-->[SPS30] SHDLC protocol: ",buf);
  }

  sprintf(buf, "%d.%d", v.DRV_major, v.DRV_minor);
  DEBUG("-->[SPS30] Library level : ",buf); 
}

void Sensors::am2320Init() {
    am2320.begin();  // temp/humidity sensor
}

/// Print some sensors values
void Sensors::printValues() {
    char output[100];
    sprintf(output, "PM1:%03d PM25:%03d PM10:%03d H:%02d%% T:%02d°C", pm1, pm25, pm10, (int)humi, (int)temp);
    DEBUG("-->[SENSORS]", output);
}

void Sensors::DEBUG(const char *text, const char *textb) {
    if (devmode) {
        _debugPort.print(text);
        if (textb) {
            _debugPort.print(" ");
            _debugPort.print(textb);
        }
        _debugPort.println();
    }
}

#if !defined(NO_GLOBAL_INSTANCES) && !defined(NO_GLOBAL_SENSORSHANDLER)
Sensors sensors;
#endif
