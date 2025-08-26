#include <Arduino.h>
#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <AsyncTCP.h>
#include <Adafruit_NeoPixel.h>
#include <ArduinoJson.h>

// WiFi cấu hình
const char* ssid = "BeKindHome";
const char* password = "Bekind.vn";

// LED cấu hình
#define LED_PIN    48
#define LED_COUNT  1
Adafruit_NeoPixel led(LED_COUNT, LED_PIN, NEO_GRB + NEO_KHZ800);

// Webserver
AsyncWebServer server(80);
AsyncWebSocket ws("/ws");

// HTML giao diện (giữ nguyên như bạn đã có)
extern const char index_html[] PROGMEM;

// trạng thái LED
struct {
  uint8_t r = 0, g = 255, b = 0;
  uint8_t brightness = 255;
  bool on = true;
  String effect = "none";
  unsigned long effectSpeed = 800;
} state;

// =================== LED HELPER ===================
void applyColor(uint8_t r, uint8_t g, uint8_t b) {
  if (!state.on) {
    led.setPixelColor(0, led.Color(0,0,0));
  } else {
    uint8_t rr = map(state.brightness, 0, 255, 0, r);
    uint8_t gg = map(state.brightness, 0, 255, 0, g);
    uint8_t bb = map(state.brightness, 0, 255, 0, b);
    led.setPixelColor(0, led.Color(rr,gg,bb));
  }
  led.show();
}

void setColor(uint8_t r, uint8_t g, uint8_t b) {
  state.r = r; state.g = g; state.b = b;
  applyColor(r,g,b);
}

// =================== GỬI STATE VỀ WEB ===================
void broadcastState() {
  StaticJsonDocument<256> doc;
  doc["r"] = state.r;
  doc["g"] = state.g;
  doc["b"] = state.b;
  doc["brightness"] = state.brightness;
  doc["on"] = state.on;
  doc["effect"] = state.effect;
  doc["speed"] = state.effectSpeed;
  String json;
  serializeJson(doc, json);
  ws.textAll(json);
}

// =================== WEBSOCKET HANDLER ===================
void handleWsMessage(void *arg, uint8_t *data, size_t len) {
  DynamicJsonDocument doc(512);
  DeserializationError err = deserializeJson(doc, data, len);
  if (err) return;

  String cmd = doc["cmd"] | "";
  if (cmd == "setColor") {
    String hex = doc["color"];
    long number = strtol(hex.c_str(), NULL, 16);
    uint8_t r = (number >> 16) & 0xFF;
    uint8_t g = (number >> 8) & 0xFF;
    uint8_t b = number & 0xFF;
    setColor(r,g,b);
    state.effect = "none";
  }
  else if (cmd == "setRGB") {
    setColor(doc["r"], doc["g"], doc["b"]);
    state.effect = "none";
  }
  else if (cmd == "toggle") {
    state.on = !state.on;
    applyColor(state.r, state.g, state.b);
  }
  else if (cmd == "setBrightness") {
    state.brightness = doc["brightness"];
    applyColor(state.r, state.g, state.b);
  }
  else if (cmd == "setEffect") {
    state.effect = String((const char*)doc["effect"]);
    state.effectSpeed = doc["speed"] | 800;
  }
  else if (cmd == "getState") {
    // chỉ trả về state
  }

  broadcastState();
}

void onWsEvent(AsyncWebSocket *server, AsyncWebSocketClient *client,
               AwsEventType type, void *arg, uint8_t *data, size_t len) {
  if (type == WS_EVT_DATA) handleWsMessage(arg,data,len);
}

const char index_html[] PROGMEM = R"rawliteral(
<!doctype html>
<html lang="vi">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>ESP32 RGB — Pro Dashboard</title>
<style>
  :root{
    --bg: #0f1724;
    --card: #0b1220;
    --accent: #00c2ff;
    --muted: #9aa6b2;
  }
  *{box-sizing:border-box}
  body{
    margin:0; font-family:Inter, system-ui, -apple-system, "Segoe UI", Roboto, "Helvetica Neue", Arial;
    background: linear-gradient(180deg,#071023 0%, #6b768dff 60%);
    color:#e6eef6;
    -webkit-font-smoothing:antialiased;
  }
  header{
    display:flex; align-items:center; gap:16px;
    padding:18px 28px; background:linear-gradient(90deg, rgba(255,255,255,0.03), transparent);
    border-bottom:1px solid rgba(255,255,255,0.03);
  }
  header h1{margin:0;font-size:18px; letter-spacing:0.4px}
  .wrap{max-width:1100px;margin:24px auto;padding:16px}
  .grid{display:grid;grid-template-columns:1fr 360px;gap:18px}
  .card{background:linear-gradient(180deg, rgba(255,255,255,0.02), rgba(255,255,255,0.01));
       border-radius:12px;padding:18px;border:1px solid rgba(255,255,255,0.03)}
  .panel-title{font-weight:600;margin-bottom:12px;color:#dff6ff}
  /* preview */
  .preview-wrap{display:flex;flex-direction:column;align-items:center;gap:12px}
  .preview{width:220px;height:220px;border-radius:18px;border:1px solid rgba(255,255,255,0.04);
           box-shadow:0 10px 30px rgba(0,0,0,0.6);display:flex;align-items:center;justify-content:center;font-weight:700}
  .controls{display:flex;flex-direction:column;gap:12px}
  .row{display:flex;gap:10px;align-items:center}
  input[type="color"]{width:72px;height:72px;border-radius:12px;border:none;padding:0;background:#fff;cursor:pointer}
  .hex{background:rgba(255,255,255,0.02);border-radius:8px;padding:8px 10px;border:1px solid rgba(255,255,255,0.02);min-width:110px;color:var(--muted)}
  .btn{padding:10px 16px;border-radius:12px;border:none;cursor:pointer;font-weight:700}
  .btn-send{background:linear-gradient(90deg,#00d7ff,#0077ff);color:#002032;box-shadow:0 8px 18px rgba(0,119,255,0.18)}
  .btn-toggle{background:transparent;border:1px solid rgba(255,255,255,0.06);color:#e6eef6}
  .sliders label{font-size:13px;color:var(--muted);margin-bottom:6px}
  input[type="range"]{width:100%}
  .preset-grid{display:flex;flex-wrap:wrap;gap:8px}
  .preset{width:42px;height:42px;border-radius:8px;border:1px solid rgba(255,255,255,0.03);cursor:pointer;box-shadow:0 6px 18px rgba(0,0,0,0.6)}
  .effects{display:flex;flex-wrap:wrap;gap:8px}
  .effect{padding:8px 12px;border-radius:10px;border:1px solid rgba(255,255,255,0.03);cursor:pointer;background:transparent}
  .small{font-size:12px;color:var(--muted)}
  footer{margin-top:20px;text-align:center;color:var(--muted);font-size:12px}
  @media (max-width:900px){
    .grid{grid-template-columns:1fr; padding:0 12px}
    .preview{width:160px;height:160px}
  }
</style>
</head>
<body>
<header>
  <h1>ESP32 RGB — Pro Dashboard</h1>
  <div style="margin-left:auto" id="status" class="small">connecting...</div>
</header>

<div class="wrap">
  <div class="grid">
    <div class="card">
      <div class="panel-title">Live Preview</div>
      <div class="preview-wrap">
        <div id="preview" class="preview">LED</div>
        <div class="small">HEX: <span id="hex">#00ff00</span> · Brightness: <span id="bVal">100%</span></div>
      </div>

      <hr style="margin:14px 0;border:none;border-top:1px solid rgba(255,255,255,0.02)">

      <div class="controls">
        <div style="display:flex;gap:12px;align-items:center">
          <input type="color" id="colorPicker" value="#00ff00">
          <div class="hex" id="hexInput" contenteditable="true">#00ff00</div>
          <button class="btn btn-send" id="sendBtn">Send</button>
          <button class="btn btn-toggle" id="toggleBtn">ON</button>
        </div>

        <div style="display:flex;gap:12px;margin-top:6px">
          <div style="flex:1">
            <label class="small">Brightness</label>
            <input type="range" id="brightness" min="0" max="100" value="100">
          </div>
          <div style="width:120px">
            <label class="small">Auto-send</label>
            <select id="autosend" style="width:100%;padding:8px;border-radius:8px;background:transparent;border:1px solid rgba(255,255,255,0.03)">
              <option value="on">On (live)</option>
              <option value="off" selected>Off (manual)</option>
            </select>
          </div>
        </div>

        <div style="margin-top:12px">
          <div class="row">
            <div style="flex:1">
              <label class="small">R: <span id="rVal">0</span></label>
              <input type="range" id="r" min="0" max="255" value="0">
            </div>
            <div style="flex:1">
              <label class="small">G: <span id="gVal">255</span></label>
              <input type="range" id="g" min="0" max="255" value="255">
            </div>
            <div style="flex:1">
              <label class="small">B: <span id="bVal">0</span></label>
              <input type="range" id="b" min="0" max="255" value="0">
            </div>
          </div>
        </div>

        <div style="margin-top:14px">
          <div class="panel-title">Presets</div>
          <div class="preset-grid" id="presets">
            <!-- javascript will populate -->
          </div>
        </div>

        <div style="margin-top:14px">
          <div class="panel-title">Effects</div>
          <div class="effects">
            <button class="effect" data-effect="none">None</button>
            <button class="effect" data-effect="fade">Fade</button>
            <button class="effect" data-effect="breathe">Breathe</button>
            <button class="effect" data-effect="blink">Blink</button>
            <button class="effect" data-effect="rainbow">Rainbow</button>
          </div>
          <div style="margin-top:8px" class="small">Speed:
            <input type="range" id="effectSpeed" min="100" max="2000" value="800" style="vertical-align:middle;width:220px">
            <span id="speedVal">800ms</span>
          </div>
        </div>
      </div>
      <footer>Pro Dashboard · WebSocket realtime control</footer>
    </div>

    <div class="card">
      <div class="panel-title">Advanced / Info</div>
      <div class="small" style="margin-bottom:10px">Realtime connection, state sync, and non-blocking effects implemented on ESP32.</div>
      <div style="margin-bottom:8px"><strong>Connection:</strong> <span id="connState">disconnected</span></div>
      <div style="margin-bottom:8px"><strong>Last update:</strong> <span id="lastUpdate">-</span></div>
      <div style="margin-bottom:8px"><strong>Current effect:</strong> <span id="currEffect">none</span></div>
      <hr style="margin:12px 0;border:none;border-top:1px solid rgba(255,255,255,0.02)">
      <div class="panel-title">Quick actions</div>
      <div style="display:flex;gap:8px;flex-wrap:wrap">
        <button class="btn" onclick="sendPreset('ff0000')">Red</button>
        <button class="btn" onclick="sendPreset('00ff00')">Green</button>
        <button class="btn" onclick="sendPreset('0000ff')">Blue</button>
        <button class="btn" onclick="sendPreset('ffffff')">White</button>
        <button class="btn" onclick="wsSend({cmd:'getState'})">Refresh State</button>
      </div>
    </div>
  </div>
</div>

<script>
  // Utils
  function pad(n){ return n.toString(16).padStart(2,'0'); }
  function rgbToHex(r,g,b){ return '#'+pad(r)+pad(g)+pad(b); }
  function hexToRgb(hex){ let i = parseInt(hex.replace('#',''),16); return [(i>>16)&255,(i>>8)&255,i&255]; }

  // DOM
  const colorPicker = document.getElementById('colorPicker');
  const hexInput = document.getElementById('hexInput');
  const preview = document.getElementById('preview');
  const rS = document.getElementById('r'), gS = document.getElementById('g'), bS = document.getElementById('b');
  const rVal = document.getElementById('rVal'), gVal = document.getElementById('gVal'), bVal = document.getElementById('bVal');
  const sendBtn = document.getElementById('sendBtn'), toggleBtn=document.getElementById('toggleBtn');
  const brightness = document.getElementById('brightness');
  const autosend = document.getElementById('autosend');
  const presetsEl = document.getElementById('presets');
  const effects = document.querySelectorAll('.effect');
  const effectSpeed = document.getElementById('effectSpeed'), speedVal = document.getElementById('speedVal');
  const status = document.getElementById('status');
  const connState = document.getElementById('connState');
  const lastUpdate = document.getElementById('lastUpdate');
  const currEffect = document.getElementById('currEffect');
  const hexSpan = document.getElementById('hex');
  let autoSend = false;
  let ws;

  // presets list
  const PRESETS = ['ff3b30','ff9500','ffcc00','34c759','007aff','5856d6','ff2d55','ffffff','000000','00ffd1'];

  function initPresets(){
    PRESETS.forEach(h=>{
      const b = document.createElement('button');
      b.className='preset';
      b.style.background = '#'+h;
      b.onclick = ()=>{ setFromHex('#'+h); sendPreset(h); };
      presetsEl.appendChild(b);
    });
  }

  function setFromHex(hex){
    colorPicker.value = hex;
    hexInput.innerText = hex;
    const [r,g,b] = hexToRgb(hex);
    rS.value=r; gS.value=g; bS.value=b; rVal.innerText=r; gVal.innerText=g; bVal.innerText=b;
    preview.style.background = hex;
    hexSpan.innerText = hex;
    document.getElementById('bVal').innerText = Math.round((brightness.value/100)*100)+'%';
  }

  // handle sliders change
  function updateFromSliders(send=false){
    const r = +rS.value, g=+gS.value, b=+bS.value;
    rVal.innerText=r; gVal.innerText=g; bVal.innerText=b;
    const hex = rgbToHex(r,g,b);
    setFromHex(hex);
    if(autoSend || send) sendRGB(r,g,b);
  }

  colorPicker.addEventListener('input', ()=>{ setFromHex(colorPicker.value); if(autoSend) sendHex(colorPicker.value); });
  hexInput.addEventListener('input', ()=>{ let v = hexInput.innerText.trim(); if(/^#([0-9A-Fa-f]{6})$/.test(v)){ setFromHex(v); if(autoSend) sendHex(v); }});
  rS.addEventListener('input', ()=> updateFromSliders(autoSend));
  gS.addEventListener('input', ()=> updateFromSliders(autoSend));
  bS.addEventListener('input', ()=> updateFromSliders(autoSend));
  brightness.addEventListener('input', ()=>{ document.getElementById('bVal').innerText = brightness.value+'%'; if(autoSend) sendBrightness(); });
  autosend.addEventListener('change', ()=> autoSend = autosend.value==='on');

  sendBtn.addEventListener('click', ()=> sendHex(colorPicker.value));
  toggleBtn.addEventListener('click', ()=> wsSend({cmd:'toggle'}) );

  effectSpeed.addEventListener('input', ()=>{ speedVal.innerText = effectSpeed.value+'ms'; });

  effects.forEach(el=>{
    el.addEventListener('click', ()=>{
      effects.forEach(x=>x.style.borderColor='rgba(255,255,255,0.03)');
      el.style.borderColor = '#00d7ff';
      const eff = el.dataset.effect;
      currEffect.innerText = eff;
      wsSend({cmd:'setEffect', effect: eff, speed: +effectSpeed.value});
    });
  });

  function sendHex(hex){
    const clean = hex.replace('#','');
    wsSend({cmd:'setColor', color: clean});
  }
  function sendRGB(r,g,b){
    wsSend({cmd:'setRGB', r:r, g:g, b:b});
  }
  function sendBrightness(){
    wsSend({cmd:'setBrightness', brightness: Math.round((+brightness.value/100)*255)});
  }
  function sendPreset(hex){
    // hex string passed without #
    wsSend({cmd:'setColor', color: hex});
  }

  // WebSocket
  function startWS(){
    let loc = window.location;
    let wsProtocol = (loc.protocol === 'https:') ? 'wss:' : 'ws:';
    let url = wsProtocol + '//' + loc.host + '/ws';
    ws = new WebSocket(url);
    ws.onopen = ()=>{
      connState.innerText='connected';
      status.innerText = 'connected';
      wsSend({cmd:'getState'});
    };
    ws.onmessage = (evt)=>{
      try {
        const d = JSON.parse(evt.data);
        if(d.r !== undefined){
          setFromHex(rgbToHex(d.r,d.g,d.b));
          brightness.value = Math.round((d.brightness/255)*100);
          document.getElementById('bVal').innerText = brightness.value+'%';
          toggleBtn.innerText = d.on ? 'ON' : 'OFF';
          currEffect.innerText = d.effect ? d.effect : 'none';
          lastUpdate.innerText = new Date().toLocaleTimeString();
        }
      } catch(e){ console.warn(e); }
    };
    ws.onclose = ()=>{
      connState.innerText='disconnected';
      status.innerText = 'disconnected';
      setTimeout(startWS, 1500); // retry
    };
    ws.onerror = (e)=>{ console.warn('ws err', e); }
  }

  function wsSend(obj){
    if(ws && ws.readyState===1){
      ws.send(JSON.stringify(obj));
    }
  }

  // init
  initPresets();
  setFromHex('#00ff00');
  startWS();
  speedVal.innerText = effectSpeed.value+'ms';
</script>
</body>
</html>
)rawliteral";


// =================== SETUP ===================
void setup() {
  Serial.begin(115200);

  WiFi.begin(ssid, password);
  Serial.print("Đang kết nối WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\n✅ WiFi OK!");
  Serial.println(WiFi.localIP());

  led.begin();
  led.show();

  // WebSocket
  ws.onEvent(onWsEvent);
  server.addHandler(&ws);

  // Giao diện chính
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *req){
    req->send_P(200, "text/html", index_html);
  });

  server.begin();
}

// =================== LOOP ===================
unsigned long lastEffect = 0;
int breatheVal = 255;
int breatheDir = -5;

void loop() {
  ws.cleanupClients();

  unsigned long now = millis();
  if (state.effect == "blink") {
    if (now - lastEffect > state.effectSpeed) {
      lastEffect = now;
      state.on = !state.on;
      applyColor(state.r,state.g,state.b);
    }
  }
  else if (state.effect == "fade") {
    if (now - lastEffect > 40) {
      lastEffect = now;
      static int step=0, dir=5;
      step += dir;
      if(step>=255||step<=0) dir*=-1;
      uint8_t r=(state.r*step)/255;
      uint8_t g=(state.g*step)/255;
      uint8_t b=(state.b*step)/255;
      applyColor(r,g,b);
    }
  }
  else if (state.effect == "breathe") {
    if (now - lastEffect > 30) {
      lastEffect = now;
      breatheVal += breatheDir;
      if(breatheVal<=30||breatheVal>=255) breatheDir*=-1;
      uint8_t r=(state.r*breatheVal)/255;
      uint8_t g=(state.g*breatheVal)/255;
      uint8_t b=(state.b*breatheVal)/255;
      applyColor(r,g,b);
    }
  }
  else if (state.effect == "rainbow") {
    if (now - lastEffect > 30) {
      lastEffect = now;
      static uint16_t j=0;
      uint32_t c = led.gamma32(led.ColorHSV(j));
      led.setPixelColor(0,c);
      led.show();
      j+=256;
    }
  }
}
