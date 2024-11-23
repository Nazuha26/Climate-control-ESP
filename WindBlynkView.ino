// ------------------------- Підключення бібліотек -------------------------
#define BLYNK_PRINT Serial
#include <ESP8266WiFi.h>
#include <BlynkSimpleEsp8266.h>
#include <DHT.h>

// ------------------------- Налаштування Blynk та Wi-Fi -------------------
// Ваш токен Blynk
#define BLYNK_AUTH_TOKEN "Ваш_Token" // Замість цього впишіть ваш токен пристрою Blynk
char auth[] = BLYNK_AUTH_TOKEN;

// Дані для підключення до Wi-Fi
char ssid[] = "Ваш_SSID";    // Замість цього впишіть назву вашої мережі Wi-Fi
char pass[] = "Ваш_Пароль";  // Замість цього впишіть пароль вашої мережі Wi-Fi

// ------------------------- Віртуальні піни Blynk -------------------------
#define STATUS_VPIN V5          // Статус програми
#define BUTTON_PIN V0           // Кнопка запуску/зупинки
#define TEMPERATURE_PIN V3      // Відображення температури
#define HUMIDITY_PIN V4         // Відображення вологості
#define TERMINAL_PIN V7         // Термінал для введення команд
#define SHOW_SPEED_PIN V8       // Відображення швидкості вітру
#define SHOW_DIRECTION_PIN V9   // Відображення напрямку вітру

// ------------------------- Налаштування датчика DHT11 --------------------
#define DHT_PIN D4
#define DHT_TYPE DHT21          // Датчик DHT21
DHT dht(DHT_PIN, DHT_TYPE);

// ------------------------- Налаштування ультразвукових датчиків ----------
// HC-SR04 №1 Канал 1
#define TRIG_PIN_1 D0
#define ECHO_PIN_1 D1

// HC-SR04 №2 Канал 2 (Канал 1 в зворотному напрямку)
#define TRIG_PIN_2 D2
#define ECHO_PIN_2 D3

// HC-SR04 №3 Канал 3
#define TRIG_PIN_3 D5
#define ECHO_PIN_3 D6

// HC-SR04 №4 Канал 4 (Канал 3 в зворотному напрямку)
#define TRIG_PIN_4 D7
#define ECHO_PIN_4 D8

// ------------------------- Константи та змінні ---------------------------
unsigned long INTERVAL_MS = 4000;              // Інтервал між вимірами в мс за замовчуванням
const int NUM_MEASUREMENTS = 50;               // Кількість вимірювань для усереднення
const unsigned long MEASUREMENT_DELAY = 5;     // Затримка між вимірюваннями в мс

// Відстані між датчиками (в метрах)
const float SENSOR_DISTANCE_1 = 0.3584;  // Канал 1
const float SENSOR_DISTANCE_2 = 0.3584;   // Канал 1 зворотний
const float SENSOR_DISTANCE_3 = 0.3596;  // Канал 2
const float SENSOR_DISTANCE_4 = 0.3596;   // Канал 2 зворотний

const int NUM_DIRECTIONS = 8;    // Кількість напрямків вітру

// Масиви назв напрямків
const char* directionNames[NUM_DIRECTIONS] = {
  "North", "North-East", "East", "South-East",
  "South", "South-West", "West", "North-West"
};

// Змінні для зберігання даних
double windSpeed = 0.0;             // Швидкість вітру
int windDirectionDegrees = 0;       // Напрямок вітру в градусах
bool isRunning = false;             // Статус програми (працює/зупинена)
unsigned long programStartTime = 0; // Час старту програми
unsigned long totalRunTime = 0;     // Загальний час роботи
float lastTemperature = 0.0;        // Остання виміряна температура
float lastHumidity = 0.0;           // Остання виміряна вологість
int currentDirectionIndex = -1;     // Індекс поточного напрямку

// Масиви для зберігання часу по напрямках
unsigned long directionTimes[NUM_DIRECTIONS] = {0};

// Лічильник вимірювань
int measurementCount = 0;

// Константи для команд терміналу
const String CMD_SET_INTERVAL = "set_interval";
const String CMD_RESET_ALL = "reset_all";

// ------------------------- Налаштування таймерів -------------------------
BlynkTimer timer;
WidgetTerminal terminal(TERMINAL_PIN);
int readSensorTimerId = -1;
int updateMeasurementsTimerId = -1;

// ------------------------- Функція налаштування --------------------------
void setup() {
  Serial.begin(921600);
  Blynk.begin(auth, ssid, pass, "blynk.cloud", 80);

  dht.begin();

  // Налаштування пінів ультразвукових датчиків
  pinMode(TRIG_PIN_1, OUTPUT);
  pinMode(ECHO_PIN_1, INPUT);
  pinMode(TRIG_PIN_2, OUTPUT);
  pinMode(ECHO_PIN_2, INPUT);
  pinMode(TRIG_PIN_3, OUTPUT);
  pinMode(ECHO_PIN_3, INPUT);
  pinMode(TRIG_PIN_4, OUTPUT);
  pinMode(ECHO_PIN_4, INPUT);

  // Таймер для оновлення даних на екрані кожну секунду
  timer.setInterval(1000L, updateBlynkData);

  // Ініціалізація терміналу
  terminal.println("Термінал готовий");
  terminal.println("Доступні команди:");
  terminal.println(CMD_SET_INTERVAL + " <milliseconds>");
  terminal.println(CMD_RESET_ALL);
  terminal.flush();

  // Скидання всіх даних при старті
  resetAll();
}

// ------------------------- Головний цикл програми ------------------------
void loop() {
  Blynk.run();
  timer.run();
}

// ------------------------- Функції оновлення даних -----------------------
// Оновлення даних на екрані
void updateBlynkData() {
  updateProgramStatus();

  if (isRunning) {
    Blynk.virtualWrite(TEMPERATURE_PIN, lastTemperature);
    Blynk.virtualWrite(HUMIDITY_PIN, lastHumidity);
  }
}

// Оновлення статусу програми
void updateProgramStatus() {
  unsigned long elapsedSeconds = totalRunTime / 1000;
  String statusMessage;

  if (isRunning) {
    elapsedSeconds += (millis() - programStartTime) / 1000;
    statusMessage = "Працює: " + String(elapsedSeconds) + " с";
  } else {
    statusMessage = "Призупинено (" + String(elapsedSeconds) + " с)";
  }

  // Додаємо номер вимірювання
  statusMessage += " | Вимірювання: " + String(measurementCount);

  Blynk.virtualWrite(STATUS_VPIN, statusMessage);
}

// ------------------------- Обгортки для таймерів -------------------------
void readSensorDataWrapper() {
  readSensorData();
  readSensorTimerId = timer.setInterval(INTERVAL_MS, readSensorData);
}

void updateMeasurementsWrapper() {
  updateMeasurements();
  updateMeasurementsTimerId = timer.setInterval(INTERVAL_MS, updateMeasurements);
}

// ------------------------- Функції зчитування даних ----------------------
// Зчитування даних з датчика DHT11
void readSensorData() {
  if (!isRunning) return;

  float humidity = dht.readHumidity();
  float temperature = dht.readTemperature();

  if (isnan(humidity) || isnan(temperature)) {
    Serial.println("Помилка зчитування з датчика DHT11!");
    return;
  }

  lastTemperature = temperature;
  lastHumidity = humidity;
}

// Оновлення вимірювань
void updateMeasurements() {
  if (!isRunning) return;

  calculateWindParameters();

  measurementCount++;

  int newDirectionIndex = getDirectionIndex(windDirectionDegrees);

  if (newDirectionIndex != currentDirectionIndex) {
    currentDirectionIndex = newDirectionIndex;
  }

  // Записуємо поточні значення у піни
  Blynk.virtualWrite(SHOW_SPEED_PIN, windSpeed);
  Blynk.virtualWrite(SHOW_DIRECTION_PIN, directionNames[currentDirectionIndex]);

  printExcelData();
}

// ------------------------- Обробка подій Blynk ---------------------------
// Обробка кнопки вкл/викл (V0)
BLYNK_WRITE(BUTTON_PIN) {
  int buttonState = param.asInt();
  if (buttonState == 1) {
    // Призупинення програми
    if (isRunning) {
      isRunning = false;
      totalRunTime += millis() - programStartTime;
      Blynk.virtualWrite(STATUS_VPIN, "Програма призупинена (" + String(totalRunTime / 1000) + " с)");

      // Видаляємо таймери
      if (readSensorTimerId != -1) {
        timer.deleteTimer(readSensorTimerId);
        readSensorTimerId = -1;
      }
      if (updateMeasurementsTimerId != -1) {
        timer.deleteTimer(updateMeasurementsTimerId);
        updateMeasurementsTimerId = -1;
      }
    }
  } else {
    // Запуск програми
    if (!isRunning) {
      isRunning = true;
      programStartTime = millis();
      Blynk.virtualWrite(STATUS_VPIN, "Програма працює: 0 с");

      // Створюємо таймери із затримкою
      readSensorTimerId = timer.setTimeout(INTERVAL_MS, readSensorDataWrapper);
      updateMeasurementsTimerId = timer.setTimeout(INTERVAL_MS, updateMeasurementsWrapper);
    }
  }
}

// Обробка введення в термінал (V7)
BLYNK_WRITE(TERMINAL_PIN) {
  String cmd = param.asStr();
  cmd.trim();

  if (cmd.startsWith(CMD_SET_INTERVAL)) {
    String valueStr = cmd.substring(CMD_SET_INTERVAL.length());
    valueStr.trim();
    unsigned long value = valueStr.toInt();
    if (value > 0) {
      INTERVAL_MS = value;
      terminal.println("Інтервал між вимірами встановлено на " + String(INTERVAL_MS) + " мс");

      // Перезапускаємо таймери при необхідності
      if (isRunning) {
        if (readSensorTimerId != -1) {
          timer.deleteTimer(readSensorTimerId);
        }
        if (updateMeasurementsTimerId != -1) {
          timer.deleteTimer(updateMeasurementsTimerId);
        }
        readSensorTimerId = timer.setTimeout(INTERVAL_MS, readSensorDataWrapper);
        updateMeasurementsTimerId = timer.setTimeout(INTERVAL_MS, updateMeasurementsWrapper);
      }
    } else {
      terminal.println("Невірне значення для " + CMD_SET_INTERVAL);
    }
  } else if (cmd.equalsIgnoreCase(CMD_RESET_ALL)) {
    resetAll();
    terminal.println("Виконано " + CMD_RESET_ALL);
  } else {
    terminal.println("Невідома команда");
  }
  terminal.flush();
}

// ------------------------- Функції управління програмою ------------------
// Функція для скидання всіх значень
void resetAll() {
  // Скидаємо значення змінних та масивів
  lastTemperature = 0.0;
  lastHumidity = 0.0;
  windSpeed = 0.0;
  windDirectionDegrees = 0;
  currentDirectionIndex = -1;
  measurementCount = 0;

  Blynk.virtualWrite(TEMPERATURE_PIN, lastTemperature);
  Blynk.virtualWrite(HUMIDITY_PIN, lastHumidity);
  Blynk.virtualWrite(SHOW_SPEED_PIN, windSpeed);
  Blynk.virtualWrite(SHOW_DIRECTION_PIN, "undefined");

  isRunning = false;
  totalRunTime = 0;
  Blynk.virtualWrite(BUTTON_PIN, 1);
  Blynk.virtualWrite(STATUS_VPIN, "Програма призупинена (0 с)");

  // Видалення таймерів, якщо вони існують
  if (readSensorTimerId != -1) {
    timer.deleteTimer(readSensorTimerId);
    readSensorTimerId = -1;
  }
  if (updateMeasurementsTimerId != -1) {
    timer.deleteTimer(updateMeasurementsTimerId);
    updateMeasurementsTimerId = -1;
  }

  // Очищення терміналу та виведення повідомлень
  terminal.clear();
  terminal.println("        Термінал готовий          ");

  char buffer[40];
  sprintf(buffer, "%-32s", "----------------------------------");
  terminal.println(buffer);

  sprintf(buffer, "|       Доступні команди:        |");
  terminal.println(buffer);

  sprintf(buffer, "| %-30s |", (CMD_SET_INTERVAL + " <milliseconds>").c_str());
  terminal.println(buffer);

  sprintf(buffer, "| %-30s |", CMD_RESET_ALL.c_str());
  terminal.println(buffer);

  sprintf(buffer, "%-32s", "---------Оновлено успішно---------");
  terminal.println(buffer);

  terminal.flush();
}

// ------------------------- Функції виводу даних --------------------------
// Функція для виводу даних в Excel
void printExcelData() {
  // Виводимо поточні значення піни у відповідні стовпці
  Serial.print("CELL,SET,E");
  Serial.print(measurementCount + 1); // Починаємо з 2-го рядка
  Serial.print(",");
  Serial.println(windSpeed);

  Serial.print("CELL,SET,F");
  Serial.print(measurementCount + 1);
  Serial.print(",");
  Serial.println(directionNames[currentDirectionIndex]);

  Serial.print("CELL,SET,G");
  Serial.print(measurementCount + 1);
  Serial.print(",");
  Serial.println(lastTemperature);

  Serial.print("CELL,SET,H");
  Serial.print(measurementCount + 1);
  Serial.print(",");
  Serial.println(lastHumidity);

  Serial.print("CELL,SET,A9,");
  Serial.println(INTERVAL_MS);
}

// ------------------------- Функції обчислення ----------------------------
// Функція для визначення індексу напрямку за градусами
int getDirectionIndex(double degrees) {
  degrees = fmod(degrees + 360, 360);

  if (degrees < 22.5 || degrees >= 337.5) return 0; // Північ
  else if (degrees >= 22.5 && degrees < 67.5) return 1; // Північно-Схід
  else if (degrees >= 67.5 && degrees < 112.5) return 2; // Схід
  else if (degrees >= 112.5 && degrees < 157.5) return 3; // Південно-Схід
  else if (degrees >= 157.5 && degrees < 202.5) return 4; // Південь
  else if (degrees >= 202.5 && degrees < 247.5) return 5; // Південно-Захід
  else if (degrees >= 247.5 && degrees < 292.5) return 6; // Захід
  else return 7; // Північно-Захід
}

// Функція для обчислення параметрів вітру
void calculateWindParameters() {
  unsigned long totalImpulseTime1 = 0;
  unsigned long totalImpulseTime2 = 0;
  unsigned long totalImpulseTime3 = 0;
  unsigned long totalImpulseTime4 = 0;

  for (int i = 0; i < NUM_MEASUREMENTS; i++) {
    // Вимірювання для каналу 1
    digitalWrite(TRIG_PIN_1, LOW);
    delayMicroseconds(2);
    digitalWrite(TRIG_PIN_1, HIGH);
    delayMicroseconds(10);
    digitalWrite(TRIG_PIN_1, LOW);
    totalImpulseTime1 += pulseIn(ECHO_PIN_1, HIGH);
    delay(MEASUREMENT_DELAY);

    // Вимірювання для каналу 2 (Канал 1 в зворотному напрямку)
    digitalWrite(TRIG_PIN_2, LOW);
    delayMicroseconds(2);
    digitalWrite(TRIG_PIN_2, HIGH);
    delayMicroseconds(10);
    digitalWrite(TRIG_PIN_2, LOW);
    totalImpulseTime2 += pulseIn(ECHO_PIN_2, HIGH);
    delay(MEASUREMENT_DELAY);

    // Вимірювання для каналу 3
    digitalWrite(TRIG_PIN_3, LOW);
    delayMicroseconds(2);
    digitalWrite(TRIG_PIN_3, HIGH);
    delayMicroseconds(10);
    digitalWrite(TRIG_PIN_3, LOW);
    totalImpulseTime3 += pulseIn(ECHO_PIN_3, HIGH);
    delay(MEASUREMENT_DELAY);

    // Вимірювання для каналу 4 (Канал 3 в зворотному напрямку)
    digitalWrite(TRIG_PIN_4, LOW);
    delayMicroseconds(2);
    digitalWrite(TRIG_PIN_4, HIGH);
    delayMicroseconds(10);
    digitalWrite(TRIG_PIN_4, LOW);
    totalImpulseTime4 += pulseIn(ECHO_PIN_4, HIGH);
    delay(MEASUREMENT_DELAY);
  }

  // Обчислюємо середній час імпульсу для кожного каналу
  float avgImpulseTime1 = totalImpulseTime1 / (float)NUM_MEASUREMENTS;
  float avgImpulseTime2 = totalImpulseTime2 / (float)NUM_MEASUREMENTS;
  float avgImpulseTime3 = totalImpulseTime3 / (float)NUM_MEASUREMENTS;
  float avgImpulseTime4 = totalImpulseTime4 / (float)NUM_MEASUREMENTS;

  // Обчислюємо реальну швидкість ультразвуку в чотирьох каналах
  float soundSpeed1 = SENSOR_DISTANCE_1 / avgImpulseTime1 * 1e6;
  float soundSpeed2 = SENSOR_DISTANCE_2 / avgImpulseTime2 * 1e6;
  float soundSpeed3 = SENSOR_DISTANCE_3 / avgImpulseTime3 * 1e6;
  float soundSpeed4 = SENSOR_DISTANCE_4 / avgImpulseTime4 * 1e6;

  float windSpeedChannel1 = (soundSpeed1 - soundSpeed2) / 2.0;  // Швидкість вітру в першому напрямку
  float windSpeedChannel2 = (soundSpeed3 - soundSpeed4) / 2.0;  // Швидкість вітру в напрямку який перпендикулярен першому 

  windSpeed = sqrt(pow(windSpeedChannel1, 2) + pow(windSpeedChannel2, 2));  // Загальна швидкість вітру

  windDirectionDegrees = int(atan2(windSpeedChannel1, windSpeedChannel2) * 180.0 / PI);  // Напрямок вітру в градусах

  if (windDirectionDegrees < 0) {
    windDirectionDegrees += 360;  // Приводимо до діапазону 0-360 градусів
  }

  // For debug
  Serial.println("Wind Speed: " + String(windSpeed));
  Serial.println("Wind Direction Degrees: " + String(windDirectionDegrees));
}
