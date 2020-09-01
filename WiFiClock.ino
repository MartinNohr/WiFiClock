/*
 Martin Nohr test programs for clock and weather
*/

#include <NTPClient.h>

#include <heltec.h>
#include <WiFi.h>
#include <ringbufcpp.h>

#define OLED Heltec.display

// time details
const long utcOffsetInSeconds = -6 * 3600;
char daysOfTheWeek[7][12] = { "Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday" };
// Define NTP Client to get time
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org", utcOffsetInSeconds, 3600);

#include "DHT.h"

#include <ThingerESP32.h>

#define DHTPIN 32
// rotary switch
#define BTNPUSH 27
#define BTNA 12
#define BTNB 14

#define LED 25

int buttons;
DHT dht(DHTPIN, DHT22);
#define MAX_KEY_BUF 10
RingBufCPP<int, MAX_KEY_BUF> btnBuf;
enum BUTTONS { BTN_UP, BTN_DOWN, BTN_SELECT, BTN_LONG, BTN_NONE };

ThingerESP32 thing("MartinNohr", "TempHum", "zHIyT&vMRt!d");

// interrupt handlers
void IRAM_ATTR IntBtnCenter()
{
	static unsigned long pressedTime = 0;
	noInterrupts();
	bool val = digitalRead(BTNPUSH);
	unsigned long currentTime = millis();
	int btn;
	if (currentTime > pressedTime + 20) {
		if (val == true) {
			if (currentTime > pressedTime + 750) {
				btn = BTN_LONG;
				btnBuf.add(btn);
				//Serial.println("long press");
			}
			else {
				btn = BTN_SELECT;
				btnBuf.add(btn);
				//Serial.println("press");
			}
		}
		//else {
		//	Serial.println("press");
		//}
		// got one, note time so we can ignore until ready again
		pressedTime = currentTime;
	}
	interrupts();
}

// state table for the rotary encoder
#define T true
#define F false
#define DIRECTIONS 2
#define MAXSTATE 4
#define CONTACTS 2
bool stateTest[DIRECTIONS][MAXSTATE][CONTACTS] =
{
	{{T,F},{F,F},{F,T},{T,T}},
	{{F,T},{F,F},{T,F},{T,T}}
};
#define A 0
#define B 1
#define ROTARY_RETRIES 10
void IRAM_ATTR IntBtnAB()
{
	static bool forward;
	static int state = 0;
	static int tries;
	static bool lastValA = true;
	static bool lastValB = true;
	noInterrupts();
	bool valA = digitalRead(BTNA);
	bool valB = digitalRead(BTNB);
	//Serial.println("A:" + String(valA) + " B:" + String(valB));
	//Serial.println("start state: " + String(state));
	//Serial.println("forward: " + String(forward));
	if (valA == lastValA && valB == lastValB)
		return;
	if (state == 0) {
		// starting
		// see if one of the first tests is correct, then go to state 1
		if (stateTest[0][state][A] == valA && stateTest[0][state][B] == valB) {
			//Serial.println("down");
			forward = false;
			tries = ROTARY_RETRIES;
			++state;
		}
		else if (stateTest[1][state][A] == valA && stateTest[1][state][B] == valB) {
			//Serial.println("up");
			forward = true;
			tries = ROTARY_RETRIES;
			++state;
		}
	}
	else {
		// check if we can advance
		if (stateTest[forward ? 1 : 0][state][A] == valA && stateTest[forward ? 1 : 0][state][B] == valB) {
			tries = ROTARY_RETRIES;
			++state;
		}
		else {
			//state = 0;
		}
	}
	//Serial.println("end state: " + String(state));
	//Serial.println("forward: " + String(forward));
	if (state == MAXSTATE) {
		// we're done
		//Serial.println(String(forward ? "+" : "-"));
		state = 0;
		int btn = forward ? BTN_UP : BTN_DOWN;
		btnBuf.add(btn);
	}
	else if ((tries-- <= 0 && state > 0) || (valA == true && valB == true)) {
		// something failed, start over
		//Serial.println("failed");
		//int btn = BTN_NONE;
		//btnBuf.add(btn);
		state = 0;
	}
	lastValA = valA;
	lastValB = valB;
	interrupts();
}

void WIFISetUp(void)
{
	// Set WiFi to station mode and disconnect from an AP if it was previously connected
	WiFi.disconnect(true);
	delay(1000);
	WiFi.mode(WIFI_STA);
	WiFi.setAutoConnect(true);
	WiFi.begin("NohrNet", "8017078120");
	delay(100);

	byte count = 0;
	OLED->clear();
	while (WiFi.status() != WL_CONNECTED && count < 10)
	{
		count++;
		delay(500);
		OLED->drawString(0, 0, "Connecting...");
		OLED->display();
	}

	OLED->clear();
	if (WiFi.status() == WL_CONNECTED)
	{
		OLED->drawString(0, 0, "Connecting...OK.");
		OLED->display();
		//		delay(500);
	}
	else
	{
		OLED->clear();
		OLED->drawString(0, 0, "Connecting...Failed");
		OLED->display();
		while (1);
	}
	OLED->drawString(0, 10, "WIFI Setup done");
	OLED->display();
	delay(500);
}

void WIFIScan(void)
{
	OLED->drawString(0, 20, "Scan start...");
	OLED->display();

	int n = WiFi.scanNetworks();
	OLED->drawString(0, 30, "Scan done");
	OLED->display();
	delay(500);
	OLED->clear();
	// find the ones with strength above -90
	for (int ix = 0; ix < n; ++ix) {
		if (WiFi.RSSI(ix) < -90) {
			n = ix;
			break;
		}
	}

	if (n == 0)
	{
		OLED->clear();
		OLED->drawString(0, 0, "no network found");
		OLED->display();
		while (1);
	}
	else
	{
		OLED->drawString(0, 0, (String)n);
		OLED->drawString(14, 0, "networks found:");
		OLED->display();
		delay(500);

		for (int i = 0; i < n; ++i) {
			if (i && ((i / 6) * 6 == i)) {
				delay(3000);
				OLED->clear();
			}
			// Print SSID and RSSI for each network found
            OLED->drawString(0, ((i % 6) + 1) * 9, (String)(i + 1));
            OLED->drawString(6, ((i % 6) + 1) * 9, ":");
            OLED->drawString(12, ((i % 6) + 1) * 9, (String)(WiFi.SSID(i)));
            OLED->drawString(90, ((i % 6) + 1) * 9, " (");
            OLED->drawString(98, ((i % 6) + 1) * 9, (String)(WiFi.RSSI(i)));
            OLED->drawString(114, ((i % 6) + 1) * 9, ")");
			//            display.println((WiFi.encryptionType(i) == WIFI_AUTH_OPEN)?" ":"*");
			OLED->display();
			delay(10);
		}
	}
	OLED->display();
	delay(800);
	OLED->clear();
}

void setup()
{
	Serial.begin(115200);
	pinMode(LED, OUTPUT);
	digitalWrite(LED, HIGH);
	pinMode(BTNPUSH, INPUT_PULLUP);
	pinMode(BTNA, INPUT_PULLUP);
	pinMode(BTNB, INPUT_PULLUP);
	pinMode(DHTPIN, INPUT_PULLUP);
	attachInterrupt(BTNPUSH, IntBtnCenter, CHANGE);
	attachInterrupt(BTNA, IntBtnAB, CHANGE);
	attachInterrupt(BTNB, IntBtnAB, CHANGE);
	dht.begin();
	thing.add_wifi("NohrNet", "8017078120");
	thing["dht22"] >> [](pson& out) {
		out["humidity"] = dht.readHumidity();
		out["temperature"] = dht.readTemperature(true);
	};
	thing["led"] << digitalPin(LED);
	Heltec.begin(true /*DisplayEnable Enable*/, false /*LoRa Enable*/, true /*Serial Enable*/);

	WIFISetUp();
	//WIFIScan();
	digitalWrite(LED, LOW);
	Serial.println("WiFi connected");
	Serial.println("IP address: ");
	Serial.println(WiFi.localIP());
	timeClient.begin();
	OLED->clear();
	pinMode(17, OUTPUT);
}

int timeZone = -6;
#define SECS_PER_HOUR 3600
bool showProgress = false;
void loop()
{
	timeClient.update();
	time_t timenow = timeClient.getEpochTime();
	struct tm* gtime;
	gtime = gmtime(&timenow);
	//Serial.println("year: " + String(gtime->tm_year + 1900));
	//Serial.println("month: " + String(gtime->tm_mon + 1));
	//Serial.println("day: " + String(gtime->tm_mday));
	
	//int touch = touchRead(TOUCH_PAD_NUM2_GPIO_NUM);
	//Serial.println(String(touch));
	//delay(1000);
    //int temp = temperatureRead();
    //Serial.println("temp:" + String(temp));
	OLED->clear();
	OLED->setFont(ArialMT_Plain_24);
	char line[50];
	int hour = gtime->tm_hour;
	const char* ampm = (hour < 13 ? "AM" : "PM");
	hour = hour % 12;
	if (hour == 0)
		hour = 12;
	sprintf(line, "%2d:%02d", hour, gtime->tm_min);
	OLED->drawString(0, 0, line);
	OLED->setFont(ArialMT_Plain_10);
	OLED->drawString(58, 0, ampm);
	OLED->setFont(ArialMT_Plain_16);
	sprintf(line, "%d/%d/%2d", gtime->tm_mon + 1, gtime->tm_mday, (gtime->tm_year + 1900) % 100);
	OLED->drawString(74, 8, line);
	OLED->setFont(ArialMT_Plain_16);
	//OLED->drawString(0, 45, daysOfTheWeek[gtime->tm_wday]);
	float hum = dht.readHumidity();
	if (hum > 0 && hum < 100) {
		float temp = dht.readTemperature(true);
		float ctemp = dht.convertFtoC(temp);
		sprintf(line, "%dF  %0.1fC   %d%%", (int)(temp + 0.5), ctemp, (int)(hum + 0.5));
	}
	else
		sprintf(line, "no temp sensor");
	OLED->drawString(0, 45, line);
	OLED->display();
	static int waitTime = 100;
	static int waitIncrement = 10;
	//OLED->drawString(25, 28, String(waitTime) + " +/-" + String(waitIncrement));
	for (int x = 0; x <= 10; ++x) {
		if (showProgress) {
			OLED->drawProgressBar(0, 33, 20, 6, x * 10);
			OLED->display();
		}
		thing.handle();
		for (int ix = 0; ix < waitTime; ++ix) {
			if (!btnBuf.isEmpty())
				break;
			delay(1);
		}
	}
	while (!btnBuf.isEmpty()) {
		int btn;
		btnBuf.pull(&btn);
		//Serial.println("button: " + String(btn));
		switch (btn) {
		case BTN_SELECT:
			showProgress = !showProgress;
			if (!showProgress) {
				OLEDDISPLAY_COLOR oldColor = OLED->getColor();
				OLED->setColor(BLACK);
				OLED->fillRect(0, 30, 21, 12);
				OLED->setColor(oldColor);
				OLED->display();
			}
			break;
		case BTN_UP:
		case BTN_DOWN:
			if (btn == BTN_UP)
				waitTime += waitIncrement;
			else
				waitTime -= waitIncrement;
			waitTime = constrain(waitTime, 0, 10000);
			break;
		case BTN_LONG:
			if (waitIncrement == 10)
				waitIncrement = 100;
			else
				waitIncrement = 10;
			break;
		}
	}
	//Serial.println(timeClient.getFormattedTime());
	//digitalWrite(LED, HIGH);   // turn the LED on (HIGH is the voltage level)
	//delay(2000);              // wait for a second
	//digitalWrite(LED, LOW);    // turn the LED off by making the voltage LOW
	//delay(2000);              // wait for a second
	//float hum = dht.readHumidity();
	//float temp = dht.readTemperature(true);
	//if (isnan(hum) || isnan(temp)) {
	//	Serial.println(F("Failed to read from DHT sensor!"));
	//	return;
	//}
	//Serial.println("Humidity: " + String(hum) + "% " + "Temperature: " + String(temp));
}
