#include <FS.h>
#include <ArduinoJson.h>
#include <ESP8266WiFi.h>
#include <WiFiManager.h>         
#include <DHT.h>

//#define DEBUG   //If you comment this line, the DPRINT & DPRINTLN lines are defined as blank.
#ifdef DEBUG    //Macros are usually in all capital letters.
#define DPRINT(...)    Serial.print(__VA_ARGS__)     //DPRINT is a macro, debug print
#define DPRINTLN(...)  Serial.println(__VA_ARGS__)   //DPRINTLN is a macro, debug print with new line
#else
#define DPRINT(...)     //now defines a blank line
#define DPRINTLN(...)   //now defines a blank line
#endif

char thingspeakApiUrl[] = "api.thingspeak.com";
char thingspeakParameters[61] = "/update?api_key=5C934TN5I8BRTLZN&field1=%s&field2=%s";
//const long thingspeakRequestInterval = 60000;
//unsigned long thingspeakPreviousMillis = 0;

WiFiManager wifiManager;
bool shouldSaveConfig = false;

DHT dht(2, DHT22, 11); // 11 works fine for ESP8266
char str_temp[6];
char str_humid[6];
//unsigned long dhtPreviousMillis = 0;        // will store last temp was read
//const long dhtSensorInterval = 30000;              // interval at which to read sensor

const unsigned long sleepIntervalInMicro = 55 * 1000000;
//callback notifying us of the need to save config
void saveConfigCallback()
{
	DPRINTLN("Should save config");
	shouldSaveConfig = true;
}

void gettemperature()
{
	//unsigned long currentMillis = millis();
	//if (currentMillis - dhtPreviousMillis >= dhtSensorInterval)
	//{
	//dhtPreviousMillis = currentMillis;
	float h = dht.readHumidity();          // Read humidity (percent)
	float t = dht.readTemperature();     // Read temperature as Fahrenheit
	if (isnan(h) || isnan(t))
	{
		DPRINTLN("Failed to read from DHT sensor!");
		return;
	}
	DPRINTLN("DHT sensor read");
	dtostrf(t, 4, 2, str_temp);
	dtostrf(h, 4, 2, str_humid);
	//}
}

void sendThingspeakRequest()
{
	//unsigned long currentMillis = millis();
	//if (currentMillis - thingspeakPreviousMillis >= thingspeakRequestInterval)
	//{
	//thingspeakPreviousMillis = currentMillis;
	WiFiClient client;
	const int httpPort = 80;
	if (!client.connect(thingspeakApiUrl, httpPort))
	{
		DPRINTLN("connection failed");
		return;
	}
	// We now create a URI for the request
	char parametersBuffer[61];
	sprintf(parametersBuffer, thingspeakParameters, str_temp, str_humid);
	String parameters(parametersBuffer);
	String url(thingspeakApiUrl);
	DPRINT("Requesting URL: ");
	DPRINTLN(url);
	// This will send the request to the server
	client.print(String("GET ") + parameters + " HTTP/1.1\r\n" +
		"Host: " + url + "\r\n" +
		"Connection: close\r\n\r\n");
	int timeout = millis() + 5000;
	while (client.available() == 0)
	{
		if (timeout - millis() < 0)
		{
			DPRINTLN(">>> Client Timeout !");
			client.stop();
			return;
		}
	}
	// Read all the lines of the reply from server and print them to Serial
	while (client.available())
	{
		String line = client.readStringUntil('\r');
		DPRINT(line);
	}
	DPRINTLN();
	DPRINTLN("closing connection");
	//}
}

void setup()
{
#ifdef DEBUG
	Serial.begin(115200);
#endif
	dht.begin();
	//read configuration from FS json
	DPRINTLN("mounting FS...");
	if (SPIFFS.begin())
	{
		DPRINTLN("mounted file system");
		if (SPIFFS.exists("/config.json"))
		{
			//file exists, reading and loading
			DPRINTLN("reading config file");
			File configFile = SPIFFS.open("/config.json", "r");
			if (configFile)
			{
				DPRINTLN("opened config file");
				size_t size = configFile.size();
				// Allocate a buffer to store contents of the file.

				std::unique_ptr<char[]> buf(new char[size]);
				configFile.readBytes(buf.get(), size);
				DynamicJsonBuffer jsonBuffer;
				JsonObject& json = jsonBuffer.parseObject(buf.get());
				json.printTo(Serial);
				if (json.success())
				{
					DPRINTLN("\nparsed json");
					strcpy(thingspeakApiUrl, json["thingspeakApiUrl"]);
					strcpy(thingspeakParameters, json["thingspeakParameters"]);
				}
				else
				{
					DPRINTLN("failed to load json config");
				}
			}
		}
	}
	else
	{
		DPRINTLN("failed to mount FS");
	}
	DPRINTLN(thingspeakApiUrl);
	DPRINTLN(thingspeakParameters);

	WiFiManagerParameter custom_thingspeak_api_url("server", "thingspeakApiUrl", thingspeakApiUrl, 19);
	WiFiManagerParameter custom_thingspeak_parameters("params", "thingspeakParameters", thingspeakParameters, 61);
	wifiManager.addParameter(&custom_thingspeak_api_url);
	wifiManager.addParameter(&custom_thingspeak_parameters);
	wifiManager.setSaveConfigCallback(saveConfigCallback);
	wifiManager.autoConnect("ESP8266AP");
	//if you get here you have connected to the WiFi
	DPRINTLN("connected...yeey :)");
	strcpy(thingspeakApiUrl, custom_thingspeak_api_url.getValue());
	strcpy(thingspeakParameters, custom_thingspeak_parameters.getValue());

	if (shouldSaveConfig)
	{
		DPRINTLN("saving config");
		DynamicJsonBuffer jsonBuffer;
		JsonObject& json = jsonBuffer.createObject();
		json["thingspeakApiUrl"] = thingspeakApiUrl;
		json["thingspeakParameters"] = thingspeakParameters;
		json["ip"] = WiFi.localIP().toString();
		json["gateway"] = WiFi.gatewayIP().toString();
		json["subnet"] = WiFi.subnetMask().toString();
		File configFile = SPIFFS.open("/config.json", "w");
		if (!configFile) {
			DPRINTLN("failed to open config file for writing");
		}
		json.prettyPrintTo(Serial);
		json.printTo(configFile);
		configFile.close();
		//end save
	}

	DPRINTLN("local ip");
	DPRINTLN(WiFi.localIP());
	DPRINTLN(WiFi.gatewayIP());
	DPRINTLN(WiFi.subnetMask());
	gettemperature();
	sendThingspeakRequest();
	ESP.deepSleep(sleepIntervalInMicro);
}



void loop()
{
	
}
