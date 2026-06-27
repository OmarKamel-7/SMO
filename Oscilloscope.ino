// =====================================================================
// scope.ino  —  SMO Handheld Oscilloscope Module
// =====================================================================
//
// This file is a second tab in the same Arduino sketch as smo.ino.
// It relies on globals already defined there: tft, drawHeader(),
// COLOR_BG / COLOR_HEADER / COLOR_ACCENT / COLOR_WHITE / COLOR_SELECTED,
// waitRelease(), runLoop(), and the BTN_* pin defines.
//
// Entry points called from smo.ino:
//   scopeEnter();                 -> one-time setup / static UI draw
//   runLoop(runOscilloscope);     -> called every ~25ms until BACK
//
// DESIGN NOTES / OPTIMIZATIONS
// ---------------------------------------------------------------------
// 1. NO FULL-SCREEN REDRAWS. The grid is drawn once into the frame.
//    Every subsequent frame only erases the *previous* waveform pixels
//    (per column) and draws the *new* ones, using a stored prevY[]
//    array. This is the standard flicker-free scope-trace technique:
//    erase-old-line -> draw-new-line, column by column.
// 2. Status bars are redrawn only when the underlying value text
//    actually changes (cached previous values), so static labels never
//    flicker and we avoid needless text rendering.
// 3. Acquisition stores raw 12-bit ADC codes (uint16_t) — no floating
//    point inside the timing-critical sample loop. Voltage conversion,
//    RMS, frequency etc. are computed once per *frame* (200 samples),
//    not once per *sample*.
// 4. A single linear capture buffer (bigger than the displayed window)
//    is used so we can search for a trigger point and still have a
//    full screen's worth of post-trigger samples — without a true
//    circular buffer's index-wrap overhead.
// =====================================================================

// ---------------------------------------------------------------------
// LAYOUT CONSTANTS
// ---------------------------------------------------------------------
#define SCR_W            240
#define SCR_H            240

#define TOPBAR_Y0        0
#define TOPBAR_H         14

#define GRID_X0          20
#define GRID_X1          220
#define GRID_W           (GRID_X1 - GRID_X0)      // 200 px -> 1 px per sample column
#define GRID_Y0          15
#define H_DIVS           10
#define V_DIVS           8
#define DIV_PX_X         (GRID_W / H_DIVS)        // 20 px / div
#define DIV_PX_Y         22                       // px / div (8 * 22 = 176)
#define GRID_Y1          (GRID_Y0 + V_DIVS * DIV_PX_Y) // 191
#define GRID_CENTER_Y    (GRID_Y0 + (V_DIVS / 2) * DIV_PX_Y)

#define BOTBAR_Y0        (GRID_Y1 + 1)            // 192
#define BOTBAR_H         (SCR_H - BOTBAR_Y0)       // 48

#define SAMPLE_COUNT     GRID_W                    // 200 displayed samples, 1px/sample
#define TRIG_SEARCH      100                        // extra pre-capture for trigger search
#define CAP_SIZE         (SAMPLE_COUNT + TRIG_SEARCH)

// ---------------------------------------------------------------------
// COLORS (RGB565) — dark, "commercial DSO" palette
// ---------------------------------------------------------------------
#define SCOPE_BG         0x0000   // pure black scope background
#define GRID_LINE        0x2104   // dim grey-blue
#define GRID_LINE_MAJOR  0x39C7   // slightly brighter every 5th div
#define CENTER_LINE      0x4A69   // soft blue-grey center axis
#define WAVE_COLOR       0x07E0   // classic phosphor green
#define WAVE_COLOR_DIM   0x0320   // dimmer green for erase-fallback compare
#define TRIG_MARKER      0xFD20   // orange (matches COLOR_ACCENT)
#define TEXT_DIM         0x8410   // grey label text
#define TEXT_BRIGHT      0xFFFF

// ---------------------------------------------------------------------
// ENUMS / SETTINGS
// ---------------------------------------------------------------------
enum TriggerMode { TRIG_AUTO = 0, TRIG_NORMAL, TRIG_RISING, TRIG_FALLING };
enum VoltDivMode { VDIV_AUTO = 0, VDIV_100MV, VDIV_200MV, VDIV_500MV, VDIV_1V };

const char* TRIG_NAMES[] = { "AUTO", "NORM", "RISE", "FALL" };
const char* VDIV_NAMES[] = { "AUTO", "100mV", "200mV", "500mV", "1V" };

// Time/Div table, expressed in microseconds per division
const uint32_t TIMEDIV_US[] = { 20, 50, 100, 200, 500, 1000, 2000, 5000, 10000 };
const char* TIMEDIV_NAMES[] = { "20us", "50us", "100us", "200us", "500us",
                                 "1ms", "2ms", "5ms", "10ms" };
const uint8_t TIMEDIV_COUNT = sizeof(TIMEDIV_US) / sizeof(TIMEDIV_US[0]);

const float VDIV_VOLTS[] = { 0.0f /*auto - computed*/, 0.1f, 0.2f, 0.5f, 1.0f };

// ADC reference (Pico/RP2350 ADC: 12-bit, 3.3V ref)
#define ADC_MAX_CODE     4095.0f
#define ADC_VREF         3.3f
#define SCOPE_ADC_INPUT  0      // GPIO26 == ADC channel 0

// ---------------------------------------------------------------------
// STATE
// ---------------------------------------------------------------------
struct ScopeState {
  uint8_t  timeDivIndex   = 5;          // default 1ms/div
  uint8_t  voltDivIndex   = VDIV_AUTO;
  uint8_t  triggerMode    = TRIG_AUTO;
  bool     averagingOn    = false;
  bool     peakDetectOn   = false;

  float    voltsPerDiv    = 1.0f;       // active (resolved) volts/div, incl. auto
  uint32_t sampleIntervalUs = 1000;

  bool     inSettingsMenu = false;
  int8_t   settingsIndex  = 0;

  bool     triggered      = false;
  uint16_t triggerLevelRaw = 2048;
};

ScopeState scope;

// Linear capture buffer (raw ADC codes)
uint16_t capBuf[CAP_SIZE];

// Currently displayed sample window (post-trigger-aligned)
uint16_t dispBuf[SAMPLE_COUNT];

// Previous frame's plotted Y per column, for erase/redraw. -1 = none yet.
int16_t prevY[SAMPLE_COUNT];
bool firstFrame = true;

// Cached measurement values, used to avoid redundant text redraws
struct Measurements {
  float vmin = 0, vmax = 0, vpp = 0, vavg = 0, vrms = 0;
  float freqHz = 0, periodS = 0, dutyPct = 0;
  float lastVoltageSample = 0;
};
Measurements meas;

// Cache of previous status-bar strings, to skip redundant prints
char prevTopBar[40]  = "";
char prevBotBar1[40] = "";
char prevBotBar2[40] = "";

// Button edge-detect state local to the scope screen
bool sLastUp = HIGH, sLastDown = HIGH, sLastLeft = HIGH, sLastRight = HIGH;
bool sLastSelect = HIGH, sLastBack = HIGH;

// ---------------------------------------------------------------------
// FORWARD DECLARATIONS
// ---------------------------------------------------------------------
void drawGridStatic();
void drawStatusBars(bool force);
void handleScopeButtons();
void acquireWaveform();
uint16_t readRawSample();
bool findTrigger(uint16_t &startIndex);
void resolveVoltsPerDiv();
void computeMeasurements();
void drawWaveform();
int16_t sampleToY(uint16_t raw);
void drawSettingsMenu();
void handleSettingsButtons();
void fmtFreq(float hz, char* out, size_t n);
void fmtVolt(float v, char* out, size_t n);

// =====================================================================
// ENTRY POINT — called once from openApp() before runLoop()
// =====================================================================
void scopeEnter()
{
  // Configure the RP2350 ADC for raw, fast reads on GPIO26 (ADC0)
  adc_gpio_init(26);
  adc_select_input(SCOPE_ADC_INPUT);

  scope.inSettingsMenu = false;
  firstFrame = true;

  for (uint16_t i = 0; i < SAMPLE_COUNT; i++) prevY[i] = -1;

  prevTopBar[0] = prevBotBar1[0] = prevBotBar2[0] = '\0';

  tft.fillScreen(SCOPE_BG);
  drawGridStatic();
  drawStatusBars(true);
}

// =====================================================================
// MAIN LOOP FUNCTION — called repeatedly by runLoop(runOscilloscope)
// =====================================================================
void runOscilloscope()
{
  if (scope.inSettingsMenu) {
    handleSettingsButtons();
    return;
  }

  handleScopeButtons();

  resolveVoltsPerDiv();
  acquireWaveform();
  computeMeasurements();
  drawWaveform();
  drawStatusBars(false);
}

// =====================================================================
// GRID  (drawn once — never touched again except for waveform overlay)
// =====================================================================
void drawGridStatic()
{
  // Plot area background
  tft.fillRect(GRID_X0, GRID_Y0, GRID_W, GRID_Y1 - GRID_Y0, SCOPE_BG);

  // Vertical (time) grid lines
  for (uint8_t i = 0; i <= H_DIVS; i++) {
    int x = GRID_X0 + i * DIV_PX_X;
    uint16_t c = (i == H_DIVS / 2) ? GRID_LINE_MAJOR : GRID_LINE;
    tft.drawFastVLine(x, GRID_Y0, GRID_Y1 - GRID_Y0, c);
  }

  // Horizontal (voltage) grid lines
  for (uint8_t i = 0; i <= V_DIVS; i++) {
    int y = GRID_Y0 + i * DIV_PX_Y;
    uint16_t c = (i == V_DIVS / 2) ? CENTER_LINE : GRID_LINE;
    tft.drawFastHLine(GRID_X0, y, GRID_W, c);
  }

  // Tick marks on the center lines (small "+" graticule ticks, classic DSO look)
  int midX = GRID_X0 + (H_DIVS / 2) * DIV_PX_X;
  for (uint8_t i = 0; i <= V_DIVS; i++) {
    int y = GRID_Y0 + i * DIV_PX_Y;
    tft.drawFastHLine(midX - 3, y, 6, GRID_LINE_MAJOR);
  }
  int midY = GRID_CENTER_Y;
  for (uint8_t i = 0; i <= H_DIVS; i++) {
    int x = GRID_X0 + i * DIV_PX_X;
    tft.drawFastVLine(x, midY - 3, 6, GRID_LINE_MAJOR);
  }

  // Border frame
  tft.drawRect(GRID_X0 - 1, GRID_Y0 - 1, GRID_W + 2, (GRID_Y1 - GRID_Y0) + 2, TEXT_DIM);

  // Static bar separators
  tft.drawFastHLine(0, TOPBAR_H, SCR_W, GRID_LINE_MAJOR);
  tft.drawFastHLine(0, BOTBAR_Y0 - 1, SCR_W, GRID_LINE_MAJOR);
}

// =====================================================================
// BUTTON HANDLING (scope screen)
// =====================================================================
void handleScopeButtons()
{
  bool up     = digitalRead(BTN_UP);
  bool down   = digitalRead(BTN_DOWN);
  bool left   = digitalRead(BTN_L);
  bool right  = digitalRead(BTN_R);
  bool select = digitalRead(BTN_SELECT);

  // UP / DOWN -> Volt/Div
  if (sLastUp == HIGH && up == LOW) {
    if (scope.voltDivIndex < VDIV_1V) scope.voltDivIndex++;
  }
  if (sLastDown == HIGH && down == LOW) {
    if (scope.voltDivIndex > VDIV_AUTO) scope.voltDivIndex--;
  }

  // LEFT / RIGHT -> Time/Div
  if (sLastLeft == HIGH && left == LOW) {
    if (scope.timeDivIndex > 0) scope.timeDivIndex--;
  }
  if (sLastRight == HIGH && right == LOW) {
    if (scope.timeDivIndex < TIMEDIV_COUNT - 1) scope.timeDivIndex++;
  }

  // SELECT -> open settings menu
  if (sLastSelect == HIGH && select == LOW) {
    waitRelease(BTN_SELECT);
    scope.inSettingsMenu = true;
    scope.settingsIndex = 0;
    drawSettingsMenu();
  }

  sLastUp = up;
  sLastDown = down;
  sLastLeft = left;
  sLastRight = right;
  sLastSelect = select;
}

// =====================================================================
// SETTINGS MENU (Trigger mode / Averaging / Peak detect)
// =====================================================================
const char* settingsLabels[] = { "Trigger Mode", "Averaging", "Peak Detect", "Back to Scope" };
const uint8_t settingsCount = 4;

void drawSettingsMenu()
{
  tft.fillScreen(COLOR_BG);
  drawHeader("Scope Settings");

  for (uint8_t i = 0; i < settingsCount; i++) {
    int y = 50 + i * 40;
    bool sel = (i == scope.settingsIndex);

    if (sel) {
      tft.fillRoundRect(15, y, 210, 32, 6, COLOR_ACCENT);
      tft.setTextColor(COLOR_SELECTED);
    } else {
      tft.drawRoundRect(15, y, 210, 32, 6, COLOR_ACCENT);
      tft.setTextColor(COLOR_WHITE);
    }

    tft.setTextSize(1);
    tft.setCursor(25, y + 11);
    tft.print(settingsLabels[i]);

    // Current value, right-aligned-ish
    char valStr[16] = "";
    switch (i) {
      case 0: strncpy(valStr, TRIG_NAMES[scope.triggerMode], sizeof(valStr)); break;
      case 1: strncpy(valStr, scope.averagingOn ? "ON" : "OFF", sizeof(valStr)); break;
      case 2: strncpy(valStr, scope.peakDetectOn ? "ON" : "OFF", sizeof(valStr)); break;
      default: valStr[0] = '\0'; break;
    }
    tft.setCursor(160, y + 11);
    tft.print(valStr);
  }
}

void handleSettingsButtons()
{
  bool up     = digitalRead(BTN_UP);
  bool down   = digitalRead(BTN_DOWN);
  bool select = digitalRead(BTN_SELECT);
  bool back   = digitalRead(BTN_BACK);

  bool changed = false;

  if (sLastUp == HIGH && up == LOW) {
    scope.settingsIndex--;
    if (scope.settingsIndex < 0) scope.settingsIndex = settingsCount - 1;
    changed = true;
  }
  if (sLastDown == HIGH && down == LOW) {
    scope.settingsIndex++;
    if (scope.settingsIndex >= settingsCount) scope.settingsIndex = 0;
    changed = true;
  }

  if (sLastSelect == HIGH && select == LOW) {
    switch (scope.settingsIndex) {
      case 0:
        scope.triggerMode = (scope.triggerMode + 1) % 4;
        break;
      case 1:
        scope.averagingOn = !scope.averagingOn;
        break;
      case 2:
        scope.peakDetectOn = !scope.peakDetectOn;
        break;
      case 3:
        scope.inSettingsMenu = false;
        break;
    }
    changed = true;
  }

  // BACK inside settings also returns to the scope (does not exit the app)
  if (sLastBack == HIGH && back == LOW) {
    waitRelease(BTN_BACK);
    scope.inSettingsMenu = false;
    changed = true;
  }

  if (changed && scope.inSettingsMenu) {
    drawSettingsMenu();
  } else if (changed && !scope.inSettingsMenu) {
    // Returning to the live scope view: redraw the grid + force status bars
    tft.fillScreen(SCOPE_BG);
    drawGridStatic();
    for (uint16_t i = 0; i < SAMPLE_COUNT; i++) prevY[i] = -1;
    drawStatusBars(true);
  }

  sLastUp = up;
  sLastDown = down;
  sLastSelect = select;
  sLastBack = back;
}

// =====================================================================
// ACQUISITION
// =====================================================================

// Single fast ADC read. Kept free of floating point and string work so
// the sample-timing loop stays as tight/predictable as possible.
inline uint16_t readRawSample()
{
  return adc_read();   // 12-bit code, 0..4095
}

// Captures CAP_SIZE raw samples at the interval implied by the current
// Time/Div setting, locates a trigger point, then copies SAMPLE_COUNT
// samples (one per screen column) into dispBuf starting at that point.
void acquireWaveform()
{
  // Resolve sample interval for the active timebase.
  // 10 horizontal divisions span the whole grid width (SAMPLE_COUNT px),
  // so: interval_us = (us_per_div * 10) / SAMPLE_COUNT
  uint32_t totalWindowUs = TIMEDIV_US[scope.timeDivIndex] * H_DIVS;
  scope.sampleIntervalUs = totalWindowUs / SAMPLE_COUNT;
  if (scope.sampleIntervalUs < 2) scope.sampleIntervalUs = 2; // ADC hw floor

  // --- Capture loop: minimal work per iteration ---
  uint32_t nextDue = micros();
  for (uint16_t i = 0; i < CAP_SIZE; i++) {
    while ((int32_t)(micros() - nextDue) < 0) {
      // busy-wait for precise sample spacing (no delay() overhead/jitter)
    }
    nextDue += scope.sampleIntervalUs;

    uint16_t raw = readRawSample();

    if (scope.averagingOn) {
      // Cheap 2x oversample-and-average: one extra read, integer math only.
      uint16_t raw2 = readRawSample();
      raw = (raw + raw2) >> 1;
    }

    capBuf[i] = raw;
  }

  // --- Trigger search ---
  uint16_t startIndex = 0;
  bool got = findTrigger(startIndex);
  scope.triggered = got;

  if (got) {
    memcpy(dispBuf, &capBuf[startIndex], SAMPLE_COUNT * sizeof(uint16_t));
  } else if (scope.triggerMode == TRIG_AUTO) {
    // Auto mode never stalls: free-run from the start of the buffer.
    memcpy(dispBuf, &capBuf[0], SAMPLE_COUNT * sizeof(uint16_t));
  }
  // NORMAL mode with no trigger found: keep previous dispBuf (frame held).

  // --- Optional peak-detect: widen visual extremes using min/max of
  // small neighbor windows instead of plain samples, to catch glitches
  // that would otherwise be missed at slow timebases. ---
  if (scope.peakDetectOn && (got || scope.triggerMode == TRIG_AUTO)) {
    uint16_t tmp[SAMPLE_COUNT];
    memcpy(tmp, dispBuf, sizeof(tmp));
    for (uint16_t i = 1; i < SAMPLE_COUNT - 1; i++) {
      uint16_t lo = min(tmp[i - 1], min(tmp[i], tmp[i + 1]));
      uint16_t hi = max(tmp[i - 1], max(tmp[i], tmp[i + 1]));
      // Alternate min/max per column to visualize envelope without
      // doubling sample count or buffer size.
      dispBuf[i] = (i & 1) ? hi : lo;
    }
  }
}

// Locates a trigger crossing inside the search window of capBuf.
// Auto-levels the trigger threshold to the midpoint of that window's
// min/max for stable triggering across varying signal amplitudes.
bool findTrigger(uint16_t &startIndex)
{
  uint16_t winMin = 65535, winMax = 0;
  for (uint16_t i = 0; i < TRIG_SEARCH; i++) {
    uint16_t v = capBuf[i];
    if (v < winMin) winMin = v;
    if (v > winMax) winMax = v;
  }
  scope.triggerLevelRaw = (winMin + winMax) / 2;

  if (scope.triggerMode == TRIG_AUTO && (winMax - winMin) < 20) {
    // Flat / noisy signal: nothing meaningful to trigger on.
    return false;
  }

  for (uint16_t i = 1; i < TRIG_SEARCH; i++) {
    bool prevBelow = capBuf[i - 1] < scope.triggerLevelRaw;
    bool nowBelow  = capBuf[i]     < scope.triggerLevelRaw;

    bool risingEdge  = prevBelow && !nowBelow;
    bool fallingEdge = !prevBelow && nowBelow;

    bool match = false;
    switch (scope.triggerMode) {
      case TRIG_RISING:  match = risingEdge;  break;
      case TRIG_FALLING: match = fallingEdge; break;
      case TRIG_AUTO:
      case TRIG_NORMAL:  match = risingEdge || fallingEdge; break;
    }

    if (match) {
      startIndex = i;
      return true;
    }
  }
  return false;
}

// =====================================================================
// VOLT/DIV RESOLUTION (handles AUTO scaling)
// =====================================================================
void resolveVoltsPerDiv()
{
  if (scope.voltDivIndex != VDIV_AUTO) {
    scope.voltsPerDiv = VDIV_VOLTS[scope.voltDivIndex];
    return;
  }

  // Auto-scale: fit last captured min/max into ~6 of the 8 divisions.
  // Falls back to a sane default before the first capture exists.
  float vpp = meas.vpp > 0.05f ? meas.vpp : 1.0f;
  float target = vpp / 6.0f;

  // Snap to the nearest "nice" step for a cleaner on-screen readout.
  static const float steps[] = { 0.05f, 0.1f, 0.2f, 0.5f, 1.0f, 2.0f };
  float chosen = steps[0];
  for (float s : steps) {
    chosen = s;
    if (s >= target) break;
  }
  scope.voltsPerDiv = chosen;
}

// =====================================================================
// MEASUREMENTS
// =====================================================================
void computeMeasurements()
{
  uint32_t sum = 0;
  uint64_t sumSq = 0;
  uint16_t vmin = 65535, vmax = 0;

  for (uint16_t i = 0; i < SAMPLE_COUNT; i++) {
    uint16_t v = dispBuf[i];
    sum += v;
    sumSq += (uint32_t)v * (uint32_t)v;
    if (v < vmin) vmin = v;
    if (v > vmax) vmax = v;
  }

  const float codeToVolt = ADC_VREF / ADC_MAX_CODE;

  meas.vmin = vmin * codeToVolt;
  meas.vmax = vmax * codeToVolt;
  meas.vpp  = meas.vmax - meas.vmin;
  meas.vavg = (sum / (float)SAMPLE_COUNT) * codeToVolt;

  float meanSq = sumSq / (float)SAMPLE_COUNT;
  meas.vrms = sqrtf(meanSq) * codeToVolt;

  meas.lastVoltageSample = dispBuf[SAMPLE_COUNT - 1] * codeToVolt;

  // --- Frequency / period via mid-level crossing counting ---
  uint16_t midLevel = (vmin + vmax) / 2;
  uint16_t crossingIndices[SAMPLE_COUNT];
  uint16_t crossCount = 0;
  uint32_t highSamples = 0;

  for (uint16_t i = 1; i < SAMPLE_COUNT; i++) {
    bool prevBelow = dispBuf[i - 1] < midLevel;
    bool nowBelow  = dispBuf[i]     < midLevel;
    if (prevBelow && !nowBelow) {           // rising edge only -> one per period
      crossingIndices[crossCount++] = i;
    }
    if (dispBuf[i] >= midLevel) highSamples++;
  }

  if (crossCount >= 2) {
    uint16_t spanSamples = crossingIndices[crossCount - 1] - crossingIndices[0];
    float avgPeriodSamples = spanSamples / (float)(crossCount - 1);
    float periodUs = avgPeriodSamples * scope.sampleIntervalUs;
    meas.periodS = periodUs / 1.0e6f;
    meas.freqHz  = (periodUs > 0) ? (1.0e6f / periodUs) : 0;
  } else {
    meas.freqHz = 0;
    meas.periodS = 0;
  }

  meas.dutyPct = (SAMPLE_COUNT > 0)
                 ? (highSamples * 100.0f / SAMPLE_COUNT)
                 : 0;
}

// =====================================================================
// WAVEFORM RENDERING — erase-old / draw-new, per column
// =====================================================================

// Maps a raw ADC code to a screen Y pixel using the active volts/div,
// centered on the grid's middle line.
int16_t sampleToY(uint16_t raw)
{
  const float codeToVolt = ADC_VREF / ADC_MAX_CODE;
  float volts = raw * codeToVolt;
  float voltsFromCenter = volts - (ADC_VREF / 2.0f); // assume signal centered at Vref/2 (typical AC-coupled-via-bias input)

  float pixelsPerVolt = DIV_PX_Y / scope.voltsPerDiv;
  int16_t y = GRID_CENTER_Y - (int16_t)(voltsFromCenter * pixelsPerVolt);

  if (y < GRID_Y0) y = GRID_Y0;
  if (y > GRID_Y1 - 1) y = GRID_Y1 - 1;
  return y;
}

// Clears one full column of the plot area back to a "clean grid" state
// and redraws whatever gridlines intersect that column. This replaces
// the old per-pixel erase approach: a diagonal line segment connecting
// two samples can touch several pixels in a column (Bresenham), and
// erasing only the endpoint dot left ghost trails behind. Clearing the
// whole column is still a *partial* redraw (one 1px-wide x 176px-tall
// strip, not the full screen), so it stays cheap and flicker-free while
// guaranteeing no leftover trace pixels survive between frames.
void eraseColumn(int16_t x)
{
  // Wipe the column to the scope background color in one call.
  tft.drawFastVLine(x, GRID_Y0, GRID_Y1 - GRID_Y0, SCOPE_BG);

  int colInGrid = x - GRID_X0;

  // Re-draw the vertical gridline if this column sits on one.
  if (colInGrid % DIV_PX_X == 0) {
    uint16_t c = ((colInGrid / DIV_PX_X) == H_DIVS / 2) ? GRID_LINE_MAJOR : GRID_LINE;
    tft.drawFastVLine(x, GRID_Y0, GRID_Y1 - GRID_Y0, c);
  }

  // Re-draw every horizontal gridline's single pixel at this column.
  for (uint8_t i = 0; i <= V_DIVS; i++) {
    int16_t gy = GRID_Y0 + i * DIV_PX_Y;
    uint16_t c = (i == V_DIVS / 2) ? GRID_LINE_MAJOR : GRID_LINE;
    tft.drawPixel(x, gy, c);
  }

  // Re-draw the small "+" graticule ticks that live on the center
  // row/column (see drawGridStatic()).
  int midX = GRID_X0 + (H_DIVS / 2) * DIV_PX_X;
  if (x >= midX - 3 && x <= midX + 3) {
    for (uint8_t i = 0; i <= V_DIVS; i++) {
      tft.drawPixel(x, GRID_Y0 + i * DIV_PX_Y, GRID_LINE_MAJOR);
    }
  }
  if (colInGrid % DIV_PX_X == 0) {
    int midY = GRID_CENTER_Y;
    tft.drawFastVLine(x, midY - 3, 6, GRID_LINE_MAJOR);
  }
}

void drawWaveform()
{
  // Skip drawing entirely if NORMAL mode is waiting on a trigger
  // (dispBuf was intentionally left unchanged in acquireWaveform()).
  if (!scope.triggered && scope.triggerMode == TRIG_NORMAL) return;

  for (uint16_t i = 0; i < SAMPLE_COUNT; i++) {
    int16_t x = GRID_X0 + i;

    // Erase this column's previous content completely before drawing
    // the new sample/line into it. This is what actually fixes the
    // "old trace doesn't disappear" issue: clearing just the old dot
    // wasn't enough because connecting lines span extra pixels.
    if (!firstFrame) {
      eraseColumn(x);
    }

    int16_t newY = sampleToY(dispBuf[i]);

    // Thick dot (2px) for the sample itself.
    tft.drawPixel(x, newY, WAVE_COLOR);
    tft.drawPixel(x, newY + 1, WAVE_COLOR);

    // Connect to the previous column so the trace reads as continuous.
    if (i > 0) {
      int16_t prevNewY = sampleToY(dispBuf[i - 1]);
      tft.drawLine(x - 1, prevNewY, x, newY, WAVE_COLOR);
      tft.drawLine(x - 1, prevNewY + 1, x, newY + 1, WAVE_COLOR); // thickness
    }

    prevY[i] = newY;
  }

  firstFrame = false;

  // --- Trigger marker (small left-edge arrow at the trigger voltage) ---
  // Erase the previous marker's exact footprint first, then redraw the
  // grid border there, before placing the new marker. Without this the
  // marker also ghosts/duplicates as the trigger level moves.
  static int16_t prevTrigY = -1;
  int16_t trigY = sampleToY(scope.triggerLevelRaw);

  if (prevTrigY >= 0 && prevTrigY != trigY) {
    tft.fillRect(GRID_X0 - 2, prevTrigY - 4, 7, 9, COLOR_BG);
    tft.drawFastVLine(GRID_X0 - 1, prevTrigY - 4, 9, TEXT_DIM); // restore border sliver
  }

  tft.fillTriangle(GRID_X0 - 1, trigY - 3, GRID_X0 - 1, trigY + 3, GRID_X0 + 4, trigY, TRIG_MARKER);
  prevTrigY = trigY;
}

// =====================================================================
// STATUS BARS
// =====================================================================
void fmtFreq(float hz, char* out, size_t n)
{
  if (hz >= 1000000.0f) snprintf(out, n, "%.2fMHz", hz / 1000000.0f);
  else if (hz >= 1000.0f) snprintf(out, n, "%.2fkHz", hz / 1000.0f);
  else snprintf(out, n, "%.1fHz", hz);
}

void fmtVolt(float v, char* out, size_t n)
{
  snprintf(out, n, "%.2fV", v);
}

void drawStatusBars(bool force)
{
  char sampleRateStr[16];
  float sps = 1.0e6f / scope.sampleIntervalUs;
  if (sps >= 1.0e6f) snprintf(sampleRateStr, sizeof(sampleRateStr), "%.1fMSa/s", sps / 1.0e6f);
  else snprintf(sampleRateStr, sizeof(sampleRateStr), "%.0fkSa/s", sps / 1000.0f);

  char topLine[40];
  const char* vdivLabel = (scope.voltDivIndex == VDIV_AUTO) ? "AUTO" : VDIV_NAMES[scope.voltDivIndex];
  snprintf(topLine, sizeof(topLine), "%s/div %s/div %s %s",
           TIMEDIV_NAMES[scope.timeDivIndex], vdivLabel,
           TRIG_NAMES[scope.triggerMode], sampleRateStr);

  if (force || strcmp(topLine, prevTopBar) != 0) {
    tft.fillRect(0, TOPBAR_Y0, SCR_W, TOPBAR_H, COLOR_BG);
    tft.setTextColor(TEXT_BRIGHT);
    tft.setTextSize(1);
    tft.setCursor(2, 3);
    tft.print(topLine);
    strncpy(prevTopBar, topLine, sizeof(prevTopBar));
  }

  char freqStr[16], vppStr[16], rmsStr[16], voltStr[16];
  fmtFreq(meas.freqHz, freqStr, sizeof(freqStr));
  fmtVolt(meas.vpp, vppStr, sizeof(vppStr));
  fmtVolt(meas.vrms, rmsStr, sizeof(rmsStr));
  fmtVolt(meas.lastVoltageSample, voltStr, sizeof(voltStr));

  char botLine1[40], botLine2[40];
  snprintf(botLine1, sizeof(botLine1), "Freq:%-10s Vpp:%-8s", freqStr, vppStr);
  snprintf(botLine2, sizeof(botLine2), "RMS:%-10s V:%-8s Duty:%.0f%%", rmsStr, voltStr, meas.dutyPct);

  if (force || strcmp(botLine1, prevBotBar1) != 0 || strcmp(botLine2, prevBotBar2) != 0) {
    tft.fillRect(0, BOTBAR_Y0, SCR_W, BOTBAR_H, COLOR_BG);
    tft.setTextColor(TEXT_BRIGHT);
    tft.setTextSize(1);
    tft.setCursor(2, BOTBAR_Y0 + 4);
    tft.print(botLine1);
    tft.setCursor(2, BOTBAR_Y0 + 16);
    tft.print(botLine2);

    // Min/Max/Avg on a third compact line if room allows
    char botLine3[40];
    snprintf(botLine3, sizeof(botLine3), "Min:%.2fV Max:%.2fV Avg:%.2fV",
             meas.vmin, meas.vmax, meas.vavg);
    tft.setTextColor(TEXT_DIM);
    tft.setCursor(2, BOTBAR_Y0 + 28);
    tft.print(botLine3);

    strncpy(prevBotBar1, botLine1, sizeof(prevBotBar1));
    strncpy(prevBotBar2, botLine2, sizeof(prevBotBar2));
  }
}


