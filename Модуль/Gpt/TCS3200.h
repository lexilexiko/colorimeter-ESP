// TCS3200.h
#ifndef TCS3200_H
#define TCS3200_H

#include <Arduino.h>

class TCS3200 {
public:
    TCS3200(int S0, int S1, int S2, int S3, int sensorOut);
    void begin();
    int readRed();
    int readGreen();
    int readBlue();

private:
    int S0, S1, S2, S3, sensorOut;
    int readColor(int S2_state, int S3_state);
};

#endif