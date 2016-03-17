#include <NeoPixelBus.h>
#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <ESP8266WebServer.h>
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


WiFiManager wifiManager;
char static_ip[16] = "192.168.0.5";
char static_gw[16] = "192.168.0.1";
char static_sn[16] = "255.255.255.0";

char thingspeakApiUrl[] = "api.thingspeak.com";
char thingspeakParameters[61] = "/update?api_key=5C934TN5I8BRTLZN&field1=%s&field2=%s";
const long thingspeakRequestInterval = 60000;
unsigned long thingspeakPreviousMillis = 0;

ESP8266WebServer server(80);
String webString = "";

const byte dhtPin = 2;
DHT dht(dhtPin, DHT22, 11); // 11 works fine for ESP8266
char str_temp[6];
char str_humid[6];
     
unsigned long dhtPreviousReadMillis = 0;        // will store last temp was read
const long dhtSensorInterval = 30000;              // interval at which to read sensor

const byte ledsPin = 3;
const byte numPixels = 33;
NeoPixelBus strip = NeoPixelBus(numPixels, ledsPin);


void handle_root() {
	server.send(200, "text/plain", "Hello from the weather esp8266, read from /temp or /humidity. Reset settings - /reset.Leds control - /leds");
	delay(100);
}

void setup()
{	
#ifdef DEBUG
	Serial.begin(115200);
#endif
	dht.begin();

	strip.Begin();
	strip.Show();

	DPRINTLN(static_ip);
	DPRINTLN(thingspeakApiUrl);
	DPRINTLN(thingspeakParameters);
	
	//set static ip
	IPAddress _ip, _gw, _sn;
	_ip.fromString(static_ip);
	_gw.fromString(static_gw);
	_sn.fromString(static_sn);

	wifiManager.setSTAStaticIPConfig(_ip, _gw, _sn);
	wifiManager.autoConnect("ESP8266AP");
	DPRINTLN("connected...yeey :)");


	DPRINTLN("local ip");
	DPRINTLN(WiFi.localIP());
	DPRINTLN(WiFi.gatewayIP());
	DPRINTLN(WiFi.subnetMask());

	server.on("/", handle_root);

	server.on("/temp", []() {  // if you add this subdirectory to your webserver call, you get text below :)		
		webString = "Temperature: " + String(str_temp) + " C";   // Arduino has a hard time with float to string
		server.send(200, "text/plain", webString);            // send to someones browser when asked
	});

	server.on("/humidity", []() {  		
		webString = "Humidity: " + String(str_humid) + "%";
		server.send(200, "text/plain", webString);               
	});

	server.on("/reset", []() {
		wifiManager.resetSettings();
		ESP.reset();
		delay(5000);
	});

	server.on("/leds", []() {
		DPRINTLN("LEDS");
   String r = server.arg("r");
   String g = server.arg("g");
   String b = server.arg("b");
		for (int i = 0; i < numPixels; i++)
		{
			strip.SetPixelColor(i, RgbColor(r.toInt(), g.toInt(), b.toInt())); 
			strip.Show(); 
			delay(50); 
		}
		server.send(200, "text/plain", "OK");
	});
	server.begin();
}

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
	}
}

void gettemperature()
{
	unsigned long currentMillis = millis();
	if (currentMillis - dhtPreviousReadMillis >= dhtSensorInterval)
	{
		dhtPreviousReadMillis = currentMillis;
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
	}
}

void loop()
{	
	server.handleClient();
	sendThingspeakRequest();
	gettemperature();
}
