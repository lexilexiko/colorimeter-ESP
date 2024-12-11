// TCS3200.cpp
#include "TCS3200.h"

TCS3200::TCS3200(int s0, int s1, int s2, int s3, int sensorOut)
    : S0(s0), S1(s1), S2(s2), S3(s3), sensorOut(sensorOut) {}

void TCS3200::begin() {
    pinMode(S0, OUTPUT);
    pinMode(S1, OUTPUT);
    pinMode(S2, OUTPUT);
    pinMode(S3, OUTPUT);
    pinMode(sensorOut, INPUT);

    digitalWrite(S0, HIGH);
    digitalWrite(S1, LOW);
}

int TCS3200::readColor(int S2_state, int S3_state) {
    digitalWrite(S2, S2_state);
    digitalWrite(S3, S3_state);
    delay(50); // Задержка для стабилизации
    return pulseIn(sensorOut, LOW);
}

int TCS3200::readRed() {
    return readColor(LOW, LOW);
}

int TCS3200::readGreen() {
    return readColor(HIGH, HIGH);
}

int TCS3200::readBlue() {
    return readColor(LOW, HIGH);
}