// ------------------------- Підключення бібліотек -------------------------
#include <DHT.h>

// ------------------------- Налаштування датчика DHT21 --------------------
#define DHT_PIN D4
#define DHT_TYPE DHT21          // Датчик DHT21
DHT dht(DHT_PIN, DHT_TYPE);

// ------------------------- Налаштування ультразвукових датчиків ----------
#define TRIG_PIN_1 D0
#define ECHO_PIN_1 D1

#define TRIG_PIN_2 D2
#define ECHO_PIN_2 D3

#define TRIG_PIN_3 D5
#define ECHO_PIN_3 D6

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

// Масив назв напрямків
const char* directionNames[NUM_DIRECTIONS] = {
  "North", "North-East", "East", "South-East",
  "South", "South-West", "West", "North-West"
};

// Змінні для зберігання даних
double windSpeed = 0.0;             // Швидкість вітру
int windDirectionDegrees = 0;       // Напрямок вітру в градусах
float lastTemperature = 0.0;        // Остання виміряна температура
float lastHumidity = 0.0;           // Остання виміряна вологість
int currentDirectionIndex = -1;     // Індекс поточного напрямку

// Лічильник вимірювань
int measurementCount = 0;

unsigned long lastMeasurementTime = 0;

// ------------------------- Функція налаштування --------------------------
void setup() {
  Serial.begin(921600);
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

  // Скидання всіх даних при старті
  resetAll();
}

// ------------------------- Головний цикл програми ------------------------
void loop() {
  unsigned long currentTime = millis();

  if (currentTime - lastMeasurementTime >= INTERVAL_MS) {
    lastMeasurementTime = currentTime;

    readSensorData();
    updateMeasurements();
  }
}

// ------------------------- Функції зчитування даних ----------------------
void readSensorData() {
  float humidity = dht.readHumidity();
  float temperature = dht.readTemperature();

  if (isnan(humidity) || isnan(temperature)) {
    Serial.println("Помилка зчитування з датчика DHT21!");
    return;
  }

  lastTemperature = temperature;
  lastHumidity = humidity;
}

// ------------------------- Функції оновлення вимірювань ------------------
void updateMeasurements() {
  calculateWindParameters();

  measurementCount++;

  int newDirectionIndex = getDirectionIndex(windDirectionDegrees);

  if (newDirectionIndex != currentDirectionIndex) {
    currentDirectionIndex = newDirectionIndex;
  }

  printExcelData();
}

// ------------------------- Функції виводу даних --------------------------
void printExcelData() {
  // Виводимо поточні значення у відповідні стовпці
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

    // Вимірювання для каналу 2 (Канал 1 у зворотному напрямку)
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

    // Вимірювання для каналу 4 (Канал 3 у зворотному напрямку)
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
  float windSpeedChannel2 = (soundSpeed3 - soundSpeed4) / 2.0;  // Швидкість вітру у напрямку, перпендикулярному першому

  windSpeed = sqrt(pow(windSpeedChannel1, 2) + pow(windSpeedChannel2, 2));  // Загальна швидкість вітру

  windDirectionDegrees = int(atan2(windSpeedChannel1, windSpeedChannel2) * 180.0 / PI);  // Напрямок вітру в градусах

  if (windDirectionDegrees < 0) {
    windDirectionDegrees += 360;  // Приводимо до діапазону 0-360 градусів
  }

  // Для відлагодження
  Serial.println("Wind Speed: " + String(windSpeed));
  Serial.println("Wind Direction Degrees: " + String(windDirectionDegrees));
}

// ------------------------- Функції управління програмою ------------------
void resetAll() {
  // Скидаємо значення змінних
  lastTemperature = 0.0;
  lastHumidity = 0.0;
  windSpeed = 0.0;
  windDirectionDegrees = 0;
  currentDirectionIndex = -1;
  measurementCount = 0;
}
