#include <FS.h>
#include <ArduinoJson.h>
#include <ESP8266WiFi.h>
#include <WiFiManager.h>         
#include <DHT.h>

#define DEBUG   //If you comment this line, the DPRINT & DPRINTLN lines are defined as blank.
#ifdef DEBUG    //Macros are usually in all capital letters.
#define DPRINT(...)    Serial.print(__VA_ARGS__)     //DPRINT is a macro, debug print
#define DPRINTLN(...)  Serial.println(__VA_ARGS__)   //DPRINTLN is a macro, debug print with new line
#else
#define DPRINT(...)     //now defines a blank line
#define DPRINTLN(...)   //now defines a blank line
#endif

char thingspeakApiUrl[] = "api.thingspeak.com";
char thingspeakParameters[] = "/update?api_key=OL9BLT52MX7C5PZ6&field1=%s&field2=%s&field3=%s&field4=%s&field5=%s&field6=%s&field7=%s&field8=%s";

WiFiManager wifiManager;
bool shouldSaveConfig = false;

DHT dht4(D4, DHT22, 11); // 11 works fine for ESP8266
DHT dht3(D3, DHT22, 11);
DHT dht2(D2, DHT22, 11);
DHT dht1(D1, DHT22, 11);

char str_temp1[6];
char str_humid1[6];
char str_temp2[6];
char str_humid2[6];
char str_temp3[6];
char str_humid3[6];
char str_temp4[6];
char str_humid4[6];

const unsigned long sleepIntervalInMicro = 55 * 1000000;
//callback notifying us of the need to save config
void saveConfigCallback()
{
	DPRINTLN("Should save config");
	shouldSaveConfig = true;
}

void gettemperature()
{	
	float h = dht1.readHumidity();          // Read humidity (percent)
  float t = dht1.readTemperature();     // Read temperature as Fahrenheit
  if (isnan(h) || isnan(t))
  {
    DPRINTLN("Failed to read from DHT1 sensor!");    
  }
  else
  {
    DPRINTLN("DHT1 sensor read");
    dtostrf(t, 4, 2, str_temp1);
    dtostrf(h, 4, 2, str_humid1);
  }

  h = dht2.readHumidity();          // Read humidity (percent)
  t = dht2.readTemperature();     // Read temperature as Fahrenheit
  if (isnan(h) || isnan(t))
  {
    DPRINTLN("Failed to read from DHT2 sensor!");    
  }
  else
  {
    DPRINTLN("DHT2 sensor read");
    dtostrf(t, 4, 2, str_temp2);
    dtostrf(h, 4, 2, str_humid2);
  }

  h = dht3.readHumidity();          // Read humidity (percent)
  t = dht3.readTemperature();     // Read temperature as Fahrenheit
  if (isnan(h) || isnan(t))
  {
    DPRINTLN("Failed to read from DHT3 sensor!");    
  }
  else
  {
    DPRINTLN("DHT3 sensor read");
    dtostrf(t, 4, 2, str_temp3);
    dtostrf(h, 4, 2, str_humid3);
  }

  h = dht4.readHumidity();          // Read humidity (percent)
  t = dht4.readTemperature();     // Read temperature as Fahrenheit
  if (isnan(h) || isnan(t))
  {
    DPRINTLN("Failed to read from DHT4 sensor!");    
  }
  else
  {
    DPRINTLN("DHT4 sensor read");
    dtostrf(t, 4, 2, str_temp4);
    dtostrf(h, 4, 2, str_humid4);
  }
}

void sendThingspeakRequest()
{
	WiFiClient client;
	const int httpPort = 80;
	if (!client.connect(thingspeakApiUrl, httpPort))
	{
		DPRINTLN("connection failed");
		return;
	}
	// We now create a URI for the request
	char parametersBuffer[145];
	sprintf(parametersBuffer, thingspeakParameters, str_temp1, str_humid1 , str_temp2, str_humid2, str_temp3, str_humid3, str_temp4, str_humid4);
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
}

void setup()
{
  //wifiManager.resetSettings();  
#ifdef DEBUG
	Serial.begin(115200);
#endif
	dht1.begin();
	dht2.begin();
	dht3.begin();
	dht4.begin();
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
	
	wifiManager.setSaveConfigCallback(saveConfigCallback);
	wifiManager.autoConnect("ESP8266AP");
	//if you get here you have connected to the WiFi
	DPRINTLN("connected...yeey :)");
	
	if (shouldSaveConfig)
	{
		DPRINTLN("saving config");
		DynamicJsonBuffer jsonBuffer;
		JsonObject& json = jsonBuffer.createObject();
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
