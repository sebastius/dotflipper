// The v3 hardware for the emulator: a chain of CHAIN_BOARDS boards with flip dot panels, the virtual
// clock of the ESP32-S3, and the bridge to the page (web requests, settings, flipped dots).
//
// The model of the boards is the one in simulate/simulate595.cpp: 48 bits of 595s per board shifting
// on the rising edge of SCLK, a flip-flop to the next board on the falling edge, LATCH copies to the
// outputs, an output is on when its latched bit is 1 and nOE is low, DIR picks 20 V or ground, JP1
// closed on the last board. A dot flips when its line and its row are on, on opposite sides.
// nOE comes from a model of the RMT once the firmware has handed it over.
// The model also checks the rules of the simulator and counts what goes wrong.

#include <map>
#include <string>

#include "Arduino.h"
#include "ArduinoOTA.h"
#include "ESPmDNS.h"
#include "Preferences.h"
#include "WebServer.h"
#include "display.h"
#include "shiftchain.h"
#include "simulatechain.h"

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#define EXPORT extern "C" EMSCRIPTEN_KEEPALIVE
#else
#define EXPORT extern "C"
#endif

void setup();
void loop();

EmuSerial Serial;
EmuMDNS MDNS;
EmuOTA ArduinoOTA;

// Virtual time

static uint64_t cycles;
uint64_t emucycles() { return cycles; }
void emuadvance(uint64_t c) { cycles += c; }
static void rmtupdate();
uint32_t chaincycles() { cycles += 2; rmtupdate(); return (uint32_t)cycles; }

void configTzTime(const char *tz, const char *, const char *, const char *) {
  setenv("TZ", tz, 1);
  tzset();
}

bool getLocalTime(struct tm *info, uint32_t) {
  time_t now = time(nullptr);
  localtime_r(&now, info);
  return true;
}

// The boards

#define BOARDS CHAIN_BOARDS
static bool sr[BOARDS][48], ff[BOARDS], latched[BOARDS][48];
static bool pdata, psclk, platch, pnoe = true, pdirx1, pdirrow;
static bool dots[BOARDS][32][16];
static bool coilon[BOARDS];
static uint64_t coilstart[BOARDS];

struct Flip { double us; int packed; };  // packed: x | y << 8 | color << 16
static Flip flips[16384];  // Startup alone flips every dot twice
static int flipcount;
static long dropped;

// What went wrong: rule 1 LATCH or DIR with nOE low, rule 2 more than one coil on a board,
// rule 5 a pulse that isn't PULSE_MICROS long
static long rule1, rule2, rule5, pulses;
static double pulseshortest = 1e9, pulselongest;

static void updatemodel() {
  for (int b = 0; b < BOARDS; b++) {
    int coils = 0, lastx = 0, lasty = 0;
    for (int x = 0; x < 32 && !pnoe; x++) {
      if (!latched[b][x]) continue;
      for (int y = 0; y < 16; y++) {
        if (!latched[b][32 + y] || pdirx1 == pdirrow) continue;
        coils++;
        lastx = x;
        lasty = y;
      }
    }
    if (coils > 1) rule2++;
    bool on = coils > 0;
    if (on && !coilon[b]) {
      coilstart[b] = cycles;
      pulses++;
      bool color = pdirrow;  // Row to 20 V: yellow
      if (dots[b][lastx][lasty] != color) {
        dots[b][lastx][lasty] = color;
        int x = (b % PANELS_X) * 32 + lastx, y = (b / PANELS_X) * 16 + lasty;
        if (flipcount < (int)(sizeof(flips) / sizeof(flips[0]))) {
          flips[flipcount++] = { cycles / 240.0, x | y << 8 | color << 16 };
        } else {
          dropped++;
        }
      }
    } else if (!on && coilon[b]) {
      double length = (cycles - coilstart[b]) / 240.0;
      if (length < pulseshortest) pulseshortest = length;
      if (length > pulselongest) pulselongest = length;
      if (length < PULSE_MICROS - 1 || length > PULSE_MICROS + 1) rule5++;
    }
    coilon[b] = on;
  }
}

// The RMT on nOE: low for the pulse, then high for the rest, on the clock of the model.
// It catches up with the clock on every call, nOE goes high exactly at the end of the pulse.
static bool rmtowned, rmtactive, rmtlow;
static uint16_t rmton, rmtrest;
static uint64_t rmtlowend, rmtend;

static void rmtupdate() {
  if (!rmtactive) return;
  if (rmtlow && cycles >= rmtlowend) {
    uint64_t now = cycles;
    cycles = rmtlowend;
    pnoe = true;
    rmtlow = false;
    updatemodel();
    cycles = now;
  }
  if (!rmtlow && cycles >= rmtend) rmtactive = false;
}

bool pulsebegin(uint16_t on, uint16_t rest) { rmtowned = true; rmton = on; rmtrest = rest; return true; }

bool pulsestart() {
  cycles += 4;
  rmtupdate();
  if (!rmtowned) return false;
  if (rmtactive) rule5++;  // A pulse on top of a running one
  rmtactive = rmtlow = true;
  rmtlowend = cycles + (uint64_t)rmton * 240;
  rmtend = rmtlowend + (uint64_t)rmtrest * 240;
  pnoe = false;
  updatemodel();
  return true;
}

bool pulsebusy() { cycles += 2; rmtupdate(); return rmtactive; }

void pulserelease() {
  cycles += 4;
  rmtowned = rmtactive = rmtlow = false;
  pnoe = true;
  updatemodel();
}

bool readnoe() { cycles += 4; rmtupdate(); return pnoe; }

void setdata(bool high) { cycles += 4; rmtupdate(); pdata = high; }

void setsclk(bool high) {
  cycles += 4;
  rmtupdate();
  if (high && !psclk) {
    for (int b = BOARDS - 1; b >= 0; b--) {
      for (int i = 47; i > 0; i--) sr[b][i] = sr[b][i - 1];
      sr[b][0] = b == 0 ? pdata : ff[b - 1];
    }
  } else if (!high && psclk) {
    for (int b = 0; b < BOARDS; b++) ff[b] = sr[b][47];
  }
  psclk = high;
}

void setlatch(bool high) {
  cycles += 4;
  rmtupdate();
  if (high && !platch) {
    if (!pnoe) rule1++;
    memcpy(latched, sr, sizeof(sr));
    updatemodel();
  }
  platch = high;
}

void setnoe(bool high) {
  cycles += 4;
  rmtupdate();
  if (rmtowned) return;  // The pin belongs to the RMT
  pnoe = high;
  updatemodel();
}

void setdirx1(bool high) {
  cycles += 4;
  rmtupdate();
  if (high != pdirx1 && !pnoe) rule1++;
  pdirx1 = high;
  updatemodel();
}

void setdirrow(bool high) {
  cycles += 4;
  rmtupdate();
  if (high != pdirrow && !pnoe) rule1++;
  pdirrow = high;
  updatemodel();
}

bool readreturn() { cycles += 4; rmtupdate(); return ff[BOARDS - 1]; }  // JP1 closed on the last board

// Web server: the page sends the requests of the web interface here

WebServer *emuserver;

static std::string urldecode(const std::string &s) {
  std::string out;
  for (size_t i = 0; i < s.size(); i++) {
    if (s[i] == '+') out += ' ';
    else if (s[i] == '%' && i + 2 < s.size()) { out += (char)strtol(s.substr(i + 1, 2).c_str(), nullptr, 16); i += 2; }
    else out += s[i];
  }
  return out;
}

WebServer::Reply WebServer::request(const char *method, const char *path, const char *query, const char *body) {
  args.clear();
  std::string q = query;
  size_t start = 0;
  while (start < q.size()) {
    size_t end = q.find('&', start);
    if (end == std::string::npos) end = q.size();
    std::string pair = q.substr(start, end - start);
    size_t eq = pair.find('=');
    if (!pair.empty()) args[urldecode(pair.substr(0, eq))] = eq == std::string::npos ? String() : String(urldecode(pair.substr(eq + 1)));
    start = end + 1;
  }
  if (body && *body) args["plain"] = body;
  reply = Reply();
  auto route = routes.find(std::string(path) + (strcmp(method, "POST") == 0 ? " POST" : " GET"));
  if (route != routes.end()) route->second();
  else if (notfound) notfound();
  return reply;
}

static WebServer::Reply lastreply;

EXPORT const char *emurequest(const char *method, const char *path, const char *query, const char *body) {
  lastreply = emuserver->request(method, path, query, body);
  return lastreply.body.c_str();
}
EXPORT int emureplycode() { return lastreply.code; }
EXPORT const char *emureplytype() { return lastreply.type.c_str(); }

// Settings in the localStorage of the browser

#ifdef __EMSCRIPTEN__
EM_JS(char *, jsload, (const char *key), {
  const value = localStorage.getItem('flipdot.' + UTF8ToString(key));
  if (value === null) return 0;
  const size = lengthBytesUTF8(value) + 1;
  const pointer = _malloc(size);
  stringToUTF8(value, pointer, size);
  return pointer;
});
EM_JS(void, jssave, (const char *key, const char *value), {
  localStorage.setItem('flipdot.' + UTF8ToString(key), UTF8ToString(value));
});
String emuload(const char *key) {
  char *value = jsload(key);
  String result(value);
  free(value);
  return result;
}
void emusave(const char *key, const char *value) { jssave(key, value); }
#else
static std::map<std::string, std::string> storage;
String emuload(const char *key) { return String(storage[key]); }
void emusave(const char *key, const char *value) { storage[key] = value; }
#endif

// Running the firmware

static bool started;

EXPORT void emustart() {
  for (int b = 0; b < BOARDS; b++) {  // Power on: the 595s hold garbage, R1-R3 keep nOE high and DIR low
    for (int i = 0; i < 48; i++) sr[b][i] = latched[b][i] = rand() & 1;
    ff[b] = rand() & 1;
  }
  setup();
  started = true;
}

// Runs loop() until the virtual clock reaches until (in microseconds since start). A pass of loop()
// that doesn't wait for anything takes 1 ms, so an idle firmware doesn't spin forever.
EXPORT void emurun(double until) {
  if (!started) return;
  uint64_t target = (uint64_t)(until * 240);
  if (target > cycles + 240000000ULL) cycles = target - 240000000ULL;  // More than a second behind: skip
  for (int passes = 0; cycles < target && passes < 5000; passes++) {
    uint64_t before = cycles;
    loop();
    if (cycles - before < 240000) cycles = before + 240000;
    rmtupdate();
  }
}

EXPORT double emunow() { return cycles / 240.0; }
EXPORT int emuflipcount() { return flipcount; }
EXPORT Flip *emuflips() { return flips; }
EXPORT void emuclearflips() { flipcount = 0; }
EXPORT int emucols() { return COLS; }
EXPORT int emurows() { return ROWS; }
EXPORT int emudot(int x, int y) { return dots[(x / 32) + (y / 16) * PANELS_X][x % 32][y % 16]; }

// Counters for the page: pulses, rule breaks, pulse lengths, boards found, fault
static char status[256];
EXPORT const char *emustatus() {
  snprintf(status, sizeof(status),
           "{\"pulses\":%ld,\"rule1\":%ld,\"rule2\":%ld,\"rule5\":%ld,\"pulse_min_us\":%.1f,\"pulse_max_us\":%.1f,"
           "\"boards\":%d,\"boards_found\":%d,\"fault\":%s,\"dropped\":%ld}",
           pulses, rule1, rule2, rule5, pulses ? pulseshortest : 0, pulselongest, CHAIN_BOARDS, chainboardsfound,
           chainfault ? "true" : "false", dropped);
  return status;
}

#ifndef __EMSCRIPTEN__
// Native test: run a few virtual minutes and print the screen
int main(int argc, char **argv) {
  double seconds = argc > 1 ? atof(argv[1]) : 10;
  emustart();
  double t = 0;
  for (; t < 2e6; t += 50000) emurun(t);
  // Animation to play, with its id, or game:id for a game with random buttons for every player
  bool game = argc > 2 && strncmp(argv[2], "game:", 5) == 0;
  if (argc > 2) {
    char query[64];
    snprintf(query, sizeof(query), "id=%s", argv[2] + (game ? 5 : 0));
    emurequest("POST", game ? "/api/game" : "/api/play", query, "");
    printf("%s %s: %d\n", game ? "game" : "play", argv[2], emureplycode());
  }
  for (; t < seconds * 1e6; t += 50000) {
    emurun(t);
    emuclearflips();
    if (game && rand() % 4 == 0) {
      char query[64];
      int button = 1 << (rand() % 6);
      snprintf(query, sizeof(query), "p=%d&b=%d&d=%d", rand() % 4, rand() % 2 ? button : 0, button);
      emurequest("POST", "/api/input", query, "");
    }
  }
  for (int y = 0; y < ROWS; y++) {
    for (int x = 0; x < COLS; x++) putchar(emudot(x, y) ? '#' : '.');
    putchar('\n');
  }
  printf("%s\n", emustatus());
  printf("%s\n", emurequest("GET", "/api/state", "", ""));
  return 0;
}
#endif
