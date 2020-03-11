/*
 * HelTec Automation(TM) WIFI_Kit_32 factory test code, witch includ
 * follow functions:
 *
 * - Basic OLED function test;
 *
 * - Basic serial port test(in baud rate 115200);
 *
 * - LED blink test;
 *
 * - WIFI join and scan test;
 *
 * - Timer test and some other Arduino basic functions.
 *
 * by Aaron.Lee from HelTec AutoMation, ChengDu, China
 * ??????????????
 * www.heltec.cn
 *
 * this project also realess in GitHub:
 * https://github.com/HelTecAutomation/Heltec_ESP32
*/

#include <NTPClient.h>

#include "heltec.h"
#include "WiFi.h"
#include "images.h"

const long utcOffsetInSeconds = -6 * 3600;

char daysOfTheWeek[7][12] = { "Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday" };

// Define NTP Client to get time
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org", utcOffsetInSeconds, 3600);

void logo() {
	Heltec.display->clear();
	Heltec.display->drawXbm(0, 5, logo_width, logo_height, (const unsigned char*)logo_bits);
	Heltec.display->display();
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
	while (WiFi.status() != WL_CONNECTED && count < 10)
	{
		count++;
		delay(500);
		Heltec.display->drawString(0, 0, "Connecting...");
		Heltec.display->display();
	}

	Heltec.display->clear();
	if (WiFi.status() == WL_CONNECTED)
	{
		Heltec.display->drawString(0, 0, "Connecting...OK.");
		Heltec.display->display();
		//		delay(500);
	}
	else
	{
		Heltec.display->clear();
		Heltec.display->drawString(0, 0, "Connecting...Failed");
		Heltec.display->display();
		while (1);
	}
	Heltec.display->drawString(0, 10, "WIFI Setup done");
	Heltec.display->display();
	delay(500);
}

void WIFIScan(void)
{
	Heltec.display->drawString(0, 20, "Scan start...");
	Heltec.display->display();

	int n = WiFi.scanNetworks();
	Heltec.display->drawString(0, 30, "Scan done");
	Heltec.display->display();
	delay(500);
	Heltec.display->clear();
	// find the ones with strength above -90
	for (int ix = 0; ix < n; ++ix) {
		if (WiFi.RSSI(ix) < -90) {
			n = ix;
			break;
		}
	}

	if (n == 0)
	{
		Heltec.display->clear();
		Heltec.display->drawString(0, 0, "no network found");
		Heltec.display->display();
		while (1);
	}
	else
	{
		Heltec.display->drawString(0, 0, (String)n);
		Heltec.display->drawString(14, 0, "networks found:");
		Heltec.display->display();
		delay(500);

		for (int i = 0; i < n; ++i) {
			if (i && ((i / 6) * 6 == i)) {
				delay(3000);
				Heltec.display->clear();
			}
			// Print SSID and RSSI for each network found
            Heltec.display->drawString(0, ((i % 6) + 1) * 9, (String)(i + 1));
            Heltec.display->drawString(6, ((i % 6) + 1) * 9, ":");
            Heltec.display->drawString(12, ((i % 6) + 1) * 9, (String)(WiFi.SSID(i)));
            Heltec.display->drawString(90, ((i % 6) + 1) * 9, " (");
            Heltec.display->drawString(98, ((i % 6) + 1) * 9, (String)(WiFi.RSSI(i)));
            Heltec.display->drawString(114, ((i % 6) + 1) * 9, ")");
			//            display.println((WiFi.encryptionType(i) == WIFI_AUTH_OPEN)?" ":"*");
			Heltec.display->display();
			delay(10);
		}
	}
	Heltec.display->display();
	delay(800);
	Heltec.display->clear();
}

void setup()
{
	pinMode(LED, OUTPUT);
	digitalWrite(LED, HIGH);

	Heltec.begin(true /*DisplayEnable Enable*/, false /*LoRa Enable*/, true /*Serial Enable*/);

	logo();
	delay(300);
	Heltec.display->clear();

	WIFISetUp();
	//WIFIScan();
	digitalWrite(LED, LOW);
	Serial.println("WiFi connected");
	Serial.println("IP address: ");
	Serial.println(WiFi.localIP());
	timeClient.begin();
	Heltec.display->clear();
}

void loop()
{
    timeClient.update();
	//int touch = touchRead(TOUCH_PAD_NUM2_GPIO_NUM);
	//Serial.println(String(touch));
	//delay(1000);
    //int temp = temperatureRead();
    //Serial.println("temp:" + String(temp));
	Heltec.display->clear();
	Heltec.display->setFont(ArialMT_Plain_24);
	char line[50];
	int hour = timeClient.getHours();
	const char* ampm = (hour < 13 ? "AM" : "PM");
	hour = hour % 12;
	if (hour == 0)
		hour = 12;
    sprintf(line, "%2d:%02d:%02d %s", hour, timeClient.getMinutes(), timeClient.getSeconds(), ampm);
	Heltec.display->drawString(0, 0, line);
	Heltec.display->display();
    Serial.println(timeClient.getFormattedTime());
	delay(1000);
}
