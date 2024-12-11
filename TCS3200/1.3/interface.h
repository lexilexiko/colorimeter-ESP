#ifndef INTERFACE_H
#define INTERFACE_H

#include <Arduino.h>

/**
 * Генерирует HTML-страницу для интерфейса управления колориметром.
 * @param red Текущий красный цвет.
 * @param green Текущий зеленый цвет.
 * @param blue Текущий синий цвет.
 * @param prevRed Предыдущий красный цвет.
 * @param prevGreen Предыдущий зеленый цвет.
 * @param prevBlue Предыдущий синий цвет.
 * @param sensorInitialized Состояние датчика (включен/выключен).
 * @param isCalibrated Статус завершенности калибровки.
 * @param calStep Текущий этап калибровки.
 * @param normalizeEnabled Флаг состояния нормализации.
 * @return Сформированный HTML-код.
 */
String generateInterface(int red, int green, int blue, int prevRed, int prevGreen, int prevBlue, 
                         bool sensorInitialized, bool isCalibrated, int calStep, bool normalizeEnabled) {
    String html = R"rawliteral(
<!DOCTYPE html>
<html lang="ru">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>ESP32 Колориметр</title>
    <style>
        body {
            font-family: Arial, sans-serif;
            background-color: #f4f4f9;
            margin: 0;
            padding: 0;
            display: flex;
            flex-direction: column;
            align-items: center;
        }
        h1 {
            margin: 20px 0;
            color: #333;
        }
        .color-box {
            width: 150px;
            height: 150px;
            margin: 10px;
            border-radius: 10px;
            border: 2px solid #ddd;
        }
        .info {
            font-size: 18px;
            color: #555;
            margin: 5px 0;
        }
        .button {
            display: inline-block;
            padding: 10px 20px;
            margin: 10px;
            font-size: 16px;
            color: white;
            background-color: #4CAF50;
            border: none;
            border-radius: 5px;
            cursor: pointer;
            transition: background-color 0.3s ease;
        }
        .button:hover {
            background-color: #45a049;
        }
        .button:disabled {
            background-color: #aaa;
            cursor: not-allowed;
        }
        .button-container {
            display: flex;
            flex-wrap: wrap;
            justify-content: center;
            gap: 10px;
        }
        .color-section {
            display: flex;
            justify-content: center;
            align-items: center;
            gap: 20px;
            margin-top: 20px;
        }
        .color-info {
            text-align: center;
            color: #333;
        }
        footer {
            margin-top: 20px;
            font-size: 14px;
            color: #777;
        }
    </style>
</head>
<body>
    <h1>КолорМетр</h1>

    <div class="color-section">
        <div class="color-info">
            <div class="color-box" style="background-color: rgb(%PREV_RED%, %PREV_GREEN%, %PREV_BLUE%);"></div>
            <p>Было:</p>
            <p>R: %PREV_RED%</p>
            <p>G: %PREV_GREEN%</p>
            <p>B: %PREV_BLUE%</p>
        </div>
        <div class="color-info">
            <div class="color-box" style="background-color: rgb(%RED%, %GREEN%, %BLUE%);"></div>
            <p>Стало:</p>
            <p>R: %RED%</p>
            <p>G: %GREEN%</p>
            <p>B: %BLUE%</p>
        </div>
    </div>

    <div class="button-container">
        <button class="button" onclick="enableSensor()" %SENSOR_DISABLED%>Включить датчик</button>
        <button class="button" onclick="scanColor()">Сканировать</button>
    </div>

    <h2>Управление LED</h2>
    <div class="button-container">
        <button id="led-toggle" class="button" onclick="toggleLed()">Загрузка...</button>
    </div>
    <h2>Управление яркостью</h2>

    <h2>Калибровка</h2>
    <div class="button-container">
        %CALIBRATION_BUTTONS%
        <button class="button" onclick="resetCalibration()" %RESET_DISABLED%>Сбросить калибровку</button>
    </div>

    <h2>Нормализация</h2>
    <div class="button-container">
        <button class="button" onclick="toggleNormalization()">Нормализация: %NORMALIZE_STATE%</button>
    </div>

    <footer>КолорМетр © 2024</footer>

    <script>
        let ledState = false;

        function enableSensor() {
            fetch('/enable').then(response => response.text()).then(() => location.reload());
        }

        function scanColor() {
            fetch('/scan')
                .then(response => response.json())
                .then(data => location.reload());
        }

        function calibrate(color) {
            fetch(`/calibrate_${color}`).then(response => response.text()).then(() => location.reload());
        }

        function resetCalibration() {
            fetch('/reset_calibration').then(response => response.text()).then(() => location.reload());
        }

        function toggleLed() {
            const action = ledState ? '/led/off' : '/led/on';
            fetch(action).then(() => {
                ledState = !ledState;
                updateLedButton();
            });
        }

        function updateLedButton() {
            const button = document.getElementById('led-toggle');
            button.textContent = ledState ? 'Выключить LED' : 'Включить LED';
        }
        
        
        function toggleNormalization() {
            fetch('/toggle_normalization').then(() => location.reload());
        }

        fetch('/led/status')
            .then(response => response.json())
            .then(data => {
                ledState = data.led === 'on';
                updateLedButton();
            });
    </script>
</body>
</html>
    )rawliteral";

    // Замена токенов на актуальные данные
    html.replace("%RED%", String(red));
    html.replace("%GREEN%", String(green));
    html.replace("%BLUE%", String(blue));
    html.replace("%PREV_RED%", String(prevRed));
    html.replace("%PREV_GREEN%", String(prevGreen));
    html.replace("%PREV_BLUE%", String(prevBlue));
    html.replace("%SENSOR_DISABLED%", sensorInitialized ? "disabled" : "");
    html.replace("%NORMALIZE_STATE%", normalizeEnabled ? "включена" : "выключена");

    String calibrationButtons;
    if (!isCalibrated) {
        if (calStep == 0) calibrationButtons = "<button class='button' onclick='calibrate(\"black\")'>Калибровка черного</button>";
        else if (calStep == 1) calibrationButtons = "<button class='button' onclick='calibrate(\"white\")'>Калибровка белого</button>";
        else if (calStep == 2) calibrationButtons = "<button class='button' onclick='calibrate(\"red\")'>Калибровка красного</button>";
        else if (calStep == 3) calibrationButtons = "<button class='button' onclick='calibrate(\"green\")'>Калибровка зелёного</button>";
        else if (calStep == 4) calibrationButtons = "<button class='button' onclick='calibrate(\"blue\")'>Калибровка синего</button>";
    } else {
        calibrationButtons = "<p>Калибровка завершена!</p>";
    }
    html.replace("%CALIBRATION_BUTTONS%", calibrationButtons);

    String resetDisabled = isCalibrated ? "" : "disabled";
    html.replace("%RESET_DISABLED%", resetDisabled);

    return html;
}

#endif
