#include <WiFiS3.h>

const char* ssid = "Revotech_2.4G";
const char* password = "16188075";

const int redLight = A0;
const int yellowLight = A1;
const int greenLight = A2;

// threshold-based per-light status
const int threshold = 500;

WiFiServer server(80);

void setup() {
	Serial.begin(115200);

	pinMode(redLight, INPUT);
	pinMode(yellowLight, INPUT);
	pinMode(greenLight, INPUT);

	Serial.println("Connecting to WiFi...");
	WiFi.begin(ssid, password);

	unsigned long startTime = millis();
	while (WiFi.status() != WL_CONNECTED) {
		delay(500);
		Serial.print(".");
		if (millis() - startTime > 20000) {
			Serial.println("\nFailed to connect within 20s");
			break;
		}
	}

	delay(500);
	if (WiFi.status() == WL_CONNECTED) {
		Serial.println("Connected! IP: ");
		Serial.println(WiFi.localIP());
		server.begin();
	} else {
		Serial.println("WiFi connection failed!");
	}
}

void loop() {
	// Check WiFi connection status periodically
	static unsigned long lastCheck = 0;
	if (millis() - lastCheck > 5000) {
		lastCheck = millis();
		if (WiFi.status() != WL_CONNECTED) {
			Serial.println("WiFi disconnected! Reconnecting...");
			WiFi.begin(ssid, password);
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

	String req = client.readStringUntil('\r');
	client.flush();

	int redValue = analogRead(redLight);
	int yellowValue = analogRead(yellowLight);
	int greenValue = analogRead(greenLight);

	bool redOn = redValue > threshold;
	bool yellowOn = yellowValue > threshold;
	bool greenOn = greenValue > threshold;

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

	if (req.indexOf("/adc") != -1) {
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
		client.println(greenStat);
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
	client.println("<p>Red: <span id='red'>0</span>/1023</p>");
	client.println("<div class='bar'><div class='fill' id='red-bar' style='width:0%'></div></div>");
	client.println("</div>");
	client.println("<div class='light yellow-light'>");
	client.println("<p>Yellow: <span id='yellow'>0</span>/1023</p>");
	client.println("<div class='bar'><div class='fill' id='yellow-bar' style='width:0%'></div></div>");
	client.println("</div>");
	client.println("<div class='light green-light'>");
	client.println("<p>Green: <span id='green'>0</span>/1023</p>");
	client.println("<div class='bar'><div class='fill' id='green-bar' style='width:0%'></div></div>");
	client.println("</div>");
	client.println("<div class='status' id='status'>--</div>");
	client.println("<div class='conn-status'>Status: <span id='conn'>Connecting...</span></div>");
	client.println("<script>");
	client.println("let retries=0;");
	client.println("function fetchData(){fetch('/adc').then(r=>{");
	client.println("if(r.ok){document.getElementById('conn').innerText='Connected';retries=0;}");
	client.println("return r.text();}).then(data=>{");
	client.println("let p=data.split(',');let r=parseInt(p[0]),y=parseInt(p[1]),g=parseInt(p[2]);");
	client.println("document.getElementById('red').innerText=p[0];");
	client.println("document.getElementById('red-bar').style.width=(r/1023*100)+'%';");
	client.println("document.getElementById('yellow').innerText=p[1];");
	client.println("document.getElementById('yellow-bar').style.width=(y/1023*100)+'%';");
	client.println("document.getElementById('green').innerText=p[2];");
	client.println("document.getElementById('green-bar').style.width=(g/1023*100)+'%';");
	client.println("document.getElementById('status').innerText=p[3];");
	client.println("}).catch(e=>{retries++;");
	client.println("document.getElementById('conn').innerText='Reconnecting... ('+retries+')';});}");
	client.println("setInterval(fetchData,200);fetchData();");
	client.println("</script></body></html>");
}
