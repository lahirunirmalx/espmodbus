const char INDEX_HTML[] PROGMEM = R"INDEX_HTML(
<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="utf-8"/>
<meta name="viewport" content="width=device-width,initial-scale=1"/>
<title>HP Power Supply Controller</title>
<style>
:root{
  /* Classic HP Beige/Tan palette - No orange */
  --hp-beige:#e8e0d0;
  --hp-cream:#f5f0e6;
  --hp-tan:#d4c9b5;
  --hp-brown:#8b7355;
  --hp-dark:#3d3429;
  --hp-border:#b8a888;
  --hp-shadow:#c9bfab;
  --hp-text:#2c2416;
  --hp-accent:#1e5a8a;
  --hp-green:#2d5a27;
  --hp-red:#8b2020;
}
html,body{height:100%;margin:0;background:#d0c8b8;color:var(--hp-text);font:13px/1.4 "Helvetica Neue",Helvetica,Arial,sans-serif}

.app{
  max-width:1280px;
  margin:12px auto;
  padding:16px 20px;
  background:var(--hp-beige);
  border:2px solid var(--hp-border);
  border-radius:3px;
  box-shadow:0 1px 0 var(--hp-cream) inset,0 2px 4px rgba(0,0,0,0.15);
}

/* Header strip like HP instruments */
.header-strip{
  background:linear-gradient(180deg,#4a4035 0%,#3d3429 100%);
  color:#f0e8d8;
  padding:8px 14px;
  margin:-16px -20px 14px -20px;
  border-bottom:3px solid var(--hp-accent);
  display:flex;
  align-items:center;
  justify-content:space-between;
}
.header-strip h1{
  margin:0;
  font-size:16px;
  font-weight:500;
  letter-spacing:1px;
  text-transform:uppercase;
}
.model-badge{
  font-size:11px;
  background:var(--hp-accent);
  color:#fff;
  padding:2px 8px;
  border-radius:2px;
  font-weight:600;
}

.status-chip{
  display:flex;
  align-items:center;
  gap:6px;
  font-size:11px;
  color:#ccc;
}
.status-led{
  width:8px;height:8px;
  border-radius:50%;
  background:#444;
  border:1px solid #222;
}
.status-led.on{background:#5a5;box-shadow:0 0 4px #5a5}
.status-led.err{background:#a33;box-shadow:0 0 4px #a33}

/* Grid layout */
.grid{display:grid;grid-template-columns:1fr 1fr;gap:12px}
@media(max-width:900px){.grid{grid-template-columns:1fr}}

/* Panel sections */
.panel{
  background:var(--hp-cream);
  border:1px solid var(--hp-border);
  border-radius:2px;
  padding:10px 12px;
  box-shadow:inset 0 1px 0 #fff;
}
.panel-title{
  font-size:11px;
  font-weight:600;
  text-transform:uppercase;
  letter-spacing:0.5px;
  color:var(--hp-brown);
  margin:0 0 10px 0;
  padding-bottom:6px;
  border-bottom:1px solid var(--hp-tan);
}

/* Control row */
.ctrl-row{display:flex;gap:10px;margin:6px 0;flex-wrap:wrap;align-items:center}

/* HP Style Buttons */
.hp-btn{
  font:12px/1 "Helvetica Neue",Helvetica,Arial,sans-serif;
  padding:6px 12px;
  border:1px solid var(--hp-border);
  border-radius:2px;
  background:linear-gradient(180deg,#f8f4ec 0%,#e8e0d0 100%);
  color:var(--hp-text);
  cursor:pointer;
  box-shadow:0 1px 0 #fff inset,0 1px 2px rgba(0,0,0,0.1);
}
.hp-btn:hover{background:linear-gradient(180deg,#fff 0%,#f0e8d8 100%)}
.hp-btn:active{background:#ddd8c8;box-shadow:inset 0 1px 2px rgba(0,0,0,0.15)}
.hp-btn.primary{background:var(--hp-accent);color:#fff;border-color:#164570}
.hp-btn.primary:hover{background:#2a6a9a}

/* Toggle switches */
.hp-toggle{
  display:inline-flex;
  align-items:center;
  gap:6px;
  padding:6px 10px;
  border:1px solid var(--hp-border);
  border-radius:2px;
  background:#e0d8c8;
  cursor:pointer;
  font-size:11px;
  font-weight:600;
  text-transform:uppercase;
}
.hp-toggle .ind{
  width:10px;height:10px;
  border-radius:50%;
  background:#666;
  border:1px solid #444;
}
.hp-toggle.on{background:#d8e8d0;border-color:#6a6}
.hp-toggle.on .ind{background:#4a4;box-shadow:0 0 6px #4a4}
.hp-toggle.off .ind{background:#888}

/* ========== VFD DISPLAY SCREEN ========== */
.vfd-screen{
  background:linear-gradient(145deg,#0a0f08 0%,#0d1209 50%,#080c06 100%);
  border:3px solid #1a1a1a;
  border-radius:8px;
  padding:12px 16px;
  box-shadow:
    inset 0 0 30px rgba(0,0,0,0.8),
    inset 0 0 60px rgba(0,20,0,0.3),
    0 2px 8px rgba(0,0,0,0.4);
  position:relative;
  overflow:hidden;
}
/* CRT screen curvature effect */
.vfd-screen::before{
  content:'';
  position:absolute;
  inset:0;
  background:radial-gradient(ellipse at center,transparent 0%,transparent 70%,rgba(0,0,0,0.3) 100%);
  pointer-events:none;
}
/* Scanline effect */
.vfd-screen::after{
  content:'';
  position:absolute;
  inset:0;
  background:repeating-linear-gradient(
    0deg,
    transparent 0px,
    transparent 2px,
    rgba(0,0,0,0.1) 2px,
    rgba(0,0,0,0.1) 4px
  );
  pointer-events:none;
}
.vfd-row{
  display:flex;
  justify-content:space-between;
  align-items:baseline;
  padding:8px 0;
  border-bottom:1px solid rgba(100,180,80,0.15);
}
.vfd-row:last-child{border-bottom:none}
.vfd-label{
  font-size:12px;
  font-weight:700;
  color:#5aff5a;
  text-transform:uppercase;
  letter-spacing:2px;
  text-shadow:
    0 0 4px #5aff5a,
    0 0 8px rgba(90,255,90,0.6),
    0 0 16px rgba(90,255,90,0.3);
}
.vfd-value{
  font-family:"DSEG7 Classic","Consolas","Courier New",monospace;
  font-size:42px;
  font-weight:700;
  color:#7fff5a;
  letter-spacing:3px;
  text-shadow:
    0 0 4px #7fff5a,
    0 0 12px rgba(127,255,90,0.7),
    0 0 24px rgba(127,255,90,0.4),
    0 0 40px rgba(127,255,90,0.2);
  position:relative;
}
/* Ghost digits for VFD effect */
.vfd-value::before{
  content:'88.888';
  position:absolute;
  left:0;top:0;
  color:rgba(60,100,50,0.15);
  text-shadow:none;
  pointer-events:none;
}
.vfd-unit{
  font-size:18px;
  font-weight:700;
  color:#7fff5a;
  margin-left:10px;
  letter-spacing:1px;
  text-shadow:
    0 0 4px #7fff5a,
    0 0 10px rgba(127,255,90,0.7),
    0 0 20px rgba(127,255,90,0.4);
}
/* SET mode indicator - amber/yellow when output is OFF */
.vfd-screen.set-mode .vfd-value{
  color:#ffaa00;
  text-shadow:
    0 0 4px #ffaa00,
    0 0 12px rgba(255,170,0,0.7),
    0 0 24px rgba(255,170,0,0.4),
    0 0 40px rgba(255,170,0,0.2);
}
.vfd-screen.set-mode .vfd-value::before{
  color:rgba(100,80,30,0.15);
}
.vfd-screen.set-mode .vfd-label{
  color:#ffcc44;
  text-shadow:
    0 0 4px #ffcc44,
    0 0 8px rgba(255,204,68,0.6),
    0 0 16px rgba(255,204,68,0.3);
}
.vfd-screen.set-mode .vfd-unit{
  color:#ffaa00;
  text-shadow:
    0 0 4px #ffaa00,
    0 0 10px rgba(255,170,0,0.7),
    0 0 20px rgba(255,170,0,0.4);
}
/* SET mode label badge */
.vfd-screen::after{
  content:'OUTPUT';
  position:absolute;
  top:6px;right:10px;
  font-size:9px;
  font-weight:700;
  color:#5aff5a;
  letter-spacing:1px;
  text-shadow:0 0 4px #5aff5a;
  z-index:5;
}
.vfd-screen.set-mode::after{
  content:'SET';
  color:#ffaa00;
  text-shadow:0 0 4px #ffaa00;
}

/* Knob assembly */
.knob-assembly{display:flex;gap:16px;align-items:center;margin:16px 0 24px 0;padding-left:10px}
.knob-outer{
  width:70px;height:70px;
  background:#d8d0c0;
  border:2px solid #a09080;
  border-radius:50%;
  position:relative;
  box-shadow:0 2px 4px rgba(0,0,0,0.2);
}
/* Tick marks around the knob - generated via SVG background */
.knob-outer::before{
  content:'';
  position:absolute;
  inset:-8px;
  border-radius:50%;
  background-image:url("data:image/svg+xml,%3Csvg xmlns='http://www.w3.org/2000/svg' viewBox='0 0 100 100'%3E%3Cg stroke='%233d3429' stroke-width='2' stroke-linecap='round'%3E%3Cline x1='50' y1='8' x2='50' y2='15' transform='rotate(-135 50 50)'/%3E%3Cline x1='50' y1='8' x2='50' y2='13' transform='rotate(-108 50 50)'/%3E%3Cline x1='50' y1='8' x2='50' y2='13' transform='rotate(-81 50 50)'/%3E%3Cline x1='50' y1='8' x2='50' y2='13' transform='rotate(-54 50 50)'/%3E%3Cline x1='50' y1='8' x2='50' y2='13' transform='rotate(-27 50 50)'/%3E%3Cline x1='50' y1='8' x2='50' y2='15' transform='rotate(0 50 50)'/%3E%3Cline x1='50' y1='8' x2='50' y2='13' transform='rotate(27 50 50)'/%3E%3Cline x1='50' y1='8' x2='50' y2='13' transform='rotate(54 50 50)'/%3E%3Cline x1='50' y1='8' x2='50' y2='13' transform='rotate(81 50 50)'/%3E%3Cline x1='50' y1='8' x2='50' y2='13' transform='rotate(108 50 50)'/%3E%3Cline x1='50' y1='8' x2='50' y2='15' transform='rotate(135 50 50)'/%3E%3C/g%3E%3C/svg%3E");
  background-size:contain;
  pointer-events:none;
}
/* Min/Max labels */
.knob-outer::after{
  content:'MIN                    MAX';
  position:absolute;
  bottom:-18px;
  left:50%;
  transform:translateX(-50%);
  font-size:8px;
  font-weight:600;
  color:var(--hp-brown);
  white-space:nowrap;
  letter-spacing:0.5px;
}
.knob-inner{
  position:absolute;
  inset:6px;
  background:linear-gradient(145deg,#e8e0d0 0%,#c8c0b0 50%,#d0c8b8 100%);
  border-radius:50%;
  border:1px solid #b0a090;
  box-shadow:inset 0 2px 4px rgba(0,0,0,0.1),0 1px 0 rgba(255,255,255,0.5);
}
.knob-pointer{
  position:absolute;
  left:50%;
  top:6px;
  width:4px;
  height:calc(50% - 6px);
  background:linear-gradient(180deg,var(--hp-dark) 0%,#5a4a3a 100%);
  transform-origin:50% 100%;
  transform:translateX(-50%) rotate(0deg);
  border-radius:2px;
  box-shadow:0 1px 2px rgba(0,0,0,0.3);
}
/* Center cap */
.knob-inner::after{
  content:'';
  position:absolute;
  left:50%;top:50%;
  width:12px;height:12px;
  transform:translate(-50%,-50%);
  background:linear-gradient(145deg,#a09080 0%,#706050 100%);
  border-radius:50%;
  border:1px solid #605040;
}

/* Input group */
.input-group{display:flex;flex-direction:column;gap:4px}
.input-group label{font-size:10px;color:#666;text-transform:uppercase;letter-spacing:0.5px}
.input-group input{
  width:80px;
  font:600 14px/1 "Consolas",monospace;
  padding:6px 8px;
  border:1px solid var(--hp-border);
  border-radius:2px;
  background:#fff;
}

/* Analog meter */
.meter-box{
  width:180px;
  height:100px;
  background:#f8f4ec;
  border:1px solid var(--hp-border);
  border-radius:2px;
  position:relative;
  overflow:hidden;
}
.meter-box canvas{position:absolute;left:0;top:0}

/* Analog Oscilloscope Style Graph */
.scope-box{
  background:linear-gradient(145deg,#0a0d08 0%,#0d100a 50%,#080a06 100%);
  border:4px solid #2a2520;
  border-radius:16px;
  padding:8px;
  margin-top:10px;
  position:relative;
  box-shadow:
    inset 0 0 40px rgba(0,0,0,0.9),
    inset 0 0 80px rgba(0,30,0,0.2),
    0 3px 10px rgba(0,0,0,0.5);
}
/* CRT bezel */
.scope-box::before{
  content:'';
  position:absolute;
  inset:4px;
  border-radius:12px;
  background:radial-gradient(ellipse at center,transparent 0%,transparent 60%,rgba(0,0,0,0.4) 100%);
  pointer-events:none;
  z-index:1;
}
/* Glass reflection */
.scope-box::after{
  content:'';
  position:absolute;
  top:6px;left:10%;right:10%;
  height:20%;
  background:linear-gradient(180deg,rgba(255,255,255,0.06) 0%,transparent 100%);
  border-radius:50%;
  pointer-events:none;
  z-index:2;
}
.scope-box canvas{
  width:100%;
  height:120px;
  display:block;
  border-radius:8px;
}
.scope-label{
  position:absolute;
  bottom:4px;right:12px;
  font-size:9px;
  font-weight:600;
  color:#4a5a40;
  letter-spacing:1px;
  text-transform:uppercase;
  z-index:3;
}

/* Table */
.tableWrap{max-height:200px;overflow:auto;border:1px solid var(--hp-border);border-radius:2px;margin-top:8px}
table{border-collapse:collapse;width:100%;font-size:11px;background:#fff}
th,td{border-bottom:1px solid #ddd;padding:4px 8px;text-align:left}
thead th{background:var(--hp-tan);position:sticky;top:0;font-weight:600}
tbody tr:nth-child(even){background:#f8f4ec}

/* Footer */
.footer-bar{
  display:flex;gap:12px;align-items:center;justify-content:space-between;
  margin-top:12px;padding-top:10px;
  border-top:1px solid var(--hp-tan);
  font-size:10px;color:#888;
}
.footer-badge{
  background:var(--hp-cream);
  border:1px solid var(--hp-border);
  padding:3px 8px;
  border-radius:2px;
}
</style>
</head>
<body>
<div class="app">
  <div class="header-strip">
    <div style="display:flex;align-items:center;gap:12px">
      <h1>Hewlett Packard</h1>
      <span class="model-badge">E3631A</span>
    </div>
    <div style="display:flex;align-items:center;gap:16px">
      <span style="font-size:12px">Triple Output DC Power Supply</span>
      <div class="status-chip">
        <span class="status-led" id="sysLed"></span>
        <span id="sysText">STANDBY</span>
      </div>
    </div>
  </div>

  <!-- Global Controls -->
  <div class="panel" style="margin-bottom:10px">
    <div class="panel-title">System Control</div>
    <div class="ctrl-row">
      <button id="linkToggle" class="hp-toggle off"><span class="ind"></span>TRACKING</button>
      <div style="margin-left:auto;display:flex;gap:6px">
        <button id="refreshBtn" class="hp-btn">REFRESH</button>
        <button id="downloadCsv" class="hp-btn">EXPORT</button>
      </div>
    </div>
  </div>

  <!-- Dual Channel -->
  <div class="grid">
    <!-- Channel 1 -->
    <div class="panel">
      <div class="panel-title">Output 1 — +25V / 1A</div>
      <div class="vfd-screen">
        <div class="vfd-row">
          <span class="vfd-label">Voltage</span>
          <span><span class="vfd-value" id="vout1">--.---</span><span class="vfd-unit">V</span></span>
        </div>
        <div class="vfd-row">
          <span class="vfd-label">Current</span>
          <span><span class="vfd-value" id="iout1">--.---</span><span class="vfd-unit">A</span></span>
        </div>
        <div class="vfd-row">
          <span class="vfd-label">Power</span>
          <span><span class="vfd-value" id="pout1">--.--</span><span class="vfd-unit">W</span></span>
        </div>
      </div>
      <div class="ctrl-row" style="margin-top:10px">
        <button id="oe1" class="hp-toggle off"><span class="ind"></span>OUTPUT</button>
        <span class="footer-badge">STATUS: <b id="status1">—</b></span>
      </div>
      <div class="ctrl-row">
        <div class="knob-assembly">
          <div class="knob-outer">
            <div class="knob-inner">
              <div class="knob-pointer" id="kn1v"></div>
            </div>
          </div>
          <div class="input-group">
            <label>Voltage</label>
            <input id="vset1" type="number" step="0.01" min="0" value="12.00">
            <button class="hp-btn primary" id="applyV1">SET</button>
          </div>
        </div>
        <div class="knob-assembly">
          <div class="knob-outer">
            <div class="knob-inner">
              <div class="knob-pointer" id="kn1i"></div>
            </div>
          </div>
          <div class="input-group">
            <label>Current</label>
            <input id="iset1" type="number" step="0.001" min="0" value="1.000">
            <button class="hp-btn primary" id="applyI1">SET</button>
          </div>
        </div>
      </div>
      <div class="ctrl-row" style="gap:16px">
        <div class="meter-box"><canvas id="gV1" width="180" height="100"></canvas></div>
        <div class="meter-box"><canvas id="gI1" width="180" height="100"></canvas></div>
      </div>
      <div class="scope-box">
        <canvas id="lineCh1" width="400" height="120"></canvas>
        <span class="scope-label">CH1 Trace</span>
      </div>
    </div>

    <!-- Channel 2 -->
    <div class="panel">
      <div class="panel-title">Output 2 — +25V / 1A</div>
      <div class="vfd-screen">
        <div class="vfd-row">
          <span class="vfd-label">Voltage</span>
          <span><span class="vfd-value" id="vout2">--.---</span><span class="vfd-unit">V</span></span>
        </div>
        <div class="vfd-row">
          <span class="vfd-label">Current</span>
          <span><span class="vfd-value" id="iout2">--.---</span><span class="vfd-unit">A</span></span>
        </div>
        <div class="vfd-row">
          <span class="vfd-label">Power</span>
          <span><span class="vfd-value" id="pout2">--.--</span><span class="vfd-unit">W</span></span>
        </div>
      </div>
      <div class="ctrl-row" style="margin-top:10px">
        <button id="oe2" class="hp-toggle off"><span class="ind"></span>OUTPUT</button>
        <span class="footer-badge">STATUS: <b id="status2">—</b></span>
      </div>
      <div class="ctrl-row">
        <div class="knob-assembly">
          <div class="knob-outer">
            <div class="knob-inner">
              <div class="knob-pointer" id="kn2v"></div>
            </div>
          </div>
          <div class="input-group">
            <label>Voltage</label>
            <input id="vset2" type="number" step="0.01" min="0" value="12.00">
            <button class="hp-btn primary" id="applyV2">SET</button>
          </div>
        </div>
        <div class="knob-assembly">
          <div class="knob-outer">
            <div class="knob-inner">
              <div class="knob-pointer" id="kn2i"></div>
            </div>
          </div>
          <div class="input-group">
            <label>Current</label>
            <input id="iset2" type="number" step="0.001" min="0" value="1.000">
            <button class="hp-btn primary" id="applyI2">SET</button>
          </div>
        </div>
      </div>
      <div class="ctrl-row" style="gap:16px">
        <div class="meter-box"><canvas id="gV2" width="180" height="100"></canvas></div>
        <div class="meter-box"><canvas id="gI2" width="180" height="100"></canvas></div>
      </div>
      <div class="scope-box">
        <canvas id="lineCh2" width="400" height="120"></canvas>
        <span class="scope-label">CH2 Trace</span>
      </div>
    </div>
  </div>

  <!-- Scanner -->
  <div class="panel" style="margin-top:10px">
    <div class="panel-title">Register Scanner</div>
    <div class="ctrl-row">
      <select id="scanCh" style="padding:4px;border:1px solid var(--hp-border);border-radius:2px">
        <option value="1">CH1</option>
        <option value="2">CH2</option>
      </select>
      <div class="input-group" style="flex-direction:row;align-items:center;gap:4px">
        <label style="margin:0">START</label>
        <input type="text" id="scanStart" value="0x0000" style="width:60px">
      </div>
      <div class="input-group" style="flex-direction:row;align-items:center;gap:4px">
        <label style="margin:0">END</label>
        <input type="text" id="scanEnd" value="0x0020" style="width:60px">
      </div>
      <button id="doScan" class="hp-btn primary">SCAN</button>
      <button id="exportScan" class="hp-btn">EXPORT</button>
    </div>
    <div class="tableWrap">
      <table id="scanTable">
        <thead><tr><th>ADDR</th><th>DEC</th><th>RAW</th><th>REGISTER</th><th>VALUE</th></tr></thead>
        <tbody></tbody>
      </table>
    </div>
  </div>

  <div class="footer-bar">
    <span class="footer-badge">POLL: 1.0s</span>
    <span class="footer-badge">GPIB: /api/status/{ch}</span>
    <span style="color:#666">© HEWLETT-PACKARD COMPANY</span>
  </div>
</div>

<script>
const $=s=>document.querySelector(s);
const $$=s=>Array.from(document.querySelectorAll(s));
const clamp=(n,a,b)=>Math.max(a,Math.min(b,n));
const map=(n,a1,a2,b1,b2)=>b1+(n-a1)*(b2-b1)/(a2-a1);

let linkMode=false;
const series={ch1:{v:[],i:[]},ch2:{v:[],i:[]}};
const MAX_POINTS=60;

function drawGauge(canvas,value,min,max,label,unit){
  const ctx=canvas.getContext('2d');
  const W=canvas.width,H=canvas.height;
  ctx.clearRect(0,0,W,H);
  
  ctx.fillStyle='#f8f4ec';ctx.fillRect(0,0,W,H);
  
  const cx=W/2,cy=H-8,r=H-20;
  const startAngle=Math.PI,endAngle=2*Math.PI;
  
  ctx.strokeStyle='#d0c8b8';ctx.lineWidth=8;ctx.lineCap='round';
  ctx.beginPath();ctx.arc(cx,cy,r,startAngle,endAngle);ctx.stroke();
  
  ctx.strokeStyle='#3d3429';ctx.lineWidth=1;
  for(let i=0;i<=10;i++){
    const a=startAngle+i*(endAngle-startAngle)/10;
    const inner=r-12,outer=r-4;
    ctx.beginPath();
    ctx.moveTo(cx+Math.cos(a)*inner,cy+Math.sin(a)*inner);
    ctx.lineTo(cx+Math.cos(a)*outer,cy+Math.sin(a)*outer);
    ctx.stroke();
  }
  
  const pct=clamp((value-min)/(max-min),0,1);
  const needleAngle=startAngle+pct*(endAngle-startAngle);
  const needleLen=r-18;
  
  ctx.strokeStyle='#8b2020';ctx.lineWidth=2;ctx.lineCap='round';
  ctx.beginPath();
  ctx.moveTo(cx,cy);
  ctx.lineTo(cx+Math.cos(needleAngle)*needleLen,cy+Math.sin(needleAngle)*needleLen);
  ctx.stroke();
  
  ctx.beginPath();ctx.arc(cx,cy,4,0,2*Math.PI);
  ctx.fillStyle='#3d3429';ctx.fill();
  
  ctx.fillStyle='#3d3429';ctx.font='bold 12px Helvetica';ctx.textAlign='center';
  ctx.fillText(value.toFixed(2)+' '+unit,cx,cy-r/2);
  ctx.font='9px Helvetica';ctx.fillStyle='#888';
  ctx.fillText(label,cx,H-1);
}

function drawScope(canvas,data,color='#40ff40'){
  const ctx=canvas.getContext('2d');
  const W=canvas.width,H=canvas.height;
  
  // Dark phosphor screen background
  ctx.fillStyle='#0a0d08';
  ctx.fillRect(0,0,W,H);
  
  // Subtle green CRT glow in center
  const grd=ctx.createRadialGradient(W/2,H/2,0,W/2,H/2,W/2);
  grd.addColorStop(0,'rgba(20,40,15,0.3)');
  grd.addColorStop(1,'rgba(5,10,5,0)');
  ctx.fillStyle=grd;
  ctx.fillRect(0,0,W,H);
  
  // Grid lines - major
  ctx.strokeStyle='rgba(60,100,50,0.25)';
  ctx.lineWidth=1;
  const gridX=8,gridY=4;
  for(let i=1;i<gridX;i++){
    ctx.beginPath();
    ctx.moveTo(i*W/gridX,0);
    ctx.lineTo(i*W/gridX,H);
    ctx.stroke();
  }
  for(let i=1;i<gridY;i++){
    ctx.beginPath();
    ctx.moveTo(0,i*H/gridY);
    ctx.lineTo(W,i*H/gridY);
    ctx.stroke();
  }
  
  // Grid - minor (subdivisions)
  ctx.strokeStyle='rgba(60,100,50,0.1)';
  for(let i=1;i<gridX*5;i++){
    if(i%(gridX)===0)continue;
    ctx.beginPath();
    ctx.moveTo(i*W/(gridX*5),0);
    ctx.lineTo(i*W/(gridX*5),H);
    ctx.stroke();
  }
  for(let i=1;i<gridY*5;i++){
    if(i%(gridY)===0)continue;
    ctx.beginPath();
    ctx.moveTo(0,i*H/(gridY*5));
    ctx.lineTo(W,i*H/(gridY*5));
    ctx.stroke();
  }
  
  // Center crosshair
  ctx.strokeStyle='rgba(80,140,60,0.4)';
  ctx.lineWidth=1;
  ctx.beginPath();ctx.moveTo(W/2,0);ctx.lineTo(W/2,H);ctx.stroke();
  ctx.beginPath();ctx.moveTo(0,H/2);ctx.lineTo(W,H/2);ctx.stroke();
  
  if(data.length<2)return;
  const ymin=Math.min(...data),ymax=Math.max(...data);
  const range=ymax-ymin||1;
  
  // Phosphor afterglow (wider, dimmer trace)
  ctx.strokeStyle='rgba(100,255,100,0.15)';
  ctx.lineWidth=8;
  ctx.lineCap='round';ctx.lineJoin='round';
  ctx.beginPath();
  data.forEach((v,i)=>{
    const x=map(i,0,data.length-1,8,W-8);
    const y=map(v,ymax,ymin,10,H-10);
    if(i===0)ctx.moveTo(x,y);else ctx.lineTo(x,y);
  });
  ctx.stroke();
  
  // Secondary glow
  ctx.strokeStyle='rgba(120,255,100,0.3)';
  ctx.lineWidth=4;
  ctx.beginPath();
  data.forEach((v,i)=>{
    const x=map(i,0,data.length-1,8,W-8);
    const y=map(v,ymax,ymin,10,H-10);
    if(i===0)ctx.moveTo(x,y);else ctx.lineTo(x,y);
  });
  ctx.stroke();
  
  // Main phosphor trace
  ctx.strokeStyle=color;
  ctx.lineWidth=2;
  ctx.shadowColor=color;
  ctx.shadowBlur=6;
  ctx.beginPath();
  data.forEach((v,i)=>{
    const x=map(i,0,data.length-1,8,W-8);
    const y=map(v,ymax,ymin,10,H-10);
    if(i===0)ctx.moveTo(x,y);else ctx.lineTo(x,y);
  });
  ctx.stroke();
  ctx.shadowBlur=0;
  
  // Bright center of trace
  ctx.strokeStyle='rgba(200,255,200,0.8)';
  ctx.lineWidth=1;
  ctx.beginPath();
  data.forEach((v,i)=>{
    const x=map(i,0,data.length-1,8,W-8);
    const y=map(v,ymax,ymin,10,H-10);
    if(i===0)ctx.moveTo(x,y);else ctx.lineTo(x,y);
  });
  ctx.stroke();
}

function setKnobAngle(id,val,min,max){
  const el=$(id);if(!el)return;
  const deg=map(clamp(val,min,max),min,max,-135,135);
  el.style.transform=`translateX(-50%) rotate(${deg}deg)`;
}

async function getStatus(ch){
  try{const r=await fetch('/api/status/'+ch);const j=await r.json();if(!j.ok)throw 0;return j;}catch(e){return null;}
}
async function writeReg(ch,reg,val){
  const r=await fetch(`/api/write/${ch}?reg=${reg}&val=${val}`,{method:'POST'});return r.json();
}
async function doLink(){return(await fetch('/api/link',{method:'POST'})).json();}

function updateUI(ch,d){
  if(!d)return;
  const s=ch===1?'1':'2';
  
  // When output OFF: show SET values, when ON: show actual OUTPUT values
  let dispV, dispA, dispP;
  if(d.outputOn){
    dispV = d.outV;
    dispA = d.outA;
    dispP = d.outP;
  } else {
    dispV = d.setV;
    dispA = d.setA;
    dispP = d.setV * d.setA; // calculated from set values
  }
  
  $(`#vout${s}`).textContent=dispV.toFixed(3);
  $(`#iout${s}`).textContent=dispA.toFixed(3);
  $(`#pout${s}`).textContent=dispP.toFixed(2);
  
  // Add/remove 'set-mode' class to show visual difference
  const vfdScreen = $(`#vout${s}`).closest('.vfd-screen');
  if(vfdScreen){
    vfdScreen.classList.toggle('set-mode', !d.outputOn);
  }
  
  if(document.activeElement!==$(`#vset${s}`))$(`#vset${s}`).value=d.setV.toFixed(2);
  if(document.activeElement!==$(`#iset${s}`))$(`#iset${s}`).value=d.setA.toFixed(3);
  setKnobAngle(`#kn${s}v`,d.setV,0,60);
  setKnobAngle(`#kn${s}i`,d.setA,0,10);
  
  const oeBtn=$(`#oe${s}`);
  oeBtn.classList.toggle('on',d.outputOn);
  oeBtn.classList.toggle('off',!d.outputOn);
  $(`#status${s}`).textContent=d.ok?'OK':'ERR';
  
  drawGauge($(`#gV${s}`),dispV,0,60,'VOLTAGE','V');
  drawGauge($(`#gI${s}`),dispA,0,10,'CURRENT','A');
  
  const ser=series[`ch${s}`];
  ser.v.push(dispV);ser.i.push(dispA);
  if(ser.v.length>MAX_POINTS){ser.v.shift();ser.i.shift();}
  drawScope($(`#lineCh${s}`),ser.v,'#40ff40');
}

$('#linkToggle').onclick=async()=>{
  linkMode=!linkMode;
  const btn=$('#linkToggle');
  btn.classList.toggle('on',linkMode);
  btn.classList.toggle('off',!linkMode);
  if(linkMode)await doLink();
};

$('#refreshBtn').onclick=()=>pollAll();

$('#oe1').onclick=async()=>{
  const on=$('#oe1').classList.contains('on');
  await writeReg(1,0x12,on?0:1);
  if(linkMode)await writeReg(2,0x12,on?0:1);
  pollAll();
};
$('#oe2').onclick=async()=>{
  const on=$('#oe2').classList.contains('on');
  await writeReg(2,0x12,on?0:1);
  pollAll();
};

$('#applyV1').onclick=async()=>{const v=parseFloat($('#vset1').value);if(!isNaN(v)){await writeReg(1,0,Math.round(v*100));if(linkMode)await writeReg(2,0,Math.round(v*100));pollAll();}};
$('#applyV2').onclick=async()=>{const v=parseFloat($('#vset2').value);if(!isNaN(v)){await writeReg(2,0,Math.round(v*100));pollAll();}};
$('#applyI1').onclick=async()=>{const a=parseFloat($('#iset1').value);if(!isNaN(a)){await writeReg(1,1,Math.round(a*1000));if(linkMode)await writeReg(2,1,Math.round(a*1000));pollAll();}};
$('#applyI2').onclick=async()=>{const a=parseFloat($('#iset2').value);if(!isNaN(a)){await writeReg(2,1,Math.round(a*1000));pollAll();}};

function toHex(n){return '0x'+('0000'+n.toString(16)).slice(-4).toUpperCase();}
function parseAddr(s){s=s.trim();return s.startsWith('0x')?parseInt(s,16):parseInt(s,10);}

$('#doScan').onclick=async()=>{
  const ch=$('#scanCh').value;
  const start=parseAddr($('#scanStart').value),end=parseAddr($('#scanEnd').value);
  if(isNaN(start)||isNaN(end)||end<start)return;
  const r=await fetch(`/api/scan/${ch}?start=${$('#scanStart').value}&end=${$('#scanEnd').value}`);
  const j=await r.json();if(!j.ok)return;
  const tbody=$('#scanTable tbody');tbody.innerHTML='';
  j.rows.forEach(x=>{
    const tr=document.createElement('tr');
    tr.innerHTML=`<td>${toHex(x.addr)}</td><td>${x.addr}</td><td>${x.dec}</td><td>${x.meaning||''}</td><td>${x.interpretation||''}</td>`;
    tbody.appendChild(tr);
  });
};

$('#exportScan').onclick=()=>{
  const rows=[['addr_hex','addr_dec','raw','meaning','value']];
  $$('#scanTable tbody tr').forEach(tr=>{rows.push([...tr.children].map(td=>td.textContent));});
  const blob=new Blob([rows.map(r=>r.join(',')).join('\n')],{type:'text/csv'});
  const a=document.createElement('a');a.href=URL.createObjectURL(blob);a.download='scan.csv';a.click();
};

$('#downloadCsv').onclick=()=>{
  const rows=[['timestamp','channel','voltage','current']];
  const now=Date.now();
  ['ch1','ch2'].forEach((ch,ci)=>{
    series[ch].v.forEach((v,i)=>{rows.push([new Date(now-1000*(series[ch].v.length-i)).toISOString(),ci+1,v,series[ch].i[i]||0]);});
  });
  const blob=new Blob([rows.map(r=>r.join(',')).join('\n')],{type:'text/csv'});
  const a=document.createElement('a');a.href=URL.createObjectURL(blob);a.download='data.csv';a.click();
};

async function pollAll(){
  const d1=await getStatus(1),d2=await getStatus(2);
  const ok=d1&&d1.ok&&d2&&d2.ok;
  $('#sysLed').className='status-led '+(ok?'on':'err');
  $('#sysText').textContent=ok?'ONLINE':'ERROR';
  updateUI(1,d1);updateUI(2,d2);
}
setInterval(pollAll,1000);pollAll();
</script>
</body>
</html>
)INDEX_HTML";