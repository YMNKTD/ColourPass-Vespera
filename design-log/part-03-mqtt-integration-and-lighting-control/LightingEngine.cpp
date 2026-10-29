
#include "LightingEngine.h"
#include <math.h>

// ============================================
// HELPER FUNCTIONS
// ============================================

namespace {

uint8_t byteValue(float x) {
  if (x < 0.0f) return 0;
  if (x > 255.0f) return 255;
  return static_cast<uint8_t>(x + 0.5f);
}

float wave01(float phase) {
  return 0.5f + 0.5f * sinf(phase);
}

}

// ============================================
// INITIALISATION
// ============================================

void LightingEngine::begin() {
  strip_.begin();
  strip_.setBrightness(Config::LOCAL_BRIGHTNESS);
  strip_.clear();
  strip_.show();
}

// ============================================
// PIXEL CONTROL
// ============================================

void LightingEngine::setPixel(
  uint8_t col,
  uint8_t row,
  uint8_t r,
  uint8_t g,
  uint8_t b) {
  if (
    col >= Config::VESPERA_COLUMNS || row >= Config::VESPERA_ROWS) return;

  int i = 3 * (col * Config::VESPERA_ROWS + row);

  frame_[i] = r;
  frame_[i + 1] = g;
  frame_[i + 2] = b;
}

void LightingEngine::setColumn(
  uint8_t col,
  uint8_t r,
  uint8_t g,
  uint8_t b) {
  for (
    uint8_t row = 0;
    row < Config::VESPERA_ROWS;
    ++row) {
    setPixel(col, row, r, g, b);
  }
}

void LightingEngine::solid(
  uint8_t r,
  uint8_t g,
  uint8_t b) {
  for (
    uint8_t col = 0;
    col < Config::VESPERA_COLUMNS;
    ++col) {
    setColumn(col, r, g, b);
  }
}

// ============================================
// STATIC MODE NAMES
// ============================================

const char* LightingEngine::staticName(
  uint8_t index) {
  static const char* names[STATIC_COUNT] = {
    "YELLOW",
    "GREEN",
    "CYAN",
    "BLUE",
    "PURPLE",
    "PINK"
  };

  return names[index % STATIC_COUNT];
}

// ============================================
// AMBIENT MODE NAMES
// ============================================

const char* LightingEngine::ambientName(
  uint8_t step) {
  static const char* names[AMBIENT_STEPS] = {
    "BLUE MOOD",
    "RAINBOW",
    "BLUE MOOD",
    "FOREST",
    "SUNSET",
    "DISCO",
    "BAYERN"
  };

  return names[step % AMBIENT_STEPS];
}

// ============================================
// AMBIENT MODE CYCLING
// ============================================

uint8_t LightingEngine::nextAmbientStep(uint8_t step) {
  return (step + 1) % AMBIENT_STEPS;
}

// ============================================
// ANIMATION DETECTION
// ============================================

bool LightingEngine::isAnimated(
  OutputKind kind,
  uint8_t effect) {

  // Red flashing for invalid tickets
  if (kind == OUT_DENIED) {
    return true;
  }

  // NEW: Green-white flow for valid tickets
  if (kind == OUT_GRANTED) {
    return true;
  }

  if (kind != OUT_AMBIENT) {
    return false;
  }

  uint8_t step = effect % AMBIENT_STEPS;

  return step == 0 || step == 2 || step == 3 || step == 4 || step == 5;
}

// ============================================
// MAIN LIGHTING RENDER FUNCTION
// ============================================

void LightingEngine::render(
  OutputKind kind,
  uint8_t effect,
  unsigned long elapsedMs) {

  float t = elapsedMs / 1000.0f;

  // ==========================================
  // WHITE STANDBY
  // ==========================================

  if (kind == OUT_STANDBY) {

    solid(
      Config::STANDBY_WHITE,
      Config::STANDBY_WHITE,
      Config::STANDBY_WHITE);

  }

  // ==========================================
  // LIGHTS OFF
  // ==========================================

  else if (kind == OUT_OFF) {

    solid(0, 0, 0);

  }

  // ==========================================
  // ACCESS GRANTED
  // NEW: DYNAMIC GREEN-WHITE FLOW
  // ==========================================

  else if (kind == OUT_GRANTED) {

    for (
      uint8_t col = 0;
      col < Config::VESPERA_COLUMNS;
      ++col) {

      for (
        uint8_t row = 0;
        row < Config::VESPERA_ROWS;
        ++row) {

        // Green-white gradient flows
        // across the Vespera matrix.

        float phase =
          col * 0.60f + row * 0.18f - t * 4.0f;

        float blend = wave01(phase);

        // Green: (0, 220, 25)
        // White: (220, 220, 220)

        uint8_t r = byteValue(
          220.0f * blend);

        uint8_t g = 220;

        uint8_t b = byteValue(
          25.0f + 195.0f * blend);

        setPixel(
          col,
          row,
          r,
          g,
          b);
      }
    }

  }

  // ==========================================
  // ACCESS DENIED - RED FLASHING
  // ==========================================

  else if (kind == OUT_DENIED) {

    bool on = ((elapsedMs / 270UL) % 2UL == 0UL);

    solid(
      on ? 255 : 0,
      0,
      0);

  }

  // ==========================================
  // STATIC MODE - SIX COLOURS
  // ==========================================

  else if (kind == OUT_STATIC) {

    // YELLOW -> GREEN -> CYAN ->
    // BLUE -> PURPLE -> PINK -> YELLOW

    static const uint8_t colours[STATIC_COUNT][3] = {
      { 235, 205, 0 },
      { 0, 220, 30 },
      { 0, 205, 220 },
      { 0, 45, 240 },
      { 145, 15, 205 },
      { 245, 65, 150 }
    };

    uint8_t i = effect % STATIC_COUNT;

    solid(
      colours[i][0],
      colours[i][1],
      colours[i][2]);

  }

  // ==========================================
  // AMBIENT MODE - SIX THEMES
  // ==========================================

  else if (kind == OUT_AMBIENT) {

    uint8_t step = effect % AMBIENT_STEPS;

    switch (step) {

        // ========================================
        // BLUE MOOD - DYNAMIC
        // ========================================

      case 0:
      case 2:

        for (
          uint8_t col = 0;
          col < Config::VESPERA_COLUMNS;
          ++col) {

          for (
            uint8_t row = 0;
            row < Config::VESPERA_ROWS;
            ++row) {

            float u =
              (col + 0.40f * row) / float(Config::VESPERA_COLUMNS - 1);

            float phase = u * 5.3f - t * 1.05f;

            float blend = wave01(phase);

            setPixel(
              col,
              row,
              byteValue(15 + 125 * blend),
              byteValue(75 - 55 * blend),
              byteValue(210 - 15 * blend));
          }
        }

        break;

        // ========================================
        // RAINBOW - STATIC
        // ========================================

      case 1:
        {

          static const uint8_t rainbow[6][3] = {
            { 235, 25, 35 },
            { 250, 115, 10 },
            { 240, 220, 0 },
            { 25, 195, 50 },
            { 25, 80, 230 },
            { 155, 20, 200 }
          };

          for (
            uint8_t col = 0;
            col < Config::VESPERA_COLUMNS;
            ++col) {

            uint8_t p =
              (col * 6) / Config::VESPERA_COLUMNS;

            setColumn(
              col,
              rainbow[p][0],
              rainbow[p][1],
              rainbow[p][2]);
          }

          break;
        }

        // ========================================
        // FOREST - DYNAMIC
        // ========================================

      case 3:

        for (
          uint8_t col = 0;
          col < Config::VESPERA_COLUMNS;
          ++col) {

          for (
            uint8_t row = 0;
            row < Config::VESPERA_ROWS;
            ++row) {

            float w = wave01(
              col * 0.55f + row * 0.23f - t * 1.15f);

            setPixel(
              col,
              row,
              byteValue(5 + 25 * w),
              byteValue(45 + 145 * w),
              byteValue(20 + 65 * w));
          }
        }

        break;

        // ========================================
        // SUNSET - DYNAMIC
        // ========================================

      case 4:

        for (
          uint8_t col = 0;
          col < Config::VESPERA_COLUMNS;
          ++col) {

          for (
            uint8_t row = 0;
            row < Config::VESPERA_ROWS;
            ++row) {

            float w = wave01(
              0.42f * col + 0.25f * row - t * 0.8f);

            setPixel(
              col,
              row,
              byteValue(135 + 110 * w),
              byteValue(20 + 115 * w),
              byteValue(115 - 100 * w));
          }
        }

        break;

        // ========================================
        // DISCO - DYNAMIC
        // ========================================

      case 5:

        for (
          uint8_t col = 0;
          col < Config::VESPERA_COLUMNS;
          ++col) {

          for (
            uint8_t row = 0;
            row < Config::VESPERA_ROWS;
            ++row) {

            float a =
              col * 1.15f + row * 0.46f + t * 3.6f;

            setPixel(
              col,
              row,
              byteValue(
                105 + 150 * wave01(a)),
              byteValue(
                35 + 220 * wave01(a + 2.0944f)),
              byteValue(
                65 + 190 * wave01(a + 4.1888f)));
          }
        }

        break;

        // ========================================
        // BAYERN - STATIC RED / WHITE
        // ========================================

      case 6:

        for (
          uint8_t col = 0;
          col < Config::VESPERA_COLUMNS;
          ++col) {

          if (col % 2 == 0) {

            setColumn(
              col,
              220, 0, 0);

          } else {

            setColumn(
              col,
              200, 200, 200);
          }
        }

        break;
    }
  }

  // Update local NeoPixel preview
  copyToLocal();
}

// ============================================
// LOCAL NEOPIXEL OUTPUT
// ============================================

void LightingEngine::copyToLocal() {

  for (
    uint8_t p = 0;
    p < Config::LOCAL_PIXELS;
    ++p) {

    uint8_t col = uint8_t(
      p * Config::VESPERA_COLUMNS / Config::LOCAL_PIXELS);

    int i = 3 * (col * Config::VESPERA_ROWS);

    strip_.setPixelColor(
      p,
      frame_[i],
      frame_[i + 1],
      frame_[i + 2]);
  }

  strip_.show();
}
