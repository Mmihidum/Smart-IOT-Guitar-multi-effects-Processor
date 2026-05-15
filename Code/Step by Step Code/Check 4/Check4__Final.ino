#include <WiFi.h>
#include <WebServer.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>

const char* ssid = "ESP32_GUITAR";
const char* password = "12345678";

WebServer server(80);
LiquidCrystal_I2C lcd(0x27, 16, 2);   // LCD blank නම් 0x3F try කරන්න

#define LED_READY 2
#define LED_EFFECT 4
#define LED_LOOP 5

#define BTN_MODE 18
#define BTN_LOOP1 19
#define BTN_LOOP2 23

int cmdId = 0;
String latestCmd = "NONE";
unsigned long lastBtnTime = 0;

void updateLCD(String line1, String line2) {
  lcd.clear();
  if (line1.length() > 16) line1 = line1.substring(0, 16);
  if (line2.length() > 16) line2 = line2.substring(0, 16);
  lcd.setCursor(0, 0);
  lcd.print(line1);
  lcd.setCursor(0, 1);
  lcd.print(line2);
}

void setCommand(String cmd) {
  latestCmd = cmd;
  cmdId++;
}

void handleLCD() {
  String line1 = server.arg("l1");
  String line2 = server.arg("l2");
  String effectLed = server.arg("effect");
  String loopLed = server.arg("loop");

  if (line1 == "") line1 = "ESP32 Guitar FX";
  if (line2 == "") line2 = "Web Ready";

  updateLCD(line1, line2);

  if (effectLed == "1") digitalWrite(LED_EFFECT, HIGH);
  if (effectLed == "0") digitalWrite(LED_EFFECT, LOW);

  if (loopLed == "1") digitalWrite(LED_LOOP, HIGH);
  if (loopLed == "0") digitalWrite(LED_LOOP, LOW);

  server.send(200, "text/plain", "OK");
}

void handleCmd() {
  String json = "{";
  json += "\"id\":" + String(cmdId) + ",";
  json += "\"cmd\":\"" + latestCmd + "\"";
  json += "}";
  server.send(200, "application/json", json);
}

const char MAIN_page[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1.0">
<title>ESP32 Guitar FX Dual Looper</title>
<style>
body{font-family:Arial;background:#080808;color:white;text-align:center;padding:15px;}
h2{color:#00ff88;}
.card{background:#1b1b1b;padding:14px;margin:12px 0;border-radius:16px;}
button{background:#00ff88;border:none;border-radius:12px;padding:12px 14px;margin:5px;font-weight:bold;}
.red{background:#ff4444;color:white;}
.blue{background:#44aaff;color:white;}
.yellow{background:#ffaa00;color:#111;}
.gray{background:#777;color:white;}
input[type=range]{width:90%;}
input[type=file]{margin:8px;color:white;}
.status{background:#333;padding:12px;border-radius:12px;font-weight:bold;}
</style>
</head>

<body>
<h2>ESP32 Guitar FX Final</h2>
<p>Live Tone Converter + AUX Output + Dual Loop Pedal</p>

<button onclick="startMic()">Start Mic</button>

<div class="card">
<h3>Tone Modes</h3>
<button onclick="setMode('Clean')">Clean</button>
<button onclick="setMode('Distortion')">Distortion</button>
<button onclick="setMode('Rock')">Rock</button>
<button onclick="setMode('Bass')">Bass</button>
<button onclick="setMode('Echo')">Echo</button>
<button onclick="setMode('Reverb')">Reverb</button>
</div>

<div class="card">
<h3>Controls</h3>
Volume <span id="volVal">1.0</span><br>
<input type="range" min="0" max="2" step="0.1" value="1" oninput="setVolume(this.value)"><br>
Gain <span id="gainVal">1.0</span><br>
<input type="range" min="0" max="3" step="0.1" value="1" oninput="setGain(this.value)"><br>
Bass EQ <span id="bassVal">0</span>dB<br>
<input type="range" min="-20" max="20" step="1" value="0" oninput="setBassEQ(this.value)"><br>
Mid EQ <span id="midVal">0</span>dB<br>
<input type="range" min="-20" max="20" step="1" value="0" oninput="setMidEQ(this.value)"><br>
Treble EQ <span id="trebleVal">0</span>dB<br>
<input type="range" min="-20" max="20" step="1" value="0" oninput="setTrebleEQ(this.value)">
</div>

<div class="card">
<h3>Loop 1</h3>
<button class="red" onclick="startLoopRecord(1)">Record L1</button>
<button class="blue" onclick="stopLoopRecord(1)">Stop Rec L1</button>
<button onclick="playLoop(1)">Play L1</button>
<button class="gray" onclick="stopLoop(1)">Stop L1</button>
<button class="yellow" onclick="clearLoop(1)">Clear L1</button>
<button onclick="downloadLoop(1)">Download L1</button>
<input type="file" accept="audio/*" onchange="loadLoopFile(1,this)">
<div id="loopStatus1">Loop 1: Empty</div>
</div>

<div class="card">
<h3>Loop 2</h3>
<button class="red" onclick="startLoopRecord(2)">Record L2</button>
<button class="blue" onclick="stopLoopRecord(2)">Stop Rec L2</button>
<button onclick="playLoop(2)">Play L2</button>
<button class="gray" onclick="stopLoop(2)">Stop L2</button>
<button class="yellow" onclick="clearLoop(2)">Clear L2</button>
<button onclick="downloadLoop(2)">Download L2</button>
<input type="file" accept="audio/*" onchange="loadLoopFile(2,this)">
<div id="loopStatus2">Loop 2: Empty</div>
</div>

<div class="card">
<h3>Normal Recording</h3>
<button class="red" onclick="startRecording()">Start Record</button>
<button class="blue" onclick="stopRecording()">Stop & Download</button>
</div>

<div class="card">
<h3>Metronome</h3>
BPM <span id="bpmVal">120</span><br>
<input type="range" min="40" max="220" value="120" oninput="setBPM(this.value)">
<br>
<button onclick="startMetronome()">Metronome ON</button>
<button onclick="stopMetronome()">Metronome OFF</button>
</div>

<div class="status" id="status">Status: Ready</div>

<script>
let audioContext, mic, analyser;
let gainNode, volumeNode, bassEQ, midEQ, trebleEQ;
let activeNodes = [];

let currentMode = "Clean";
let currentVol = "1.0";
let currentGain = "1.0";

let micStream;
let mediaRecorder;
let recordedChunks = [];

let loopRecorders = {1:null, 2:null};
let loopChunks = {1:[], 2:[]};
let loopBuffers = {1:null, 2:null};
let loopSources = {1:null, 2:null};
let loopBlobs = {1:null, 2:null};
let loopStates = {1:"empty", 2:"empty"};

let metroTimer = null;
let bpm = 120;

let lastCmdId = -1;
let modeList = ["Clean","Distortion","Rock","Bass","Echo","Reverb"];
let modeIndex = 0;

function anyLoopActive(){
  return loopStates[1] === "playing" || loopStates[1] === "recording" ||
         loopStates[2] === "playing" || loopStates[2] === "recording";
}

function lcd(l1,l2,effect,loop){
  fetch("/lcd?l1=" + encodeURIComponent(l1) +
        "&l2=" + encodeURIComponent(l2) +
        "&effect=" + effect +
        "&loop=" + loop).catch(e => console.log(e));
}

function statusText(t){
  document.getElementById("status").innerHTML = "Status: " + t;
  lcd("Status:", t, "1", anyLoopActive() ? "1" : "0");
}

async function startMic(){
  try{
    audioContext = new (window.AudioContext || window.webkitAudioContext)();
    await audioContext.resume();

    micStream = await navigator.mediaDevices.getUserMedia({
      audio:{echoCancellation:false, noiseSuppression:false, autoGainControl:false}
    });

    mic = audioContext.createMediaStreamSource(micStream);
    analyser = audioContext.createAnalyser();
    analyser.fftSize = 2048;

    setupNodes();
    setMode("Clean");

    alert("Mic Access Granted");
    statusText("Mic ON");
  }catch(err){
    alert("Mic Error: " + err.message);
    statusText("Mic Error");
  }
}

function setupNodes(){
  gainNode = audioContext.createGain();
  gainNode.gain.value = 1;

  volumeNode = audioContext.createGain();
  volumeNode.gain.value = 1;

  bassEQ = audioContext.createBiquadFilter();
  bassEQ.type = "lowshelf";
  bassEQ.frequency.value = 250;

  midEQ = audioContext.createBiquadFilter();
  midEQ.type = "peaking";
  midEQ.frequency.value = 1000;
  midEQ.Q.value = 1;

  trebleEQ = audioContext.createBiquadFilter();
  trebleEQ.type = "highshelf";
  trebleEQ.frequency.value = 3000;
}

function checkMic(){
  if(!audioContext || !mic){
    alert("First click Start Mic");
    lcd("Start Mic First", "No Audio", "0", "0");
    return false;
  }
  return true;
}

function clearAudio(){
  try{ mic.disconnect(); }catch(e){}
  activeNodes.forEach(n => { try{ n.disconnect(); }catch(e){} });
  activeNodes = [];
}

function makeDistortionCurve(amount){
  let samples = 44100;
  let curve = new Float32Array(samples);
  let deg = Math.PI / 180;
  for(let i=0; i<samples; i++){
    let x = i * 2 / samples - 1;
    curve[i] = ((3 + amount) * x * 20 * deg) / (Math.PI + amount * Math.abs(x));
  }
  return curve;
}

function finalConnect(last){
  last.connect(gainNode);
  gainNode.connect(bassEQ);
  bassEQ.connect(midEQ);
  midEQ.connect(trebleEQ);
  trebleEQ.connect(volumeNode);
  volumeNode.connect(analyser);
  analyser.connect(audioContext.destination);
}

function setMode(mode){
  if(!checkMic()) return;

  currentMode = mode;
  modeIndex = modeList.indexOf(mode);
  if(modeIndex < 0) modeIndex = 0;

  clearAudio();

  if(mode === "Clean"){
    finalConnect(mic);
    activeNodes = [gainNode,bassEQ,midEQ,trebleEQ,volumeNode,analyser];
  } else if(mode === "Distortion"){
    let shaper = audioContext.createWaveShaper();
    shaper.curve = makeDistortionCurve(800);
    shaper.oversample = "4x";
    mic.connect(shaper);
    finalConnect(shaper);
    activeNodes = [shaper,gainNode,bassEQ,midEQ,trebleEQ,volumeNode,analyser];
  } else if(mode === "Rock"){
    let hp = audioContext.createBiquadFilter();
    hp.type = "highpass";
    hp.frequency.value = 220;
    let shaper = audioContext.createWaveShaper();
    shaper.curve = makeDistortionCurve(400);
    shaper.oversample = "4x";
    mic.connect(hp);
    hp.connect(shaper);
    finalConnect(shaper);
    activeNodes = [hp,shaper,gainNode,bassEQ,midEQ,trebleEQ,volumeNode,analyser];
  } else if(mode === "Bass"){
    let lp = audioContext.createBiquadFilter();
    lp.type = "lowpass";
    lp.frequency.value = 300;
    mic.connect(lp);
    finalConnect(lp);
    activeNodes = [lp,gainNode,bassEQ,midEQ,trebleEQ,volumeNode,analyser];
  } else if(mode === "Echo"){
    let delay = audioContext.createDelay();
    delay.delayTime.value = 0.35;
    let feedback = audioContext.createGain();
    feedback.gain.value = 0.35;
    finalConnect(mic);
    mic.connect(delay);
    delay.connect(feedback);
    feedback.connect(delay);
    delay.connect(audioContext.destination);
    activeNodes = [delay,feedback,gainNode,bassEQ,midEQ,trebleEQ,volumeNode,analyser];
  } else if(mode === "Reverb"){
    let d1 = audioContext.createDelay();
    let d2 = audioContext.createDelay();
    let d3 = audioContext.createDelay();
    d1.delayTime.value = 0.08;
    d2.delayTime.value = 0.16;
    d3.delayTime.value = 0.24;
    let g1 = audioContext.createGain();
    let g2 = audioContext.createGain();
    let g3 = audioContext.createGain();
    g1.gain.value = 0.45;
    g2.gain.value = 0.30;
    g3.gain.value = 0.20;
    finalConnect(mic);
    mic.connect(d1); d1.connect(g1); g1.connect(audioContext.destination);
    mic.connect(d2); d2.connect(g2); g2.connect(audioContext.destination);
    mic.connect(d3); d3.connect(g3); g3.connect(audioContext.destination);
    activeNodes = [d1,d2,d3,g1,g2,g3,gainNode,bassEQ,midEQ,trebleEQ,volumeNode,analyser];
  }

  document.getElementById("status").innerHTML = "Status: " + mode + " Tone";
  lcd("Mode: " + currentMode, "V:" + currentVol + " G:" + currentGain, "1", anyLoopActive() ? "1" : "0");
}

function nextMode(){
  if(!checkMic()) return;
  modeIndex++;
  if(modeIndex >= modeList.length) modeIndex = 0;
  setMode(modeList[modeIndex]);
}

function setVolume(v){
  currentVol = v;
  document.getElementById("volVal").innerHTML = v;
  if(volumeNode) volumeNode.gain.value = parseFloat(v);
  lcd("Mode: " + currentMode, "V:" + currentVol + " G:" + currentGain, "1", anyLoopActive() ? "1" : "0");
}

function setGain(v){
  currentGain = v;
  document.getElementById("gainVal").innerHTML = v;
  if(gainNode) gainNode.gain.value = parseFloat(v);
  lcd("Mode: " + currentMode, "V:" + currentVol + " G:" + currentGain, "1", anyLoopActive() ? "1" : "0");
}

function setBassEQ(v){ document.getElementById("bassVal").innerHTML = v; if(bassEQ) bassEQ.gain.value = parseFloat(v); }
function setMidEQ(v){ document.getElementById("midVal").innerHTML = v; if(midEQ) midEQ.gain.value = parseFloat(v); }
function setTrebleEQ(v){ document.getElementById("trebleVal").innerHTML = v; if(trebleEQ) trebleEQ.gain.value = parseFloat(v); }

// Normal Recording
function startRecording(){
  if(!micStream){ alert("Start mic first"); return; }
  recordedChunks = [];
  mediaRecorder = new MediaRecorder(micStream);
  mediaRecorder.ondataavailable = e => recordedChunks.push(e.data);
  mediaRecorder.start();
  statusText("Recording...");
}

function stopRecording(){
  if(!mediaRecorder){ alert("Recording not started"); return; }
  mediaRecorder.stop();
  mediaRecorder.onstop = function(){
    let blob = new Blob(recordedChunks, {type:"audio/webm"});
    let url = URL.createObjectURL(blob);
    let a = document.createElement("a");
    a.href = url;
    a.download = "guitar_recording.webm";
    a.click();
    statusText("Record Saved");
  }
}

// Dual Loop System
function loopButtonAction(slot){
  if(loopStates[slot] === "empty"){
    startLoopRecord(slot);
  }else if(loopStates[slot] === "recording"){
    stopLoopRecord(slot);
  }else if(loopStates[slot] === "saved" || loopStates[slot] === "stopped"){
    playLoop(slot);
  }else if(loopStates[slot] === "playing"){
    stopLoop(slot);
  }
}

function startLoopRecord(slot){
  if(!micStream){
    alert("Start mic first");
    lcd("Start Mic First", "Loop " + slot, "0", "0");
    return;
  }

  stopLoop(slot);

  loopChunks[slot] = [];
  loopRecorders[slot] = new MediaRecorder(micStream);

  loopRecorders[slot].ondataavailable = e => {
    if(e.data.size > 0) loopChunks[slot].push(e.data);
  };

  loopRecorders[slot].start();
  loopStates[slot] = "recording";

  document.getElementById("loopStatus" + slot).innerHTML = "Loop " + slot + ": Recording...";
  lcd("Loop " + slot, "Recording...", "1", "1");
}

function stopLoopRecord(slot){
  if(!loopRecorders[slot]){
    alert("Loop " + slot + " recording not started");
    return;
  }

  loopRecorders[slot].stop();

  loopRecorders[slot].onstop = async function(){
    loopBlobs[slot] = new Blob(loopChunks[slot], {type:"audio/webm"});
    let arrayBuffer = await loopBlobs[slot].arrayBuffer();

    try{
      loopBuffers[slot] = await audioContext.decodeAudioData(arrayBuffer);
      loopStates[slot] = "saved";
      document.getElementById("loopStatus" + slot).innerHTML = "Loop " + slot + ": Saved";
      lcd("Loop " + slot, "Saved", "1", anyLoopActive() ? "1" : "0");
    }catch(e){
      loopStates[slot] = "empty";
      document.getElementById("loopStatus" + slot).innerHTML = "Loop " + slot + ": Decode Error";
      lcd("Loop " + slot, "Decode Error", "1", anyLoopActive() ? "1" : "0");
      alert("Loop decode error. Try shorter recording.");
    }
  };
}

function playLoop(slot){
  if(!audioContext){
    alert("Start mic first");
    return;
  }

  if(!loopBuffers[slot]){
    alert("Loop " + slot + " is empty");
    lcd("Loop " + slot, "Empty", "1", anyLoopActive() ? "1" : "0");
    return;
  }

  stopLoop(slot);

  let src = audioContext.createBufferSource();
  src.buffer = loopBuffers[slot];
  src.loop = true;

  let loopGain = audioContext.createGain();
  loopGain.gain.value = 1.0;

  src.connect(loopGain);
  loopGain.connect(audioContext.destination);
  src.start(0);

  loopSources[slot] = src;
  loopStates[slot] = "playing";

  document.getElementById("loopStatus" + slot).innerHTML = "Loop " + slot + ": Playing";
  lcd("Loop " + slot, "Playing...", "1", "1");
}

function stopLoop(slot){
  if(loopSources[slot]){
    try{ loopSources[slot].stop(); }catch(e){}
    try{ loopSources[slot].disconnect(); }catch(e){}
    loopSources[slot] = null;
  }

  if(loopBuffers[slot]){
    loopStates[slot] = "stopped";
    document.getElementById("loopStatus" + slot).innerHTML = "Loop " + slot + ": Saved / Stopped";
  }else{
    loopStates[slot] = "empty";
    document.getElementById("loopStatus" + slot).innerHTML = "Loop " + slot + ": Empty";
  }

  lcd("Loop " + slot, "Stopped", "1", anyLoopActive() ? "1" : "0");
}

function clearLoop(slot){
  stopLoop(slot);
  loopBuffers[slot] = null;
  loopBlobs[slot] = null;
  loopChunks[slot] = [];
  loopRecorders[slot] = null;
  loopStates[slot] = "empty";

  document.getElementById("loopStatus" + slot).innerHTML = "Loop " + slot + ": Empty";
  lcd("Loop " + slot, "Cleared", "1", anyLoopActive() ? "1" : "0");
}

function downloadLoop(slot){
  if(!loopBlobs[slot]){
    alert("Loop " + slot + " empty. Record first.");
    return;
  }

  let url = URL.createObjectURL(loopBlobs[slot]);
  let a = document.createElement("a");
  a.href = url;
  a.download = "guitar_loop_" + slot + ".webm";
  a.click();

  lcd("Loop " + slot, "Downloaded", "1", anyLoopActive() ? "1" : "0");
}

async function loadLoopFile(slot, input){
  if(!audioContext){
    alert("First click Start Mic");
    return;
  }

  let file = input.files[0];
  if(!file) return;

  try{
    let arrayBuffer = await file.arrayBuffer();
    loopBuffers[slot] = await audioContext.decodeAudioData(arrayBuffer);
    loopBlobs[slot] = file;
    loopStates[slot] = "saved";

    document.getElementById("loopStatus" + slot).innerHTML = "Loop " + slot + ": File Loaded";
    lcd("Loop " + slot, "File Loaded", "1", anyLoopActive() ? "1" : "0");
  }catch(e){
    alert("File load error");
    lcd("Loop " + slot, "Load Error", "1", anyLoopActive() ? "1" : "0");
  }
}

// Metronome
function setBPM(v){
  bpm = parseInt(v);
  document.getElementById("bpmVal").innerHTML = bpm;
}

function startMetronome(){
  if(!audioContext){
    alert("Start mic first");
    return;
  }

  stopMetronome();

  metroTimer = setInterval(() => {
    let osc = audioContext.createOscillator();
    let g = audioContext.createGain();
    osc.frequency.value = 1000;
    g.gain.value = 0.4;
    osc.connect(g);
    g.connect(audioContext.destination);
    osc.start();
    osc.stop(audioContext.currentTime + 0.05);
  }, 60000 / bpm);

  statusText("Metronome ON");
}

function stopMetronome(){
  if(metroTimer){
    clearInterval(metroTimer);
    metroTimer = null;
    statusText("Metronome OFF");
  }
}

// Physical button command polling
async function pollButtons(){
  try{
    let res = await fetch("/cmd");
    let data = await res.json();

    if(data.id !== lastCmdId){
      lastCmdId = data.id;

      if(data.cmd === "MODE_NEXT") nextMode();
      if(data.cmd === "LOOP1_ACTION") loopButtonAction(1);
      if(data.cmd === "LOOP2_ACTION") loopButtonAction(2);
    }
  }catch(e){}
}

setInterval(pollButtons, 400);
</script>
</body>
</html>
)rawliteral";

void handleRoot() {
  server.send_P(200, "text/html", MAIN_page);
  updateLCD("Client Connected", "Web Ready");
}

void readButtons() {
  if (millis() - lastBtnTime < 300) return;

  if (digitalRead(BTN_MODE) == LOW) {
    setCommand("MODE_NEXT");
    updateLCD("Button:", "Mode Next");
    lastBtnTime = millis();
  }

  if (digitalRead(BTN_LOOP1) == LOW) {
    setCommand("LOOP1_ACTION");
    updateLCD("Button:", "Loop 1 Action");
    lastBtnTime = millis();
  }

  if (digitalRead(BTN_LOOP2) == LOW) {
    setCommand("LOOP2_ACTION");
    updateLCD("Button:", "Loop 2 Action");
    lastBtnTime = millis();
  }
}

void setup() {
  Serial.begin(115200);

  pinMode(LED_READY, OUTPUT);
  pinMode(LED_EFFECT, OUTPUT);
  pinMode(LED_LOOP, OUTPUT);

  pinMode(BTN_MODE, INPUT_PULLUP);
  pinMode(BTN_LOOP1, INPUT_PULLUP);
  pinMode(BTN_LOOP2, INPUT_PULLUP);

  digitalWrite(LED_READY, LOW);
  digitalWrite(LED_EFFECT, LOW);
  digitalWrite(LED_LOOP, LOW);

  Wire.begin(21, 22);

  lcd.init();
  lcd.backlight();

  updateLCD("ESP32 Guitar FX", "Starting...");

  WiFi.mode(WIFI_AP);
  WiFi.softAP(ssid, password);

  delay(1000);

  digitalWrite(LED_READY, HIGH);

  updateLCD("WiFi:ESP32", "IP:192.168.4.1");

  server.on("/", handleRoot);
  server.on("/lcd", handleLCD);
  server.on("/cmd", handleCmd);
  server.begin();

  Serial.println("ESP32 Guitar FX AUX Dual Loop Final Started");
  Serial.println(WiFi.softAPIP());
}

void loop() {
  server.handleClient();
  readButtons();
}
