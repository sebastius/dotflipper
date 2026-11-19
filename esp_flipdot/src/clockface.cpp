#include <Arduino.h>

#include "config.h"
#include "display.h"
#include "fonts.h"
#include "clockface.h"

void clocksetup() {
  configTzTime(CLOCK_TIMEZONE, CLOCK_NTP_SERVER1, CLOCK_NTP_SERVER2);
}

// The faces are drawn on a 32x32 face (two panels of 32x16). On a larger screen every dot of the
// face becomes a block of SCALE x SCALE dots, centered: on 64x64 the face fills the screen and lines
// that stayed clear of the seam between two panels still do. The analog clock uses every dot.

#define FACE  32
#define SCALE (min(COLS, ROWS) / FACE)
#define FACE_LEFT ((COLS - FACE * SCALE) / 2)
#define FACE_TOP  ((ROWS - FACE * SCALE) / 2)

static void plot(int16_t x, int16_t y, bool on) {
  for (uint8_t dx = 0; dx < SCALE; dx++) {
    for (uint8_t dy = 0; dy < SCALE; dy++) {
      next_state[FACE_LEFT + x * SCALE + dx][FACE_TOP + y * SCALE + dy] = on;
    }
  }
}

// Big: hours on the top half, minutes on the bottom half

static void drawbigglyph(uint8_t glyph, uint8_t x0, uint8_t y0) {
  for (uint8_t y = 0; y < BIGFONT_HEIGHT; y++) {
    for (uint8_t x = 0; x < BIGFONT_WIDTH; x++) {
      plot(x0 + x, y0 + y, (bigFont[glyph][y] >> (BIGFONT_WIDTH - 1 - x)) & 1);
    }
  }
}

// Two glyphs centered on a 32x16 half of the face starting at row y0
static void drawbigpair(uint8_t left, uint8_t right, uint8_t y0) {
  drawbigglyph(left,   2, y0 + 1);
  drawbigglyph(right, 18, y0 + 1);
}

// Date: HH:MM on the top half, DD-MM on the bottom half

static void drawmedline(const uint8_t *glyphs, uint8_t count, uint8_t y0) {
  uint8_t width = count - 1;  // One dot between glyphs
  for (uint8_t i = 0; i < count; i++) {
    width += medFont[glyphs[i]].width;
  }
  uint8_t x0 = (FACE - width) / 2;
  for (uint8_t i = 0; i < count; i++) {
    const MedGlyph &g = medFont[glyphs[i]];
    for (uint8_t y = 0; y < MEDFONT_HEIGHT; y++) {
      for (uint8_t x = 0; x < g.width; x++) {
        plot(x0 + x, y0 + y, (g.rows[y] >> (g.width - 1 - x)) & 1);
      }
    }
    x0 += g.width + 1;
  }
}

static void drawmedpair(uint8_t a, uint8_t b, uint8_t separator, uint8_t y0) {
  const uint8_t glyphs[5] = { (uint8_t)(a / 10), (uint8_t)(a % 10), separator, (uint8_t)(b / 10), (uint8_t)(b % 10) };
  drawmedline(glyphs, 5, y0 + (FACE / 2 - MEDFONT_HEIGHT) / 2);
}

// Analog: the center lies between four dots, so positions are rounded
// symmetrically around it to keep the face symmetric.

static int16_t symmetric(float v, int16_t size) {
  return v >= size / 2 ? (int16_t)v : size - 1 - (int16_t)(size - v);
}

static void dot(float x, float y) {
  setdot(symmetric(x, COLS), symmetric(y, ROWS), true);
}

// Hand from the center, angle in degrees clockwise from 12 o'clock, length on the 32x32 face.
// A thick hand gets SCALE extra lines on either side.
static void hand(float angle, float length, bool thick) {
  float s = sinf(angle * DEG_TO_RAD), c = -cosf(angle * DEG_TO_RAD);
  for (float r = 0; r <= length * SCALE; r += 0.25f) {
    dot(COLS / 2 + s * r, ROWS / 2 + c * r);
    for (uint8_t k = 1; thick && k <= SCALE; k++) {
      dot(COLS / 2 + s * r - c * 0.6f * k, ROWS / 2 + c * r + s * 0.6f * k);
      dot(COLS / 2 + s * r + c * 0.6f * k, ROWS / 2 + c * r - s * 0.6f * k);
    }
  }
}

static void drawanalog(const struct tm &now, bool seconds) {
  const float radius = min(COLS, ROWS) / 2 - 0.5f;
  for (uint8_t h = 0; h < 12; h++) {
    float s = sinf(h * 30 * DEG_TO_RAD), c = -cosf(h * 30 * DEG_TO_RAD);
    // Hours are a square of SCALE dots at the edge, 12, 3, 6 and 9 twice as large
    uint8_t size = (h % 3 == 0 ? 2 : 1) * SCALE;
    for (uint8_t i = 0; i < size; i++) {
      for (uint8_t j = 0; j < size; j++) {
        float r = radius - i, t = j - (size - 1) / 2.0f;
        dot(COLS / 2 + s * r - c * t, ROWS / 2 + c * r + s * t);
      }
    }
  }

  hand((now.tm_hour % 12 + now.tm_min / 60.0f) * 30, 8, true);
  hand((now.tm_min + now.tm_sec / 60.0f) * 6, 12.5f, false);
  if (seconds) {
    hand(now.tm_sec * 6, 13.5f, false);
  }
}

// Words: "kwart over vier", one word per line, rounded to the nearest 5 minutes: 21:44 is kwart voor tien.

static const char *const hourwords[12] = {
  "TWAALF", "EEN", "TWEE", "DRIE", "VIER", "VIJF", "ZES", "ZEVEN", "ACHT", "NEGEN", "TIEN", "ELF",
};

// Words before the hour for every 5 minutes. From 20 past on it's about the next hour.
static const char *const minutewords[12][3] = {
  { },
  { "VIJF", "OVER" },
  { "TIEN", "OVER" },
  { "KWART", "OVER" },
  { "TIEN", "VOOR", "HALF" },
  { "VIJF", "VOOR", "HALF" },
  { "HALF" },
  { "VIJF", "OVER", "HALF" },
  { "TIEN", "OVER", "HALF" },
  { "KWART", "VOOR" },
  { "TIEN", "VOOR" },
  { "VIJF", "VOOR" },
};

static const WordGlyph *wordglyph(char letter) {
  for (const WordGlyph &g : wordFont) {
    if (g.letter == letter) {
      return &g;
    }
  }
  return nullptr;
}

static void drawword(const char *word, uint8_t y0) {
  int8_t width = -1;
  for (const char *c = word; *c; c++) {
    width += wordglyph(*c)->width + 1;
  }
  uint8_t x0 = (FACE - width) / 2;
  for (const char *c = word; *c; c++) {
    const WordGlyph *g = wordglyph(*c);
    for (uint8_t y = 0; y < WORDFONT_HEIGHT; y++) {
      for (uint8_t x = 0; x < g->width; x++) {
        plot(x0 + x, y0 + y, (g->rows[y] >> (g->width - 1 - x)) & 1);
      }
    }
    x0 += g->width + 1;
  }
}

static void drawwords(const struct tm &now) {
  uint8_t minutes = now.tm_min + 2;  // 2 or less rounds down, 3 or more up: 58 is already the next hour
  uint8_t hour = now.tm_hour + minutes / 60;
  uint8_t five = (minutes % 60) / 5;

  const char *words[4];
  uint8_t count = 0;
  for (const char *word : minutewords[five]) {
    if (word) {
      words[count++] = word;
    }
  }
  words[count++] = hourwords[(hour + (five >= 4 ? 1 : 0)) % 12];
  if (five == 0) {
    words[count++] = "UUR";
  }

  // Lines are 8 rows apart, centered on the face. With 2 or 4 lines that keeps every line clear of
  // the seams between the panels; 3 lines sit 4 rows lower, so the middle one crosses a seam.
  uint8_t top = (FACE - (count * 8 - 1)) / 2;
  for (uint8_t i = 0; i < count; i++) {
    drawword(words[i], top + i * 8);
  }
}

// Binary: hours, minutes and seconds as BCD columns, bottom block is 1.
// A filled block is a one, an open block a zero. Blocks are 8 rows apart, clear of the seam.

static void drawbinary(const struct tm &now) {
  const uint8_t values[6] = {
    (uint8_t)(now.tm_hour / 10), (uint8_t)(now.tm_hour % 10),
    (uint8_t)(now.tm_min / 10),  (uint8_t)(now.tm_min % 10),
    (uint8_t)(now.tm_sec / 10),  (uint8_t)(now.tm_sec % 10),
  };
  const uint8_t bits[6] = { 2, 4, 3, 4, 3, 4 };
  const uint8_t columns[6] = { 0, 5, 11, 16, 22, 27 };
  for (uint8_t c = 0; c < 6; c++) {
    for (uint8_t bit = 0; bit < bits[c]; bit++) {
      bool on = (values[c] >> bit) & 1;
      uint8_t y0 = FACE - 7 - bit * 8;
      for (uint8_t x = 0; x < 4; x++) {
        for (uint8_t y = 0; y < 6; y++) {
          plot(columns[c] + x, y0 + y, on || x == 0 || x == 3 || y == 0 || y == 5);
        }
      }
    }
  }
}

void clockdraw(const struct tm &now, ClockStyle style, bool seconds) {
  memset(next_state, 0, sizeof(next_state));
  switch (style) {
    case CLOCK_BINARY:
      drawbinary(now);
      break;
    case CLOCK_WORDS:
      drawwords(now);
      break;
    case CLOCK_SECONDS:
      drawmedpair(now.tm_hour, now.tm_min, MEDFONT_COLON, 0);
      drawbigpair(now.tm_sec / 10, now.tm_sec % 10, FACE / 2);
      break;
    case CLOCK_BIG:
      drawbigpair(now.tm_hour / 10, now.tm_hour % 10, 0);
      drawbigpair(now.tm_min / 10, now.tm_min % 10, FACE / 2);
      break;
    case CLOCK_DATE:
      drawmedpair(now.tm_hour, now.tm_min, MEDFONT_COLON, 0);
      drawmedpair(now.tm_mday, now.tm_mon + 1, MEDFONT_DASH, FACE / 2);
      break;
    case CLOCK_ANALOG:
      drawanalog(now, seconds);
      break;
  }
}

void clockdrawunsynced() {
  memset(next_state, 0, sizeof(next_state));
  drawbigpair(BIGFONT_DASH, BIGFONT_DASH, 0);
  drawbigpair(BIGFONT_DASH, BIGFONT_DASH, FACE / 2);
}
