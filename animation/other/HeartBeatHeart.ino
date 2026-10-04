#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

// ---------- Экран ----------
#define SCREEN_W  128
#define SCREEN_H  64
#define OLED_ADDR 0x3C      // если не заработает, попробуй 0x3D
#define SDA_PIN   21
#define SCL_PIN   22

Adafruit_SSD1306 display(SCREEN_W, SCREEN_H, &Wire, -1);

// ---------- Таблицы точек ----------
struct Pt { float x; float y; };

const Pt PQRST[] = {
  {0.000f,  0.00f}, {0.200f,  0.00f}, {0.240f,  0.12f}, {0.280f,  0.16f},
  {0.320f,  0.12f}, {0.360f,  0.00f}, {0.400f,  0.00f}, {0.435f, -0.08f},
  {0.460f,  0.55f}, {0.485f,  1.00f}, {0.510f,  0.55f}, {0.535f, -0.32f},
  {0.565f, -0.06f}, {0.610f,  0.00f}, {0.680f,  0.10f}, {0.730f,  0.26f},
  {0.780f,  0.30f}, {0.830f,  0.16f}, {0.880f,  0.03f}, {0.940f,  0.00f},
  {1.000f,  0.00f}
};

const Pt HEART[] = {
  {0.000f, 0.000f}, {0.410f, 0.000f}, {0.361f, 0.032f}, {0.295f, 0.097f},
  {0.236f, 0.177f}, {0.184f, 0.267f}, {0.144f, 0.368f}, {0.121f, 0.458f},
  {0.118f, 0.530f}, {0.134f, 0.610f}, {0.174f, 0.664f}, {0.230f, 0.693f},
  {0.289f, 0.700f}, {0.354f, 0.682f}, {0.413f, 0.635f}, {0.459f, 0.563f},
  {0.490f, 0.494f}, {0.521f, 0.563f}, {0.567f, 0.635f}, {0.626f, 0.682f},
  {0.691f, 0.700f}, {0.750f, 0.693f}, {0.806f, 0.664f}, {0.846f, 0.610f},
  {0.862f, 0.530f}, {0.859f, 0.458f}, {0.836f, 0.368f}, {0.796f, 0.267f},
  {0.744f, 0.177f}, {0.685f, 0.097f}, {0.619f, 0.032f}, {0.570f, 0.000f},
  {1.000f, 0.000f}
};

const int PQRST_N = sizeof(PQRST) / sizeof(PQRST[0]);
const int HEART_N = sizeof(HEART) / sizeof(HEART[0]);

// ---------- Настройки ----------
#define K_ECG   0
#define K_HEART 1

const int   BASE_Y  = 48;     // y изолинии на экране
const float AMP     = 42.0f;  // высота зубца R (y = 1.0) в пикселях
const int   W_ECG   = 42;     // ширина одного удара ЭКГ в пикселях
const int   W_HEART = (int)(AMP * 1.10f + 0.5f);  // пропорции как на скриншоте

const int MIN_BEATS = 2;      // сколько ударов ЭКГ до сердечка: от 2
const int MAX_BEATS = 6;      // ... до 6 включительно
const int SPEED     = 2;      // пикселей за кадр (скорость движения влево)
const int FRAME_MS  = 30;     // период кадра
const int SPACING = 20;       // промежуток между символами в пикселях

// ---------- Очередь символов на экране ----------
const int MAXC = 8;
int cycX[MAXC];               // левый край символа
int cycK[MAXC];               // тип: K_ECG или K_HEART
int nCyc = 0;
int beatsToHeart = 0;

int cycW(int k) { return (k == K_HEART) ? W_HEART : W_ECG; }

void addCycle() {
    int x0 = (nCyc == 0) ? SCREEN_W : cycX[nCyc - 1] + cycW(cycK[nCyc - 1]) + SPACING;
  int k;
  if (beatsToHeart <= 0) {
    k = K_HEART;
    beatsToHeart = random(MIN_BEATS, MAX_BEATS + 1);  // 2..6
  } else {
    k = K_ECG;
    beatsToHeart--;
  }
  cycX[nCyc] = x0;
  cycK[nCyc] = k;
  nCyc++;
}

void drawCycle(int x0, int k) {
  const Pt *t = (k == K_HEART) ? HEART : PQRST;
  int n = (k == K_HEART) ? HEART_N : PQRST_N;
  int w = cycW(k);
  int px = x0 + (int)roundf(t[0].x * w);
  int py = BASE_Y - (int)roundf(t[0].y * AMP);
  for (int i = 1; i < n; i++) {
    int sx = x0 + (int)roundf(t[i].x * w);
    int sy = BASE_Y - (int)roundf(t[i].y * AMP);
    display.drawLine(px, py, sx, sy, SSD1306_WHITE);
    px = sx;
    py = sy;
    display.drawFastHLine(x0 + w, BASE_Y, SPACING + 1, SSD1306_WHITE);
  }
}

void setup() {
  Wire.begin(SDA_PIN, SCL_PIN);
  Wire.setClock(400000);
  if (!display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR)) {
    for (;;) delay(1000);                // экран не найден
  }
  randomSeed(esp_random());
  beatsToHeart = random(MIN_BEATS, MAX_BEATS + 1);
  display.clearDisplay();
  display.display();
}

void loop() {
  uint32_t t0 = millis();

  // сдвиг влево
  for (int i = 0; i < nCyc; i++) cycX[i] -= SPEED;

  // убираем символы, полностью ушедшие за левый край
    while (nCyc > 0 && cycX[0] + cycW(cycK[0]) + SPACING < 0) {
    for (int i = 1; i < nCyc; i++) { cycX[i - 1] = cycX[i]; cycK[i - 1] = cycK[i]; }
    nCyc--;
  }

  // добавляем новые справа
  while (nCyc < MAXC &&
         (nCyc == 0 || cycX[nCyc - 1] + cycW(cycK[nCyc - 1]) <= SCREEN_W)) {
    addCycle();
  }

  // рисуем
  display.clearDisplay();
  for (int i = 0; i < nCyc; i++) drawCycle(cycX[i], cycK[i]);
  display.display();

  uint32_t dt = millis() - t0;
  if (dt < FRAME_MS) delay(FRAME_MS - dt);
}
