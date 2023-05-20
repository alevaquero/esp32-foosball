
// Foosball scoreboard display

#include <WiFi.h>
#include <ESPmDNS.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebSrv.h>
#include <ESP32-HUB75-MatrixPanel-I2S-DMA.h>

#include "config.h"
#include "digit.h"

#define PANEL_RES_X 64      // Number of pixels wide of each INDIVIDUAL panel module. 
#define PANEL_RES_Y 32     // Number of pixels tall of each INDIVIDUAL panel module.
#define PANEL_CHAIN 1      // Total number of panels chained one to another

#define BUTTON_PIN 21

MatrixPanel_I2S_DMA *dma_display = nullptr;

uint16_t myBLACK = dma_display->color565(0, 0, 0);
uint16_t myWHITE = dma_display->color565(255, 255, 255);
uint16_t myRED = dma_display->color565(255, 0, 0);
uint16_t myGREEN = dma_display->color565(0, 255, 0);
uint16_t myBLUE = dma_display->color565(0, 0, 255);

// Code taken from https://github.com/bogd/esp32-morphing-clock. Thanks!
Digit digit1(0, CLOCK_X,                                                 PANEL_HEIGHT-CLOCK_Y-2*(CLOCK_SEGMENT_HEIGHT)-3, myBLUE);
Digit digit0(0, CLOCK_X + (CLOCK_SEGMENT_WIDTH+CLOCK_SEGMENT_SPACING),   PANEL_HEIGHT-CLOCK_Y-2*(CLOCK_SEGMENT_HEIGHT)-3, myBLUE);
Digit digit3(0, 2+CLOCK_X+2*(CLOCK_SEGMENT_WIDTH+CLOCK_SEGMENT_SPACING)+3, PANEL_HEIGHT-CLOCK_Y-2*(CLOCK_SEGMENT_HEIGHT)-3, myRED);
Digit digit2(0, 2+CLOCK_X+3*(CLOCK_SEGMENT_WIDTH+CLOCK_SEGMENT_SPACING)+3, PANEL_HEIGHT-CLOCK_Y-2*(CLOCK_SEGMENT_HEIGHT)-3, myRED);

AsyncWebServer server(80);

void printWifiStatus() {
  // print the SSID of the network you're attached to:
  Serial.print("SSID: ");
  Serial.println(WiFi.SSID());
  
  // print your WiFi shield's IP address:
  IPAddress ip = WiFi.localIP();
  Serial.print("IP Address: ");

  Serial.println(ip);
  // print the received signal strength:
  long rssi = WiFi.RSSI();
  Serial.print("signal strength (RSSI):");
  Serial.print(rssi);
  Serial.println(" dBm");
}

uint8_t blue_goals = 0;
uint8_t red_goals = 0;
uint8_t prev_blue_goals = 0;
uint8_t prev_red_goals = 0;
bool reset_score = true;
bool blue_win = false;
bool red_win = false;
int pos = 0;

void display_border(uint16_t color) {
  dma_display->drawRect(0, 0, dma_display->width(), dma_display->height(), color);
  dma_display->drawRect(1, 1, dma_display->width()-2, dma_display->height()-2, color);

  dma_display->drawLine(dma_display->width()/2-1, 2, dma_display->width()/2-1, dma_display->height()-2, color);
  dma_display->drawLine(dma_display->width()/2, 2, dma_display->width()/2, dma_display->height()-2, color);
}

void flashy_border(int pos) {
  uint16_t c1 = dma_display->color444(85 * (pos%3 + 1), 85 * (pos%3 + 1), 85 * (pos%3 + 1));
  uint16_t c2 = dma_display->color444(85 * ((pos+1)%3 + 1), 85 * ((pos+1)%3 + 1), 85 * ((pos+1)%3 + 1));
  uint16_t c3 = dma_display->color444(85 * ((pos+2)%3 + 1), 85 * ((pos+2)%3 + 1), 85 * ((pos+2)%3 + 1));
  for (int i = 0; i < dma_display->width(); i=i+6) {
    dma_display->drawRect(i, 0, 2, 2, c3);
    dma_display->drawRect(i+2, 0, 2, 2, c2);
    dma_display->drawRect(i+4, 0, 2, 2, c1);
    
    dma_display->drawRect(i, dma_display->height()-2, 2, 2, c3);
    dma_display->drawRect(i+2, dma_display->height()-2, 2, 2, c2);
    dma_display->drawRect(i+4, dma_display->height()-2, 2, 2, c1);
  }
  for (int i = 0; i < dma_display->height(); i=i+6) {
    dma_display->drawRect(0, i, 2, 2, c3);
    dma_display->drawRect(0, i+2, 2, 2, c2);
    dma_display->drawRect(0, i+4, 2, 2, c1);
    
    dma_display->drawRect(dma_display->width()/2-1, i, 2, 2, c3);
    dma_display->drawRect(dma_display->width()/2-1, i+2, 2, 2, c2);
    dma_display->drawRect(dma_display->width()/2-1, i+4, 2, 2, c1);

    dma_display->drawRect(dma_display->width()-2, i, 2, 2, c3);
    dma_display->drawRect(dma_display->width()-2, i+2, 2, 2, c2);
    dma_display->drawRect(dma_display->width()-2, i+4, 2, 2, c1);
  }
}


void display_winner(uint16_t color, int pos) {
  // Display the bottom "WINS 10-9" only once
  if (pos == 0) {
    // clean screen
    dma_display->fillScreen(myBLACK);
  
    dma_display->setTextWrap(false);
  
    dma_display->setTextSize(1);  // size 1 == 8 pixels high
    dma_display->setTextColor(myWHITE);
    int width = 5*6 + 18;
    if (blue_goals > 9)
      width += 6;
    if (red_goals > 9)
      width += 6;
    dma_display->setCursor((dma_display->width() - width)/2, 22);
    if (color == myBLUE) {
      dma_display->print("WINS ");
      dma_display->print(blue_goals);
      dma_display->print("-");
      dma_display->print(red_goals);
    } else if (color == myRED) {
      dma_display->print("WINS ");
      dma_display->print(red_goals);
      dma_display->print("-");
      dma_display->print(blue_goals);
    }
  }

  // Display the flashy RED/BLUE
  dma_display->setTextSize(2);     // size 1 == 8 pixels high
  int c = 0;
    if (pos%40 < 20) {
      c = 65 + pos%20 * 10;
    } else {
      c = 255 - pos%20 * 10;
    }
    
    if (color == myBLUE) {
      dma_display->setCursor(9, 4);
      dma_display->setTextColor(dma_display->color565(0, 0, c));
      dma_display->println("BLUE");
    } else if (color == myRED) {
      dma_display->setCursor(15, 4);
      dma_display->setTextColor(dma_display->color565(c, 0, 0));
      dma_display->println("RED");
    }    
    delay(10);
}

void reset_values() {
  blue_goals = 0;
  red_goals = 0;
  prev_blue_goals = 0;
  prev_red_goals = 0;
  reset_score = true;
  red_win = false;
  blue_win = false;
}

unsigned long last_flashy;

void setup() {
  Serial.begin(115200);
  while (!Serial) {
    ; // wait for serial port to connect.
  }
  Serial.println("in setup");

  pinMode(BUTTON_PIN, INPUT_PULLDOWN);
  
  Serial.print("Connecting to WiFi ");
  Serial.println(WIFI_SSID);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("");
  Serial.println("WiFi connected.");
  printWifiStatus();

  if(!MDNS.begin("foosball-disp")) {
    Serial.println("Error starting mDNS");
    return;
  }

  // Handle scoreboard goals
  server.on("/goal", HTTP_POST, [](AsyncWebServerRequest *request) {
  if(request->hasParam("team")) {
    AsyncWebParameter* team = request->getParam("team");
    Serial.printf("Got a POST goal for %s team\n", team->value().c_str());
    char json_response[40];
    if (team->value() == "blue") {
      blue_goals++;
      sprintf(json_response, "{ red: %d, blue: %d }", red_goals, blue_goals);
      request->send(200, "application/json", json_response);          
    } else if (team->value() == "red") {
      red_goals++;
      sprintf(json_response, "{ red: %d, blue: %d }", red_goals, blue_goals);
      request->send(200, "application/json", json_response);  
    } else {
      request->send(500, "text/plain", "Invalid team parameter");  
    }
  } else 
    request->send(500, "text/plain", "No team parameter");
  });

  // Handle scoreboard reset (set to 0s)
  server.on("/reset", HTTP_POST, [](AsyncWebServerRequest *request){
    reset_values();
    request->send(200, "text/plain", "OK");
  });

  // Handle winner team
  server.on("/win", HTTP_POST, [](AsyncWebServerRequest *request) {
    if(request->hasParam("team")) {
      AsyncWebParameter* team = request->getParam("team");
      Serial.printf("Got a POST win for %s team\n", team->value().c_str());
      if (team->value() == "blue") {
        blue_win = true;
        pos = 0;
        request->send(200, "text/plain", "OK");
      } else if (team->value() == "red") {
        red_win = true;
        pos = 0;
        request->send(200, "text/plain", "OK"); 
      } else {
        request->send(500, "text/plain", "Invalid team parameter");  
      }
    } else 
      request->send(500, "text/plain", "No team parameter");
  });
  
  server.begin();
  
  // Module configuration
  HUB75_I2S_CFG mxconfig(
    PANEL_RES_X,   // module width
    PANEL_RES_Y,   // module height
    PANEL_CHAIN    // Chain length
  );

  // Display Setup
  dma_display = new MatrixPanel_I2S_DMA(mxconfig);
  dma_display->begin();
  dma_display->setBrightness8(90); //0-255
  dma_display->clearScreen();

  last_flashy = millis();
}

void blink_border(uint16_t color) {
  display_border(myWHITE);
  delay(100);
  display_border(color);
  delay(100);
  display_border(myWHITE);
  delay(100);
  display_border(color);
  delay(100);
  display_border(myWHITE);
}

int last_button_state = LOW;
void loop() {
  int button_state = digitalRead(BUTTON_PIN);
  if (last_button_state == LOW && button_state == HIGH) {
    reset_values();
  }
  last_button_state = button_state;
  
  if (reset_score) {
    dma_display->fillScreen(myBLACK);
    display_border(myWHITE);
    digit0.Draw(blue_goals % 10);
    digit1.Draw(blue_goals / 10);   

    digit2.Draw(red_goals % 10);
    digit3.Draw(red_goals / 10);
    reset_score = false;
    pos = 0; 
  }

  if (red_win) {
    display_winner(myRED, pos++);
    // Autoreset after about 10min (60000*10ms)
    if (pos > 60000) {
      reset_values();
    }
  } else if (blue_win) {
    display_winner(myBLUE, pos++);
    // Autoreset after about 10min (60000*10ms)
    if (pos > 60000) {
      reset_values();
    }
  } else {
    if (prev_blue_goals != blue_goals) {
      display_border(myBLUE);
      int b0 = blue_goals % 10;
      int b1 = blue_goals / 10;
      if (b0!=digit0.Value()) digit0.Morph(b0);
      if (b1!=digit1.Value()) digit1.Morph(b1);
      blink_border(myBLUE);
      prev_blue_goals = blue_goals;
    } else if (prev_red_goals != red_goals) {
      display_border(myRED);
      int r0 = red_goals % 10;
      int r1 = red_goals / 10;
      if (r0!=digit2.Value()) digit2.Morph(r0);
      if (r1!=digit3.Value()) digit3.Morph(r1);
      blink_border(myRED);
      prev_red_goals = red_goals;
    } else {
      if (millis() - last_flashy > 200) {
        flashy_border(pos++);
        last_flashy = millis();
      }
    }
  }
}
