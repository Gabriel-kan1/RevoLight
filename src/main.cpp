#include "secrets.h"
#include <WiFi.h>

const int redLight    = 34;
const int yellowLight = 35;
const int greenLight  = 32;
const int ADC_MAX     = 4095;
const int threshold   = 3800;

bool redOn;
bool yellowOn;
bool greenOn;

WiFiServer server(80);

void setup() {
	Serial.begin(115200);

	pinMode(redLight, INPUT);
	pinMode(yellowLight, INPUT);
	pinMode(greenLight, INPUT);

	Serial.println("\nConnecting to WiFi...");
	WiFi.begin(ssid, password);

	// Wait for connection with IP assignment (up to 10s)
	unsigned long startTime = millis();
	while (millis() - startTime < 10000) {
		if (WiFi.status() == WL_CONNECTED) {
			IPAddress ip = WiFi.localIP();
			// Check if IP is valid (not 0.0.0.0)
			if (ip != INADDR_NONE && ip[0] != 0) {
				break; // Valid IP obtained
			}
		}
		delay(250);
		Serial.print(".");
	}

	delay(500);
	if (WiFi.status() == WL_CONNECTED) {
		IPAddress ip = WiFi.localIP();
		if (ip != INADDR_NONE && ip[0] != 0) {
			Serial.println("\nConnected! IP: ");
			Serial.println(ip);
			server.begin();
		} else {
			Serial.println("\nConnected but IP not assigned yet");
		}
	} else {
		Serial.println("\nNot connected — will reconnect in background");
	}
}

void loop() {
	// Check WiFi connection status periodically and start server when ready
	static unsigned long lastCheck = 0;
	static bool serverStarted = false;
	if (millis() - lastCheck > 3000) {
		lastCheck = millis();
		if (WiFi.status() != WL_CONNECTED) {
			Serial.println("WiFi not ready — attempting reconnect...");
			WiFi.begin(ssid, password);
			serverStarted = false; // Reset on disconnect
		} else if (!serverStarted) {
			server.begin();
			serverStarted = true;
		}
	}

	WiFiClient client = server.available();
	if (!client) return;

	unsigned long startTime = millis();
	const unsigned long timeout = 2000;

	while (!client.available()) {
		if (!client.connected() || millis() - startTime > timeout) {
			client.stop();
			return;
		}
		delay(1);
	}

	// Read full request headers to avoid socket corruption
	String header = "";
	unsigned long hdrStart = millis();
	while (client.connected() && millis() - hdrStart < timeout) {
		String line = client.readStringUntil('\n');
		if (line.length() <= 1) break; // blank line = end of headers
		header += line;
	}

	// Extract path from first request line
	String path = "/";
	int crPos = header.indexOf('\r');
	if (crPos != -1) {
		String firstLine = header.substring(0, crPos);
		int p1 = firstLine.indexOf(' ');
		int p2 = firstLine.indexOf(' ', p1 + 1);
		if (p1 != -1 && p2 != -1) path = firstLine.substring(p1 + 1, p2);
	}

	// Quickly handle favicon requests
	if (path == "/favicon.ico") {
		client.println("HTTP/1.1 204 No Content");
		client.println("Connection: close");
		client.println();
		client.stop();
		return;
	}

	int redValue = analogRead(redLight);
	int yellowValue = analogRead(yellowLight);
	int greenValue = analogRead(greenLight);

	redOn = redValue > threshold;
	yellowOn = yellowValue > threshold;
	greenOn = greenValue > threshold;

	String redStat = redOn ? "ON" : "OFF";
	String yellowStat = yellowOn ? "ON" : "OFF";
	String greenStat = greenOn ? "ON" : "OFF";

	String status = "";
	if (!redOn && !yellowOn && !greenOn) {
		status = "no light ON";
	} else {
		bool first = true;
		if (redOn) { if (!first) status += "+"; status += "Red"; first = false; }
		if (yellowOn) { if (!first) status += "+"; status += "Yellow"; first = false; }
		if (greenOn) { if (!first) status += "+"; status += "Green"; first = false; }
		status += " ON";
	}

	// ---------- DO NOT PRINT ANYTHING TO SERIAL HERE ----------
	// Serial printing during HTTP response causes corruption.

	if (path == "/adc") {
		client.println("HTTP/1.1 200 OK");
		client.println("Content-Type: text/plain");
		client.println("Connection: close");
		client.println("Cache-Control: no-cache");
		client.println();
		client.print(redValue); client.print(",");
		client.print(yellowValue); client.print(",");
		client.print(greenValue); client.print(",");
		client.print(status); client.print(",");
		client.print(redStat); client.print(",");
		client.print(yellowStat); client.print(",");
		client.print(greenStat); client.print(",");
		client.println(ADC_MAX);
		client.flush();
		client.stop();
		return;
	}

	// ---------- HTML ----------
	client.println("HTTP/1.1 200 OK");
	client.println("Content-Type: text/html");
	client.println("Connection: close");
	client.println("Cache-Control: no-cache");
	client.println();
	client.println("<!DOCTYPE html><html><head><style>");
	client.println("body{font-family:Arial;margin:20px;}");
	client.println(".light{margin:15px 0;padding:10px;border-radius:5px;}");
	client.println(".bar{width:300px;height:30px;background:#ddd;border-radius:5px;overflow:hidden;}");
	client.println(".fill{height:100%;background-color:#999;transition:width 0.1s;}");
	client.println(".red-light .fill{background-color:#ff4444;}");
	client.println(".yellow-light .fill{background-color:#ffdd44;}");
	client.println(".green-light .fill{background-color:#44ff44;}");
	client.println(".status{font-size:24px;font-weight:bold;margin-top:20px;padding:20px;text-align:center;border-radius:5px;}");
	client.println(".conn-status{font-size:12px;color:#666;margin-top:20px;}");
	client.println("</style></head><body>");
	client.println("<h2>Traffic Light ADC Monitor</h2>");
	client.println("<div class='light red-light'>");
	client.println("<p>Red: <span id='red'>0</span></p>");
	client.println("<div class='bar'><div class='fill' id='red-bar' style='width:0%'></div></div>");
	client.println("</div>");
	client.println("<div class='light yellow-light'>");
	client.println("<p>Yellow: <span id='yellow'>0</span></p>");
	client.println("<div class='bar'><div class='fill' id='yellow-bar' style='width:0%'></div></div>");
	client.println("</div>");
	client.println("<div class='light green-light'>");
	client.println("<p>Green: <span id='green'>0</span></p>");
	client.println("<div class='bar'><div class='fill' id='green-bar' style='width:0%'></div></div>");
	client.println("</div>");
	client.println("<div class='status' id='status'>--</div>");
	client.println("<div class='conn-status'>Status: <span id='conn'>Connecting...</span></div>");
	client.println("<script>");
	client.println("let retries=0;");
	client.println("function fetchData(){fetch('/adc').then(r=>{");
	client.println("if(r.ok){document.getElementById('conn').innerText='Connected';retries=0;}");
	client.println("return r.text();}).then(data=>{");
	client.println("let p=data.split(',');let r=parseInt(p[0]),y=parseInt(p[1]),g=parseInt(p[2]),max=parseInt(p[7]);");
	client.println("document.getElementById('red').innerText=r+' ('+p[4]+') / '+r+'/'+max;");
	client.println("document.getElementById('red-bar').style.width=(r/max*100)+'%';");
	client.println("document.getElementById('yellow').innerText=y+' ('+p[5]+') / '+y+'/'+max;");
	client.println("document.getElementById('yellow-bar').style.width=(y/max*100)+'%';");
	client.println("document.getElementById('green').innerText=g+' ('+p[6]+') / '+g+'/'+max;");
	client.println("document.getElementById('green-bar').style.width=(g/max*100)+'%';");
	client.println("document.getElementById('status').innerText=p[3].replace(/\\+/g, ', ');");
	client.println("}).catch(e=>{retries++;");
	client.println("document.getElementById('conn').innerText='Reconnecting... ('+retries+')';});}");
	client.println("setInterval(fetchData,500);fetchData();");
	client.println("</script></body></html>");
}
