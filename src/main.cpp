

#include <WiFi.h>
#include <WebServer.h>
#include <Wire.h>
#include <TFT_eSPI.h> // The TFT_eSPI library
#include <ArduinoJson.h>
#include <HTTPClient.h>
#define USE_ESP_WIFIMANAGER_NTP     false
#include <ESP_WiFiManager.h>

#define SCREEN_WIDTH 480
#define SCREEN_HEIGHT 320
#define ZEILENHOEHE 22

const char* ntpServer = "de.pool.ntp.org";
const long  gmtOffset_sec = 3600;
const int   daylightOffset_sec = 3600;
const char* months[] PROGMEM = {"-","Jan","Feb","Mär","Apr","Mai","Jun","Jul","Aug","Sep","Okt","Now","Dez" };
const char* days[] PROGMEM = {"So","Mo","Di","Mi","Do","Fr","Sa" };
struct tm timeinfo;

TFT_eSPI tft = TFT_eSPI();
HTTPClient http;

void setMessage(String msg, int y_pos)
{
    TFT_eSprite m = TFT_eSprite(&tft);
    m.setColorDepth(8);
    m.createSprite(tft.width()-5, ZEILENHOEHE);
    //m.fillSprite(TFT_TRANSPARENT);
    m.fillSprite(TFT_BLACK);

    //m.setTextFont()
    m.setFreeFont(&FreeMonoBold12pt7b);
    m.setTextDatum(TL_DATUM);
    m.setTextColor(TFT_GREEN);
    m.setCursor(0, ZEILENHOEHE-4);
    m.print(msg);

    //m.pushSprite(4, y_pos, TFT_TRANSPARENT);
    m.pushSprite(4, y_pos);
    m.deleteSprite();
}

uint16_t read16(fs::File& f)
{
    uint16_t result;
    ((uint8_t*)&result)[0] = f.read(); // LSB
    ((uint8_t*)&result)[1] = f.read(); // MSB
    return result;
}
uint32_t read32(fs::File& f)
{
    uint32_t result;
    ((uint8_t*)&result)[0] = f.read(); // LSB
    ((uint8_t*)&result)[1] = f.read();
    ((uint8_t*)&result)[2] = f.read();
    ((uint8_t*)&result)[3] = f.read(); // MSB
    return result;
}

void drawBmp(const char* filename, int16_t x, int16_t y)
{

    if ((x >= tft.width()) || (y >= tft.height()))
        return;

    fs::File bmpFS;

    bmpFS = SPIFFS.open(filename, "r");

    if (!bmpFS)
    {

        Serial.print("File not found:");
        Serial.println(filename);
        return;
    }

    uint32_t seekOffset;
    uint16_t w, h, row;
    uint8_t r, g, b;

    if (read16(bmpFS) == 0x4D42)
    {
        read32(bmpFS);
        read32(bmpFS);
        seekOffset = read32(bmpFS);
        read32(bmpFS);
        w = read32(bmpFS);
        h = read32(bmpFS);

        if ((read16(bmpFS) == 1) && (read16(bmpFS) == 24) && (read32(bmpFS) == 0))
        {
            y += h - 1;

            bool oldSwapBytes = tft.getSwapBytes();
            tft.setSwapBytes(true);
            bmpFS.seek(seekOffset);

            uint16_t padding = (4 - ((w * 3) & 3)) & 3;
            uint8_t lineBuffer[w * 3 + padding];

            for (row = 0; row < h; row++)
            {

                bmpFS.read(lineBuffer, sizeof(lineBuffer));
                uint8_t* bptr = lineBuffer;
                uint16_t* tptr = (uint16_t*)lineBuffer;
                // Convert 24 to 16 bit colours
                for (uint16_t col = 0; col < w; col++)
                {
                    b = *bptr++;
                    g = *bptr++;
                    r = *bptr++;
                    *tptr++ = ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3);
                }

                // Push the pixel row to screen, pushImage will crop the line if needed
                // y is decremented as the BMP image is drawn bottom up
                tft.pushImage(x, y--, w, 1, (uint16_t*)lineBuffer);
            }
            tft.setSwapBytes(oldSwapBytes);
        }
        else
            Serial.println("[WARNING]: BMP format not recognized.");
    }
    bmpFS.close();
}

// the setup function runs once when you press reset or power the board
void setup()
{
    Serial.begin(115200);
    Serial.println("");
     
    // --------------- Init Display -------------------------

    // Initialise the TFT screen
    tft.init();

    // Set the rotation before we calibrate
    tft.setRotation(1);

    // Clear the screen
    tft.fillScreen(TFT_BLACK);

    // Set "cursor" at top left corner of display (0,0) and select font 4
    tft.setCursor(0, 0, 4);

    // Set the font colour to be white with a black background
    tft.setTextColor(TFT_GREEN, TFT_BLACK);

    // We can now plot text on screen using the "print" class
    tft.println("Startup"); 

    tft.println("Starting Wifi Config");

    Serial.print(F("\nStarting WifiMan on ")); Serial.println(ARDUINO_BOARD); 
    Serial.println(ESP_WIFIMANAGER_VERSION);
    ESP_WiFiManager ESP_wifiManager("CxExtDisplay");
    ESP_wifiManager.autoConnect("WifiManAP");

    if (WiFi.status() == WL_CONNECTED)
    {
        tft.println("Connected "+ WiFi.localIP().toString());

        configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);

        if (!getLocalTime(&timeinfo))
        {
            tft.println("Fehler NTP");
        }
        else
        {
            tft.println("NTP OK");
        }
    }

    if (!SPIFFS.begin(true))
    {
        tft.println("[ERROR]: SPIFFS initialisation failed!");
        while (1)
            yield(); // We stop here
    }
    tft.println("SPIFFS initialised.");

    // Check for free space

    tft.print("Free Space: ");
    tft.println(SPIFFS.totalBytes() - SPIFFS.usedBytes());

    tft.println("Startup complete");

    delay(500);
    tft.fillScreen(TFT_BLACK);
}

// the loop function runs over and over again until power down or reset
void loop()
{
    //tft.fillScreen(TFT_BLACK);

    //drawBmp("/logo.bmp", 0, 0);
    tft.drawRect(0, 0, tft.width(), tft.height(), TFT_BLUE);
    
    /*
    tft.setCursor(4, 18);
    tft.setFreeFont(&FreeMonoBold12pt7b);
    tft.println("Hallo Welt");
    tft.println("   Welt");*/
    
    if (WiFi.status() == WL_CONNECTED)
    {
        int i = 1;

        // Send request
        http.useHTTP10(true);
        http.setConnectTimeout(1000);
        http.setTimeout(1000);
        http.begin("http://Bastet.lan:1337/api/osd/");
        int result = http.GET();
        //tft.println(result);

        if (200 == result)
        {
            // Parse response
            StaticJsonDocument<1024> doc;
            DeserializationError error = deserializeJson(doc, http.getStream());
            if (error)
            {
                Serial.println("deserializeJson() failed: ");
                Serial.println(error.f_str());
            }
            else
            {
                //tft.println("deserializeJson() success ");
                JsonArray arr = doc.as<JsonArray>();

                
                for (JsonVariant value : arr)
                {
                    //tft.println(value.as<char*>());
                    
                    /*char buf[50] = value.as<char*>();
                    char *found = strstr(buf, "<APP>");
                    if (NULL != found)
                    {
                        strcpy(buf, buf + 6);
                    }
                    setMessage(buf, i);*/

                    String s = value.as<String>();
                    s.replace("<APP>", "");
                    s.trim();
                    setMessage(s, i);
                    
                    //setMessage(value.as<String>(),i);
                    
                    i += ZEILENHOEHE;
                }
            }
        }
        else
            Serial.println("HTTP Error: "+result);

        // Disconnect
        http.end();

        //Print Time and Date at the End
        
        if (!getLocalTime(&timeinfo))
        {
            Serial.println("Failed to obtain time");
        }
        else
        {
            char timeStringBuff[50];
            //strftime(timeStringBuff, sizeof(timeStringBuff), "%a, %d %b %Y %H:%M:%S", &timeinfo);
            sprintf(timeStringBuff,"%s %d %s %d, %02d:%02d:%02d",
                days[timeinfo.tm_wday],
                timeinfo.tm_mday,
                months[timeinfo.tm_mon],
                timeinfo.tm_year+1900,
                timeinfo.tm_hour,
                timeinfo.tm_min,
                timeinfo.tm_sec);
            //Serial.println(timeStringBuff);
            setMessage(timeStringBuff, i);
            i += ZEILENHOEHE;
        }

        //alten kram unten drunter überschreiben
        for(;i<(SCREEN_HEIGHT-ZEILENHOEHE);i+=ZEILENHOEHE)
        {
            setMessage("", i);
        }
    }
    else
        Serial.println("no Wifi!");

    for (int i = 0;i < 1000;i++)
    {        
        delay(1);
    }
}