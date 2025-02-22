#include <WiFiClientSecure.h>
#include <WiFi.h>
#include <Adafruit_GFX.h>     // Core graphics library
#include <Adafruit_ST7789.h>  // Hardware-specific library for ST7789
#include <SPI.h>
#include <secrets.h>
#include "driver/rtc_io.h"

// Use dedicated hardware SPI pins
Adafruit_ST7789 tft = Adafruit_ST7789(TFT_CS, TFT_DC, TFT_RST);

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

// wifi
WiFiClientSecure wfcs;

// info for a track
struct TrackInfo {
  String id;
  int duration_ms;
  int progress_ms;
};

// info for a token
struct TokenData {
  String access_token;
  String refresh_token;
  int expires_at;  // milliseconds
};

// default credentials
struct TokenData credentials = {
  "",
  REFRESH_TOKEN,
  0
};

void setup(void) {
  Serial.begin(115200);

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

  // hello!
  Serial.println();
  Serial.println(F("Hello! Booting up Bootleg Spotify Car Thing"));

  // set input button pins
  pinMode(d0_pin, INPUT_PULLUP);
  pinMode(d1_pin, INPUT);
  pinMode(d2_pin, INPUT);

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
      esp_deep_sleep_start();
      return;
    }
    previous_d0 = button_state;
  }

  button_state = digitalRead(d1_pin);
  if (button_state != previous_d1) {
    if (button_state == HIGH) {
      // button press
      Serial.println("d1 click");
      drawBigText("d1");
      toggleLiked();
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
      struct TrackInfo info = getTrackInfo();
      seek(info.progress_ms - 16000);  // go back 16 seconds
    } else {
      tft.fillScreen(ST77XX_BLACK);
    }
    previous_d2 = button_state;
  }

  delay(50);  // debounce
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

// input: a string like "access_token":"...","refresh_token":"...","expires_in":3600}
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

  // expires at now + expires_in - 5 minutes
  int expires_at = millis() + (expires_in.toInt() * 1000) - 300000;

  return { access_token, refresh_token, expires_at };
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

void refresh_if_expired() {
  if (credentials.expires_at < millis()) {
    refresh();
  }
}

// this was just for testing
void nextSong() {
  Serial.println("next song...");
  refresh_if_expired();

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

struct TrackInfo getTrackInfo() {
  Serial.println("track info...");
  refresh_if_expired();
  struct TrackInfo err_resp = { "", 0, 0 };

  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("trackInfo() failed: Wifi not connected");
    return err_resp;
  }

  if (wfcs.connect("api.spotify.com", 443)) {
    wfcs.println("GET /v1/me/player/currently-playing HTTP/1.1");
    wfcs.println("Host: api.spotify.com");
    wfcs.print("Authorization: Bearer ");
    wfcs.println(credentials.access_token);
    wfcs.println();

    int step_id = 0;
    int step_pm = 0;
    int step_dm = 0;
    String id = "";
    String progress_ms = "";
    String duration_ms = "";
    unsigned long startMillis = millis();
    int count = 0;

    while (startMillis + 5000 > millis() && (step_id >= 0 || step_pm >= 0 || step_dm >= 0)) {
      while (wfcs.available()) {
        char c = wfcs.read();

        // FSMs go brrrrrr, but for real these could break at any time
        // There are multiple instances of "id" in the response, so cheat and look for com/v1/tracks/..." instead (part of the href)
        switch (step_id) {
          case 0:
            if (c == 'c') {
              step_id = 1;
            }
            break;
          case 1:
            if (c == 'o') {
              step_id = 2;
            } else {
              step_id = 0;
            }
            break;
          case 2:
            if (c == 'm') {
              step_id = 3;
            } else {
              step_id = 0;
            }
            break;
          case 3:
            if (c == '/') {
              step_id = 4;
            } else {
              step_id = 0;
            }
            break;
          case 4:
            if (c == 'v') {
              step_id = 5;
            } else {
              step_id = 0;
            }
            break;
          case 5:
            if (c == '1') {
              step_id = 6;
            } else {
              step_id = 0;
            }
            break;
          case 6:
            if (c == '/') {
              step_id = 7;
            } else {
              step_id = 0;
            }
            break;
          case 7:
            if (c == 't') {
              step_id = 8;
            } else {
              step_id = 0;
            }
            break;
          case 8:
            if (c == 'r') {
              step_id = 9;
            } else {
              step_id = 0;
            }
            break;
          case 9:
            if (c == 'a') {
              step_id = 10;
            } else {
              step_id = 0;
            }
            break;
          case 10:
            if (c == 'c') {
              step_id = 11;
            } else {
              step_id = 0;
            }
            break;
          case 11:
            if (c == 'k') {
              step_id = 12;
            } else {
              step_id = 0;
            }
            break;
          case 12:
            if (c == 's') {
              step_id = 13;
            } else {
              step_id = 0;
            }
            break;
          case 13:
            if (c == '/') {
              step_id = 14;
            } else {
              step_id = 0;
            }
            break;
          case 14:
            if (c == '"') {
              step_id = -1;
            } else {
              id += c;
            }
            break;
        }

        switch (step_pm) {
          case 0:
            if (c == '"') {
              step_pm = 1;
            }
            break;
          case 1:
            if (c == 'p') {
              step_pm = 2;
            } else {
              step_pm = 0;
            }
            break;
          case 2:
            if (c == 'r') {
              step_pm = 3;
            } else {
              step_pm = 0;
            }
            break;
          case 3:
            if (c == 'o') {
              step_pm = 4;
            } else {
              step_pm = 0;
            }
            break;
          case 4:
            if (c == 'g') {
              step_pm = 5;
            } else {
              step_pm = 0;
            }
            break;
          case 5:
            if (c == 'r') {
              step_pm = 6;
            } else {
              step_pm = 0;
            }
            break;
          case 6:
            if (c == 'e') {
              step_pm = 7;
            } else {
              step_pm = 0;
            }
            break;
          case 7:
            if (c == 's') {
              step_pm = 8;
            } else {
              step_pm = 0;
            }
            break;
          case 8:
            if (c == 's') {
              step_pm = 9;
            } else {
              step_pm = 0;
            }
            break;
          case 9:
            if (c == '_') {
              step_pm = 10;
            } else {
              step_pm = 0;
            }
            break;
          case 10:
            if (c == 'm') {
              step_pm = 11;
            } else {
              step_pm = 0;
            }
            break;
          case 11:
            if (c == 's') {
              step_pm = 12;
            } else {
              step_pm = 0;
            }
            break;
          case 12:
            if (c == '"') {
              step_pm = 13;
            } else {
              step_pm = 0;
            }
            break;
          case 13:
            if (c == '0' || c == '1' || c == '2' || c == '3' || c == '4' || c == '5' || c == '6' || c == '7' || c == '8' || c == '9') {
              progress_ms += c;
              step_pm = 14;
            }
            break;
          case 14:
            if (c == '0' || c == '1' || c == '2' || c == '3' || c == '4' || c == '5' || c == '6' || c == '7' || c == '8' || c == '9') {
              progress_ms += c;
            } else {
              step_pm = -1;
            }
            break;
        }

        switch (step_dm) {
          case 0:
            if (c == '"') {
              step_dm = 1;
            }
            break;
          case 1:
            if (c == 'd') {
              step_dm = 2;
            } else {
              step_dm = 0;
            }
            break;
          case 2:
            if (c == 'u') {
              step_dm = 3;
            } else {
              step_dm = 0;
            }
            break;
          case 3:
            if (c == 'r') {
              step_dm = 4;
            } else {
              step_dm = 0;
            }
            break;
          case 4:
            if (c == 'a') {
              step_dm = 5;
            } else {
              step_dm = 0;
            }
            break;
          case 5:
            if (c == 't') {
              step_dm = 6;
            } else {
              step_dm = 0;
            }
            break;
          case 6:
            if (c == 'i') {
              step_dm = 7;
            } else {
              step_dm = 0;
            }
            break;
          case 7:
            if (c == 'o') {
              step_dm = 8;
            } else {
              step_dm = 0;
            }
            break;
          case 8:
            if (c == 'n') {
              step_dm = 9;
            } else {
              step_dm = 0;
            }
            break;
          case 9:
            if (c == '_') {
              step_dm = 10;
            } else {
              step_dm = 0;
            }
            break;
          case 10:
            if (c == 'm') {
              step_dm = 11;
            } else {
              step_dm = 0;
            }
            break;
          case 11:
            if (c == 's') {
              step_dm = 12;
            } else {
              step_dm = 0;
            }
            break;
          case 12:
            if (c == '"') {
              step_dm = 13;
            } else {
              step_dm = 0;
            }
            break;
          case 13:
            if (c == '0' || c == '1' || c == '2' || c == '3' || c == '4' || c == '5' || c == '6' || c == '7' || c == '8' || c == '9') {
              duration_ms += c;
              step_dm = 14;
            }
            break;
          case 14:
            if (c == '0' || c == '1' || c == '2' || c == '3' || c == '4' || c == '5' || c == '6' || c == '7' || c == '8' || c == '9') {
              duration_ms += c;
            } else {
              step_dm = -1;
            }
            break;
        }
      }
    }

    wfcs.stop();

    return { id, duration_ms.toInt(), progress_ms.toInt() };
  } else {
    Serial.println("trackInfo() failed: couldn't connect");
  }

  return err_resp;
}

void seek(int ms) {
  Serial.print("seek ");
  Serial.print(ms);
  Serial.println("...");
  refresh_if_expired();
  if (ms < 0) {
    ms = 0;
  }

  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("seek() failed: Wifi not connected");
    return;
  }

  if (wfcs.connect("api.spotify.com", 443)) {
    wfcs.println(String("PUT /v1/me/player/seek?position_ms=") + String(ms) + " HTTP/1.1");
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

void toggleLiked() {
  Serial.println("toggle liked...");

  // get current track (getTrackInfo())
  struct TrackInfo info = getTrackInfo();
  if (info.id == "") {
    Serial.println("toggleLiked() failed: no track id");
    return;
  }

  bool alreadyLiked = false;
  // check user's saved tracks (GET https://api.spotify.com/v1/me/tracks/contains?ids=...)
  if (wfcs.connect("api.spotify.com", 443)) {
    wfcs.println(String("GET /v1/me/tracks/contains?ids=") + info.id + " HTTP/1.1");
    wfcs.println("Host: api.spotify.com");
    wfcs.print("Authorization: Bearer ");
    wfcs.println(credentials.access_token);
    wfcs.println("Content-Length: 0");
    wfcs.println();

    // response is an array of booleans
    // Example: [false,true]

    int step_bool = 0;
    unsigned long startMillis = millis();
    while (startMillis + 5000 > millis() && step_bool >= 0) {
      while (wfcs.available()) {
        char c = wfcs.read();

        switch (step_bool) {
          case 0:
            if (c == '\r') {
              step_bool = 1;
            }
            break;
          case 1:
            if (c == '\n') {
              step_bool = 2;
            } else {
              step_bool = 0;
            }
            break;
          case 2:
            if (c == '\r') {
              step_bool = 3;
            } else {
              step_bool = 0;
            }
            break;
          case 3:
            if (c == '\n') {
              step_bool = 4;
            } else {
              step_bool = 0;
            }
            break;
          case 4:
            if (c == '[') {
              step_bool = 5;
            } else {
              step_bool = 0;
            }
            break;
          case 5:
            if (c == 't') {
              alreadyLiked = true;
            } else if (c == 'f') {
              alreadyLiked = false;
            }
            step_bool = -1;
            break;
        }
      }
    }

    wfcs.stop();
  } else {
    Serial.println("toggleLiked() failed: couldn't connect (0)");
  }

  if (alreadyLiked) {
    if (wfcs.connect("api.spotify.com", 443)) {
      wfcs.println(String("DELETE /v1/me/tracks?ids=") + info.id + " HTTP/1.1");
      wfcs.println("Host: api.spotify.com");
      wfcs.print("Authorization: Bearer ");
      wfcs.println(credentials.access_token);
      wfcs.println("Content-Length: 0");
      wfcs.println();
      // ignore response
      wfcs.stop();
    } else {
      Serial.println("toggleLiked() failed: couldn't connect (1)");
    }
  } else {
    if (wfcs.connect("api.spotify.com", 443)) {
      wfcs.println(String("PUT /v1/me/tracks?ids=") + info.id + " HTTP/1.1");
      wfcs.println("Host: api.spotify.com");
      wfcs.print("Authorization: Bearer ");
      wfcs.println(credentials.access_token);
      wfcs.println("Content-Length: 0");
      wfcs.println();
      // ignore response
      wfcs.stop();
    } else {
      Serial.println("toggleLiked() failed: couldn't connect (2)");
    }
  }
}
