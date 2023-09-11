#include <SPI.h>
#include <TFT_eSPI.h> // Hardware-specific library
#include <WiFi.h>
#include <PubSubClient.h>
#include <string.h>
#include "NotoSans_Bold.h"
#include "OpenFontRender.h"

#define TTF_FONT NotoSans_Bold

#define DARKER_GREY 0x18E3

#define MIN_POWER_IN 0
#define MAX_POWER_IN 9000
#define MIN_POWER_OUT 0
#define MAX_POWER_OUT 3000

#define POWER_IN_GREEN 3000
#define POWER_IN_ORANGE 6000

#define POWER_IN_ANGLE_MIN 90
#define POWER_IN_ANGLE_MAX 360
#define POWER_OUT_ANGLE_MIN 0
#define POWER_OUT_ANGLE_MAX 90

const char * ssid     = "dlink-1188"; // Change this to your WiFi SSID
const char * password = "kKCfQGQnMW2N797pr2"; // Change this to your WiFi password
const char * mqtt_server = "pi-server1.local";

const char * consumption_topic = "tic_raw/SINSTS";
const char * injection_topic = "tic_raw/SINSTI";

WiFiClient espClient;
PubSubClient client(espClient);

TFT_eSPI tft = TFT_eSPI();            // Invoke custom library with default width and height
TFT_eSprite spr = TFT_eSprite(&tft);  // Declare Sprite object "spr" with pointer to "tft" object
OpenFontRender ofr;

bool initMeter = true;
int consumptionValue = 0;
int injectionValue = 0;

void callback(char* topic, byte* payload, unsigned int length) {
  String msg;
  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.print("] : ");
  for (int i = 0; i < length; i++) {
    Serial.print((char)payload[i]);
    msg += String((char)payload[i]);
  }
  Serial.println();
  
  msg.remove(0,10); // Remove {"data": "
  msg.remove(5);
  
  while (msg.startsWith("0") && msg.length()) {
    msg.remove(0,1);
  }

  if (strcmp(topic, consumption_topic) == 0) {
    consumptionValue = msg.toInt();
  } else if (strcmp(topic, injection_topic) == 0) {
    injectionValue = msg.toInt();
  }
}

void reconnect() {
  // Loop until we're reconnected
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    // Create a random client ID
    String clientId = "ESP32Client-";
    clientId += String(random(0xffff), HEX);
    // Attempt to connect
    if (client.connect(clientId.c_str())) {
      Serial.println("connected");
      client.subscribe(injection_topic);
      client.subscribe(consumption_topic);
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      // Wait 5 seconds before retrying
      delay(5000);
    }
  }
}

void setup(void) {
  Serial.begin(115200);
  // We start by connecting to a WiFi network

  Serial.println();
  Serial.println("******************************************************");
  Serial.print("Connecting to ");
  Serial.println(ssid);

  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
      delay(500);
      Serial.print(".");
  }

  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
  
  client.setServer(mqtt_server, 1883);
  client.setCallback(callback);

  tft.begin();
  tft.setRotation(0);
  tft.fillScreen(TFT_BLACK);
}


void loop() {

  static int oldInjectionValue = 0;
  static int oldConsumptionValue = 0;
  static bool isAnimated = false;
  static int animationSteps = 10;

  if (!client.connected()) {
    reconnect();
  }
  client.loop();
  if ((consumptionValue != oldConsumptionValue) || (injectionValue != oldInjectionValue)) {
    int value_to_display;
    int old_value;
    if (injectionValue != 0) {
      value_to_display = injectionValue;
      old_value = oldInjectionValue; 
    } else {
      value_to_display = consumptionValue;
      old_value = oldConsumptionValue;
    }
    if (isAnimated) {
      int step = (value_to_display - old_value) / animationSteps;
      
      for (int i = 0 ; i < animationSteps ; i++) {
        displayMeter(old_value, consumptionValue != 0);
        old_value += step;
        delay(300);
      }
    }
    displayMeter(value_to_display, consumptionValue != 0);

    oldInjectionValue = injectionValue;
    oldConsumptionValue = consumptionValue;
  }
  if (isAnimated) {
    delay(300);
  } else {
    delay(3000);
  }
}

void displayMeter(int value, bool grid_to_house)
{
  static uint8_t radius = tft.width() / 2;
  static int16_t xpos = tft.width() / 2;
  static int16_t ypos = tft.height() / 2;

  // Loading a font takes a few milliseconds, so for test purposes it is done outside the test loop
  if (ofr.loadFont(TTF_FONT, sizeof(TTF_FONT))) {
    Serial.println("Render initialize error");
    return;
  }

  if (grid_to_house == false) {
    ringMeter(xpos, ypos, radius, value, "Watts", grid_to_house);
  } else {
    ringMeter(xpos, ypos, radius, value, "Watts", grid_to_house);
  }
  ofr.unloadFont(); // Recover space used by font metrics etc
}

void ringMeter(int x, int y, int r, int val, const char *units, bool grid_to_house)
{
  if (initMeter) {
    initMeter = false;
    tft.fillCircle(x, y, r, TFT_BLACK);
    uint16_t tmp = r - 3;
    tft.drawArc(x, y, tmp, tmp - tmp / 5, POWER_IN_ANGLE_MIN, POWER_IN_ANGLE_MAX, TFT_DARKGREY, TFT_BLACK);
    tft.drawArc(x, y, tmp, tmp - tmp / 5, POWER_OUT_ANGLE_MIN, POWER_IN_ANGLE_MIN, 0x630C, TFT_BLACK);
  }

  r -= 3;
  int val_angle = 0;
  if (grid_to_house) {
    val_angle = map(val, MIN_POWER_IN, MAX_POWER_IN, POWER_IN_ANGLE_MIN, POWER_IN_ANGLE_MAX);
  } else {
    val_angle = map(val, MIN_POWER_OUT, MAX_POWER_OUT, POWER_OUT_ANGLE_MIN, POWER_OUT_ANGLE_MAX);
  }

  ofr.setDrawer(spr); // Link renderer to sprite (font will be rendered in sprite spr)
  uint8_t w = 140;
  uint8_t h = 70;
  spr.createSprite(w, h);
  spr.fillSprite(TFT_BLACK); // (TFT_BLUE); // (DARKER_GREY);
  
  char str_buf[8];         // Buffed for string
  itoa (val, str_buf, 10); // Convert value to string (null terminated)
  ofr.setCursor(w/2, -10);
  ofr.setFontSize(110);
  if (grid_to_house) {
    ofr.setFontColor(TFT_WHITE, TFT_BLACK);
  } else {
    ofr.setFontColor(TFT_SKYBLUE, TFT_BLACK);
  }
  ofr.cprintf(str_buf);
  
  spr.pushSprite(x-(w/2), y - 30); // Push sprite containing the val number
  
  spr.deleteSprite();                   // Recover used memory

  // Make the TFT the print destination, print the units label direct to the TFT
  ofr.setDrawer(tft);
  ofr.setFontColor(TFT_GOLD, TFT_BLACK);
  ofr.setFontSize(r / 2.0);
  ofr.setCursor(x, y + 30);
  ofr.cprintf("Watts");

  ofr.unloadFont(); // Recover space used by font metrics etc

  // Allocate a value to the arc thickness dependant of radius
  uint8_t thickness = r / 5;
  if ( r < 25 ) thickness = r / 3;

  // Foreground color
  uint32_t fg_color;
  if (val > POWER_IN_ORANGE) {
    fg_color = TFT_RED;
  } else if (val > POWER_IN_GREEN) {
    fg_color = TFT_ORANGE;
  } else {
    fg_color = TFT_GREEN;
  }
  //Serial.println();
  //Serial.println(val_angle);
  if (grid_to_house) {
    tft.drawArc(x, y, r, r - thickness, POWER_IN_ANGLE_MIN, val_angle, fg_color, TFT_DARKGREY);
    tft.drawArc(x, y, r, r - thickness, val_angle, POWER_IN_ANGLE_MAX, TFT_DARKGREY, TFT_BLACK);
    tft.drawArc(x, y, r, r - thickness, POWER_OUT_ANGLE_MIN, POWER_OUT_ANGLE_MAX, 0x630C, TFT_BLACK);
  } else {
    tft.drawArc(x, y, r, r - thickness, POWER_OUT_ANGLE_MAX - val_angle, POWER_OUT_ANGLE_MAX, TFT_SKYBLUE, 0x630C);
    tft.drawArc(x, y, r, r - thickness, POWER_OUT_ANGLE_MIN, POWER_OUT_ANGLE_MAX - val_angle, 0x630C, TFT_BLACK);
    tft.drawArc(x, y, r, r - thickness, POWER_IN_ANGLE_MIN, POWER_IN_ANGLE_MAX, TFT_DARKGREY, TFT_DARKGREY);
  }
}
