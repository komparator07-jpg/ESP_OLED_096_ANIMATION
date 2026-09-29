#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#define SCREEN_WIDTH  128
#define SCREEN_HEIGHT 64
#define OLED_RESET    -1
#define SCREEN_ADDR   0x3C

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

const int   BASE_Y  = 42;
const float AMP     = 32.0f;
const float BPM     = 75.0f;
const float STEP_MS = 18.0f;

uint8_t ecgBuf[SCREEN_WIDTH];
float   phaseAcc = 0.0f;
unsigned long lastMicros = 0;

struct Pt { float t, a; };
const Pt PQRST[] = {
  {0.000f,  0.00f},
  {0.200f,  0.00f},
  {0.240f,  0.12f},
  {0.280f,  0.16f},
  {0.320f,  0.12f},
  {0.360f,  0.00f},
  {0.400f,  0.00f},
  {0.435f, -0.08f},
  {0.460f,  0.55f},
  {0.485f,  1.00f},
  {0.510f,  0.55f},
  {0.535f, -0.32f},
  {0.565f, -0.06f},
  {0.610f,  0.00f},
  {0.680f,  0.10f},
  {0.730f,  0.26f},
  {0.780f,  0.30f},
  {0.830f,  0.16f},
  {0.880f,  0.03f},
  {0.940f,  0.00f},
  {1.000f,  0.00f}
};
const int PQRST_LEN = sizeof(PQRST) / sizeof(PQRST[0]);

float ecgSample(float phase) {
  if (phase <= PQRST[0].t)             return PQRST[0].a;
  if (phase >= PQRST[PQRST_LEN - 1].t) return PQRST[PQRST_LEN - 1].a;
  for (int i = 0; i < PQRST_LEN - 1; i++) {
    if (phase >= PQRST[i].t && phase <= PQRST[i + 1].t) {
      float span = PQRST[i + 1].t - PQRST[i].t;
      float k = (span > 0.0f) ? (phase - PQRST[i].t) / span : 0.0f;
      k = k * k * (3.0f - 2.0f * k);
      return PQRST[i].a + (PQRST[i + 1].a - PQRST[i].a) * k;
    }
  }
  return 0.0f;
}

inline uint8_t toY(float v) {
  int y = BASE_Y - (int)(v * AMP);
  if (y < 0) y = 0;
  if (y > SCREEN_HEIGHT - 1) y = SCREEN_HEIGHT - 1;
  return (uint8_t)y;
}

void setup() {
  Wire.begin(21, 22);
  if (!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDR)) {
    for (;;);
  }
  display.clearDisplay();
  display.display();

  float phaseStep = STEP_MS / (60000.0f / BPM);
  for (int x = 0; x < SCREEN_WIDTH; x++) {
    phaseAcc += phaseStep;
    if (phaseAcc >= 1.0f) phaseAcc -= 1.0f;
    ecgBuf[x] = toY(ecgSample(phaseAcc));
  }

  lastMicros = micros();
}

void loop() {
  unsigned long nowUs = micros();
  float dtMs = (nowUs - lastMicros) / 1000.0f;
  lastMicros = nowUs;
  if (dtMs > 100.0f) dtMs = 100.0f;

  static float pixelAcc = 0.0f;
  pixelAcc += dtMs / STEP_MS;
  int shift = (int)pixelAcc;

  if (shift == 0) {
    display.clearDisplay();
    for (int x = 0; x < SCREEN_WIDTH - 1; x++)
      display.drawLine(x, ecgBuf[x], x + 1, ecgBuf[x + 1], WHITE);
    display.display();
    delay(5);
    return;
  }

  pixelAcc -= shift;
  if (shift > SCREEN_WIDTH) shift = SCREEN_WIDTH;

  memmove(&ecgBuf[0], &ecgBuf[shift], SCREEN_WIDTH - shift);

  float phaseStep = STEP_MS / (60000.0f / BPM);
  for (int i = 0; i < shift; i++) {
    phaseAcc += phaseStep;
    if (phaseAcc >= 1.0f) phaseAcc -= 1.0f;
    ecgBuf[SCREEN_WIDTH - shift + i] = toY(ecgSample(phaseAcc));
  }

  display.clearDisplay();
  for (int x = 0; x < SCREEN_WIDTH - 1; x++)
    display.drawLine(x, ecgBuf[x], x + 1, ecgBuf[x + 1], WHITE);
  display.display();
  delay(16);
}
