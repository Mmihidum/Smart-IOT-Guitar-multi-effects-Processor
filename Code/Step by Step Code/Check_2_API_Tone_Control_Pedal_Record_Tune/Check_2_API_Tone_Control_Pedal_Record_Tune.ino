#include <WiFi.h>
#include <WebServer.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>

const char* ssid = "ESP32_GUITAR";
const char* password = "12345678";

WebServer server(80);
LiquidCrystal_I2C lcd(0x27, 16, 2); // LCD blank නම් 0x3F try කරන්න

String lcdLine1 = "ESP32 Guitar FX";
String lcdLine2 = "Starting...";

void updateLCD(String line1, String line2) {
  lcd.clear();

  if (line1.length() > 16) line1 = line1.substring(0, 16);
  if (line2.length() > 16) line2 = line2.substring(0, 16);

  lcd.setCursor(0, 0);
  lcd.print(line1);

  lcd.setCursor(0, 1);
  lcd.print(line2);
}

void handleLCD() {
  String mode = server.arg("mode");
  String vol = server.arg("vol");
  String gain = server.arg("gain");
  String status = server.arg("status");

  if (status != "") {
    updateLCD("Status:", status);
  } else {
    updateLCD("Mode: " + mode, "V:" + vol + " G:" + gain);
  }

  server.send(200, "text/plain", "LCD Updated");
}

const char MAIN_page[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1.0">
<title>ESP32 Guitar FX LCD</title>

<style>
body{
  font-family:Arial;
  background:#080808;
  color:white;
  text-align:center;
  padding:15px;
}
h2{color:#00ff88;}
.card{
  background:#1b1b1b;
  padding:14px;
  margin:12px 0;
  border-radius:16px;
}
button{
  background:#00ff88;
  border:none;
  border-radius:12px;
  padding:12px 16px;
  margin:5px;
  font-weight:bold;
}
.red{background:#ff4444;color:white;}
.blue{background:#44aaff;color:white;}
input[type=range]{width:90%;}
.status{
  background:#333;
  padding:12px;
  border-radius:12px;
  font-weight:bold;
}
</style>
</head>

<body>

<h2>🎸 ESP32 Guitar FX</h2>
<p>LCD Controlled Guitar Tone Converter</p>

<button onclick="startMic()">🎤 Start Mic</button>

<div class="card">
<h3>🎛 Tone Modes</h3>
<button onclick="setMode('Clean')">Clean</button>
<button onclick="setMode('Distortion')">Distortion</button>
<button onclick="setMode('Rock')">Rock</button>
<button onclick="setMode('Bass')">Bass</button>
<button onclick="setMode('Echo')">Echo</button>
<button onclick="setMode('Reverb')">Reverb</button>
</div>

<div class="card">
<h3>🎚 Controls</h3>

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
<h3>🦶 Pedalboard</h3>
<button onclick="setMode('Overdrive')">Overdrive</button>
<button onclick="setMode('Delay')">Delay</button>
<button onclick="setMode('Reverb')">Reverb</button>
<button onclick="toggleNoise()">Noise Reduction</button>
</div>

<div class="card">
<h3>🎙 Recording</h3>
<button class="red" onclick="startRecording()">Start Record</button>
<button class="blue" onclick="stopRecording()">Stop Record</button>
</div>

<div class="card">
<h3>🎼 Tuner / Metronome</h3>
<button onclick="startTuner()">Start Tuner</button>
<button onclick="startMetronome()">Metronome ON</button>
<button onclick="stopMetronome()">Metronome OFF</button>
</div>

<div class="status" id="status">Status: Ready</div>

<script>
let audioContext, mic, analyser;
let gainNode, volumeNode, bassEQ, midEQ, trebleEQ, noiseFilter;
let activeNodes = [];
let currentMode = "Clean";
let currentVol = "1.0";
let currentGain = "1.0";
let noiseOn = false;

let micStream;
let mediaRecorder;
let recordedChunks = [];
let metroTimer = null;
let bpm = 120;

function sendLCD(statusText = ""){
  let url = "/lcd?mode=" + encodeURIComponent(currentMode) +
            "&vol=" + encodeURIComponent(currentVol) +
            "&gain=" + encodeURIComponent(currentGain);

  if(statusText !== ""){
    url += "&status=" + encodeURIComponent(statusText);
  }

  fetch(url).catch(e => console.log(e));
}

function statusText(t){
  document.getElementById("status").innerHTML = "Status: " + t;
  sendLCD(t);
}

async function startMic(){
  try{
    audioContext = new (window.AudioContext || window.webkitAudioContext)();
    await audioContext.resume();

    micStream = await navigator.mediaDevices.getUserMedia({
      audio:{
        echoCancellation:false,
        noiseSuppression:false,
        autoGainControl:false
      }
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

  noiseFilter = audioContext.createBiquadFilter();
  noiseFilter.type = "highpass";
  noiseFilter.frequency.value = 80;
}

function checkMic(){
  if(!audioContext || !mic){
    alert("First click Start Mic");
    sendLCD("Start Mic First");
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

  if(noiseOn){
    trebleEQ.connect(noiseFilter);
    noiseFilter.connect(volumeNode);
  }else{
    trebleEQ.connect(volumeNode);
  }

  volumeNode.connect(analyser);
  analyser.connect(audioContext.destination);
}

function setMode(mode){
  if(!checkMic()) return;

  currentMode = mode;
  clearAudio();

  if(mode === "Clean"){
    finalConnect(mic);
    activeNodes = [gainNode,bassEQ,midEQ,trebleEQ,noiseFilter,volumeNode,analyser];
  }

  else if(mode === "Distortion" || mode === "Overdrive"){
    let shaper = audioContext.createWaveShaper();
    shaper.curve = makeDistortionCurve(800);
    shaper.oversample = "4x";

    mic.connect(shaper);
    finalConnect(shaper);

    activeNodes = [shaper,gainNode,bassEQ,midEQ,trebleEQ,noiseFilter,volumeNode,analyser];
  }

  else if(mode === "Rock"){
    let hp = audioContext.createBiquadFilter();
    hp.type = "highpass";
    hp.frequency.value = 220;

    let shaper = audioContext.createWaveShaper();
    shaper.curve = makeDistortionCurve(400);
    shaper.oversample = "4x";

    mic.connect(hp);
    hp.connect(shaper);
    finalConnect(shaper);

    activeNodes = [hp,shaper,gainNode,bassEQ,midEQ,trebleEQ,noiseFilter,volumeNode,analyser];
  }

  else if(mode === "Bass"){
    let lp = audioContext.createBiquadFilter();
    lp.type = "lowpass";
    lp.frequency.value = 300;

    mic.connect(lp);
    finalConnect(lp);

    activeNodes = [lp,gainNode,bassEQ,midEQ,trebleEQ,noiseFilter,volumeNode,analyser];
  }

  else if(mode === "Echo" || mode === "Delay"){
    let delay = audioContext.createDelay();
    delay.delayTime.value = 0.35;

    let feedback = audioContext.createGain();
    feedback.gain.value = 0.35;

    finalConnect(mic);

    mic.connect(delay);
    delay.connect(feedback);
    feedback.connect(delay);
    delay.connect(audioContext.destination);

    activeNodes = [delay,feedback,gainNode,bassEQ,midEQ,trebleEQ,noiseFilter,volumeNode,analyser];
  }

  else if(mode === "Reverb"){
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

    activeNodes = [d1,d2,d3,g1,g2,g3,gainNode,bassEQ,midEQ,trebleEQ,noiseFilter,volumeNode,analyser];
  }

  document.getElementById("status").innerHTML = "Status: " + mode + " Tone";
  sendLCD();
}

function setVolume(v){
  currentVol = v;
  document.getElementById("volVal").innerHTML = v;
  if(volumeNode) volumeNode.gain.value = parseFloat(v);
  sendLCD();
}

function setGain(v){
  currentGain = v;
  document.getElementById("gainVal").innerHTML = v;
  if(gainNode) gainNode.gain.value = parseFloat(v);
  sendLCD();
}

function setBassEQ(v){
  document.getElementById("bassVal").innerHTML = v;
  if(bassEQ) bassEQ.gain.value = parseFloat(v);
}

function setMidEQ(v){
  document.getElementById("midVal").innerHTML = v;
  if(midEQ) midEQ.gain.value = parseFloat(v);
}

function setTrebleEQ(v){
  document.getElementById("trebleVal").innerHTML = v;
  if(trebleEQ) trebleEQ.gain.value = parseFloat(v);
}

function toggleNoise(){
  noiseOn = !noiseOn;
  if(audioContext && mic) setMode(currentMode);
  statusText(noiseOn ? "Noise ON" : "Noise OFF");
}

function startRecording(){
  if(!micStream){
    alert("Start mic first");
    sendLCD("Start Mic First");
    return;
  }

  recordedChunks = [];
  mediaRecorder = new MediaRecorder(micStream);
  mediaRecorder.ondataavailable = e => recordedChunks.push(e.data);
  mediaRecorder.start();

  statusText("Recording...");
}

function stopRecording(){
  if(!mediaRecorder){
    alert("Recording not started");
    sendLCD("No Recording");
    return;
  }

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

function startTuner(){
  if(!checkMic()) return;
  statusText("Tuner ON");
}

function startMetronome(){
  if(!audioContext){
    alert("Start mic first");
    sendLCD("Start Mic First");
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
</script>

</body>
</html>
)rawliteral";

void handleRoot() {
  server.send_P(200, "text/html", MAIN_page);
  updateLCD("Client Connected", "Web Ready");
}

void setup() {
  Serial.begin(115200);

  Wire.begin(21, 22);

  lcd.init();
  lcd.backlight();

  updateLCD("ESP32 Guitar FX", "Starting...");

  WiFi.mode(WIFI_AP);
  WiFi.softAP(ssid, password);

  delay(1000);

  updateLCD("WiFi:ESP32", "IP:192.168.4.1");

  server.on("/", handleRoot);
  server.on("/lcd", handleLCD);
  server.begin();

  Serial.println("ESP32 Guitar FX LCD Control Started");
  Serial.println(WiFi.softAPIP());
}

void loop() {
  server.handleClient();
}
