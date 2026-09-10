// #include <FS.h>
// #include <WebServer.h>

#pragma once

// ----- Variables from WiFi.ino -----
extern WiFiNet availableNetworks[];
extern int netCount;

// ----- Functions from WiFi.ino -----
void scanWiFiNetworks();
bool connectToWiFiNetwork(String ssid, String password);
String getCurrentWifiNetwork();

// ----- Functions from Menus.ino -----
void drawMainScreen();
void drawWifiConnectScreen();
void updateMainMenuValues();

// ----- Functions from Popups.ino -----
void showPopup(String message);
void clearPopup();
String onScreenKeyboard(int boxX, int boxY, int boxW, int boxH);

// ----- Functions from Buttons.ino -----
void Buttons(int x, int y);

void handleStatus() {

  String json = "{";

  json += "\"thrust\":" + String(thrust,1) + ",";
  json += "\"voltage\":" + String(voltage,1) + ",";
  json += "\"current\":" + String(current,1) + ",";
  json += "\"throttle\":" + String(throttle) + ",";
  json += "\"testTime\":" + String(testTime);

  json += "}";

  server.send(200, "application/json", json);
}

void handleTare() {

  Serial.println("Tare Load Cell");

  server.send(200, "text/plain", "OK");
}

void handleSetTestTime() {

  if (server.hasArg("time")) {

    testTime = server.arg("time").toInt();

    Serial.print("Test Time -> ");
    Serial.println(testTime);
  }

  server.send(200, "text/plain", "OK");
}

void handleScanWifi() {

  scanWiFiNetworks();

  String json = "[";

  for (int i=0; i<netCount; i++) {

    json += "{";
    json += "\"ssid\":\"" + availableNetworks[i].ssid + "\",";
    json += "\"rssi\":" + String(availableNetworks[i].rssi);
    json += "}";

    if (i < netCount-1)
      json += ",";
  }

  json += "]";

  server.send(200, "application/json", json);
}

void handleConnectWifi() {

  String ssid = server.arg("ssid");
  String password = server.arg("password");

  bool success =
      connectToWiFiNetwork(ssid, password);

  server.send(
      200,
      "text/plain",
      success ? "SUCCESS" : "FAILED"
  );
}

void handleRoot()
{
  String page = R"rawliteral(
<!DOCTYPE html>
<html>
<head>

<meta name="viewport"
      content="width=device-width,initial-scale=1">

<title>Thrust Stand</title>

<style>

body{
    font-family:Arial;
    max-width:600px;
    margin:auto;
    padding:20px;
}

.card{
    border:1px solid #ccc;
    padding:15px;
    margin:10px;
}

button{
    width:100%;
    padding:12px;
    margin-top:10px;
}

</style>

</head>
<body>

<h1>Thrust Test Stand</h1>

<div class="card">

<p>Throttle: <span id="throttle">0</span></p>

<p>Thrust: <span id="thrust">0</span></p>

<p>Voltage: <span id="voltage">0</span></p>

<p>Current: <span id="current">0</span></p>

</div>

<div class="card">

<button onclick="tare()">
Tare Load Cell
</button>

</div>

<div class="card">

<select id="testTime">

<option value="5">5 sec</option>
<option value="10">10 sec</option>
<option value="20">20 sec</option>
<option value="30">30 sec</option>
<option value="60">60 sec</option>

</select>

<button onclick="setTime()">
Set Test Time
</button>

</div>

<div class="card">

<h3>WiFi</h3>

<button onclick="scanWifi()">
Scan Networks
</button>

<div id="wifiList"></div>

</div>

<script>

function update(){

fetch('/status')
.then(r=>r.json())
.then(data=>{

document.getElementById('throttle').innerText=data.throttle;
document.getElementById('thrust').innerText=data.thrust;
document.getElementById('voltage').innerText=data.voltage;
document.getElementById('current').innerText=data.current;

});

}

function tare(){

fetch('/tare');

}

function setTime(){

let t=
document.getElementById('testTime').value;

fetch('/setTestTime?time='+t);

}

function scanWifi(){

fetch('/scanWifi')
.then(r=>r.json())
.then(list=>{

let html='';

for(let net of list){

html +=
'<button onclick="connectWifi(\''+
net.ssid+
'\')">'+
net.ssid+
' ('+
net.rssi+
')</button>';

}

document.getElementById('wifiList').innerHTML=html;

});

}

function connectWifi(ssid){

let pw=
prompt("Password for "+ssid);

if(pw==null)
return;

fetch(
'/connectWifi?ssid='+
encodeURIComponent(ssid)+
'&password='+
encodeURIComponent(pw)
);

}

setInterval(update,1000);

update();

</script>

</body>
</html>
)rawliteral";

server.send(200, "text/html", page);
}