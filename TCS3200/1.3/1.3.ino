#include <WiFi.h>
#include <WebServer.h>
#include <TCS3200.h>
#include <EEPROM.h>  // Для работы с EEPROM
#include "interface.h"

// --- Параметры Wi-Fi ---
const char* ssid = "ESP32_Colorimeter";
const char* password = "12345678";

// --- Настройка сервера ---
WebServer server(80);

// --- Пины для датчика TCS3200 ---
#define S0 2
#define S1 4
#define S2 16
#define S3 17
#define sensorOut 18
#define LED_PIN 19  // Пин для управления светодиодом

// --- Инициализация датчика ---
TCS3200 colorSensor(S0, S1, S2, S3, sensorOut);

// --- Переменные для хранения данных цвета ---
int red = 0, green = 0, blue = 0;
int prevRed = 0, prevGreen = 0, prevBlue = 0;  // Предыдущие значения
bool sensorInitialized = false;  // Флаг для отслеживания состояния датчика
bool ledState = false;           // Состояние светодиода

// --- Переменные для хранения калибровочных значений ---
int blackRed = 0, blackGreen = 0, blackBlue = 0;
int whiteRed = 0, whiteGreen = 0, whiteBlue = 0;
int redRed = 0, redGreen = 0, redBlue = 0;
int greenRed = 0, greenGreen = 0, greenBlue = 0;
int blueRed = 0, blueGreen = 0, blueBlue = 0;

// --- Флаги состояния ---
bool isCalibrated = false;  // Флаг выполнения калибровки
int calStep = 0;            // Текущий этап калибровки (0–5)
bool normalizeEnabled = true;  // Управление нормализацией

// --- Размер EEPROM ---
#define EEPROM_SIZE 512

// --- Утилита: Нормализация значения ---
/**
 * Применяет нормализацию к значениям цвета.
 * @param color Исходное значение цвета.
 * @param black Минимальное значение для черного.
 * @param white Максимальное значение для белого.
 * @param primary Калибровочное значение для данного цвета.
 * @return Нормализованное значение цвета.
 */

// --- Обработчик главной страницы ---
void handleRoot() {
    String html = generateInterface(red, green, blue, prevRed, prevGreen, prevBlue, sensorInitialized, isCalibrated, calStep, normalizeEnabled );
    server.send(200, "text/html", html);
}

// --- Включение датчика ---
void handleEnableSensor() {
    if (!sensorInitialized) {
        colorSensor.begin();
        sensorInitialized = true;
        Serial.println("Датчик TCS3200 инициализирован.");
        server.send(200, "text/plain", "Датчик включен");
    } else {
        Serial.println("Датчик уже был инициализирован.");
        server.send(200, "text/plain", "Датчик уже включен");
    }
}

int normalizeColor(int color, int black, int white, int primary) {
    if (!normalizeEnabled) {
        return color; // Если нормализация отключена, возвращаем исходное значение.
    }

    int range = primary - black;
    if (range == 0) return 0;  // Защита от деления на ноль.

    return map(color, black, primary, 0, 255);  // Нормализация в диапазон 0-255
}

// --- Сканирование цвета ---
void handleScan() {
    if (sensorInitialized) {
        // Сохраняем предыдущие значения
        prevRed = red;
        prevGreen = green;
        prevBlue = blue;

        // Считываем новые значения с датчика
        int rawRed = colorSensor.readRed();
        int rawGreen = colorSensor.readGreen();
        int rawBlue = colorSensor.readBlue();

        if (isCalibrated) {
            // Применяем нормализацию для каждого цвета
            red = normalizeColor(rawRed, blackRed, whiteRed, redRed);
            green = normalizeColor(rawGreen, blackGreen, whiteGreen, greenGreen);
            blue = normalizeColor(rawBlue, blackBlue, whiteBlue, blueBlue);
        } else {
            // Если не откалибровано, берем сырые значения
            red = rawRed;
            green = rawGreen;
            blue = rawBlue;
        }

        Serial.printf("Сканирование завершено: Было - R:%d, G:%d, B:%d; Стало - R:%d, G:%d, B:%d\n", 
                      prevRed, prevGreen, prevBlue, red, green, blue);

        // Отправляем результат в формате JSON
        String json = "{";
        json += "\"prev_red\":" + String(prevRed) + ",";
        json += "\"prev_green\":" + String(prevGreen) + ",";
        json += "\"prev_blue\":" + String(prevBlue) + ",";
        json += "\"red\":" + String(red) + ",";
        json += "\"green\":" + String(green) + ",";
        json += "\"blue\":" + String(blue);
        json += "}";

        server.send(200, "application/json", json);
    } else {
        server.send(400, "text/plain", "Ошибка: датчик не инициализирован.");
    }
}

// --- Управление нормализацией ---
void handleToggleNormalization() {
    normalizeEnabled = !normalizeEnabled;
    String status = normalizeEnabled ? "включена" : "выключена";
    server.send(200, "text/plain", "Нормализация " + status);
    Serial.println("Нормализация " + status);
}

// --- Калибровка цветов ---

// Обработчик для калибровки черного цвета
void handleCalibrateBlack() {
    blackRed = colorSensor.readRed();
    blackGreen = colorSensor.readGreen();
    blackBlue = colorSensor.readBlue();
    Serial.printf("Считаны значения для черного: R:%d, G:%d, B:%d\n", blackRed, blackGreen, blackBlue);
    calStep = 1;  // Переход к следующему этапу
    saveCalibration();  // Сохраняем калибровочные данные
    server.send(200, "text/plain", "Калибровка черного завершена, поднесите белый объект.");
}

// Обработчик для калибровки белого цвета
void handleCalibrateWhite() {
    whiteRed = colorSensor.readRed();
    whiteGreen = colorSensor.readGreen();
    whiteBlue = colorSensor.readBlue();
    Serial.printf("Считаны значения для белого: R:%d, G:%d, B:%d\n", whiteRed, whiteGreen, whiteBlue);
    calStep = 2;  // Переход к следующему этапу
    saveCalibration();  // Сохраняем калибровочные данные
    server.send(200, "text/plain", "Калибровка белого завершена, поднесите красный объект.");
}

// Обработчик для калибровки красного цвета
void handleCalibrateRed() {
    redRed = colorSensor.readRed();
    redGreen = colorSensor.readGreen();
    redBlue = colorSensor.readBlue();
    Serial.printf("Считаны значения для красного: R:%d, G:%d, B:%d\n", redRed, redGreen, redBlue);
    calStep = 3;  // Переход к следующему этапу
    saveCalibration();  // Сохраняем калибровочные данные
    server.send(200, "text/plain", "Калибровка красного завершена, поднесите зеленый объект.");
}

// Обработчик для калибровки зеленого цвета
void handleCalibrateGreen() {
    greenRed = colorSensor.readRed();
    greenGreen = colorSensor.readGreen();
    greenBlue = colorSensor.readBlue();
    Serial.printf("Считаны значения для зеленого: R:%d, G:%d, B:%d\n", greenRed, greenGreen, greenBlue);
    calStep = 4;  // Переход к следующему этапу
    saveCalibration();  // Сохраняем калибровочные данные
    server.send(200, "text/plain", "Калибровка зеленого завершена, поднесите синий объект.");
}

// Обработчик для калибровки синего цвета
void handleCalibrateBlue() {
    blueRed = colorSensor.readRed();
    blueGreen = colorSensor.readGreen();
    blueBlue = colorSensor.readBlue();
    Serial.printf("Считаны значения для синего: R:%d, G:%d, B:%d\n", blueRed, blueGreen, blueBlue);
    isCalibrated = true;
    calStep = 0;  // Завершение калибровки
    saveCalibration();  // Сохраняем калибровочные данные
    server.send(200, "text/plain", "Калибровка завершена!");
}

// --- Сброс калибровки ---
void handleResetCalibration() {
    blackRed = 0; blackGreen = 0; blackBlue = 0;
    whiteRed = 255; whiteGreen = 255; whiteBlue = 255;
    redRed = 0; redGreen = 0; redBlue = 0;
    greenRed = 0; greenGreen = 0; greenBlue = 0;
    blueRed = 0; blueGreen = 0; blueBlue = 0;

    isCalibrated = false;
    calStep = 0;
    saveCalibration(); // Сохраняем калибровочные данные
    server.send(200, "text/plain", "Калибровка сброшена.");
}

// --- Управление светодиодом ---
void handleLedOn() {
    digitalWrite(LED_PIN, HIGH);
    ledState = true;
    server.send(200, "text/plain", "LED включен");
    Serial.println("LED включен");
}

void handleLedOff() {
    digitalWrite(LED_PIN, LOW);
    ledState = false;
    server.send(200, "text/plain", "LED выключен");
    Serial.println("LED выключен");
}

void handleLedStatus() {
    String status = ledState ? "on" : "off";
    String json = "{\"led\":\"" + status + "\"}";
    server.send(200, "application/json", json);
}

// --- Сохранение калибровки в EEPROM ---
void saveCalibration() {
    int addr = 0;
    EEPROM.put(addr, blackRed); addr += sizeof(blackRed);
    EEPROM.put(addr, blackGreen); addr += sizeof(blackGreen);
    EEPROM.put(addr, blackBlue); addr += sizeof(blackBlue);

    EEPROM.put(addr, whiteRed); addr += sizeof(whiteRed);
    EEPROM.put(addr, whiteGreen); addr += sizeof(whiteGreen);
    EEPROM.put(addr, whiteBlue); addr += sizeof(whiteBlue);

    EEPROM.put(addr, redRed); addr += sizeof(redRed);
    EEPROM.put(addr, redGreen); addr += sizeof(redGreen);
    EEPROM.put(addr, redBlue); addr += sizeof(redBlue);

    EEPROM.put(addr, greenRed); addr += sizeof(greenRed);
    EEPROM.put(addr, greenGreen); addr += sizeof(greenGreen);
    EEPROM.put(addr, greenBlue); addr += sizeof(greenBlue);

    EEPROM.put(addr, blueRed); addr += sizeof(blueRed);
    EEPROM.put(addr, blueGreen); addr += sizeof(blueGreen);
    EEPROM.put(addr, blueBlue); addr += sizeof(blueBlue);

    EEPROM.commit();
    Serial.println("Калибровка сохранена в EEPROM.");
}

// --- Загрузка калибровки из EEPROM ---
void loadCalibration() {
    int addr = 0;
    EEPROM.get(addr, blackRed); addr += sizeof(blackRed);
    EEPROM.get(addr, blackGreen); addr += sizeof(blackGreen);
    EEPROM.get(addr, blackBlue); addr += sizeof(blackBlue);

    EEPROM.get(addr, whiteRed); addr += sizeof(whiteRed);
    EEPROM.get(addr, whiteGreen); addr += sizeof(whiteGreen);
    EEPROM.get(addr, whiteBlue); addr += sizeof(whiteBlue);

    EEPROM.get(addr, redRed); addr += sizeof(redRed);
    EEPROM.get(addr, redGreen); addr += sizeof(redGreen);
    EEPROM.get(addr, redBlue); addr += sizeof(redBlue);

    EEPROM.get(addr, greenRed); addr += sizeof(greenRed);
    EEPROM.get(addr, greenGreen); addr += sizeof(greenGreen);
    EEPROM.get(addr, greenBlue); addr += sizeof(greenBlue);

    EEPROM.get(addr, blueRed); addr += sizeof(blueRed);
    EEPROM.get(addr, blueGreen); addr += sizeof(blueGreen);
    EEPROM.get(addr, blueBlue); addr += sizeof(blueBlue);

    isCalibrated = true;
    Serial.println("Калибровка загружена из EEPROM.");
}

// --- Основной цикл ---
void setup() {
    Serial.begin(115200);
    pinMode(LED_PIN, OUTPUT);
    digitalWrite(LED_PIN, LOW);   
    WiFi.softAP(ssid, password);
    EEPROM.begin(EEPROM_SIZE);
    loadCalibration();
    server.on("/", HTTP_GET, handleRoot);    // Главная страница
    server.on("/enable", HTTP_GET, handleEnableSensor);   // Включение датчика
    server.on("/scan", HTTP_GET, handleScan);   // Сканирование цвета
    server.on("/calibrate_black", HTTP_GET, handleCalibrateBlack);   // Калибровка черного
    server.on("/calibrate_white", HTTP_GET, handleCalibrateWhite);   // Калибровка белого
    server.on("/calibrate_red", HTTP_GET, handleCalibrateRed);   // Калибровка красного
    server.on("/calibrate_green", HTTP_GET, handleCalibrateGreen);   // Калибровка зеленого
    server.on("/calibrate_blue", HTTP_GET, handleCalibrateBlue);   // Калибровка синего
    server.on("/reset_calibration", HTTP_GET, handleResetCalibration);   // Сбросить калибровку
    server.on("/toggle_normalization", HTTP_GET, handleToggleNormalization);
    server.on("/led/on", HTTP_GET, handleLedOn);  // Включить светодиод
    server.on("/led/off", HTTP_GET, handleLedOff);  // Выключить светодиод
    server.on("/led/status", HTTP_GET, handleLedStatus);  // Получить состояние светодиода
    server.begin();
    Serial.println("Сервер запущен...");
}

void loop() {
    server.handleClient();
       
}

