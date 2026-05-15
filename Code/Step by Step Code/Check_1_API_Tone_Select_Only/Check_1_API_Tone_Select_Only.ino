#include <WiFi.h>
#include <WebServer.h>

const char* ssid = "ESP32_GUITAR";
const char* password = "12345678";

WebServer server(80);

const char MAIN_page[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1.0">
<title>ESP32 Guitar FX</title>

<style>
body{
  font-family: Arial, sans-serif;
  text-align:center;
  background:#111;
  color:white;
  padding:20px;
}
h2{ color:#00ff99; }
button{
  padding:14px 22px;
  margin:8px;
  font-size:16px;
  border:none;
  border-radius:10px;
  background:#00ff99;
  color:#111;
  font-weight:bold;
}
.status{
  margin-top:20px;
  padding:12px;
  background:#222;
  border-radius:10px;
}
</style>
</head>

<body>

<h2>🎸 ESP32 Guitar Tone Selector</h2>

<button onclick="startMic()">Start Mic</button>
<br><br>

<button onclick="clean()">Clean</button>
<button onclick="distortion()">Distortion</button>
<button onclick="rock()">Rock</button>
<button onclick="bass()">Bass</button>
<button onclick="echo()">Echo</button>
<button onclick="reverb()">Reverb</button>

<div class="status" id="status">Status: Ready</div>

<script>
let audioContext;
let mic;
let source;
let activeNodes = [];

function setStatus(text){
  document.getElementById("status").innerHTML = "Status: " + text;
}

function clearAudio(){
  activeNodes.forEach(node => {
    try { node.disconnect(); } catch(e){}
  });

  if(source){
    try { source.disconnect(); } catch(e){}
  }

  activeNodes = [];
}

async function startMic(){
  try{
    setStatus("Start button clicked...");

    audioContext = new (window.AudioContext || window.webkitAudioContext)();
    await audioContext.resume();

    if(!navigator.mediaDevices || !navigator.mediaDevices.getUserMedia){
      alert("Mic API not supported in this browser");
      setStatus("Mic API not supported");
      return;
    }

    const stream = await navigator.mediaDevices.getUserMedia({
      audio:{
        echoCancellation:false,
        noiseSuppression:false,
        autoGainControl:false
      }
    });

    source = audioContext.createMediaStreamSource(stream);
    mic = source;

    clean();

    alert("Mic Access Granted");
    setStatus("Mic ON - Clean Mode");

  }catch(err){
    alert("Mic Error: " + err.message);
    setStatus("Mic Error: " + err.message);
  }
}

function checkMic(){
  if(!audioContext || !mic){
    alert("First click Start Mic");
    return false;
  }
  return true;
}

function clean(){
  if(!checkMic()) return;

  clearAudio();

  let gain = audioContext.createGain();
  gain.gain.value = 1.2;

  mic.connect(gain);
  gain.connect(audioContext.destination);

  activeNodes = [gain];
  setStatus("Clean Guitar Tone");
}

function makeDistortionCurve(amount){
  let samples = 44100;
  let curve = new Float32Array(samples);
  let deg = Math.PI / 180;

  for(let i = 0; i < samples; ++i){
    let x = i * 2 / samples - 1;
    curve[i] = ((3 + amount) * x * 20 * deg) / (Math.PI + amount * Math.abs(x));
  }

  return curve;
}

function distortion(){
  if(!checkMic()) return;

  clearAudio();

  let shaper = audioContext.createWaveShaper();
  shaper.curve = makeDistortionCurve(700);
  shaper.oversample = "4x";

  let gain = audioContext.createGain();
  gain.gain.value = 0.8;

  mic.connect(shaper);
  shaper.connect(gain);
  gain.connect(audioContext.destination);

  activeNodes = [shaper, gain];
  setStatus("Distortion Guitar Tone");
}

function rock(){
  if(!checkMic()) return;

  clearAudio();

  let filter = audioContext.createBiquadFilter();
  filter.type = "highpass";
  filter.frequency.value = 250;

  let shaper = audioContext.createWaveShaper();
  shaper.curve = makeDistortionCurve(350);
  shaper.oversample = "4x";

  let gain = audioContext.createGain();
  gain.gain.value = 0.9;

  mic.connect(filter);
  filter.connect(shaper);
  shaper.connect(gain);
  gain.connect(audioContext.destination);

  activeNodes = [filter, shaper, gain];
  setStatus("Rock Guitar Tone");
}

function bass(){
  if(!checkMic()) return;

  clearAudio();

  let lowpass = audioContext.createBiquadFilter();
  lowpass.type = "lowpass";
  lowpass.frequency.value = 250;

  let gain = audioContext.createGain();
  gain.gain.value = 1.5;

  mic.connect(lowpass);
  lowpass.connect(gain);
  gain.connect(audioContext.destination);

  activeNodes = [lowpass, gain];
  setStatus("Bass Style Guitar Tone");
}

function echo(){
  if(!checkMic()) return;

  clearAudio();

  let delay = audioContext.createDelay();
  delay.delayTime.value = 0.35;

  let feedback = audioContext.createGain();
  feedback.gain.value = 0.35;

  let gain = audioContext.createGain();
  gain.gain.value = 0.9;

  mic.connect(gain);
  gain.connect(audioContext.destination);

  mic.connect(delay);
  delay.connect(feedback);
  feedback.connect(delay);
  delay.connect(audioContext.destination);

  activeNodes = [delay, feedback, gain];
  setStatus("Echo Guitar Tone");
}

function reverb(){
  if(!checkMic()) return;

  clearAudio();

  let delay1 = audioContext.createDelay();
  delay1.delayTime.value = 0.08;

  let delay2 = audioContext.createDelay();
  delay2.delayTime.value = 0.16;

  let delay3 = audioContext.createDelay();
  delay3.delayTime.value = 0.24;

  let gain1 = audioContext.createGain();
  gain1.gain.value = 0.45;

  let gain2 = audioContext.createGain();
  gain2.gain.value = 0.30;

  let gain3 = audioContext.createGain();
  gain3.gain.value = 0.20;

  mic.connect(audioContext.destination);

  mic.connect(delay1);
  delay1.connect(gain1);
  gain1.connect(audioContext.destination);

  mic.connect(delay2);
  delay2.connect(gain2);
  gain2.connect(audioContext.destination);

  mic.connect(delay3);
  delay3.connect(gain3);
  gain3.connect(audioContext.destination);

  activeNodes = [delay1, delay2, delay3, gain1, gain2, gain3];
  setStatus("Reverb Guitar Tone");
}
</script>

</body>
</html>
)rawliteral";

void handleRoot() {
  server.send_P(200, "text/html", MAIN_page);
}

void setup() {
  Serial.begin(115200);

  WiFi.mode(WIFI_AP);
  WiFi.softAP(ssid, password);

  Serial.println();
  Serial.println("ESP32 Guitar FX Started");
  Serial.print("WiFi Name: ");
  Serial.println(ssid);
  Serial.print("Password: ");
  Serial.println(password);
  Serial.print("Open IP: ");
  Serial.println(WiFi.softAPIP());

  server.on("/", handleRoot);
  server.begin();
}

void loop() {
  server.handleClient();
}
