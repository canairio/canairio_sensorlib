/**
 * @file main.cpp
 * @author Antonio Vanegas @hpsaturn
 * @date June 2018 - 2020
 * @brief Particle meter sensor tests
 * @license GPL3
 */

#include <Arduino.h>
#include <Sensors.hpp>

void onSensorDataOk() {
    Serial.print("-->[MAIN] PM1.0: "+sensors.getStringPM1());
    Serial.print  (" PM2.5: " + sensors.getStringPM25());
    Serial.println(" PM10: " + sensors.getStringPM10());
}

void onSensorDataError(const char * msg){
    Serial.println(msg);
}

/******************************************************************************
*  M A I N
******************************************************************************/

void setup() {
    Serial.begin(115200);
    delay(200);
    Serial.println("\n== Sensor test setup ==\n");

    Serial.println("-->[SETUP] Detecting sensors..");

    sensors.setSampleTime(5);                       // config sensors sample time interval
    sensors.setOnDataCallBack(&onSensorDataOk);     // all data read callback
    sensors.setOnErrorCallBack(&onSensorDataError); // [optional] error callback
    sensors.setDebugMode(false);                    // [optional] debug mode
    sensors.init();                                 // start all sensors and 
                                                    // force to try autodetection, 
                                                    // you can try to select one:
    // sensors.init(sensors.Sensirion);

    if(sensors.isPmSensorConfigured())
        Serial.println("-->[SETUP] Sensor configured: " + sensors.getPmDeviceSelected());

    delay(500);
}

void loop() {
    sensors.loop();  // read sensor data and showed it
}