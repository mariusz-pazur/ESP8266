#include <ESP8266WiFi.h>          //https://github.com/esp8266/Arduino

//needed for library
#include <DNSServer.h>
#include <ESP8266WebServer.h>
#include <WiFiManager.h>         //https://github.com/tzapu/WiFiManager
#include <DHT.h>
#define DHTTYPE DHT22
#define DHTPIN  2

ESP8266WebServer server(80);
WiFiManager wifiManager;
DHT dht(DHTPIN, DHTTYPE, 11); // 11 works fine for ESP8266

float humidity, temp_f;  // Values read from sensor
String webString = "";     // String to display
						   // Generally, you should use "unsigned long" for variables that hold time
unsigned long previousMillis = 0;        // will store last temp was read
const long interval = 2000;              // interval at which to read sensor

void handle_root() {
	server.send(200, "text/plain", "Hello from the weather esp8266, read from /temp or /humidity");
	delay(100);
}

void gettemperature() {
	// Wait at least 2 seconds seconds between measurements.
	// if the difference between the current time and last time you read
	// the sensor is bigger than the interval you set, read the sensor
	// Works better than delay for things happening elsewhere also
	unsigned long currentMillis = millis();

	if (currentMillis - previousMillis >= interval) {
		// save the last time you read the sensor 
		previousMillis = currentMillis;

		// Reading temperature for humidity takes about 250 milliseconds!
		// Sensor readings may also be up to 2 seconds 'old' (it's a very slow sensor)
		humidity = dht.readHumidity();          // Read humidity (percent)
		temp_f = dht.readTemperature();     // Read temperature as Fahrenheit
											// Check if any reads failed and exit early (to try again).
		if (isnan(humidity) || isnan(temp_f)) {
			Serial.println("Failed to read from DHT sensor!");
			return;
		}
	}
}

void setup() {
	// put your setup code here, to run once:
	Serial.begin(115200);
	dht.begin();

	//reset saved settings
	//wifiManager.resetSettings();

	wifiManager.setSTAStaticIPConfig(IPAddress(192, 168, 0, 5), IPAddress(192, 168, 0, 1), IPAddress(255, 255, 255, 0));
	//fetches ssid and pass from eeprom and tries to connect
	//if it does not connect it starts an access point with the specified name
	//and goes into a blocking loop awaiting configuration
	wifiManager.autoConnect("ESP8266AP");

	//if you get here you have connected to the WiFi
	Serial.println("connected...yeey :)");
	server.on("/", handle_root);

	server.on("/temp", []() {  // if you add this subdirectory to your webserver call, you get text below :)
		gettemperature();       // read sensor
		webString = "Temperature: " + String((int)temp_f) + " C";   // Arduino has a hard time with float to string
		server.send(200, "text/plain", webString);            // send to someones browser when asked
	});

	server.on("/humidity", []() {  // if you add this subdirectory to your webserver call, you get text below :)
		gettemperature();           // read sensor
		webString = "Humidity: " + String((int)humidity) + "%";
		server.send(200, "text/plain", webString);               // send to someones browser when asked
	});

	server.on("/config", []() {
		WiFi.mode(WIFI_STA);
		if (!wifiManager.startConfigPortal("ESP8266AP"))
		{
			Serial.println("failed to connect and hit timeout");
			delay(3000);
			//reset and try again, or maybe put it to deep sleep
			ESP.reset();
			delay(5000);
		}
	});

	server.on("/reset", []() {
		wifiManager.resetSettings();
		ESP.reset();
		delay(5000);
	});
	server.begin();
}

void loop() {
	// put your main code here, to run repeatedly:
	server.handleClient();
}
