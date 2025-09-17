#include <Arduino.h>
#include <ModbusMaster.h>
#include <WebServer.h>
#include <WiFi.h>

// ===== WiFi
const char *WIFI_SSID = "YOUR_WIFI_SSID";
const char *WIFI_PASS = "YOUR_WIFI_PASSWORD";

// ===== UART / RS485
#define UART_RX 16
#define UART_TX 17
#define UART_BAUD 115200
#define RS485_DE_RE_PIN 4
#define USE_RS485_DIR false

// ===== Modbus
#define MODBUS_SLAVE_ID 1

// Known regs (stable)
#define REG_SET_VOLT 0x0000   // V*100
#define REG_SET_CURR 0x0001   // A*1000
#define REG_OUT_VOLT 0x0002   // V*100
#define REG_OUT_CURR 0x0003   // A*1000
#define REG_OUT_POWER 0x0004  // W*100
#define REG_OUT_ENABLE 0x0012 // 0/1

// Community/experimental (may differ by FW)
#define REG_MPPT_ENABLE 0x001F // 0/1 (medium confidence)

ModbusMaster node;
WebServer server(80);

// ---------- HTML ----------
const char INDEX_HTML[] PROGMEM = R"HTML(<!doctype html>
<html>
<head>
  <meta charset="utf-8"/>
  <meta name="viewport" content="width=device-width,initial-scale=1"/>
  <title>SK120x Controller</title>
  <style>
    body{margin:20px;font-family:Segoe UI,Arial,sans-serif;background:#f0f0f0}
    h1{margin:.2rem 0;text-align:center}

    /* Panels mimic LabVIEW groups */
    .panel{background:#fff;border:2px solid #999;border-radius:6px;padding:12px;margin:10px;box-shadow:2px 2px 5px rgba(0,0,0,.2)}
    .title{font-weight:bold;background:#ddd;padding:4px 6px;margin:-12px -12px 8px -12px;border-bottom:2px solid #999}
    .row{display:flex;flex-wrap:wrap;gap:16px;justify-content:space-around}

    /* 7-seg style readouts (local only) */
    .seven{font-family:monospace;font-size:2.2em;color:#0c0;background:#000;padding:4px 8px;border-radius:4px;display:inline-block;min-width:110px;text-align:right;letter-spacing:1px}
    .seven.red{color:#f33}

    /* Gauges (simple circular face) */
    .gWrap{display:flex;flex-direction:column;align-items:center;gap:6px}
    .gauge{width:120px;height:120px;border-radius:50%;border:8px solid #666;position:relative;background:#222;color:#fff;display:flex;align-items:center;justify-content:center}
    .gauge span{font-weight:bold;font-size:1.4em}

    /* Buttons / inputs */
    .controls button, .btn{padding:8px 14px;margin:4px;border-radius:6px;cursor:pointer;border:1px solid #bbb;background:#eee}
    .controls button:hover, .btn:hover{background:#e2e2e2}
    input[type=number], input[type=text]{padding:8px;border-radius:6px;border:1px solid #bbb;min-width:130px}

    /* Charts */
    canvas{background:#111;border:1px solid #666;border-radius:4px}

    /* Scanner table */
    .tableWrap{max-height:350px;overflow:auto;border:1px solid #ccc;border-radius:6px}
    table{border-collapse:collapse;width:100%;background:#fff}
    th,td{border-bottom:1px solid #eee;padding:6px 8px;text-align:left;font-size:.95em}
    thead th{position:sticky;top:0;background:#ddd;border-bottom:2px solid #bbb;z-index:1}
    tbody tr:nth-child(even){background:#fafafa}
    .small{font-size:.9em;color:#666}
    .status{margin-left:10px}
    .ok{color:#0a0}.err{color:#a00}
  </style>
</head>
<body>
<h1>SK120x Web Controller</h1>

<!-- Top row: status + setpoints -->
<div class="row">
  <div class="panel" style="flex:1">
    <div class="title">Current Status</div>
    <div class="row">
      <div><b>Set Voltage</b><div id="setV" class="seven">--.--</div></div>
      <div><b>Set Current</b><div id="setA" class="seven">--.---</div></div>
      <div><b>Out Voltage</b><div id="outV" class="seven">--.--</div></div>
      <div><b>Out Current</b><div id="outA" class="seven">--.---</div></div>
      <div><b>Out Power</b><div id="outP" class="seven">--.--</div></div>
    </div>
    <div style="margin-top:10px">
      <span><b>Output:</b> <span id="outState">-</span></span> |
      <span><b>MPPT:</b> <span id="mppt">?</span></span>
    </div>
    <div class="controls">
      <button id="toggleBtn" class="btn">Toggle Output</button>
      <button id="refreshBtn" class="btn">Refresh</button>
      <span id="status" class="status"></span>
    </div>
  </div>

  <div class="panel" style="flex:1">
    <div class="title">Setpoints</div>
    <div class="row">
      <div class="gWrap">
        <div class="gauge"><span id="gaugeV">--</span>&nbsp;V</div>
        <div>
          <input type="number" id="newV" step="0.01" min="0" value="12.00"/>
          <button id="applyV" class="btn">Apply V</button>
        </div>
      </div>
      <div class="gWrap">
        <div class="gauge"><span id="gaugeA">--</span>&nbsp;A</div>
        <div>
          <input type="number" id="newA" step="0.001" min="0" value="1.000"/>
          <button id="applyA" class="btn">Apply A</button>
        </div>
      </div>
      <div class="gWrap">
        <div class="gauge"><span id="gaugeMPPT">MPPT</span></div>
        <div>
          <input type="number" id="mpptVal" step="1" min="0" max="1" value="0"/>
          <button id="applyMPPT" class="btn">Set MPPT</button>
        </div>
      </div>
    </div>
  </div>
</div>

<!-- Middle row: history + histogram -->
<div class="row">
  <div class="panel" style="flex:1">
    <div class="title">Temperature/Voltage History (style)</div>
    <canvas id="chartHistory" height="200"></canvas>
  </div>
  <div class="panel" style="flex:1">
    <div class="title">Histogram</div>
    <canvas id="chartHist" height="200"></canvas>
  </div>
</div>

<!-- Bottom row: REGISTER SCANNER -->
<div class="panel">
  <div class="title">Register Scanner</div>
  <div class="row" style="align-items:end">
    <div>
      <label>Start (hex or dec)</label><br/>
      <input type="text" id="scanStart" value="0x0000"/>
    </div>
    <div>
      <label>End (hex or dec)</label><br/>
      <input type="text" id="scanEnd" value="0x0080"/>
    </div>
    <div>
      <button id="doScan" class="btn">Scan</button>
      <button id="exportCSV" class="btn">Export CSV</button>
    </div>
    <div class="small" style="flex:1">
      Shows raw and known interpretations. Use small ranges for fast results.
    </div>
  </div>

  <div class="tableWrap" style="margin-top:10px">
    <table id="scanTable">
      <thead>
        <tr>
          <th>Address (hex)</th>
          <th>Address (dec)</th>
          <th>Raw (dec)</th>
          <th>Known</th>
          <th>Interpreted</th>
        </tr>
      </thead>
      <tbody></tbody>
    </table>
  </div>
</div>

<script>
/* -------- Helpers / UI -------- */
function setText(id,val,dp,alertLimit){
  const el=document.getElementById(id);
  if(val==null||isNaN(val)){ el.textContent='--'; el.classList.remove('red'); return; }
  el.textContent=Number(val).toFixed(dp);
  if(alertLimit && val>alertLimit) el.classList.add('red'); else el.classList.remove('red');
}
function showStatus(msg,ok=true){
  const el=document.getElementById('status');
  el.textContent=msg; el.className='status ' + (ok?'ok':'err');
  setTimeout(()=>{el.textContent=''; el.className='status';},1500);
}

/* -------- Minimal local charting (no CDN) -------- */
class LineChart {
  constructor(canvas, opts){
    this.c = canvas; this.ctx = canvas.getContext('2d');
    this.volts=[]; this.amps=[];
    this.maxPoints = (opts && opts.maxPoints) || 50;
  }
  add(v,a){
    this.volts.push(v); this.amps.push(a);
    if(this.volts.length>this.maxPoints){ this.volts.shift(); this.amps.shift(); }
  }
  draw(){
    const ctx=this.ctx, w=this.c.width, h=this.c.height;
    const DPR = window.devicePixelRatio||1;
    if(this.c.style.width===''){ this.c.style.width = w+'px'; }
    // scale canvas for HiDPI
    if(this.c.width !== this.c.clientWidth*DPR){ this.c.width=this.c.clientWidth*DPR; this.c.height=this.c.clientHeight*DPR; }
    const padL=35, padR=10, padT=10, padB=18;
    const W=this.c.width, H=this.c.height;
    ctx.fillStyle='#111'; ctx.fillRect(0,0,W,H);
    // axes
    ctx.strokeStyle='#555'; ctx.lineWidth=1;
    ctx.beginPath(); ctx.moveTo(padL, padT); ctx.lineTo(padL, H-padB); ctx.lineTo(W-padR, H-padB); ctx.stroke();

    const n=this.volts.length;
    if(!n) return;

    const vMin=Math.min(...this.volts), vMax=Math.max(...this.volts);
    const aMin=Math.min(...this.amps),  aMax=Math.max(...this.amps);
    const yMin=Math.min(vMin, aMin), yMax=Math.max(vMax, aMax);
    const range = (yMax-yMin)||1;

    // grid
    ctx.strokeStyle='#333';
    for(let i=0;i<=4;i++){
      const y = padT + (H-padT-padB)*i/4;
      ctx.beginPath(); ctx.moveTo(padL,y); ctx.lineTo(W-padR,y); ctx.stroke();
    }

    const x0=padL, x1=W-padR, y0=H-padB, y1=padT;
    const toXY = (idx,val) => {
      const x = x0 + (x1-x0) * (idx/(Math.max(1,n-1)));
      const y = y0 - (y0-y1) * ((val - yMin)/range);
      return [x,y];
    };

    // volts line (yellow)
    ctx.lineWidth=2; ctx.strokeStyle='yellow'; ctx.beginPath();
    for(let i=0;i<n;i++){ const [x,y]=toXY(i,this.volts[i]); if(i) ctx.lineTo(x,y); else ctx.moveTo(x,y); }
    ctx.stroke();

    // amps line (cyan)
    ctx.lineWidth=2; ctx.strokeStyle='cyan'; ctx.beginPath();
    for(let i=0;i<n;i++){ const [x,y]=toXY(i,this.amps[i]); if(i) ctx.lineTo(x,y); else ctx.moveTo(x,y); }
    ctx.stroke();

    // legend
    ctx.fillStyle='#ddd'; ctx.font = `${12*(window.devicePixelRatio||1)}px sans-serif`;
    ctx.fillText('Voltage', padL+6, padT+14);
    ctx.fillText('Current', padL+80, padT+14);
  }
}

class BarChart {
  constructor(canvas){
    this.c=canvas; this.ctx=canvas.getContext('2d');
    this.bins = new Map(); // key -> count
  }
  addVoltage(v){
    const k = Math.round(v);
    this.bins.set(k, (this.bins.get(k)||0)+1);
  }
  draw(){
    const ctx=this.ctx, DPR=window.devicePixelRatio||1;
    if(this.c.width !== this.c.clientWidth*DPR){ this.c.width=this.c.clientWidth*DPR; this.c.height=this.c.clientHeight*DPR; }
    const W=this.c.width, H=this.c.height, padL=35, padR=10, padT=10, padB=20;
    ctx.fillStyle='#111'; ctx.fillRect(0,0,W,H);
    ctx.strokeStyle='#555'; ctx.lineWidth=1;
    ctx.beginPath(); ctx.moveTo(padL,padT); ctx.lineTo(padL,H-padB); ctx.lineTo(W-padR,H-padB); ctx.stroke();
    const keys=[...this.bins.keys()].sort((a,b)=>a-b);
    if(!keys.length) return;
    const maxCount = Math.max(...keys.map(k=>this.bins.get(k)));
    const bw = (W-padL-padR)/keys.length;
    keys.forEach((k,i)=>{
      const count=this.bins.get(k);
      const x = padL + i*bw + 2;
      const h = (H-padT-padB) * (count/maxCount);
      const y = (H-padB) - h;
      ctx.fillStyle='lime';
      ctx.fillRect(x, y, Math.max(1,bw-4), h);
      // x labels sparse
      if(i%Math.ceil(keys.length/8)===0){
        ctx.fillStyle='#ddd'; ctx.font = `${10*DPR}px sans-serif`;
        ctx.fillText(String(k), x, H-6);
      }
    });
    // y label
    ctx.fillStyle='#ddd'; ctx.font = `${12*DPR}px sans-serif`;
    ctx.fillText('Voltage bins', padL+6, padT+14);
  }
}

/* -------- Status fetch -------- */
async function getStatus(){
  try{
    const r = await fetch('/api/status'); const j = await r.json(); if(!j.ok) throw new Error();
    setText('setV',j.setV,2); setText('setA',j.setA,3);
    setText('outV',j.outV,2,35); setText('outA',j.outA,3);
    setText('outP',j.outP,2);

    document.getElementById('gaugeV').textContent = isFinite(j.setV) ? j.setV.toFixed(1) : '--';
    document.getElementById('gaugeA').textContent = isFinite(j.setA) ? j.setA.toFixed(2) : '--';
    document.getElementById('gaugeMPPT').textContent = (j.mppt===null)?'MPPT':'MPPT ' + (j.mppt?'ON':'OFF');

    document.getElementById('outState').textContent = j.outputOn?'ON':'OFF';
    document.getElementById('mppt').textContent = (j.mppt===null)?'n/a':(j.mppt?'ON':'OFF');

    addHistory(j.outV,j.outA);
  }catch(e){}
}
async function writeReg(reg,val){ const r=await fetch(`/api/write?reg=${reg}&val=${val}`,{method:'POST'}); return r.json(); }
async function setVoltage(v){ return writeReg(0, Math.round(v*100)); }
async function setCurrent(a){ return writeReg(1, Math.round(a*1000)); }
async function setOutput(on){ return writeReg(0x12, on?1:0); }
async function setMPPT(on){ return writeReg(0x1F, on?1:0); }

document.getElementById('refreshBtn').onclick=()=>getStatus();
document.getElementById('toggleBtn').onclick=async()=>{
  const state = document.getElementById('outState').textContent.trim()==='ON';
  const r = await setOutput(!state); showStatus(r.ok?'Output toggled':(r.msg||'Error'), r.ok); getStatus();
};
document.getElementById('applyV').onclick=async()=>{
  const v=parseFloat(document.getElementById('newV').value); if(isNaN(v)) return showStatus('Bad voltage',false);
  const r=await setVoltage(v); showStatus(r.ok?'Voltage set':(r.msg||'Error'), r.ok); getStatus();
};
document.getElementById('applyA').onclick=async()=>{
  const a=parseFloat(document.getElementById('newA').value); if(isNaN(a)) return showStatus('Bad current',false);
  const r=await setCurrent(a); showStatus(r.ok?'Current set':(r.msg||'Error'), r.ok); getStatus();
};
document.getElementById('applyMPPT').onclick=async()=>{
  const v=parseInt(document.getElementById('mpptVal').value); if(isNaN(v)||v<0||v>1) return showStatus('MPPT must be 0 or 1',false);
  const r=await setMPPT(v===1); showStatus(r.ok?'MPPT set':(r.msg||'Error'), r.ok); getStatus();
};

/* -------- Charts: local renderers -------- */
const histCanvas=document.getElementById('chartHistory');
const barCanvas=document.getElementById('chartHist');
const historyChart = new LineChart(histCanvas,{maxPoints:50});
const voltHist = new BarChart(barCanvas);

function addHistory(v,a){
  if(!isFinite(v)||!isFinite(a)) return;
  historyChart.add(v,a);
  historyChart.draw();
  voltHist.addVoltage(v);
  voltHist.draw();
}

/* -------- Register Scanner -------- */
function toHex(n){return '0x'+('0000'+n.toString(16)).slice(-4).toUpperCase();}
function parseAddr(s){
  s=s.trim();
  if(s.startsWith('0x')||s.startsWith('0X')) return parseInt(s,16);
  return parseInt(s,10);
}
async function doScan(){
  const s=parseAddr(document.getElementById('scanStart').value);
  const e=parseAddr(document.getElementById('scanEnd').value);
  if(!Number.isInteger(s)||!Number.isInteger(e)||e<s){ showStatus('Bad range',false); return; }
  const r=await fetch(`/api/scan?start=${encodeURIComponent(document.getElementById('scanStart').value)}&end=${encodeURIComponent(document.getElementById('scanEnd').value)}`);
  const j=await r.json();
  if(!j.ok){ showStatus(j.msg||'Scan error',false); return; }

  const tbody=document.querySelector('#scanTable tbody');
  tbody.innerHTML='';
  j.rows.forEach(x=>{
    const tr=document.createElement('tr');
    tr.innerHTML = `
      <td>${toHex(x.addr)}</td>
      <td>${x.addr}</td>
      <td>${x.dec}</td>
      <td>${x.meaning||''}</td>
      <td>${x.interpretation||''}</td>`;
    tbody.appendChild(tr);
  });
}
document.getElementById('doScan').onclick=doScan;

/* Export CSV for scanner */
document.getElementById('exportCSV').onclick=()=>{
  const rows=[['addr_hex','addr_dec','raw_dec','known','interpreted']];
  document.querySelectorAll('#scanTable tbody tr').forEach(tr=>{
    const cols=[...tr.children].map(td=>td.textContent.replaceAll(',', ''));
    rows.push(cols);
  });
  const csv=rows.map(r=>r.join(',')).join('\n');
  const blob=new Blob([csv],{type:'text/csv'});
  const a=document.createElement('a');
  a.href=URL.createObjectURL(blob);
  a.download='sk120x_scan.csv';
  a.click();
  URL.revokeObjectURL(a.href);
};

/* -------- Kickoff -------- */
setInterval(getStatus,1000);
getStatus();
</script>
</body>
</html>
)HTML";


// ---------- RS485 Direction ----------
void preTransmission() {
#if USE_RS485_DIR
digitalWrite(RS485_DE_RE_PIN, HIGH);
delayMicroseconds(10);
#endif
}
void postTransmission() {
#ifdef USE_RS485_DIR
 delayMicroseconds(10);
  digitalWrite(RS485_DE_RE_PIN, LOW);
#endif
}

// ---------- Helpers ----------
bool mbReadU16(uint16_t reg, uint16_t &out) {
  uint8_t rc = node.readHoldingRegisters(reg, 1);
  if (rc == node.ku8MBSuccess) {
    out = node.getResponseBuffer(0);
    node.clearResponseBuffer();
    return true;
  }
  return false;
}
bool mbWriteU16(uint16_t reg, uint16_t val) {
  uint8_t rc = node.writeSingleRegister(reg, val);
  return rc == node.ku8MBSuccess;
}
static bool parseNum(const String &s, uint16_t &out) {
  char *end = nullptr;
  uint32_t v = (s.startsWith("0x") || s.startsWith("0X"))
                   ? strtoul(s.c_str(), &end, 16)
                   : strtoul(s.c_str(), &end, 10);
  if (end == s.c_str())
    return false;
  out = (uint16_t)v;
  return true;
}

// ---------- HTTP ----------
void sendJSON(const String &s) { server.send(200, "application/json", s); }

void handleIndex() { server.send_P(200, "text/html", INDEX_HTML); }

void handleStatus() {
  uint16_t sV = 0, sA = 0, oV = 0, oA = 0, oP = 0, outE = 0, mppt = 0;
  bool ok = true;
  ok &= mbReadU16(REG_SET_VOLT, sV);
  ok &= mbReadU16(REG_SET_CURR, sA);
  ok &= mbReadU16(REG_OUT_VOLT, oV);
  ok &= mbReadU16(REG_OUT_CURR, oA);
  ok &= mbReadU16(REG_OUT_POWER, oP);
  bool mpptOk = mbReadU16(REG_MPPT_ENABLE, mppt); // experimental; may fail

  String json = "{";
  json += "\"ok\":";
  json += ok ? "true" : "false";
  json += ",";
  json += "\"setV\":";
  json += String(sV / 100.0f, 2);
  json += ",";
  json += "\"setA\":";
  json += String(sA / 1000.0f, 3);
  json += ",";
  json += "\"outV\":";
  json += String(oV / 100.0f, 2);
  json += ",";
  json += "\"outA\":";
  json += String(oA / 1000.0f, 3);
  json += ",";
  json += "\"outP\":";
  json += String(oP / 100.0f, 2);
  json += ",";
  if (mbReadU16(REG_OUT_ENABLE, outE)) {
    json += "\"outputOn\":";
    json += (outE == 1 ? "true" : "false");
    json += ",";
  } else {
    json += "\"outputOn\":null,";
  }
  if (mpptOk) {
    json += "\"mppt\":";
    json += (mppt ? "true" : "false");
  } else {
    json += "\"mppt\":null";
  }
  json += "}";
  sendJSON(json);
}

void handleWrite() {
  if (!server.hasArg("reg") || !server.hasArg("val")) {
    server.send(400, "application/json",
                "{\"ok\":false,\"msg\":\"reg & val required\"}");
    return;
  }
  uint16_t reg = (uint16_t)strtoul(server.arg("reg").c_str(), nullptr, 0);
  uint16_t val = (uint16_t)strtoul(server.arg("val").c_str(), nullptr, 0);
  bool ok = mbWriteU16(reg, val);
  server.send(ok ? 200 : 500, "application/json",
              String("{\"ok\":") +
                  (ok ? "true}" : "false,\"msg\":\"write failed\"}"));
}

String meaningFor(uint16_t addr) {
  switch (addr) {
  case REG_SET_VOLT:
    return "Vset (V*100)";
  case REG_SET_CURR:
    return "Iset (A*1000)";
  case REG_OUT_VOLT:
    return "Vout (V*100)";
  case REG_OUT_CURR:
    return "Iout (A*1000)";
  case REG_OUT_POWER:
    return "Pout (W*100)";
  case REG_OUT_ENABLE:
    return "Output Enable (0/1)";
  case REG_MPPT_ENABLE:
    return "MPPT Enable (0/1) [exp]";
  default:
    return "";
  }
}

String interpretFor(uint16_t addr, uint16_t raw) {
  switch (addr) {
  case REG_SET_VOLT:
  case REG_OUT_VOLT:
    return String(raw / 100.0f, 2) + " V";
  case REG_SET_CURR:
  case REG_OUT_CURR:
    return String(raw / 1000.0f, 3) + " A";
  case REG_OUT_POWER:
    return String(raw / 100.0f, 2) + " W";
  case REG_OUT_ENABLE:
    return raw ? "ON" : "OFF";
  case REG_MPPT_ENABLE:
    return raw ? "ON" : "OFF";
  default:
    return "";
  }
}

void handleScan() {
  uint16_t start = 0x0000, end = 0x0080;
  if (server.hasArg("start")) {
    if (!parseNum(server.arg("start"), start)) {
      server.send(400, "application/json",
                  "{\"ok\":false,\"msg\":\"bad start\"}");
      return;
    }
  }
  if (server.hasArg("end")) {
    if (!parseNum(server.arg("end"), end)) {
      server.send(400, "application/json",
                  "{\"ok\":false,\"msg\":\"bad end\"}");
      return;
    }
  }
  if (end < start || (end - start) > 256) {
    server.send(400, "application/json",
                "{\"ok\":false,\"msg\":\"range too large\"}");
    return;
  }

  String json = "{\"ok\":true,\"rows\":[";
  bool first = true;
  for (uint16_t reg = start; reg <= end; reg++) {
    uint16_t v = 0;
    uint8_t rc = node.readHoldingRegisters(reg, 1);
    if (rc == node.ku8MBSuccess) {
      v = node.getResponseBuffer(0);
      node.clearResponseBuffer();
      if (!first)
        json += ",";
      first = false;
      String m = meaningFor(reg);
      String i = interpretFor(reg, v);
      json += "{\"addr\":" + String(reg) + ",\"dec\":" + String(v);
      if (m.length()) {
        json += ",\"meaning\":\"" + m + "\"";
      }
      if (i.length()) {
        json += ",\"interpretation\":\"" + i + "\"";
      }
      json += "}";
    }
    delay(5);
  }
  json += "]}";
  sendJSON(json);
}

void notFound() { server.send(404, "text/plain", "Not found"); }

void connectWiFi() {
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  Serial.print("Connecting to WiFi");
  unsigned long t0 = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - t0 < 20000) {
    delay(500);
    Serial.print(".");
  }
  Serial.println();
  if (WiFi.status() == WL_CONNECTED) {
    Serial.print("WiFi OK, IP: ");
    Serial.println(WiFi.localIP());
  } else {
    Serial.println("WiFi failed; starting AP 'SK120x-ESP32'...");
    WiFi.softAP("SK120x-ESP32");
    Serial.print("AP IP: ");
    Serial.println(WiFi.softAPIP());
  }
}

void setup() {
  Serial.begin(115200);
  delay(100);
  Serial2.begin(UART_BAUD, SERIAL_8N1, UART_RX, UART_TX);
#if USE_RS485_DIR
  pinMode(RS485_DE_RE_PIN, OUTPUT);
  digitalWrite(RS485_DE_RE_PIN, LOW);
#endif
  node.begin(MODBUS_SLAVE_ID, Serial2);
  node.preTransmission(preTransmission);
  node.postTransmission(postTransmission);

  connectWiFi();
  server.on("/", HTTP_GET, handleIndex);
  server.on("/api/status", HTTP_GET, handleStatus);
  server.on("/api/write", HTTP_POST, handleWrite);
  server.on("/api/scan", HTTP_GET, handleScan);
  server.onNotFound(notFound);
  server.begin();
  Serial.println("HTTP server started.");
}

void loop() { server.handleClient(); }
