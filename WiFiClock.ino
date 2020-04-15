/*
 Martin Nohr test programs for clock and weather
*/

#include <NTPClient.h>

#include "heltec.h"
#include "WiFi.h"
#include "images.h"
#include "FS.h"
#include "SD.h"
#include "SPI.h"

// SD details
#define SDcsPin 5                        // SD card CS pin
SPIClass spi1;


// time details
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
	setupSDcard();
	pinMode(17, OUTPUT);
}

void listDir(fs::FS& fs, const char* dirname, uint8_t levels) {
	Serial.printf("Listing directory: %s\n", dirname);

	File root = fs.open(dirname);
	if (!root) {
		Serial.println("Failed to open directory");
		return;
	}
	if (!root.isDirectory()) {
		Serial.println("Not a directory");
		return;
	}

	File file = root.openNextFile();
	while (file) {
		if (file.isDirectory()) {
			Serial.print("  DIR : ");
			Serial.print(file.name());
			time_t t = file.getLastWrite();
			struct tm* tmstruct = localtime(&t);
			Serial.printf("  LAST WRITE: %d-%02d-%02d %02d:%02d:%02d\n", (tmstruct->tm_year) + 1900, (tmstruct->tm_mon) + 1, tmstruct->tm_mday, tmstruct->tm_hour, tmstruct->tm_min, tmstruct->tm_sec);
			if (levels) {
				listDir(fs, file.name(), levels - 1);
			}
		}
		else {
			Serial.print("  FILE: ");
			Serial.print(file.name());
			Serial.print("  SIZE: ");
			Serial.print(file.size());
			time_t t = file.getLastWrite();
			struct tm* tmstruct = localtime(&t);
			Serial.printf("  LAST WRITE: %d-%02d-%02d %02d:%02d:%02d\n", (tmstruct->tm_year) + 1900, (tmstruct->tm_mon) + 1, tmstruct->tm_mday, tmstruct->tm_hour, tmstruct->tm_min, tmstruct->tm_sec);
		}
		file = root.openNextFile();
	}
}


void createDir(fs::FS& fs, const char* path) {
	Serial.printf("Creating Dir: %s\n", path);
	if (fs.mkdir(path)) {
		Serial.println("Dir created");
	}
	else {
		Serial.println("mkdir failed");
	}
}

void removeDir(fs::FS& fs, const char* path) {
	Serial.printf("Removing Dir: %s\n", path);
	if (fs.rmdir(path)) {
		Serial.println("Dir removed");
	}
	else {
		Serial.println("rmdir failed");
	}
}

void readFile(fs::FS& fs, const char* path) {
	Serial.printf("Reading file: %s\n", path);

	File file = fs.open(path);
	if (!file) {
		Serial.println("Failed to open file for reading");
		return;
	}

	Serial.print("Read from file: ");
	while (file.available()) {
		Serial.write(file.read());
	}
	file.close();
}

void writeFile(fs::FS& fs, const char* path, const char* message) {
	Serial.printf("Writing file: %s\n", path);

	File file = fs.open(path, FILE_WRITE);
	if (!file) {
		Serial.println("Failed to open file for writing");
		return;
	}
	if (file.print(message)) {
		Serial.println("File written");
	}
	else {
		Serial.println("Write failed");
	}
	file.close();
}

void appendFile(fs::FS& fs, const char* path, const char* message) {
	Serial.printf("Appending to file: %s\n", path);

	File file = fs.open(path, FILE_APPEND);
	if (!file) {
		Serial.println("Failed to open file for appending");
		return;
	}
	if (file.print(message)) {
		Serial.println("Message appended");
	}
	else {
		Serial.println("Append failed");
	}
	file.close();
}

void renameFile(fs::FS& fs, const char* path1, const char* path2) {
	Serial.printf("Renaming file %s to %s\n", path1, path2);
	if (fs.rename(path1, path2)) {
		Serial.println("File renamed");
	}
	else {
		Serial.println("Rename failed");
	}
}

void deleteFile(fs::FS& fs, const char* path) {
	Serial.printf("Deleting file: %s\n", path);
	if (fs.remove(path)) {
		Serial.println("File deleted");
	}
	else {
		Serial.println("Delete failed");
	}
}

void setupSDcard() {
	pinMode(SDcsPin, OUTPUT);
	SPIClass(1);
	spi1.begin(18, 19, 23, SDcsPin);	// SCK,MISO,MOSI,CS

	if (!SD.begin(SDcsPin, spi1)) {
		Serial.println("Card Mount Failed");
		return;
	}
	uint8_t cardType = SD.cardType();

	if (cardType == CARD_NONE) {
		Serial.println("No SD card attached");
		return;
	}

	Serial.print("SD Card Type: ");
	if (cardType == CARD_MMC) {
		Serial.println("MMC");
	}
	else if (cardType == CARD_SD) {
		Serial.println("SDSC");
	}
	else if (cardType == CARD_SDHC) {
		Serial.println("SDHC");
	}
	else {
		Serial.println("UNKNOWN");
	}

	uint64_t cardSize = SD.cardSize() / (1024 * 1024);
	Serial.printf("SD Card Size: %lluMB\n", cardSize);

	listDir(SD, "/", 0);
	//GetFileNamesFromSD(currentFolder);
}

int timeZone = -6;
#define SECS_PER_HOUR 3600
void loop()
{
	static int lastSec;
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
	if (lastSec != gtime->tm_sec) {
		lastSec = gtime->tm_sec;
		Heltec.display->clear();
		Heltec.display->setFont(ArialMT_Plain_24);
		char line[50];
		int hour = gtime->tm_hour;
		const char* ampm = (hour < 13 ? "AM" : "PM");
		hour = hour % 12;
		if (hour == 0)
			hour = 12;
		sprintf(line, "%2d:%02d:%02d", hour, gtime->tm_min, gtime->tm_sec);
		Heltec.display->drawString(0, 0, line);
		Heltec.display->setFont(ArialMT_Plain_10);
		Heltec.display->drawString(100, 0, ampm);
		Heltec.display->setFont(ArialMT_Plain_16);
		sprintf(line, "%d/%d/%4d", gtime->tm_mon + 1, gtime->tm_mday, gtime->tm_year + 1900);
		Heltec.display->drawString(0, 25, line);
		Heltec.display->setFont(ArialMT_Plain_10);
		Heltec.display->drawString(0, 45, daysOfTheWeek[gtime->tm_wday]);
		Heltec.display->display();
		//Serial.println(timeClient.getFormattedTime());
	}
	digitalWrite(17, HIGH);
	delay(5);
	digitalWrite(17, LOW);
	delay(10);
}
