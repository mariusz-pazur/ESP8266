#include <Adafruit_NeoPixel.h>

#include <FS.h>

#include <ArduinoJson.h>

#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <DNSServer.h>
#include <ESP8266WebServer.h>

#include <WiFiManager.h>         //https://github.com/tzapu/WiFiManager

#include <DHT.h>
#define DHTTYPE DHT22
#define DHTPIN  2
#define BUZZPIN 0
#define LEDSPIN 3 //RX
#define NUMPIXELS 7

char thingspeakApiUrl[] = "api.thingspeak.com";
char thingspeakParameters[61] = "/update?api_key=5C934TN5I8BRTLZN&field1=%s&field2=%s";
char static_ip[16] = "192.168.0.5";
char static_gw[16] = "192.168.0.1";
char static_sn[16] = "255.255.255.0";
//flag for saving data
bool shouldSaveConfig = false;
const long thingspeakRequestInterval = 60000;
unsigned long thingspeakPreviousMillis = 0;


ESP8266WebServer server(80);
WiFiManager wifiManager;
DHT dht(DHTPIN, DHTTYPE, 11); // 11 works fine for ESP8266

char str_temp[6];// Values read from sensor
char str_humid[6];
String webString = "";     // String to display
						   // Generally, you should use "unsigned long" for variables that hold time
unsigned long previousMillis = 0;        // will store last temp was read
const long dhtSensorInterval = 30000;              // interval at which to read sensor

Adafruit_NeoPixel pixels = Adafruit_NeoPixel(NUMPIXELS, LEDSPIN, NEO_GRB + NEO_KHZ800);

//callback notifying us of the need to save config
void saveConfigCallback()
{
	Serial.println("Should save config");
	shouldSaveConfig = true;
}

void handle_root() {
	server.send(200, "text/plain", "Hello from the weather esp8266, read from /temp or /humidity. Reset settings - /reset");
	delay(100);
}

void setup()
{
	// put your setup code here, to run once:
	Serial.begin(115200);
	dht.begin();
	//read configuration from FS json
	Serial.println("mounting FS...");
	if (SPIFFS.begin())
	{
		Serial.println("mounted file system");
		if (SPIFFS.exists("/config.json"))
		{
			//file exists, reading and loading
			Serial.println("reading config file");
			File configFile = SPIFFS.open("/config.json", "r");
			if (configFile)
			{
				Serial.println("opened config file");
				size_t size = configFile.size();
				// Allocate a buffer to store contents of the file.

				std::unique_ptr<char[]> buf(new char[size]);
				configFile.readBytes(buf.get(), size);
				DynamicJsonBuffer jsonBuffer;
				JsonObject& json = jsonBuffer.parseObject(buf.get());
				json.printTo(Serial);
				if (json.success())
				{
					Serial.println("\nparsed json");
					strcpy(thingspeakApiUrl, json["thingspeakApiUrl"]);
					strcpy(thingspeakParameters, json["thingspeakParameters"]);
					if (json["ip"])
					{
						Serial.println("setting custom ip from config");
						strcpy(static_ip, json["ip"]);
						strcpy(static_gw, json["gateway"]);
						strcpy(static_sn, json["subnet"]);
						Serial.println(static_ip);

					}
					else
					{
						Serial.println("no custom ip in config");
					}
				}
				else
				{
					Serial.println("failed to load json config");
				}
			}
		}
	}
	else
	{
		Serial.println("failed to mount FS");
	}

	Serial.println(static_ip);
	Serial.println(thingspeakApiUrl);
	Serial.println(thingspeakParameters);

	WiFiManagerParameter custom_thingspeak_api_url("server", "thingspeakApiUrl", thingspeakApiUrl, 19);
	WiFiManagerParameter custom_thingspeak_parameters("params", "thingspeakParameters", thingspeakParameters, 61);
	wifiManager.addParameter(&custom_thingspeak_api_url);
	wifiManager.addParameter(&custom_thingspeak_parameters);
	//reset saved settings
	//wifiManager.resetSettings();

	wifiManager.setSaveConfigCallback(saveConfigCallback);
	//set static ip
	IPAddress _ip, _gw, _sn;
	_ip.fromString(static_ip);
	_gw.fromString(static_gw);
	_sn.fromString(static_sn);

	wifiManager.setSTAStaticIPConfig(_ip, _gw, _sn);
	//fetches ssid and pass from eeprom and tries to connect
	//if it does not connect it starts an access point with the specified name
	//and goes into a blocking loop awaiting configuration
	wifiManager.autoConnect("ESP8266AP");

	//if you get here you have connected to the WiFi
	Serial.println("connected...yeey :)");

	strcpy(thingspeakApiUrl, custom_thingspeak_api_url.getValue());
	strcpy(thingspeakParameters, custom_thingspeak_parameters.getValue());

	if (shouldSaveConfig) {
		Serial.println("saving config");
		DynamicJsonBuffer jsonBuffer;
		JsonObject& json = jsonBuffer.createObject();
		json["thingspeakApiUrl"] = thingspeakApiUrl;
		json["thingspeakParameters"] = thingspeakParameters;
		json["ip"] = WiFi.localIP().toString();
		json["gateway"] = WiFi.gatewayIP().toString();
		json["subnet"] = WiFi.subnetMask().toString();
		File configFile = SPIFFS.open("/config.json", "w");
		if (!configFile) {
			Serial.println("failed to open config file for writing");
		}
		json.prettyPrintTo(Serial);
		json.printTo(configFile);
		configFile.close();
		//end save
	}

	Serial.println("local ip");
	Serial.println(WiFi.localIP());
	Serial.println(WiFi.gatewayIP());
	Serial.println(WiFi.subnetMask());

	server.on("/", handle_root);

	server.on("/temp", []() {  // if you add this subdirectory to your webserver call, you get text below :)		
		webString = "Temperature: " + String(str_temp) + " C";   // Arduino has a hard time with float to string
		server.send(200, "text/plain", webString);            // send to someones browser when asked
	});

	server.on("/humidity", []() {  // if you add this subdirectory to your webserver call, you get text below :)		
		webString = "Humidity: " + String(str_humid) + "%";
		server.send(200, "text/plain", webString);               // send to someones browser when asked
	});

	server.on("/reset", []() {
		wifiManager.resetSettings();
		ESP.reset();
		delay(5000);
	});
	server.on("/tone", []() {
		tone(BUZZPIN, 500, 500);
	});
  server.on("/leds", []() {
    for(int i=0;i<NUMPIXELS;i++)
    {
      pixels.setPixelColor(i, pixels.Color(0,150,0)); // Moderately bright green color.
      pixels.show(); // This sends the updated pixel color to the hardware.
      delay(50); // Delay for a period of time (in milliseconds).
  }
  });
	server.begin();
}

//void sendThingspeakRequestHttpClient()
//{
//	unsigned long currentMillis = millis();
//	if (currentMillis - thingspeakPreviousMillis >= thingspeakRequestInterval)
//	{
//		thingspeakPreviousMillis = currentMillis;
//		HTTPClient http;
//		Serial.println("[HTTP] begin...\n");
//		// configure traged server and url
//		//http.begin("https://192.168.1.12/test.html", "7a 9c f4 db 40 d3 62 5a 6e 21 bc 5c cc 66 c8 3e a1 45 59 38"); //HTTPS
//    char request[100];
//    sprintf(request, "http://%s%s"
//		http.begin("http://192.168.1.12/test.html"); //HTTP
//		USE_SERIAL.print("[HTTP] GET...\n");
//		// start connection and send HTTP header
//		int httpCode = http.GET();
//		// httpCode will be negative on error
//		if (httpCode > 0) {
//			// HTTP header has been send and Server response header has been handled
//			USE_SERIAL.printf("[HTTP] GET... code: %d\n", httpCode);
//			// file found at server
//			if (httpCode == HTTP_CODE_OK) {
//				String payload = http.getString();
//				USE_SERIAL.println(payload);
//			}
//		}
//		else {
//			USE_SERIAL.printf("[HTTP] GET... failed, error: %s\n", http.errorToString(httpCode).c_str());
//		}
//		http.end();
//	}
//}

void sendThingspeakRequest()
{
	unsigned long currentMillis = millis();
	if (currentMillis - thingspeakPreviousMillis >= thingspeakRequestInterval)
	{
		thingspeakPreviousMillis = currentMillis;
		WiFiClient client;
		const int httpPort = 80;
		if (!client.connect(thingspeakApiUrl, httpPort))
		{
			Serial.println("connection failed");
			return;
		}
		// We now create a URI for the request
    char parametersBuffer[61];
		sprintf(parametersBuffer, thingspeakParameters, str_temp, str_humid);
		String parameters(parametersBuffer);
		String url(thingspeakApiUrl);
		Serial.print("Requesting URL: ");
		Serial.println(url);
		// This will send the request to the server
		client.print(String("GET ") + parameters + " HTTP/1.1\r\n" +
			"Host: " + url + "\r\n" +
			"Connection: close\r\n\r\n");
		int timeout = millis() + 5000;
		while (client.available() == 0)
		{
			if (timeout - millis() < 0)
			{
				Serial.println(">>> Client Timeout !");
				client.stop();
				return;
			}
		}
		// Read all the lines of the reply from server and print them to Serial
		while (client.available())
		{
			String line = client.readStringUntil('\r');
			Serial.print(line);
		}
		Serial.println();
		Serial.println("closing connection");
	}
}

void gettemperature()
{
	unsigned long currentMillis = millis();
	if (currentMillis - previousMillis >= dhtSensorInterval)
	{
		previousMillis = currentMillis;
		float h = dht.readHumidity();          // Read humidity (percent)
		float t = dht.readTemperature();     // Read temperature as Fahrenheit
		if (isnan(h) || isnan(t))
		{
			Serial.println("Failed to read from DHT sensor!");
			return;
		}
		Serial.println("DHT sensor read");
		dtostrf(t, 4, 2, str_temp);
		dtostrf(h, 4, 2, str_humid);
	}
}

void loop()
{
	// put your main code here, to run repeatedly:
	server.handleClient();
	sendThingspeakRequest();
	gettemperature();
}
