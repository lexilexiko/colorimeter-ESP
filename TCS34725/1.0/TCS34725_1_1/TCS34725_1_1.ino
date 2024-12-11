#include <Wire.h>
#include <Adafruit_TCS34725.h>
#include <WiFi.h>
#include <WebServer.h>
#include <EEPROM.h>
#include "interface.h"

// --- Настройки Wi-Fi ---
const char* ssid = "ESP32_Colorimeter";
const char* password = "12345678";

// --- Настройки I2C ---
#define SDA_PIN 3  // Пин SDA (можно заменить на другие)
#define SCL_PIN 4  // Пин SCL (можно заменить на другие)

// --- Объявления объектов и переменных ---
Adafruit_TCS34725 tcs = Adafruit_TCS34725(TCS34725_INTEGRATIONTIME_600MS, TCS34725_GAIN_1X);
WebServer server(80);

bool sensorInitialized = false;
bool isCalibrated = false;
int calStep = 0; // Этап калибровки
bool normalizeEnabled = true; // Нормализация отключена по умолчанию

// Калибровочные данные
uint16_t blackCal[3], whiteCal[3], redCal[3], greenCal[3], blueCal[3];

// Текущие значения
uint16_t prevRed = 0, prevGreen = 0, prevBlue = 0;
uint16_t currRed = 0, currGreen = 0, currBlue = 0;
uint16_t clear = 0;
float colorTemperature = 0;
float lux = 0;

// --- EEPROM настройки ---
#define EEPROM_SIZE 64

// --- Загрузка и сохранение калибровки ---
void loadCalibrationData() {
    EEPROM.get(0, blackCal);
    EEPROM.get(12, whiteCal);
    EEPROM.get(24, redCal);
    EEPROM.get(36, greenCal);
    EEPROM.get(48, blueCal);
    
    if (blackCal[0] == 0 && whiteCal[0] == 0) {
        isCalibrated = false;
    } else {
        isCalibrated = true;
    }
}

void saveCalibrationData() {
    EEPROM.put(0, blackCal);
    EEPROM.put(12, whiteCal);
    EEPROM.put(24, redCal);
    EEPROM.put(36, greenCal);
    EEPROM.put(48, blueCal);
    EEPROM.commit();
}

// --- Инициализация датчика ---
void initializeSensor() {
    if (tcs.begin()) {
        sensorInitialized = true;
        Serial.println("Датчик инициализирован.");
    } else {
        Serial.println("Ошибка инициализации датчика.");
    }
}

// --- Нормализация цвета ---
int normalizeColor(int color, int black, int white) {
    if (!normalizeEnabled || !isCalibrated) return color; // Если нормализация выключена или калибровка не выполнена
    int range = white - black;
    if (range == 0) return 0;
    return map(color, black, white, 0, 255);
}

// --- Обработчики веб-сервера ---
void handleEnableSensor() {
    if (!sensorInitialized) {
        initializeSensor();
        server.send(200, "text/plain", "Датчик включен");
    } else {
        server.send(200, "text/plain", "Датчик уже включен");
    }
}

void handleScan() {
    if (!sensorInitialized) {
        server.send(400, "text/plain", "Датчик не включён");
        return;
    }

    uint16_t r, g, b, c;
    tcs.getRawData(&r, &g, &b, &c);

    prevRed = currRed;
    prevGreen = currGreen;
    prevBlue = currBlue;

    currRed = normalizeColor(r, blackCal[0], whiteCal[0]);
    currGreen = normalizeColor(g, blackCal[1], whiteCal[1]);
    currBlue = normalizeColor(b, blackCal[2], whiteCal[2]);
    clear = c;

    colorTemperature = tcs.calculateColorTemperature_dn40(r, g, b, c);
    lux = tcs.calculateLux(r, g, b);

    // Лог данных
    Serial.printf("Сканирование:\n");
    Serial.printf("R: %d, G: %d, B: %d, Clear: %d\n", currRed, currGreen, currBlue, clear);
    Serial.printf("Temp: %.2f K, Lux: %.2f\n", colorTemperature, lux);

    String response = "{\"red\":" + String(currRed) +
                      ",\"green\":" + String(currGreen) +
                      ",\"blue\":" + String(currBlue) +
                      ",\"clear\":" + String(clear) +
                      ",\"colorTemperature\":" + String(colorTemperature) +
                      ",\"lux\":" + String(lux) + "}";
    server.send(200, "application/json", response);
}

void handleCalibration() {
    if (!sensorInitialized) {
        server.send(400, "text/plain", "Датчик не включён");
        return;
    }

    uint16_t r, g, b, c;
    tcs.getRawData(&r, &g, &b, &c);

    if (calStep == 0) {
        blackCal[0] = r; blackCal[1] = g; blackCal[2] = b;
        calStep++;
        server.send(200, "text/plain", "Калибровка черного завершена. Поднесите белый объект.");
    } else if (calStep == 1) {
        whiteCal[0] = r; whiteCal[1] = g; whiteCal[2] = b;
        calStep++;
        server.send(200, "text/plain", "Калибровка белого завершена. Поднесите красный объект.");
    } else if (calStep == 2) {
        redCal[0] = r; redCal[1] = g; redCal[2] = b;
        calStep++;
        server.send(200, "text/plain", "Калибровка красного завершена. Поднесите зеленый объект.");
    } else if (calStep == 3) {
        greenCal[0] = r; greenCal[1] = g; greenCal[2] = b;
        calStep++;
        server.send(200, "text/plain", "Калибровка зелёного завершена. Поднесите синий объект.");
    } else if (calStep == 4) {
        blueCal[0] = r; blueCal[1] = g; blueCal[2] = b;
        isCalibrated = true;
        calStep = 0;
        saveCalibrationData();
        server.send(200, "text/plain", "Калибровка завершена.");
    }
}

void handleResetCalibration() {
    memset(blackCal, 0, sizeof(blackCal));
    memset(whiteCal, 0, sizeof(whiteCal));
    memset(redCal, 0, sizeof(redCal));
    memset(greenCal, 0, sizeof(greenCal));
    memset(blueCal, 0, sizeof(blueCal));
    isCalibrated = false;
    calStep = 0;
    saveCalibrationData();
    server.send(200, "text/plain", "Калибровка сброшена");
}

void handleManualCalibrationAll() {
    if (!server.hasArg("black_r") || !server.hasArg("white_r") || !server.hasArg("red_r") ||
        !server.hasArg("green_r") || !server.hasArg("blue_r")) {
        server.send(400, "text/plain", "Ошибка: Не все параметры переданы");
        return;
    }

    blackCal[0] = server.arg("black_r").toInt();
    blackCal[1] = server.arg("black_g").toInt();
    blackCal[2] = server.arg("black_b").toInt();

    whiteCal[0] = server.arg("white_r").toInt();
    whiteCal[1] = server.arg("white_g").toInt();
    whiteCal[2] = server.arg("white_b").toInt();

    redCal[0] = server.arg("red_r").toInt();
    redCal[1] = server.arg("red_g").toInt();
    redCal[2] = server.arg("red_b").toInt();

    greenCal[0] = server.arg("green_r").toInt();
    greenCal[1] = server.arg("green_g").toInt();
    greenCal[2] = server.arg("green_b").toInt();

    blueCal[0] = server.arg("blue_r").toInt();
    blueCal[1] = server.arg("blue_g").toInt();
    blueCal[2] = server.arg("blue_b").toInt();

    saveCalibrationData();
    server.send(200, "text/plain", "Все данные калибровки сохранены");
    Serial.println("Ручная калибровка всех цветов завершена.");
}

void handleToggleNormalization() {
    normalizeEnabled = !normalizeEnabled;
    server.send(200, "text/plain", normalizeEnabled ? "Нормализация включена" : "Нормализация выключена");
}

void handleCalibrationData() {
    if (!sensorInitialized) {
        server.send(400, "text/plain", "Датчик не включён");
        return;
    }

    String calibrationData = "Калибровочные данные:\n";
    calibrationData += "Черный: R=" + String(blackCal[0]) + " G=" + String(blackCal[1]) + " B=" + String(blackCal[2]) + "\n";
    calibrationData += "Белый: R=" + String(whiteCal[0]) + " G=" + String(whiteCal[1]) + " B=" + String(whiteCal[2]) + "\n";
    calibrationData += "Красный: R=" + String(redCal[0]) + " G=" + String(redCal[1]) + " B=" + String(redCal[2]) + "\n";
    calibrationData += "Зеленый: R=" + String(greenCal[0]) + " G=" + String(greenCal[1]) + " B=" + String(greenCal[2]) + "\n";
    calibrationData += "Синий: R=" + String(blueCal[0]) + " G=" + String(blueCal[1]) + " B=" + String(blueCal[2]) + "\n";

    server.send(200, "text/plain", calibrationData);
}

void setupWiFi() {
    WiFi.softAP(ssid, password);
    Serial.println("Wi-Fi точка доступа создана");
}

void setupServer() {
    server.on("/", []() {
        String page = generateInterface(currRed, currGreen, currBlue, prevRed, prevGreen, prevBlue, sensorInitialized, isCalibrated, calStep, normalizeEnabled, colorTemperature, lux);
        server.send(200, "text/html", page);
    });
    server.on("/enable", handleEnableSensor);
    server.on("/scan", handleScan);
    server.on("/calibrate", handleCalibration);
    server.on("/reset_calibration", handleResetCalibration);
    server.on("/toggle_normalization", handleToggleNormalization);
    server.on("/calibration_data", HTTP_GET, handleCalibrationData);
    server.on("/manual_calibrate_all", handleManualCalibrationAll);
    server.begin();
    Serial.println("Веб-сервер запущен");
}

void setup() {
    Serial.begin(115200);
    Wire.begin(SDA_PIN, SCL_PIN);

    EEPROM.begin(EEPROM_SIZE);
    loadCalibrationData();
    setupWiFi();
    setupServer();
}

void loop() {
    server.handleClient();
}
