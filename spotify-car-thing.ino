#include <WiFiClientSecure.h>
#include <WiFi.h>
// #include <HTTPClient.h>
#include <Adafruit_GFX.h>     // Core graphics library
#include <Adafruit_ST7789.h>  // Hardware-specific library for ST7789
#include <SPI.h>
#include <secrets.h>

// for secrets setup, look into https://github.com/espired/esp32-spotify-controller

// Use dedicated hardware SPI pins
Adafruit_ST7789 tft = Adafruit_ST7789(TFT_CS, TFT_DC, TFT_RST);

float p = 3.1415926;

// button pins
const int d0_pin = 0;
const int d1_pin = 1;
const int d2_pin = 2;

// detect state change of button values by comparing to previous
int previous_d0 = HIGH;
int previous_d1 = LOW;
int previous_d2 = LOW;

// placeholder variable for button state
int button_state = 0;

// Initialize the Ethernet client library
// with the IP address and port of the server
// that you want to connect to (port 80 is default for HTTP):
WiFiClientSecure wfcs;

#define SERVER "example.org"
// #define SERVER "https://accounts.spotify.com"
#define PATH "/"
// #define PATH "/api/token"

struct TokenData {
  String access_token;
  String refresh_token;
  int expires_in; // TODO: change this to expires_at, in millis
};

struct TokenData credentials = {
  "",
  REFRESH_TOKEN,
  0
};

void setup(void) {
  Serial.begin(115200);
  delay(10);
  Serial.println(F("Hello! Booting up Bootleg Spotify Car Thing"));

  // set input button pins
  pinMode(d0_pin, INPUT_PULLUP);
  pinMode(d1_pin, INPUT);
  pinMode(d2_pin, INPUT);

  // turn on backlite
  pinMode(TFT_BACKLITE, OUTPUT);
  digitalWrite(TFT_BACKLITE, HIGH);

  // turn on the TFT / I2C power supply
  pinMode(TFT_I2C_POWER, OUTPUT);
  digitalWrite(TFT_I2C_POWER, HIGH);
  delay(10);

  // initialize TFT
  tft.init(135, 240);  // Init ST7789 240x135
  tft.setRotation(3);  // or 1 if mounted with buttons on the right
  tft.fillScreen(ST77XX_BLACK);

  // optimized lines
  testfastlines(ST77XX_RED, ST77XX_BLUE);
  delay(500);

  // fancy rectangles
  testfillrects(ST77XX_YELLOW, ST77XX_MAGENTA);
  delay(500);

  // clear screen
  tft.fillScreen(ST77XX_BLACK);

  Serial.println();
  Serial.print(F("Connecting to WiFi "));
  Serial.println(SSID);

  WiFi.begin(SSID, PASSWORD);

  int dot_count = 0;
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");

    switch (dot_count) {
      case 0:
        drawBigText("Wifi  .");
        break;
      case 1:
        drawBigText("Wifi  ..");
        break;
      default:
        drawBigText("Wifi  ...");
        break;
    }

    dot_count += 1;
    if (dot_count >= 3) {
      dot_count = 0;
    }
  }

  // clear screen
  tft.fillScreen(ST77XX_BLACK);

  // don't use a root cert
  wfcs.setInsecure();

  // log wifi info
  Serial.println();
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());

  refresh();

  Serial.println("all set up, nothing more");
}

void loop() {
  button_state = digitalRead(d0_pin);
  if (button_state != previous_d0) {
    if (button_state == LOW) {
      // button press
      Serial.println("d0 click");
      drawBigText("d0");
    } else {
      tft.fillScreen(ST77XX_BLACK);
    }
    previous_d0 = button_state;
  }

  button_state = digitalRead(d1_pin);
  if (button_state != previous_d1) {
    if (button_state == HIGH) {
      // button press
      Serial.println("d1 click");
      drawBigText("d1");
      nextSong();
    } else {
      tft.fillScreen(ST77XX_BLACK);
    }
    previous_d1 = button_state;
  }

  button_state = digitalRead(d2_pin);
  if (button_state != previous_d2) {
    if (button_state == HIGH) {
      // button press
      Serial.println("d2 click");
      drawBigText("d2");
    } else {
      tft.fillScreen(ST77XX_BLACK);
    }
    previous_d2 = button_state;
  }

  delay(50);
}

void drawBigText(char *text) {
  tft.fillScreen(ST77XX_BLACK);
  tft.setCursor(0, 0);
  tft.setTextColor(ST77XX_WHITE);
  tft.setTextWrap(true);
  tft.setTextSize(6);
  tft.print(text);
}

void testfastlines(uint16_t color1, uint16_t color2) {
  tft.fillScreen(ST77XX_BLACK);
  for (int16_t y = 0; y < tft.height(); y += 5) {
    tft.drawFastHLine(0, y, tft.width(), color1);
  }
  for (int16_t x = 0; x < tft.width(); x += 5) {
    tft.drawFastVLine(x, 0, tft.height(), color2);
  }
}

void testdrawrects(uint16_t color) {
  tft.fillScreen(ST77XX_BLACK);
  for (int16_t x = 0; x < tft.width(); x += 6) {
    tft.drawRect(tft.width() / 2 - x / 2, tft.height() / 2 - x / 2, x, x,
                 color);
  }
}

void testfillrects(uint16_t color1, uint16_t color2) {
  tft.fillScreen(ST77XX_BLACK);
  for (int16_t x = tft.width() - 1; x > 6; x -= 6) {
    tft.fillRect(tft.width() / 2 - x / 2, tft.height() / 2 - x / 2, x, x,
                 color1);
    tft.drawRect(tft.width() / 2 - x / 2, tft.height() / 2 - x / 2, x, x,
                 color2);
  }
}

void tftPrintTest() {
  tft.setTextWrap(false);
  tft.fillScreen(ST77XX_BLACK);
  tft.setCursor(0, 30);
  tft.setTextColor(ST77XX_RED);
  tft.setTextSize(1);
  tft.println("Hello World!");
  tft.setTextColor(ST77XX_YELLOW);
  tft.setTextSize(2);
  tft.println("Hello World!");
  tft.setTextColor(ST77XX_GREEN);
  tft.setTextSize(3);
  tft.println("Hello World!");
  tft.setTextColor(ST77XX_BLUE);
  tft.setTextSize(4);
  tft.print(1234.567);
  delay(1500);
  tft.setCursor(0, 0);
  tft.fillScreen(ST77XX_BLACK);
  tft.setTextColor(ST77XX_WHITE);
  tft.setTextSize(0);
  tft.println("Hello World!");
  tft.setTextSize(1);
  tft.setTextColor(ST77XX_GREEN);
  tft.print(p, 6);
  tft.println(" Want pi?");
  tft.println(" ");
  tft.print(8675309, HEX);  // print 8,675,309 out in HEX!
  tft.println(" Print HEX!");
  tft.println(" ");
  tft.setTextColor(ST77XX_WHITE);
  tft.println("Sketch has been");
  tft.println("running for: ");
  tft.setTextColor(ST77XX_MAGENTA);
  tft.print(millis() / 1000);
  tft.setTextColor(ST77XX_WHITE);
  tft.print(" seconds.");
}

// input: a string like "access_token":"BQAlursFATlCZyadD3rgV-x-4rlqMEdLu0cUd2p2AwETNWFMkAk3DDcnf4v0d8FL7IBx8MlU3ImRYwy0YUivvLzaZjrNjf5AJwNcCXnpo94LCsGPhW57P74PsYBJJxCPV89z9jnGeMc","token_type":"Bearer","expires_in":3600}
struct TokenData parseTokenPayload(String buffer) {
  int start = buffer.indexOf("access_token");
  String access_token = buffer.substring(start + 15);
  int end = access_token.indexOf("\"");
  access_token = access_token.substring(0, end);

  start = buffer.indexOf("refresh_token");
  String refresh_token = credentials.refresh_token;
  if (start >= 0) {
    String refresh_token = buffer.substring(start + 16);
    end = refresh_token.indexOf("\"");
    refresh_token = refresh_token.substring(0, end);
  }

  start = buffer.indexOf("expires_in");
  String expires_in = buffer.substring(start + 12);
  end = expires_in.indexOf("\"");
  if (end < 0) {
    end = expires_in.indexOf("}");
  }
  expires_in = expires_in.substring(0, end);

  // TODO: convert expires_in to expires_at

  return { access_token, refresh_token, expires_in.toInt() };
}

void refresh() {
  Serial.println("refreshing...");

  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("refresh() failed: Wifi not connected");
    return;
  }

  // can test POST to https://www.httpbin.org/post, but must include a body (add `http.println("key=value");` after `wfcs.println();` and update Content-Length)

  if (wfcs.connect("accounts.spotify.com", 443)) {
    String params = String("grant_type=refresh_token&client_id=") + CLIENT_ID + "&client_secret=" + CLIENT_SECRET + "&refresh_token=" + credentials.refresh_token;

    wfcs.println("POST /api/token?" + params + " HTTP/1.1");
    wfcs.println("Host: accounts.spotify.com");
    wfcs.println("Content-Type: application/x-www-form-urlencoded");
    wfcs.println("Content-Length: 0");
    wfcs.println();

    int i = 0;
    int step = 0;
    char buffer[511];
    unsigned long startMillis = millis();

    // looking for ['\r','\n',' '] followed by '{', then read everything until '}'
    while (startMillis + 5000 > millis() && step < 3 && i < 511) {
      if (wfcs.available()) {
        char c = wfcs.read();

        if (step == 2) {
          buffer[i++] = c;
        }

        switch (step) {
          case 0:
            if (c == '\r' || c == '\n' || c == ' ') {
              step = 1;
            }
            break;
          case 1:
            if (c == '{') {
              step = 2;
            } else {
              // step = 0;
            }
            break;
          case 2:
            if (c == '}') {
              step = 3;
            }
            break;
        }
      } else {
        delay(10);
      }
    }

    wfcs.stop();
    String payload = String(buffer);

    if (step != 3) {
      Serial.println(payload);
      Serial.println(i);
      Serial.println(step);
      Serial.println("refresh() failed: payload not found");
      return;
    }

    credentials = parseTokenPayload(payload);

    return;
  } else {
    Serial.println("refresh() failed: couldn't connect");
  }

  return;
}

void nextSong() {
  // POST https://api.spotify.com/v1/me/player/next
  // Authorization: Bearer credentials.token

  // TODO: refresh token if needed

  Serial.println("next song...");

  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("nextSong() failed: Wifi not connected");
    return;
  }

  if (wfcs.connect("api.spotify.com", 443)) {
    wfcs.println("POST /v1/me/player/next HTTP/1.1");
    wfcs.println("Host: api.spotify.com");
    wfcs.print("Authorization: Bearer ");
    wfcs.println(credentials.access_token);
    wfcs.println("Content-Length: 0");
    wfcs.println();
    // ignore any response
    wfcs.stop();
  } else {
    Serial.println("nextSong() failed: couldn't connect");
  }
}
