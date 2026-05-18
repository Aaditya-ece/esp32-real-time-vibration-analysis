#include <Adafruit_MPU6050.h>
#include <Adafruit_Sensor.h>
#include <Wire.h>
#include <WiFi.h>
#include <WebServer.h>
#include <arduinoFFT.h>

// ── WiFi ───────────────────────────────────────────────────────────────────
const char* SSID     = "Aditya";
const char* PASSWORD = "dvbl2606";

// ── FFT settings ───────────────────────────────────────────────────────────
#define SAMPLES      256
#define SAMPLE_RATE  500.0
// Resolution = 500 / 256 = 1.95 Hz/bin
// Range      = 0 to 250 Hz

// ── FFT buffers ────────────────────────────────────────────────────────────
double vReal[SAMPLES];
double vImag[SAMPLES];
ArduinoFFT<double> FFT(vReal, vImag, SAMPLES, SAMPLE_RATE);

float fftBins[SAMPLES / 2]; //keep only right frequencies
float peakHz  = 0;
float peakMag = 0;

// ── Objects ────────────────────────────────────────────────────────────────
Adafruit_MPU6050 mpu;
WebServer        server(80);

// ═══════════════════════════════════════════════════════════════════════════
//  HTML PAGE
// ═══════════════════════════════════════════════════════════════════════════
const char PAGE[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8"/>
<meta name="viewport" content="width=device-width,initial-scale=1"/>
<title>ESP32 FFT</title>
<style>
  * { box-sizing: border-box; margin: 0; padding: 0; }
  body {
    background: #0a0f14;
    color: #cde;
    font-family: monospace;
    padding: 16px;
  }
  h2 { color: #00e5ff; letter-spacing: .1em; margin-bottom: 10px; }

  /* ── Status bar ── */
  #statusBar {
    font-size: .75rem;
    color: #4a6070;
    margin-bottom: 14px;
    display: flex;
    align-items: center;
    gap: 8px;
  }
  .dot {
    width: 8px; height: 8px;
    border-radius: 50%;
    background: #4a6070;
    display: inline-block;
  }
  .dot.live {
    background: #00ff88;
    box-shadow: 0 0 8px #00ff88;
    animation: pulse 1.4s infinite;
  }
  .dot.err { background: #ff4444; }
  @keyframes pulse { 0%,100%{opacity:1} 50%{opacity:.35} }

  /* ── Info cards ── */
  .cards {
    display: flex;
    gap: 12px;
    flex-wrap: wrap;
    margin-bottom: 16px;
  }
  .card {
    background: #0d1a26;
    border: 1px solid #1a3050;
    border-radius: 8px;
    padding: 10px 18px;
    text-align: center;
    min-width: 130px;
  }
  .card-label {
    font-size: .6rem;
    color: #4a6070;
    letter-spacing: .15em;
    text-transform: uppercase;
    margin-bottom: 4px;
  }
  .card-value {
    font-size: 1.5rem;
    font-weight: bold;
    color: #fff;
  }
  .card-value span { font-size: .78rem; color: #00e5ff; }

  /* ── Canvas chart ── */
  #chartWrap {
    background: #0d1a26;
    border: 1px solid #1a3050;
    border-radius: 10px;
    padding: 14px;
  }
  #chartTitle {
    font-size: .65rem;
    color: #00e5ff;
    letter-spacing: .2em;
    text-transform: uppercase;
    margin-bottom: 10px;
  }
  canvas {
    width: 100%;
    display: block;
  }
</style>
</head>
<body>

<h2>ESP32-S3 &nbsp;·&nbsp; FFT Vibration Monitor</h2>

<div id="statusBar">
  <span class="dot" id="dot"></span>
  <span id="statusText">Connecting…</span>
</div>

<div class="cards">
  <div class="card">
    <div class="card-label">Peak Frequency</div>
    <div class="card-value" id="peakHz">— <span>Hz</span></div>
  </div>
  <div class="card">
    <div class="card-label">Peak Magnitude</div>
    <div class="card-value" id="peakMag">— <span>m/s²</span></div>
  </div>
  <div class="card">
    <div class="card-label">Resolution</div>
    <div class="card-value">1.95 <span>Hz/bin</span></div>
  </div>
  <div class="card">
    <div class="card-label">Range</div>
    <div class="card-value">0–250 <span>Hz</span></div>
  </div>
</div>

<div id="chartWrap">
  <div id="chartTitle">FFT Frequency Spectrum</div>
  <canvas id="fftCanvas" height="220"></canvas>
</div>

<script>
// ── Pure canvas FFT chart — no external libraries needed ──────────────────
const canvas  = document.getElementById('fftCanvas');
const ctx     = canvas.getContext('2d');
const BINS    = 128;
const SAMPLE_RATE = 500.0;
const FFT_SIZE    = 256;
const HZ_PER_BIN  = SAMPLE_RATE / FFT_SIZE;  // 1.953 Hz

let bins = new Array(BINS).fill(0);

// Resize canvas to match its CSS width (responsive)
function resizeCanvas() {
  canvas.width  = canvas.offsetWidth;
  canvas.height = 220;
}
resizeCanvas();
window.addEventListener('resize', () => { resizeCanvas(); drawChart(bins); });

// ── drawChart(data) ───────────────────────────────────────────────────────
// Draws a bar spectrum chart on the canvas.
// data: array of BINS magnitude values
function drawChart(data) {
  const W = canvas.width;
  const H = canvas.height;
  const PAD_LEFT = 52;    // space for Y-axis labels
  const PAD_BOT  = 30;    // space for X-axis labels
  const PAD_TOP  = 10;
  const PAD_RIGHT = 8;

  const plotW = W - PAD_LEFT - PAD_RIGHT;
  const plotH = H - PAD_BOT  - PAD_TOP;

  // Clear
  ctx.clearRect(0, 0, W, H);

  // Background
  ctx.fillStyle = '#0d1a26';
  ctx.fillRect(0, 0, W, H);

  // Find max for Y scaling (min scale = 0.5 so chart isn't empty when still)
  const maxVal = Math.max(0.5, ...data);

  // ── Grid lines ────────────────────────────────────────────────────────
  ctx.strokeStyle = '#0f2030';
  ctx.lineWidth   = 1;
  const Y_STEPS = 5;
  for (let i = 0; i <= Y_STEPS; i++) {
    const y = PAD_TOP + plotH - (i / Y_STEPS) * plotH;
    ctx.beginPath();
    ctx.moveTo(PAD_LEFT, y);
    ctx.lineTo(PAD_LEFT + plotW, y);
    ctx.stroke();

    // Y-axis labels
    ctx.fillStyle = '#4a6070';
    ctx.font = '9px monospace';
    ctx.textAlign = 'right';
    const val = ((i / Y_STEPS) * maxVal).toFixed(2);
    ctx.fillText(val, PAD_LEFT - 4, y + 3);
  }

  // ── Bars ──────────────────────────────────────────────────────────────
  const barW = plotW / BINS;
  for (let k = 0; k < BINS; k++) {
    const barH = (data[k] / maxVal) * plotH;
    const x    = PAD_LEFT + k * barW;
    const y    = PAD_TOP  + plotH - barH;

    // Bar fill — cyan with brightness scaled to height
    const alpha = 0.4 + 0.55 * (data[k] / maxVal);
    ctx.fillStyle = `rgba(0, 229, 255, ${alpha.toFixed(2)})`;
    ctx.fillRect(x, y, Math.max(barW - 0.5, 1), barH);
  }

  // ── X-axis labels (frequency) ──────────────────────────────────────────
  ctx.fillStyle  = '#4a6070';
  ctx.font       = '9px monospace';
  ctx.textAlign  = 'center';
  const X_LABEL_EVERY = 16;   // label every 16 bins (~31 Hz apart)
  for (let k = 0; k < BINS; k += X_LABEL_EVERY) {
    const hz = (k * HZ_PER_BIN).toFixed(0);
    const x  = PAD_LEFT + (k + 0.5) * barW;
    ctx.fillText(hz, x, H - 8);
  }

  // ── Axis titles ───────────────────────────────────────────────────────
  ctx.fillStyle = '#4a6070';
  ctx.font      = '10px monospace';
  ctx.textAlign = 'center';
  ctx.fillText('Frequency (Hz)', PAD_LEFT + plotW / 2, H - 1);

  // Y-axis title (rotated)
  ctx.save();
  ctx.translate(10, PAD_TOP + plotH / 2);
  ctx.rotate(-Math.PI / 2);
  ctx.fillText('Magnitude (m/s²)', 0, 0);
  ctx.restore();

  // ── Axes ──────────────────────────────────────────────────────────────
  ctx.strokeStyle = '#1a3050';
  ctx.lineWidth   = 1;
  ctx.beginPath();
  ctx.moveTo(PAD_LEFT, PAD_TOP);
  ctx.lineTo(PAD_LEFT, PAD_TOP + plotH);
  ctx.lineTo(PAD_LEFT + plotW, PAD_TOP + plotH);
  ctx.stroke();
}

// Draw empty chart immediately on load
drawChart(bins);

// ── Fetch /fft and update ─────────────────────────────────────────────────
async function update() {
  try {
    const resp = await fetch('/fft');

    if (!resp.ok) throw new Error('HTTP ' + resp.status);

    const data = await resp.json();

    // Update bins array (only first 128 — 0 to 250 Hz)
    bins = data.bins.slice(0, BINS);

    // Redraw chart
    drawChart(bins);

    // Update info cards
    document.getElementById('peakHz').innerHTML  =
      parseFloat(data.peak_hz).toFixed(1)  + ' <span>Hz</span>';
    document.getElementById('peakMag').innerHTML =
      parseFloat(data.peak_mag).toFixed(3) + ' <span>m/s²</span>';

    // Status → live
    document.getElementById('dot').className      = 'dot live';
    document.getElementById('statusText').textContent = 'Live — refreshes every 1.5 s';

  } catch (err) {
    document.getElementById('dot').className      = 'dot err';
    document.getElementById('statusText').textContent = 'Error: ' + err.message;
    console.error('Fetch error:', err);
  }
}

// First fetch immediately, then every 1.5 s
update();
setInterval(update, 1500);
</script>
</body>
</html>
)rawliteral";


// ══════════════════════════════
//  collectAndComputeFFT()
// ══════════════════════════════
void collectAndComputeFFT() {

  // Step 1: Collect 256 samples at 500 Hz (2 ms per sample)
  for (int i = 0; i < SAMPLES; i++) {
    unsigned long t0 = micros();  //store start time

    sensors_event_t a, g, temp; // container of adafruit library
    mpu.getEvent(&a, &g, &temp); // get data via I2C from sensor

    vReal[i] = a.acceleration.z;
    vImag[i] = 0.0;   // fbecause in real the imagnery vib. not exists but fft need img and real both

    while (micros() - t0 < 2000);   // wait for 2 ms window, sensor can give data in 400us also but we want only 2ms data
  }

  // Step 2: Remove DC offset (gravity ~9.81 m/s²)
  double mean = 0;
  for (int i = 0; i < SAMPLES; i++) mean += vReal[i];
  mean /= SAMPLES;
  for (int i = 0; i < SAMPLES; i++) vReal[i] -= mean;  // to find the true offset of each data

  FFT.compute(FFTDirection::Forward); // Step 3: Compute FFT
  FFT.complexToMagnitude();  // Step 4: Complex → magnitude

  // Step 5: Store results, find peak
  peakHz  = 0;
  peakMag = 0;
  for (int k = 1; k < SAMPLES / 2; k++) {
    fftBins[k] = (float)vReal[k];
    float hz = k * (SAMPLE_RATE / SAMPLES);
    if (fftBins[k] > peakMag) {
      peakMag = fftBins[k];
      peakHz  = hz;
    }
  }

  Serial.print("Peak: ");
  Serial.print(peakHz,  1); Serial.print(" Hz  |  ");
  Serial.print(peakMag, 3); Serial.println(" m/s²");
}

//  HTTP HANDLERS
// ═══════════════════════════════════════════════════════════════════════════

void handleRoot() {
  server.send_P(200, "text/html", PAGE);
}

void handleFFT() {
  String json = "{\"bins\":[";
  for (int k = 0; k < SAMPLES / 2; k++) {
    if (k > 0) json += ',';
    json += String(fftBins[k], 3);
  }
  json += "],\"peak_hz\":";
  json += String(peakHz,  2);
  json += ",\"peak_mag\":";
  json += String(peakMag, 4);
  json += "}";

  server.sendHeader("Access-Control-Allow-Origin", "*");
  server.send(200, "application/json", json);
}

//  SETUP
// ═══════════════════════════════════════════════════════════════════════════
void setup() {
  Serial.begin(115200);
  while (!Serial);
  Serial.println("\n=== ESP32-S3 FFT WebServer ===");

  Wire.begin(8, 9);
  Wire.setClock(400000);

  if (!mpu.begin()) {
    Serial.println("[ERROR] MPU not found! Check wiring.");
    while (1) delay(10);
  }
  mpu.setAccelerometerRange(MPU6050_RANGE_2_G);
  mpu.setGyroRange(MPU6050_RANGE_250_DEG);
  mpu.setFilterBandwidth(MPU6050_BAND_94_HZ);
  Serial.println("[OK] MPU6050 ready");

  WiFi.disconnect(true);
  delay(100);
  WiFi.mode(WIFI_STA);
  WiFi.begin(SSID, PASSWORD);
  Serial.print("[WiFi] Connecting");
  unsigned long t0 = millis();
  while (WiFi.status() != WL_CONNECTED) {
    if (millis() - t0 > 30000) {
      Serial.println("\n[ERROR] Timeout. Restarting.");
      ESP.restart();
    }
    delay(500);
    Serial.print(".");
  }
  Serial.print("\n[OK] Connected → http://");
  Serial.println(WiFi.localIP());

  server.on("/",    handleRoot);
  server.on("/fft", handleFFT);
  server.begin();
  Serial.println("[OK] Server started");
  Serial.println("==============================\n");
}

//  LOOP
// ═══════════════════════════════════════════════════════════════════════════
void loop() {
  collectAndComputeFFT();   // ~512 ms — collect + FFT
  server.handleClient();    // respond to any browser request
}
