#include <WebServer.h>
#include <ESPmDNS.h>
#include <ArduinoJson.h>
#include <time.h>

#include "animation.h"
#include "game.h"
#include "flipsettings.h"
#include "player.h"
#include "clockface.h"
#ifdef HARDWARE_V3
#include "shiftchain.h"
#else
#include "pca9685.h"
#endif
#include "webui.h"

static WebServer server(80);

static const char page[] PROGMEM = R"html(<!doctype html>
<html lang="nl">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>Flipdot</title>
<style>
:root {
  --bg: #f4f3ef; --card: #ffffff; --text: #1d1d1b; --muted: #6b6a65; --line: #e2e0d9;
  --accent: #e0a800; --accent-text: #1d1d1b; --off: #c9c7bf;
}
@media (prefers-color-scheme: dark) {
  :root {
    --bg: #141413; --card: #1f1f1d; --text: #ecebe6; --muted: #9a9890; --line: #33322f;
    --accent: #f2c12e; --accent-text: #141413; --off: #4a4945;
  }
}
* { box-sizing: border-box; }
body { margin: 0; background: var(--bg); color: var(--text);
  font: 16px/1.4 system-ui, -apple-system, "Segoe UI", sans-serif; }
main { max-width: 640px; margin: 0 auto; padding: 16px; }
header { display: flex; align-items: center; gap: 12px; margin-bottom: 16px; }
h1 { font-size: 22px; margin: 0; flex: 1; }
h2 { font-size: 15px; text-transform: uppercase; letter-spacing: .06em; color: var(--muted); margin: 0 0 12px; }
.status { font-size: 14px; color: var(--muted); }
.card { background: var(--card); border: 1px solid var(--line); border-radius: 12px; padding: 16px; margin-bottom: 16px; }
.row { display: flex; align-items: center; gap: 12px; min-height: 44px; }
.row + .row { border-top: 1px solid var(--line); }
.row label.grow, .row .grow { flex: 1; }
.hint { font-size: 14px; color: var(--muted); margin: 8px 0 0; }
select, input[type=number] { font: inherit; color: var(--text); background: var(--bg);
  border: 1px solid var(--line); border-radius: 8px; padding: 6px 8px; }
input[type=number] { width: 80px; }
.switch { position: relative; width: 48px; height: 28px; flex: none; }
.switch input { opacity: 0; width: 100%; height: 100%; margin: 0; position: absolute; cursor: pointer; z-index: 1; }
.switch span { position: absolute; inset: 0; background: var(--off); border-radius: 14px; transition: background .15s; }
.switch span::after { content: ""; position: absolute; width: 22px; height: 22px; left: 3px; top: 3px;
  background: #fff; border-radius: 50%; transition: transform .15s; }
.switch input:checked + span { background: var(--accent); }
.switch input:checked + span::after { transform: translateX(20px); }
.segments { display: flex; flex-wrap: wrap; gap: 6px; }
.segments label { flex: 1 0 auto; text-align: center; padding: 8px 10px; cursor: pointer; font-size: 14px;
  border: 1px solid var(--line); border-radius: 8px; }
.segments input { display: none; }
.segments label:has(input:checked) { background: var(--accent); color: var(--accent-text); font-weight: 600; }
.animation { padding: 10px 0; }
.animation + .animation { border-top: 1px solid var(--line); }
.animation .details { display: flex; flex-wrap: wrap; align-items: center; gap: 8px 16px; margin-top: 6px;
  font-size: 14px; color: var(--muted); }
.animation .details input[type=range] { width: 140px; accent-color: var(--accent); }
.animation.disabled .name { color: var(--muted); }
button { font: inherit; font-size: 14px; color: var(--text); background: var(--bg); border: 1px solid var(--line);
  border-radius: 8px; padding: 6px 12px; cursor: pointer; }
button:hover { border-color: var(--accent); }
.buttons { display: flex; gap: 8px; margin-bottom: 4px; }
#toast { position: fixed; left: 50%; bottom: 24px; transform: translateX(-50%); background: var(--text); color: var(--bg);
  padding: 8px 16px; border-radius: 8px; font-size: 14px; opacity: 0; transition: opacity .2s; pointer-events: none; }
#toast.show { opacity: 1; }
.game { display: flex; align-items: center; gap: 12px; padding: 8px 0; }
.game + .game { border-top: 1px solid var(--line); }
.game .players { font-size: 14px; color: var(--muted); }
#pad { position: fixed; inset: 0; background: var(--bg); display: none; flex-direction: column; z-index: 10;
  user-select: none; -webkit-user-select: none; touch-action: none; }
#pad.open { display: flex; }
#pad .bar { display: flex; align-items: center; gap: 8px; padding: 12px 16px; flex-wrap: wrap; }
#pad .bar h1 { flex: 1; }
#pad .controls { flex: 1; display: flex; align-items: center; justify-content: space-around; gap: 24px; padding: 16px; }
.dpad { display: grid; grid-template-columns: repeat(3, 72px); grid-template-rows: repeat(3, 72px); gap: 6px; }
.dpad button, .ab button { font-size: 26px; border-radius: 14px; background: var(--card); padding: 0; touch-action: none; }
.dpad button.down, .ab button.down { background: var(--accent); color: var(--accent-text); }
.dpad .u { grid-column: 2; } .dpad .l { grid-row: 2; grid-column: 1; }
.dpad .r { grid-row: 2; grid-column: 3; } .dpad .d { grid-row: 3; grid-column: 2; }
.ab { display: flex; gap: 16px; align-items: center; }
.ab button { width: 84px; height: 84px; border-radius: 50%; font-weight: 700; }
.ab .b { margin-top: 64px; }
#pad .keys { color: var(--muted); font-size: 13px; padding: 0 16px 16px; text-align: center; }
</style>
</head>
<body>
<main>
  <header>
    <h1>Flipdot</h1>
    <span class="status" id="status">…</span>
    <span class="switch" title="Aan/uit"><input type="checkbox" id="power" aria-label="Aan/uit"><span></span></span>
  </header>

  <section class="card">
    <h2>Klok</h2>
    <div class="row">
      <label class="grow" for="clockEnabled">Klok tonen</label>
      <span class="switch"><input type="checkbox" id="clockEnabled"><span></span></span>
    </div>
    <div class="row clockonly">
      <div class="segments grow" id="style">
        <label><input type="radio" name="style" value="big">Groot</label>
        <label><input type="radio" name="style" value="date">Tijd + datum</label>
        <label><input type="radio" name="style" value="analog">Analoog</label>
        <label><input type="radio" name="style" value="words">Woorden</label>
        <label><input type="radio" name="style" value="seconds">Seconden</label>
        <label><input type="radio" name="style" value="binary">Binair</label>
      </div>
    </div>
    <div class="row clockonly" id="fontRow">
      <label class="grow" for="font">Lettertype</label>
      <select id="font"></select>
    </div>
    <div class="row clockonly" id="secondsRow">
      <label class="grow" for="seconds">Secondewijzer</label>
      <span class="switch"><input type="checkbox" id="seconds"><span></span></span>
    </div>
    <div class="row clockonly" id="transitionRow">
      <label class="grow" for="transition">Overgang per minuut</label>
      <select id="transition">
        <option value="none">geen</option>
        <option value="dissolve">oplossen</option>
        <option value="fall">vallen</option>
        <option value="sand">zand</option>
      </select>
    </div>
    <div class="row clockonly">
      <label class="grow" for="interval">Animaties</label>
      <select id="interval">
        <option value="0">nooit</option>
        <option value="5">elke 5 minuten</option>
        <option value="10">elke 10 minuten</option>
        <option value="15">elk kwartier</option>
        <option value="20">elke 20 minuten</option>
        <option value="30">elk half uur</option>
        <option value="60">elk heel uur</option>
      </select>
    </div>
    <div class="row clockonly" id="perBreakRow">
      <label class="grow" for="perBreak">Animaties per keer</label>
      <input type="number" id="perBreak" min="1" max="10">
    </div>
    <p class="hint" id="clockHint"></p>
  </section>

  <section class="card">
    <h2>Spelletjes</h2>
    <div id="games"></div>
    <p class="hint">Besturen met de knoppen op het scherm, het toetsenbord of een gamepad aan je computer.
      Na anderhalve minuut zonder knop gaat de flipdot terug naar de klok.</p>
  </section>

  <section class="card">
    <h2>Animaties</h2>
    <div class="buttons">
      <button id="allOn">Alles aan</button>
      <button id="allOff">Alles uit</button>
    </div>
    <div id="animations"></div>
  </section>
</main>
<div id="toast"></div>
<div id="pad">
  <div class="bar">
    <h1 id="padTitle">Spel</h1>
    <div class="segments" id="seat"></div>
    <button id="restart">Opnieuw</button>
    <button id="stop">✕ Stoppen</button>
  </div>
  <div class="controls">
    <div class="dpad">
      <button class="u" data-b="1" aria-label="Omhoog">▲</button>
      <button class="l" data-b="4" aria-label="Links">◀</button>
      <button class="r" data-b="8" aria-label="Rechts">▶</button>
      <button class="d" data-b="2" aria-label="Omlaag">▼</button>
    </div>
    <div class="ab">
      <button class="b" data-b="32">B</button>
      <button class="a" data-b="16">A</button>
    </div>
  </div>
  <p class="keys">Toetsenbord: pijltjes, A = spatie of X, B = Z. Een tweede speler: WASD, A = F, B = G.
    Een gamepad aan je computer is de volgende speler. Druk op een knop om te beginnen.</p>
</div>

<script>
const $ = s => document.querySelector(s);
let catalog = [], settings = null, fonts = [];

async function api(path, body) {
  const options = body === undefined ? {} :
    { method: 'POST', headers: { 'Content-Type': 'application/json' }, body: JSON.stringify(body) };
  const response = await fetch(path, options);
  if (!response.ok) throw new Error(response.status);
  return response.json();
}

let toastTimer;
function toast(text) {
  const t = $('#toast');
  t.textContent = text;
  t.classList.add('show');
  clearTimeout(toastTimer);
  toastTimer = setTimeout(() => t.classList.remove('show'), 1500);
}

function showStatus(state) {
  let text;
  if (state.status === 'fault') text = 'Storing: noodstop, herstart nodig';
  else if (state.status === 'off') text = 'Uit';
  else if (state.status === 'clock') text = 'Klok';
  else text = (catalog.find(a => a.id === state.status) || games.find(g => g.id === state.status) || { name: state.status }).name;
  if (state.time && state.status !== 'off') text += ' · ' + state.time.slice(0, 5);
  $('#status').textContent = text;
}

async function save(change) {
  try {
    const state = await api('/api/settings', change);
    settings = state.settings;
    showStatus(state);
    updateClockVisibility();
    toast('Opgeslagen');
  } catch (e) {
    toast('Opslaan mislukt');
  }
}

function updateClockVisibility() {
  const c = settings.clock;
  document.querySelectorAll('.clockonly').forEach(e => e.style.display = c.enabled ? '' : 'none');
  $('#secondsRow').style.display = c.enabled && c.style === 'analog' ? '' : 'none';
  $('#fontRow').style.display = c.enabled && c.style === 'words' && fonts.length > 1 ? '' : 'none';
  $('#perBreakRow').style.display = c.enabled && c.interval > 0 ? '' : 'none';
  const ticking = c.style === 'seconds' || c.style === 'binary' || (c.style === 'analog' && c.seconds);
  $('#transitionRow').style.display = c.enabled && !ticking ? '' : 'none';
  const on = catalog.filter(a => settings.animations[a.id].enabled).length;
  let hint;
  if (!c.enabled) hint = on ? 'De klok staat uit: de animaties spelen achter elkaar.' : 'Klok en animaties staan uit, de klok wordt toch getoond.';
  else if (c.interval === 0) hint = 'Alleen de klok.';
  else if (!on) hint = 'Er staan geen animaties aan, dus alleen de klok.';
  else hint = 'De animaties beginnen op hele ' + (c.interval === 60 ? 'uren.' : 'veelvouden van ' + c.interval + ' minuten na het hele uur.');
  $('#clockHint').textContent = hint;
}

function renderAnimations() {
  const list = $('#animations');
  list.innerHTML = '';
  for (const a of catalog) {
    const s = settings.animations[a.id];
    const row = document.createElement('div');
    row.className = 'animation' + (s.enabled ? '' : ' disabled');
    row.innerHTML = `
      <div class="row">
        <span class="switch"><input type="checkbox" aria-label="Aan/uit"><span></span></span>
        <span class="name grow"></span>
        <button class="play">▶ Nu tonen</button>
      </div>
      <div class="details">
        <label>Snelheid <input type="range" min="25" max="400" step="25"> <span class="speed"></span></label>
        <label>Duur <input type="number" min="5" max="600" step="5"> s</label>
      </div>`;
    row.querySelector('.name').textContent = a.name;

    const enabled = row.querySelector('input[type=checkbox]');
    enabled.checked = s.enabled;
    enabled.onchange = () => {
      row.classList.toggle('disabled', !enabled.checked);
      save({ animations: { [a.id]: { enabled: enabled.checked } } });
    };

    const speed = row.querySelector('input[type=range]');
    const speedLabel = row.querySelector('.speed');
    speed.value = s.speed;
    speedLabel.textContent = s.speed + '%';
    speed.oninput = () => speedLabel.textContent = speed.value + '%';
    speed.onchange = () => save({ animations: { [a.id]: { speed: +speed.value } } });

    const duration = row.querySelector('input[type=number]');
    duration.value = s.duration;
    duration.onchange = () => {
      duration.value = Math.min(600, Math.max(5, +duration.value || 30));
      save({ animations: { [a.id]: { duration: +duration.value } } });
    };

    row.querySelector('.play').onclick = async () => {
      try {
        showStatus(await api('/api/play?id=' + encodeURIComponent(a.id), {}));
        toast(a.name + ' wordt getoond');
      } catch (e) {
        toast('Lukt niet');
      }
    };
    list.appendChild(row);
  }
}

function render() {
  $('#power').checked = settings.power;
  $('#power').onchange = e => save({ power: e.target.checked });

  const c = settings.clock;
  $('#clockEnabled').checked = c.enabled;
  $('#clockEnabled').onchange = e => save({ clock: { enabled: e.target.checked } });
  document.querySelectorAll('input[name=style]').forEach(r => {
    r.checked = r.value === c.style;
    r.onchange = () => save({ clock: { style: r.value } });
  });
  $('#font').replaceChildren(...fonts.map(f => new Option(f.name, f.id)));
  $('#font').value = c.font;
  $('#font').onchange = e => save({ clock: { font: e.target.value } });
  $('#seconds').checked = c.seconds;
  $('#seconds').onchange = e => save({ clock: { seconds: e.target.checked } });
  $('#transition').value = c.transition;
  $('#transition').onchange = e => save({ clock: { transition: e.target.value } });
  $('#interval').value = c.interval;
  $('#interval').onchange = e => save({ clock: { interval: +e.target.value } });
  $('#perBreak').value = c.animations_per_break;
  $('#perBreak').onchange = e => {
    e.target.value = Math.min(10, Math.max(1, +e.target.value || 1));
    save({ clock: { animations_per_break: +e.target.value } });
  };

  const all = enabled => () => {
    save({ animations: Object.fromEntries(catalog.map(a => [a.id, { enabled }])) }).then(renderAnimations);
  };
  $('#allOn').onclick = all(true);
  $('#allOff').onclick = all(false);

  renderAnimations();
  updateClockVisibility();
}

// Games: the controller sends the buttons of every player to /api/input when they change.
// held: buttons down now, pressed: buttons that went down since the last message (a short tap
// between two messages still counts).
let games = [], seat = 0, playing = null;
const touchHeld = [0, 0, 0, 0], keyHeld = [0, 0, 0, 0], padHeld = [0, 0, 0, 0];
const heldNow = [0, 0, 0, 0], pressedSince = [0, 0, 0, 0], lastSent = [0, 0, 0, 0];
let sending = false;

function buttonsChanged() {
  for (let p = 0; p < 4; p++) {
    const held = touchHeld[p] | keyHeld[p] | padHeld[p];
    pressedSince[p] |= held & ~heldNow[p];
    heldNow[p] = held;
  }
  sendInput();
}

async function sendInput() {
  if (sending) return;  // One message at a time; what changed meanwhile goes when this one is done
  sending = true;
  try {
    for (let again = true; again;) {
      again = false;
      for (let p = 0; p < 4; p++) {
        const d = pressedSince[p], b = heldNow[p];
        if (!d && b === lastSent[p]) continue;
        pressedSince[p] = 0;
        lastSent[p] = b;
        await fetch(`/api/input?p=${p}&b=${b}&d=${d}`, { method: 'POST' });
        again = true;
      }
    }
  } catch (e) {}
  sending = false;
}

const keymap = {
  ArrowUp: [0, 1], ArrowDown: [0, 2], ArrowLeft: [0, 4], ArrowRight: [0, 8], ' ': [0, 16], x: [0, 16], z: [0, 32],
  w: [1, 1], s: [1, 2], a: [1, 4], d: [1, 8], f: [1, 16], g: [1, 32],
};
window.gamekey = (e, down) => {
  if (!playing) return false;
  const k = keymap[e.key.length === 1 ? e.key.toLowerCase() : e.key];
  if (!k) return false;
  e.preventDefault();
  if (e.repeat) return true;
  const p = (seat + k[0]) % 4;
  keyHeld[p] = down ? keyHeld[p] | k[1] : keyHeld[p] & ~k[1];
  buttonsChanged();
  return true;
};
addEventListener('keydown', e => gamekey(e, true));
addEventListener('keyup', e => gamekey(e, false));

function pollPads() {
  if (!playing) return;
  padHeld.fill(0);
  const pads = navigator.getGamepads ? [...navigator.getGamepads()].filter(p => p) : [];
  pads.forEach((pad, i) => {
    const b = n => pad.buttons[n] && pad.buttons[n].pressed;
    const ax = pad.axes[0] || 0, ay = pad.axes[1] || 0;
    let held = 0;
    if (b(12) || ay < -0.5) held |= 1;
    if (b(13) || ay > 0.5) held |= 2;
    if (b(14) || ax < -0.5) held |= 4;
    if (b(15) || ax > 0.5) held |= 8;
    if (b(0) || b(3) || b(9)) held |= 16;
    if (b(1) || b(2)) held |= 32;
    padHeld[(seat + i + 1) % 4] |= held;  // The keyboard has this seat, the pads the next ones
  });
  buttonsChanged();
  requestAnimationFrame(pollPads);
}

document.querySelectorAll('#pad button[data-b]').forEach(button => {
  const bit = +button.dataset.b;
  const press = down => e => {
    e.preventDefault();
    button.classList.toggle('down', down);
    touchHeld[seat] = down ? touchHeld[seat] | bit : touchHeld[seat] & ~bit;
    buttonsChanged();
  };
  button.addEventListener('pointerdown', e => { button.setPointerCapture(e.pointerId); press(true)(e); });
  button.addEventListener('pointerup', press(false));
  button.addEventListener('pointercancel', press(false));
  button.addEventListener('contextmenu', e => e.preventDefault());
});

function renderSeats(players) {
  const box = $('#seat');
  box.innerHTML = '';
  if (players < 2) return;
  for (let p = 0; p < players; p++) {
    const label = document.createElement('label');
    label.innerHTML = `<input type="radio" name="seat" value="${p}">Speler ${p + 1}`;
    const radio = label.querySelector('input');
    radio.checked = p === seat;
    radio.onchange = () => { releaseAll(); seat = p; };
    box.appendChild(label);
  }
}

function releaseAll() {
  for (const list of [touchHeld, keyHeld, padHeld]) list.fill(0);
  document.querySelectorAll('#pad button.down').forEach(b => b.classList.remove('down'));
  buttonsChanged();
}

async function startGame(g) {
  try {
    showStatus(await api('/api/game?id=' + encodeURIComponent(g.id), {}));
  } catch (e) {
    toast('Lukt niet');
    return;
  }
  playing = g;
  if (seat >= g.players) seat = 0;
  $('#padTitle').textContent = g.name;
  renderSeats(g.players);
  $('#pad').classList.add('open');
  requestAnimationFrame(pollPads);
}

function closePad() {
  releaseAll();
  playing = null;
  $('#pad').classList.remove('open');
}

$('#restart').onclick = () => playing && startGame(playing);
$('#stop').onclick = async () => {
  closePad();
  try { showStatus(await api('/api/game?id=', {})); } catch (e) {}
};

function renderGames() {
  const list = $('#games');
  list.innerHTML = '';
  for (const g of games) {
    const row = document.createElement('div');
    row.className = 'game';
    row.innerHTML = '<span class="grow"><span class="name"></span> <span class="players"></span></span><button>▶ Spelen</button>';
    row.querySelector('.name').textContent = g.name;
    row.querySelector('.players').textContent = g.players > 1 ? `1–${g.players} spelers` : '1 speler';
    row.querySelector('button').onclick = () => startGame(g);
    list.appendChild(row);
  }
}

api('/api/state').then(state => {
  catalog = state.catalog;
  settings = state.settings;
  fonts = state.clock_fonts || [];
  games = state.games || [];
  render();
  renderGames();
  showStatus(state);
  setInterval(() => api('/api/state').then(s => {
    showStatus(s);
    if (playing && s.status !== playing.id) closePad();  // Nobody played for a while
  }).catch(() => {}), 3000);
}).catch(() => $('#status').textContent = 'Geen verbinding');
</script>
</body>
</html>
)html";

static void sendstate() {
  JsonDocument doc;
  doc["status"] = playerstatus();

  struct tm now;
  if (getLocalTime(&now, 0)) {
    char time[9];
    strftime(time, sizeof(time), "%H:%M:%S", &now);
    doc["time"] = time;
  } else {
    doc["time"] = nullptr;
  }

  doc["uptime"] = millis() / 1000;

#ifdef HARDWARE_V3
  JsonObject chain = doc["chain"].to<JsonObject>();
  chain["boards"] = CHAIN_BOARDS;
  chain["boards_found"] = chainboardsfound;
  chain["fault"] = chainfault;
  chain["pulses"] = chainstats.pulses;
  chain["pulse_us"] = PULSE_MICROS;  // Timed by the RMT
  chain["gap_us"] = PULSE_GAP_MICROS;
#else
  uint32_t mhz = getCpuFrequencyMhz();
  JsonObject i2c = doc["i2c"].to<JsonObject>();
  i2c["writes"] = i2cstats.writes;
  i2c["nack"] = i2cstats.nack;
  i2c["timeout"] = i2cstats.timeout;
  i2c["other"] = i2cstats.other;
  i2c["off_failed"] = i2cstats.offfailed;
  i2c["pulses"] = i2cstats.pulses;
  i2c["pulse_max_us"] = i2cstats.pulsemax / mhz;
  i2c["pulse_avg_us"] = i2cstats.pulses ? (uint32_t)(i2cstats.pulsetotal / i2cstats.pulses / mhz) : 0;
#endif

  settingstojson(doc["settings"].to<JsonObject>());

  JsonArray fonts = doc["clock_fonts"].to<JsonArray>();  // Only the fonts that fit this screen
  for (uint8_t i = 0; i < clockfonts(); i++) {
    if (clockfontfits(i)) {
      JsonObject f = fonts.add<JsonObject>();
      f["id"] = clockfontid(i);
      f["name"] = clockfontname(i);
    }
  }

  JsonArray catalog = doc["catalog"].to<JsonArray>();
  for (uint8_t i = 0; i < ANIMATION_COUNT; i++) {
    JsonObject a = catalog.add<JsonObject>();
    a["id"] = animations[i]->id;
    a["name"] = animations[i]->name;
  }

  JsonArray gamelist = doc["games"].to<JsonArray>();
  for (uint8_t i = 0; i < GAME_COUNT; i++) {
    JsonObject g = gamelist.add<JsonObject>();
    g["id"] = games[i]->id;
    g["name"] = games[i]->name;
    g["players"] = games[i]->players;
  }

  String json;
  serializeJson(doc, json);
  server.send(200, "application/json", json);
}

static void handlesettings() {
  JsonDocument doc;
  if (deserializeJson(doc, server.arg("plain")) || !doc.is<JsonObject>()) {
    server.send(400, "application/json", "{\"error\":\"invalid JSON\"}");
    return;
  }
  settingsfromjson(doc.as<JsonObjectConst>());
  savesettings();
  playerreload();
  sendstate();
}

static void handleplay() {
  int8_t index = findanimation(server.arg("id").c_str());
  if (index < 0) {
    server.send(404, "application/json", "{\"error\":\"unknown animation\"}");
    return;
  }
  playerpreview(index);
  sendstate();
}

// A game: ?id=snake starts it, an empty id stops it
static void handlegame() {
  String id = server.arg("id");
  if (id.length() == 0) {
    playerstop();
    sendstate();
    return;
  }
  int8_t index = findgame(id.c_str());
  if (index < 0) {
    server.send(404, "application/json", "{\"error\":\"unknown game\"}");
    return;
  }
  playergame(index);
  sendstate();
}

// Buttons of one player: ?p=0..3&b=held&d=pressed since the last message. Kept short, it comes often
static void handleinput() {
  gameinput(server.arg("p").toInt(), server.arg("b").toInt(), server.arg("d").toInt());
  server.send(200, "application/json", "{}");
}

void webuisetup() {
  server.on("/", HTTP_GET, []() {
    server.send_P(200, "text/html", page);
  });
  server.on("/api/state", HTTP_GET, sendstate);
  server.on("/api/settings", HTTP_POST, handlesettings);
  server.on("/api/play", HTTP_POST, handleplay);
  server.on("/api/game", HTTP_POST, handlegame);
  server.on("/api/input", HTTP_POST, handleinput);
  server.onNotFound([]() {
    server.send(404, "text/plain", "Not found");
  });
  server.begin();
  MDNS.addService("http", "tcp", 80);
}

void webuiloop() {
  server.handleClient();
}
