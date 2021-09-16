#include <Arduino.h>
#include <EEPROM.h>
#include <FS.h>
#include <ESP8266WiFi.h>
#include <ESPAsyncWebServer.h>
#include <AsyncJson.h>
#include <FastLED.h>

#define LED_PIN 15
#define BTN_PIN 5
#define NUM_LEDS 72

const int ESIZE = 2048;
const int E_DATA_START = 128;
const long LONG_PRESS_TIME = 1000;
const long VERY_LONG_PRESS_TIME = 5000;

AsyncWebServer server(80);

CRGB leds[NUM_LEDS];

DEFINE_GRADIENT_PALETTE (cyan_gp) {
  0, 11, 0, 196,
  63, 0, 109, 212,
  127, 0, 153, 252,
  191, 0, 221, 255,
  255, 11, 0, 196
};
CRGBPalette16 cyanPalette = cyan_gp;

DEFINE_GRADIENT_PALETTE (config_gp) {
  0, 255, 0, 0,
  128, 0, 0, 255,
  255, 255, 0, 0
};
CRGBPalette16 configPalette = config_gp;

boolean configured = false;
char* indexFile = "configuration.html";
int brightness = 5;
int mode = 0;
bool ledEnabled = false;

int btnLastState;
int btnCurrentState;
long pressMillis = 0;

const int iFullRainbowSpeed = 40;
int fullRainbowSpeed = iFullRainbowSpeed;
uint8_t fullRainbowHue = 0;

const int iAnimatedRainbowSpeed = 5;
int animatedRainbowSpeed = iAnimatedRainbowSpeed;
uint8_t animatedRainbowHue = 0;

const uint8_t iRandomSColor = 100;
uint8_t randomSColor = iRandomSColor;
const int iRandomSColorSpeed = 2;
int randomSColorSpeed = iRandomSColorSpeed;

const int iAnimatedPaletteSpeed = 5;
int animatedPaletteSpeed = iAnimatedPaletteSpeed;
uint8_t animatedPaletteIdx = 0;

const int iFadeToBlackSpeed = 10;
int fadeToBlackSpeed = iFadeToBlackSpeed;
const int iFadeToBlackFadeSpeed = 2;
int fadeToBlackFadeSpeed = iFadeToBlackFadeSpeed;

const int iBpmColor = 30;
int bpmColor = iBpmColor;
const int iFadeColorSpeed = 5;
int fadeColorSpeed = iFadeColorSpeed;
const CRGB iRgbColor = CRGB(33, 150, 243);
CRGB rgbColor = iRgbColor;
const CHSV iHsvColor = CHSV(207, 90, 54);
CHSV hsvColor = iHsvColor;

const int iFireSpeed = 25;
int fireSpeed = iFireSpeed;
const int iFireCooling = 55;
int fireCooling = iFireCooling;
const int iFireSparks = 120;
int fireSparks = iFireSparks;
const bool iFireReverse = false;
bool fireReverse = iFireReverse;

CRGB staticRGBColor = CRGB(255, 255, 255);


void startServer();

void writeInt(int address, int i) {
  EEPROM.write(address, i >> 8);
  EEPROM.write(address + 1, i & 0xff);
}

int readInt(int address) {
  byte b1 = EEPROM.read(address);
  byte b2 = EEPROM.read(address + 1);
  return (b1 << 8) + b2;
}

void writeRGB(int address, CRGB color) {
  writeInt(address, color.red);
  writeInt(address + 2, color.green);
  writeInt(address + 4, color.blue);
}

CRGB readRGB(int address) {
  int red = readInt(address);
  int green = readInt(address + 2);
  int blue = readInt(address + 4);

  return CRGB(red, green, blue);
}

void staticRGB(CRGB color) {
  fill_solid(leds, NUM_LEDS, color);
  FastLED.show();
}

void staticHSV(CHSV color) {
  fill_solid(leds, NUM_LEDS, color);
  FastLED.show();
}

void staticRainbow() {
  fill_rainbow(leds, NUM_LEDS, 0, 255 / NUM_LEDS);
  FastLED.show();
}

void fullRainbow() {
  for (int i = 0; i < NUM_LEDS; i++) {
    leds[i] = CHSV(fullRainbowHue, 255, 255);
  }

  EVERY_N_MILLIS_I(timer, fullRainbowSpeed) {
    timer.setPeriod(fullRainbowSpeed);
    fullRainbowHue++;
  }

  FastLED.show();
}

void animatedRainbow() {
  for (int i = 0; i < NUM_LEDS; i++) {
    leds[i] = CHSV(animatedRainbowHue + (i), 255, 255);
  }

  EVERY_N_MILLIS_I(timer, animatedRainbowSpeed) {
    timer.setPeriod(animatedRainbowSpeed);
    animatedRainbowHue++;
  }

  FastLED.show();
}

void randomSingleColor() {
  EVERY_N_MILLIS_I(timer, randomSColorSpeed) {
    timer.setPeriod(randomSColorSpeed);
    leds[0] = CHSV(randomSColor, random8(), random8(100, 255));

    for (int i = NUM_LEDS - 1; i > 0; i--) {
      leds[i] = leds[i - 1];
    }

    FastLED.show();
  }
}

void staticPalette(CRGBPalette16 palette) {
  fill_palette(leds, NUM_LEDS, 0, 255 / NUM_LEDS, palette, 255, LINEARBLEND);
  FastLED.show();
}

void animatedPalette(CRGBPalette16 palette) {
  fill_palette(leds, NUM_LEDS, animatedPaletteIdx, 255 / NUM_LEDS, palette, 255, LINEARBLEND);

  EVERY_N_MILLIS_I(timer, animatedPaletteSpeed) {
    timer.setPeriod(animatedPaletteSpeed);
    animatedPaletteIdx++;
  }

  FastLED.show();
}

void fadeToBlack(CRGBPalette16 palette) {
  EVERY_N_MILLIS_I(timer, fadeToBlackSpeed) {
    timer.setPeriod(fadeToBlackSpeed);
    leds[random8(0, NUM_LEDS - 1)] = ColorFromPalette(palette, random8(), 255, LINEARBLEND);
  }

  fadeToBlackBy(leds, NUM_LEDS, fadeToBlackFadeSpeed);

  FastLED.show();
}

void beatRGB() {
  uint8_t sinBeat = beatsin8(bpmColor, 0, NUM_LEDS - 1, 0, 0);

  leds[sinBeat] = rgbColor;

  fadeToBlackBy(leds, NUM_LEDS, fadeColorSpeed);

  FastLED.show();
}

void beatHSV() {
  uint8_t sinBeat = beatsin8(bpmColor, 0, NUM_LEDS - 1, 0, 0);

  leds[sinBeat] = hsvColor;

  fadeToBlackBy(leds, NUM_LEDS, fadeColorSpeed);

  FastLED.show();
}

void beatPalette(CRGBPalette16 palette) {
  uint8_t beatA = beatsin8(30, 0, 255);
  uint8_t beatB = beatsin8(20, 0, 255);

  fill_palette(leds, NUM_LEDS, (beatA + beatB) / 2, 10, palette, 255, LINEARBLEND);
  FastLED.show();
}

void fire() {
  EVERY_N_MILLIS_I(timer, fireSpeed) {
    timer.setPeriod(fireSpeed);
    static uint8_t heat[NUM_LEDS];

    for(int i = 0; i < NUM_LEDS; i++) {
      heat[i] = qsub8(heat[i], random8(0, ((fireCooling * 10) / NUM_LEDS) + 2));
    }

    for(int k = NUM_LEDS - 1; k >= 2; k--) {
      heat[k] = (heat[k - 1] + heat[k - 2] + heat[k - 2]) / 3;
    }

    if(random8() < fireSparks) {
      int y = random8(7);
      heat[y] = qadd8(heat[y], random8(160,255));
    }

    for(int j = 0; j < NUM_LEDS; j++) {
      CRGB color = HeatColor(heat[j]);
      int pixelnumber;
      if(reverse) {
        pixelnumber = (NUM_LEDS - 1) - j;
      } else {
        pixelnumber = j;
      }
      leds[pixelnumber] = color;
    }

    FastLED.show();
  }
}

void saveStatus() {
  writeInt(E_DATA_START, brightness);
  EEPROM.write(E_DATA_START + 2, (byte) mode);
  EEPROM.write(E_DATA_START + 3, (byte) ledEnabled ? 1 : 0);
  writeInt(E_DATA_START + 4, fullRainbowSpeed);
  writeInt(E_DATA_START + 6, animatedRainbowSpeed);
  writeInt(E_DATA_START + 8, randomSColor);
  writeInt(E_DATA_START + 10, randomSColorSpeed);
  writeInt(E_DATA_START + 12, animatedPaletteSpeed);
  writeInt(E_DATA_START + 14, fadeToBlackSpeed);
  writeInt(E_DATA_START + 16, fadeToBlackFadeSpeed);
  writeInt(E_DATA_START + 18, bpmColor);
  writeInt(E_DATA_START + 20, fadeColorSpeed);
  writeRGB(E_DATA_START + 22, rgbColor);
  writeInt(E_DATA_START + 28, fireSpeed);
  writeInt(E_DATA_START + 30, fireCooling);
  writeInt(E_DATA_START + 32, fireSparks);
  EEPROM.write(E_DATA_START + 34, (byte) fireReverse ? 1 : 0);
  writeRGB(E_DATA_START + 35, staticRGBColor);
  
  EEPROM.commit();
  Serial.println("Settings saved");
}

void stopServer() {
  server.end();
}

void stopAP() {
  WiFi.softAPdisconnect(true);
}

boolean connectToWiFi(char* ssid, char* password) {
  WiFi.begin(ssid, password);
  WiFi.setSleepMode(WIFI_NONE_SLEEP);
  Serial.print("Connecting to ");
  Serial.print(ssid);
  Serial.println(" ...");

  int i = 0;
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print(".");

    i++;
    if (i > 30) {
      return false;
    }

    delay(1000);
  }

  Serial.println();

  Serial.println("Connection established!");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
  return true;
}

void onConfigure(AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
  StaticJsonDocument<200> jsonDocument;
  DeserializationError error = deserializeJson(jsonDocument, data);

  if (error) {
    Serial.println("Json deserialization failed.");
    return;
  }

  String ssid = jsonDocument["ssid"];
  String password = jsonDocument["password"];

  char s[ssid.length() + 1];
  char p[password.length() + 1];
  ssid.toCharArray(s, sizeof(s));
  password.toCharArray(p, sizeof(p));

  EEPROM.write(0, 1);

  EEPROM.write(1, sizeof(s));
  for (int i = 0; i < sizeof(s); i++) {
    EEPROM.write(i + 2, s[i]);
  }

  EEPROM.write(63, sizeof(p));
  for (int i = 0; i < sizeof(p); i++) {
    EEPROM.write(i + 64, p[i]);
  }

  EEPROM.commit();
  FastLED.clear();
  FastLED.show();

  ESP.restart();
}

void startConfigServer() {
  DefaultHeaders::Instance().addHeader("Access-Control-Allow-Origin", "*");
  DefaultHeaders::Instance().addHeader("Access-Control-Allow-Methods", "GET, POST");
  DefaultHeaders::Instance().addHeader("Access-Control-Allow-Headers", "*");

  server.on("/configuration", HTTP_POST, [] (AsyncWebServerRequest *request) {
      request -> send(200, "OK");
    }, NULL, onConfigure);

  server.serveStatic("/", SPIFFS, "/").setDefaultFile(indexFile);

  server.onNotFound([](AsyncWebServerRequest *request) {
    if (request -> method() == HTTP_OPTIONS) request -> send(200);
    else request -> send(404, "Not found");
  });

  server.begin();
  Serial.println("HTTP Server started!");
}

void startConfiguration() {
  Serial.println("Plumbob is not configured!");
  configured = false;
  indexFile = "configuration.html";

  saveStatus();

  char* ssid = "Plumbob";
  WiFi.softAP(ssid);
  Serial.println("Starting Access Point for configuration...");
  Serial.print("SSID: '");
  Serial.print(ssid);
  Serial.println("'");
  Serial.println("Password: ''");
  Serial.print("IP: ");
  Serial.println(WiFi.softAPIP());

  Serial.println("Access Point ready!");

  startConfigServer();
}

void onEnable(AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
  StaticJsonDocument<200> jsonDocument;
  DeserializationError error = deserializeJson(jsonDocument, data);

  if (error) {
    Serial.println("Json deserialization failed.");
    return;
  }

  ledEnabled = jsonDocument["enabled"];
  Serial.print("LEDs ");
  if (ledEnabled) Serial.println("enabled");
  else Serial.println("disabled");
}

void onBrightness(AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
  StaticJsonDocument<200> jsonDocument;
  DeserializationError error = deserializeJson(jsonDocument, data);

  if (error) {
    Serial.println("Json deserialization failed.");
    return;
  }

  brightness = jsonDocument["brightness"];
  Serial.print("Brightness set to ");
  Serial.println(brightness);
}

void onSettings(AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
  StaticJsonDocument<200> jsonDocument;
  DeserializationError error = deserializeJson(jsonDocument, data);

  if (error) {
    Serial.println("Json deserialization failed.");
    return;
  }

  mode = jsonDocument["mode"];

  Serial.print("Mode set to ");
  Serial.println(mode);

  switch (mode) {
    case 1:
      if (jsonDocument.containsKey("speed")) {
        fullRainbowSpeed = jsonDocument["speed"];
        Serial.print("Speed set to ");
        Serial.println(fullRainbowSpeed);
      }
      break;
    case 2:
      if (jsonDocument.containsKey("speed")) {
        animatedRainbowSpeed = jsonDocument["speed"];
        Serial.print("Speed set to ");
        Serial.println(animatedRainbowSpeed);
      }
      break;
    case 3:
      if (jsonDocument.containsKey("color")) {
        randomSColor = jsonDocument["color"];
        Serial.print("Hue set to ");
        Serial.println(randomSColor);
      }
      if (jsonDocument.containsKey("speed")) {
        randomSColorSpeed = jsonDocument["speed"];
        Serial.print("Speed set to ");
        Serial.println(randomSColorSpeed);
      }
      break;
    case 5:
      if (jsonDocument.containsKey("speed")) {
        animatedPaletteSpeed = jsonDocument["speed"];
        Serial.print("Speed set to ");
        Serial.println(animatedPaletteSpeed);
      }
      break;
    case 6:
      if (jsonDocument.containsKey("speed")) {
        fadeToBlackSpeed = jsonDocument["speed"];
        Serial.print("Speed set to ");
        Serial.println(fadeToBlackSpeed);
      }
      if (jsonDocument.containsKey("fade")) {
        fadeToBlackFadeSpeed = jsonDocument["fade"];
        Serial.print("Fade speed set to ");
        Serial.println(fadeToBlackFadeSpeed);
      }
      break;
    case 7:
      if (jsonDocument.containsKey("bpm")) {
        bpmColor = jsonDocument["bpm"];
        Serial.print("BPM set to ");
        Serial.println(bpmColor);
      }
      if (jsonDocument.containsKey("fade")) {
        fadeColorSpeed = jsonDocument["fade"];
        Serial.print("Fade speed set to ");
        Serial.println(fadeColorSpeed);
      }
      if (jsonDocument.containsKey("red") && jsonDocument.containsKey("green") && jsonDocument.containsKey("blue")) {
        rgbColor = CRGB(jsonDocument["red"], jsonDocument["green"], jsonDocument["blue"]);
        Serial.print("Color set to ");
        Serial.print(rgbColor.red);
        Serial.print(" ");
        Serial.print(rgbColor.red);
        Serial.print(" ");
        Serial.println(rgbColor.red);
      }
      break;
    case 9:
      if (jsonDocument.containsKey("speed")) {
        fireSpeed = jsonDocument["speed"];
        Serial.print("Speed set to ");
        Serial.println(fireSpeed);
      }
      if (jsonDocument.containsKey("cooling")) {
        fireCooling = jsonDocument["cooling"];
        Serial.print("Cooling set to ");
        Serial.println(fireCooling);
      }
      if (jsonDocument.containsKey("sparks")) {
        fireSparks = jsonDocument["sparks"];
        Serial.print("Sparks set to ");
        Serial.println(fireSparks);
      }
      if (jsonDocument.containsKey("reverse")) {
        fireReverse = jsonDocument["reverse"];
        Serial.print("Fire reverse set to ");
        Serial.println(fireReverse);
      }
      break;
    case 10:
      if (jsonDocument.containsKey("red") && jsonDocument.containsKey("green") && jsonDocument.containsKey("blue")) {
        uint8_t red = jsonDocument["red"];
        uint8_t green = jsonDocument["green"];
        uint8_t blue = jsonDocument["blue"];
        
        staticRGBColor = CRGB(red, green, blue);
        Serial.print("Color set to ");
        Serial.print(red);
        Serial.print(" ");
        Serial.print(green);
        Serial.print(" ");
        Serial.println(blue);
      }
      break;
  }
}

void onSave(AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
  saveStatus();
}

void onGetSettings(AsyncWebServerRequest *request) {
  String response = "{";

  response += "\"brightness\": ";
  response += brightness;
  response += ", ";

  response += "\"mode\": ";
  response += mode;
  response += ", ";

  response += "\"led_enabled\": ";
  response += ledEnabled;
  response += ", ";

  response += "\"full_rainbow_speed\": ";
  response += fullRainbowSpeed;
  response += ", ";

  response += "\"animated_rainbow_speed\": ";
  response += animatedRainbowSpeed;
  response += ", ";

  response += "\"random_static_color\": ";
  response += randomSColor;
  response += ", ";

  response += "\"random_static_color_speed\": ";
  response += randomSColorSpeed;
  response += ", ";

  response += "\"animated_palette_speed\": ";
  response += animatedPaletteSpeed;
  response += ", ";

  response += "\"fade_to_black_speed\":";
  response += fadeToBlackSpeed;
  response += ", ";

  response += "\"fade_to_black_fade_speed\": ";
  response += fadeToBlackFadeSpeed;
  response += ", ";

  response += "\"bpm_color\": ";
  response += bpmColor;
  response += ", ";

  response += "\"fade_color_speed\": ";
  response += fadeColorSpeed;
  response += ", ";

  response += "\"b_rgb_color\": \"";
  response += rgbColor.red;
  response += ",";
  response += rgbColor.green;
  response += ",";
  response += rgbColor.blue;
  response += "\", ";

  response += "\"fire_speed\": ";
  response += fireSpeed;
  response += ", ";

  response += "\"fire_cooling\": ";
  response += fireCooling;
  response += ", ";

  response += "\"fire_sparks\": ";
  response += fireSparks;
  response += ", ";

  response += "\"fire_reverse\": ";
  response += fireReverse;
  response += ", ";

  response += "\"s_rgb_color\": \"";
  response += rgbColor.red;
  response += ",";
  response += rgbColor.green;
  response += ",";
  response += rgbColor.blue;
  response += "\"";

  response += "}";

  request -> send(200, "text/json", response);
}

void startServer() {
  DefaultHeaders::Instance().addHeader("Access-Control-Allow-Origin", "*");
  DefaultHeaders::Instance().addHeader("Access-Control-Allow-Methods", "GET, POST");
  DefaultHeaders::Instance().addHeader("Access-Control-Allow-Headers", "*");

  server.on("/enable", HTTP_POST, [] (AsyncWebServerRequest *request) {
      request -> send(200, "OK");
    }, NULL, onEnable);

  server.on("/brightness", HTTP_POST, [] (AsyncWebServerRequest *request) {
      request -> send(200, "OK");
    }, NULL, onBrightness);

    server.on("/settings", HTTP_POST, [] (AsyncWebServerRequest *request) {
      request -> send(200, "OK");
    }, NULL, onSettings);

    server.on("/save", HTTP_POST, [] (AsyncWebServerRequest *request) {
      request -> send(200, "OK");
    }, NULL, onSave);

    server.on("/get_settings", HTTP_GET, [] (AsyncWebServerRequest *request) {
      onGetSettings(request);
    }, NULL);

  server.serveStatic("/", SPIFFS, "/").setDefaultFile(indexFile);

  server.onNotFound([](AsyncWebServerRequest *request) {
    if (request -> method() == HTTP_OPTIONS) request -> send(200);
    else request -> send(404, "Not found");
  });

  server.begin();
  Serial.println("HTTP Server started!");
}

void startPlumbob() {
  Serial.println("Plumbob is configured!");
  configured = true;
  indexFile = "index.html";

  int ssidSize = EEPROM.read(1);
  char ssid[ssidSize];
  for (int i = 0; i < ssidSize; i++) {
    ssid[i] = EEPROM.read(i + 2);
  }

  int passwordSize = EEPROM.read(63);
  char password[passwordSize];
  for (int i = 0; i < passwordSize; i++) {
    password[i] = EEPROM.read(i + 64);
  }

  while (!connectToWiFi(ssid, password)) {
    Serial.print(".");
    delay(1000);
  }

  startServer();

  brightness = readInt(E_DATA_START);
  mode = EEPROM.read(E_DATA_START + 2);
  ledEnabled = EEPROM.read(E_DATA_START + 3) == 1 ? true : false;
  fullRainbowSpeed = readInt(E_DATA_START + 4);
  animatedRainbowSpeed = readInt(E_DATA_START + 6);
  randomSColor = readInt(E_DATA_START + 8);
  randomSColorSpeed = readInt(E_DATA_START + 10);
  animatedPaletteSpeed = readInt(E_DATA_START + 12);
  fadeToBlackSpeed = readInt(E_DATA_START + 14);
  fadeToBlackFadeSpeed = readInt(E_DATA_START + 16);
  bpmColor = readInt(E_DATA_START + 18);
  fadeColorSpeed = readInt(E_DATA_START + 20);
  rgbColor = readRGB(E_DATA_START + 22);
  fireSpeed = readInt(E_DATA_START + 28);
  fireCooling = readInt(E_DATA_START + 30);
  fireSparks = readInt(E_DATA_START + 32);
  fireReverse = EEPROM.read(E_DATA_START + 34) == 1 ? true : false;
  staticRGBColor = readRGB(E_DATA_START + 35);
}

void reset() {
  Serial.println("Resetting...");
  FastLED.clear();
  FastLED.show();

  for (int i = 0; i < ESIZE; i++) {
    EEPROM.write(i, 0);
  }

  EEPROM.commit();

  Serial.println("Restarting...");
  ESP.restart();
}

void setup() {
  delay(3000);

  Serial.begin(115200);
  Serial.println();

  EEPROM.begin(ESIZE);
  SPIFFS.begin();

  pinMode(BTN_PIN, INPUT);

  FastLED.addLeds<WS2811, LED_PIN, GRB>(leds, NUM_LEDS).setCorrection(TypicalLEDStrip);
  FastLED.setCorrection(TypicalPixelString);
  FastLED.clear();
  FastLED.show();

  if (EEPROM.read(0) == 0) startConfiguration();
  else startPlumbob();
}

void loop() {
  btnCurrentState = digitalRead(BTN_PIN);

  if (btnLastState == LOW && btnCurrentState == HIGH) {
    pressMillis = millis();
    btnLastState = btnCurrentState;
  } else if (btnLastState == HIGH && btnCurrentState == LOW) {
    long pressTime = millis() - pressMillis;

    if (pressTime > VERY_LONG_PRESS_TIME) {
      reset();
    } else if (pressTime > LONG_PRESS_TIME) {
      if (ledEnabled) {
        ledEnabled = false;
        Serial.println("LEDs disabled");
      } else {
        ledEnabled = true;
        Serial.println("LEDs enabled");
      }
    } else {
      mode++;
      if (mode > 10) mode = 0;
      FastLED.clear();
      FastLED.show();

      Serial.print("Mode set to ");
      Serial.println(mode);
    }

    pressMillis = 0;
    btnLastState = btnCurrentState;
  }

  FastLED.setBrightness(brightness);

  if (!configured) {
    animatedPalette(configPalette);
  } else {
    if (WiFi.status() != WL_CONNECTED) {
      Serial.println("WiFi disconnected!");
      Serial.print("Reconnecting");
      WiFi.disconnect();
      stopServer();
      startPlumbob();
    }

    if (ledEnabled) {
      switch (mode) {
        case 0:
          staticRainbow();
          break;
        case 1:
          fullRainbow();
          break;
        case 2:
          animatedRainbow();
          break;
        case 3:
          randomSingleColor();
          break;
        case 4:
          staticPalette(cyanPalette);
          break;
        case 5:
          animatedPalette(cyanPalette);
          break;
        case 6:
          fadeToBlack(cyanPalette);
          break;
        case 7:
          beatRGB();
          break;
        case 8:
          beatPalette(cyanPalette);
          break;
        case 9:
          fire();
          break;
        case 10:
          staticRGB(staticRGBColor);
          break;
      }
    } else {
      FastLED.clear();
      FastLED.show();
    }
  }
}
