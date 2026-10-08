/*
  ESP32 OLED Digital Clock + BMP280 + Date
  
  Экраны:
    1. Часы (ЧЧ:ММ с мигающим двоеточием) - Основной
    2. Дата (ДД.ММ.ГГ)
    3. Датчик BMP280 (Температура C, Давление мм рт.ст.)

  Управление:
    Кнопка на GPIO 4 (подключение к GND).
    Переключение по кругу. Через 10 сек бездействия — возврат на Экран 1.
*/

#include <WiFi.h>
#include <time.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Adafruit_BMP280.h>

// ===================== НАСТРОЙКИ WI-FI =====================
const char* WIFI_SSID     = "ВАШ_SSID";       // <-- ваш SSID
const char* WIFI_PASSWORD = "ВАШ_ПАРОЛЬ";     // <-- ваш пароль
// ===========================================================

// NTP Настройки (Россия)
const char* ntpServer1 = "0.ru.pool.ntp.org";
const char* ntpServer2 = "1.ru.pool.ntp.org";
const char* ntpServer3 = "ntp1.vniiftri.ru";

// Часовой пояс: Москва UTC+3
const long  gmtOffset_sec = 5 * 3600;
const int   daylightOffset_sec = 0;

// Дисплей OLED SSD1306 (128x64)
#define SCREEN_WIDTH  128
#define SCREEN_HEIGHT 64
#define OLED_RESET    -1
#define SCREEN_ADDRESS 0x3C

// Кнопка переключения экранов
#define BUTTON_PIN 4

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);
Adafruit_BMP280 bmp; // Поддержка BMP280 (I2C)

bool bmpStatus = false;
bool timeSynced = false;
unsigned long lastNtpTry = 0;

// Логика экранов
int currentScreen = 1;                  // 1 - Часы, 2 - Дата, 3 - BMP280
unsigned long lastButtonPress = 0;      // Время последнего действия с кнопкой
int buttonState = HIGH;
int lastButtonState = HIGH;
unsigned long lastDebounceTime = 0;
const unsigned long debounceDelay = 50; // Защита от дребезга 50мс

void setup() {
  Serial.begin(115200);

  // Настройка кнопки
  pinMode(BUTTON_PIN, INPUT_PULLUP);
  buttonState = digitalRead(BUTTON_PIN);
  lastButtonState = buttonState;

  // Инициализация I2C шины (SDA = 21, SCL = 22)
  Wire.begin(21, 22);

  // Инициализация OLED дисплея
  if (!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS)) {
    Serial.println(F("OLED не найден"));
    while (true) delay(1000);
  }

  // МГНОВЕННАЯ ОЧИСТКА И ВЫВОД ПРОЧЕРКОВ (убираем шум буфера)
  display.clearDisplay();
  showDashes();
  display.display();

  // Инициализация датчика BMP280
  if (bmp.begin(0x76) || bmp.begin(0x77)) {
    bmpStatus = true;
    Serial.println(F("BMP280 OK"));
  } else {
    bmpStatus = false;
    Serial.println(F("BMP280 не найден!"));
  }

  // Подключение к Wi-Fi
  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(true);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 40) {
    delay(500);
    attempts++;
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println(F("WiFi OK"));
    configTime(gmtOffset_sec, daylightOffset_sec, ntpServer1, ntpServer2, ntpServer3);

    struct tm timeinfo;
    int wait = 0;
    while (!getLocalTime(&timeinfo, 100) && wait < 15) {
      delay(500);
      wait++;
    }
    timeSynced = getLocalTime(&timeinfo, 100);
  } else {
    Serial.println(F("WiFi FAIL"));
  }

  // Принудительно ставим 1 экран после Wi-Fi
  currentScreen = 1;
  lastButtonPress = millis();
}

void loop() {
  // 1. Опрос кнопки
  handleButton();

  // 2. Периодическая подстройка времени (если не синхронизировано)
  if (!timeSynced && millis() - lastNtpTry > 15000) {
    lastNtpTry = millis();
    if (WiFi.status() == WL_CONNECTED) {
      struct tm ti;
      timeSynced = getLocalTime(&ti, 100);
    }
  }

  // 3. Автовозврат на 1-й экран через 10 секунд бездействия
  if (currentScreen != 1 && (millis() - lastButtonPress > 10000)) {
    currentScreen = 1;
  }

  // 4. Отрисовка текущего экрана
  struct tm timeinfo;
  bool hasTime = getLocalTime(&timeinfo, 10);
  if (hasTime) timeSynced = true;

  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);

  switch (currentScreen) {
    case 1:
      drawClockScreen(hasTime ? &timeinfo : NULL);
      break;
    case 2:
      drawDateScreen(hasTime ? &timeinfo : NULL);
      break;
    case 3:
      drawSensorScreen();
      break;
  }

  display.display();
  delay(100);
}

// ==================== ФУНКЦИИ ОТРИСОВКИ ====================

// Экран 1: Часы (ЧЧ:ММ)
void drawClockScreen(struct tm* timeinfo) {
  if (timeinfo == NULL) {
    showDashes();
    return;
  }
  char tH[3], tM[3];
  strftime(tH, sizeof(tH), "%H", timeinfo);
  strftime(tM, sizeof(tM), "%M", timeinfo);

  bool colonOn = (timeinfo->tm_sec % 2 == 0);

  display.setTextSize(4);
  display.setCursor(8, 16);
  display.print(tH);
  display.print(colonOn ? ":" : " ");
  display.print(tM);
}

// Экран 2: Дата (ДД.ММ.ГГ)
void drawDateScreen(struct tm* timeinfo) {
  if (timeinfo == NULL) {
    display.setTextSize(2);
    display.setCursor(16, 24);
    display.print("--.--.--");
    return;
  }
  char dStr[9]; // Формат ДД.ММ.ГГ
  strftime(dStr, sizeof(dStr), "%d.%m.%y", timeinfo);

  display.setTextSize(2);
  display.setCursor(16, 24);
  display.print(dStr);
}

// Экран 3: Датчик BMP280
void drawSensorScreen() {
  display.setTextSize(2);

  if (!bmpStatus) {
    display.setCursor(8, 12);
    display.print("T: --.- C");
    display.setCursor(8, 36);
    display.print("P: --- mm");
    return;
  }

  float temp = bmp.readTemperature();
  float pressPa = bmp.readPressure();

  if (isnan(temp) || isnan(pressPa) || pressPa == 0.0) {
    display.setCursor(8, 12);
    display.print("T: --.- C");
    display.setCursor(8, 36);
    display.print("P: --- mm");
    return;
  }

  float pressMm = pressPa * 0.00750062; // Перевод Па в мм рт. ст.

  // 1 строка - Температура
  display.setCursor(4, 12);
  display.print("T:");
  display.print(temp, 1);
  display.print((char)247); // Градус °
  display.print("C");

  // 2 строка - Давление
  display.setCursor(4, 36);
  display.print("P:");
  display.print((int)pressMm);
  display.print(" mm");
}

// Прочерки по умолчанию
void showDashes() {
  display.setTextSize(4);
  display.setCursor(8, 16);
  display.print("--:--");
}

// Обработка кнопки
void handleButton() {
  int reading = digitalRead(BUTTON_PIN);

  if (reading != lastButtonState) {
    lastDebounceTime = millis();
  }

  if ((millis() - lastDebounceTime) > debounceDelay) {
    if (reading != buttonState) {
      buttonState = reading;
      // Нажатие фикируется строго при переходе в LOW
      if (buttonState == LOW) {
        currentScreen++;
        if (currentScreen > 3) {
          currentScreen = 1;
        }
        lastButtonPress = millis(); // Перезапуск таймера автовозврата
      }
    }
  }
  lastButtonState = reading;
}
