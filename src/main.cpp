
#include <FS.h>
#include <WiFi.h>
#include <WebServer.h>
#include <Wire.h>
#include <TFT_eSPI.h>
#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <PageBuilder.h>
#include <AutoConnect.h>
#include <FT6236.h>

#define ZEILENHOEHE 22

//Nach x Fehlzugriffen wird der Text ROT
//und die Infos werden dann nicht mehr angezeigt
//von 1 bis x-1 wird der Text GELB und alte Infos werden angezeigt
#define STATE_RED 5

const char* ntpServer = "de.pool.ntp.org";
const char * defaultTimezone = "CET-1CEST,M3.5.0/2,M10.5.0/3";
const char* months[] PROGMEM = {"Jan","Feb","Mar","Apr","Mai","Jun","Jul","Aug","Sep","Okt","Now","Dez" };
const char* days[] PROGMEM = {"So","Mo","Di","Mi","Do","Fr","Sa" };
struct tm timeinfo;

TFT_eSPI tft = TFT_eSPI();
HTTPClient http;

//Touch
FT6236 ts = FT6236();

StaticJsonDocument<1024> doc;

WebServer Server;
AutoConnect       Portal(Server);
AutoConnectConfig Config;

ACText(start, "start");
AutoConnectAux RootPage("/", "Startseite", false, {
  start,
    });

ACInput(url, "http://Bastet.lan:1337/api/osd/", "URL");
ACSubmit(btnsave, "&#220;bernehmen", "/save");
ACSubmit(discard, "Abbrechen", "/_ac");
AutoConnectAux settings("/settings", "Server Einstellungen", true, {
  url,
  btnsave,
  discard
    });

ACText(caption2, "Gespeichert");
ACText(parameters);
AutoConnectAux save("/save", "gespeichert", false, {
  caption2,
  parameters
    });

void setMessage(String msg, int y_pos, uint16_t color)
{
    TFT_eSprite m = TFT_eSprite(&tft);
    m.setColorDepth(8);
    m.createSprite(tft.width()-5, ZEILENHOEHE);
    //m.fillSprite(TFT_TRANSPARENT);
    m.fillSprite(TFT_BLACK);

    //m.setTextFont()
    m.setFreeFont(&FreeMonoBold12pt7b);
    m.setTextDatum(TL_DATUM);
    m.setTextColor(color);
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

/*void drawBmp(const char* filename, int16_t x, int16_t y)
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
}*/

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

    Config.autoReconnect = true;
    Config.retainPortal = true;
    Portal.config(Config);

    Portal.join({ RootPage,settings,save });

    if (Portal.begin())
    {        
        tft.println("Connected "+ WiFi.localIP().toString());

        WiFi.enableAP(false);

        configTzTime( defaultTimezone, ntpServer); //sets TZ and starts NTP sync

        if (!getLocalTime(&timeinfo))
        {
            tft.println("Fehler NTP");
        }
        else
        {
            tft.println("NTP OK");
        }
    }

    if (!ts.begin(40,26,27))
    {
        Serial.println("[WARNING]: Unable to start the capacitive touchscreen.");
    }
    else
    {
        Serial.println("[INFO]: Capacitive touch started!");
    }

    /*if (!SPIFFS.begin(true))
    {
        tft.println("[ERROR]: SPIFFS initialisation failed!");
        while (1)
            yield(); // We stop here
    }
    tft.println("SPIFFS initialised.");

    // Check for free space

    tft.print("Free Space: ");
    tft.println(SPIFFS.totalBytes() - SPIFFS.usedBytes());*/

    tft.println("Startup complete");

    delay(2000);
    tft.fillScreen(TFT_BLACK);
}

// the loop function runs over and over again until power down or reset
void loop()
{
    //tft.fillScreen(TFT_BLACK);
    
    /*
    tft.setCursor(4, 18);
    tft.setFreeFont(&FreeMonoBold12pt7b);
    tft.println("Hallo Welt");
    tft.println("   Welt");*/
    
    if (WiFi.status() == WL_CONNECTED)
    {
        int i = 1;
        static int TryCnt=0;

        // Send request
        http.useHTTP10(true);
        http.setConnectTimeout(1500);
        http.setTimeout(1500);
        //http.begin("http://Bastet.lan:1337/api/osd/");
        http.begin(url.value);
        int result = http.GET();
        //tft.println(result);

        if (200 == result)
        {
            // Parse response
            StaticJsonDocument<1024> buf;
            DeserializationError error = deserializeJson(buf, http.getStream());
            if (error)
            {
                if(TryCnt < STATE_RED*2)
                    TryCnt++;

                Serial.println("deserializeJson() failed: ");
                Serial.println(error.f_str());
            }
            else
            {
                //tft.println("deserializeJson() success ");
                doc=buf; 
                TryCnt=0;
            }
        }
        else
        {
            if(TryCnt < STATE_RED*2)
                    TryCnt++;
                    
            Serial.println("HTTP Error: "+result);
        }

        // Disconnect
        http.end();

        //print Infos
        if(TryCnt < STATE_RED )
        {
            JsonArray arr=doc.as<JsonArray>();
            for (JsonVariant value : arr)
            {
                //tft.println(value.as<char*>());

                String s = value.as<String>();
                if(s != "null")
                {  
                    s.replace("<APP>", "");
                    s.replace("%    ","%");
                    s.replace("C  ","C");
                    s.trim();
                    setMessage(s, i, TFT_GREEN);
                    
                    i += ZEILENHOEHE;
                }
            }
        }

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
            //Serial.println(months[timeinfo.tm_mon]);
            //Serial.println(timeinfo.tm_mon);
            if (0==TryCnt)
                setMessage(timeStringBuff, i, TFT_GREEN);
            else if (TryCnt < STATE_RED)
                setMessage(timeStringBuff, i, TFT_YELLOW);
            else            
                setMessage(timeStringBuff, i, TFT_RED);
                
            i += ZEILENHOEHE;
        }

        //alten kram unten drunter überschreiben
        for(;i<(tft.height()-ZEILENHOEHE);i+=ZEILENHOEHE)
        {
            setMessage("", i, TFT_GREEN);
        }
    }
    else
        Serial.println("no Wifi!");

    //drawBmp("/logo.bmp", 0, 0);
    tft.drawRect(0, 0, tft.width(), tft.height(), TFT_BLUE);

    if (ts.touched())
    {

      // Retrieve a point
      TS_Point p = ts.getPoint();

      //x umdrehen damit TFT und Touch zusammen passen
      p.x = map(p.x, 0, tft.height(), tft.height(), 0);      
      //Serial.println(String(p.x) + " " + String(p.y));

      //x von oben (0) nach unten
      //y von links (0) nach rechts
      if (p.x < tft.height()/3)
      {
        Serial.println("oben gedrückt");
      }
      else if (p.x > tft.height()*2/3)
      {
        Serial.println("unten gedrückt");
      }
    }

    for (int i = 0;i < 500;i++)
    {
        Portal.handleClient();        
        delay(1);
    }
}