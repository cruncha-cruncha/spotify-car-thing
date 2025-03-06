#include <WiFiClientSecure.h>
#include <WiFi.h>
#include "secrets.h"

// RENC stands for Rotary ENCoder
#define RENC_PIN 16
#define RENC_A 14
#define RENC_B 15
#define RED_PIN 5
#define BLUE_PIN 28
#define GREEN_PIN 3
#define YELLOW_PIN 18

// for allocating static char buffers
#define MAX_TOKEN_LENGTH 1024
#define MAX_TRACK_ID_LENGTH 256

// debugging
#define DEBUG
#ifdef DEBUG
#define DEBUG_PRINTLN(x) Serial.println(x)
#define DEBUG_PRINT(x) Serial.print(x)
#else
#define DEBUG_PRINTLN(x)
#define DEBUG_PRINT(x)
#endif

// give each core a full 8K stack
bool core1_separate_stack = true;

// detect state change of button values by comparing to previous
int previous_renc = LOW;
int previous_red = LOW;
int previous_blue = LOW;
int previous_green = LOW;
int previous_yellow = LOW;

// rotary encoder rotation direction detection
int renc_rot_status = 0;

// placeholder variable for button state
int button_state = 0;

void setup(void) {
  Serial.begin(115200);
  Serial.println();
  Serial.println("Hello! Booting up Bootleg Spotify Car Thing (core 0)");

  // set input button pins
  pinMode(RENC_A, INPUT_PULLUP);
  pinMode(RENC_B, INPUT_PULLUP);
  pinMode(RENC_PIN, INPUT_PULLUP);
  pinMode(BLUE_PIN, INPUT_PULLUP);
  pinMode(GREEN_PIN, INPUT_PULLUP);
  pinMode(YELLOW_PIN, INPUT_PULLUP);
  pinMode(RED_PIN, INPUT_PULLUP);

  // quadtrature encoders are sensitive, they need interrupts for proper detection?
  attachInterrupt(RENC_A, handleChangeA, CHANGE);
  attachInterrupt(RENC_B, handleChangeB, CHANGE);

  Serial.println("core 0 all set up");
}

void loop() {
  button_state = digitalRead(RENC_PIN);
  if (button_state != previous_renc) {
    if (button_state == LOW) {
      DEBUG_PRINTLN("renc click, core 0");
      rp2040.fifo.push(1);
    }
    previous_renc = button_state;
  }

  button_state = digitalRead(BLUE_PIN);
  if (button_state != previous_blue) {
    if (button_state == LOW) {
      DEBUG_PRINTLN("blue click, core 0");
      rp2040.fifo.push(2);
    }
    previous_blue = button_state;
  }

  button_state = digitalRead(GREEN_PIN);
  if (button_state != previous_green) {
    if (button_state == LOW) {
      DEBUG_PRINTLN("green click, core 0");
      rp2040.fifo.push(3);
    }
    previous_green = button_state;
  }

  button_state = digitalRead(YELLOW_PIN);
  if (button_state != previous_yellow) {
    if (button_state == HIGH) {
      digitalWrite(LED_BUILTIN, LOW);
    } else {
      digitalWrite(LED_BUILTIN, HIGH);
      DEBUG_PRINTLN("yellow click, core 0");
      rp2040.fifo.push(4);
    }
    previous_yellow = button_state;
  }

  button_state = digitalRead(RED_PIN);
  if (button_state != previous_red) {
    if (button_state == LOW) {
      DEBUG_PRINTLN("red click, core 0");
      // rp2040.fifo.push(5);
      rp2040.reboot();
    }
    previous_red = button_state;
  }

  delay(50);  // debounce
}

void handleChangeA() {
  button_state = digitalRead(RENC_A);
  if (button_state == HIGH) {
    switch (renc_rot_status) {
      case 0:
        renc_rot_status = 1;
        break;
      case 2:
        renc_rot_status = 4;
        rp2040.fifo.push(8);
        break;
      case 1:
      case 3:
      case 4:
      case 5:
      case 6:
      case 7:
      case 8:
        break;
      default:
        renc_rot_status = 0;
    }
  } else {
    switch (renc_rot_status) {
      case 1:
      case 6:
      case 7:
        renc_rot_status = 0;
        break;
      case 3:
        renc_rot_status = 5;
        break;
      case 4:
        renc_rot_status = 8;
        break;
      case 0:
      case 2:
      case 5:
      case 8:
        break;
      default:
        renc_rot_status = 0;
    }
  }
}

void handleChangeB() {
  button_state = digitalRead(RENC_B);
  if (button_state == HIGH) {
    switch (renc_rot_status) {
      case 0:
        renc_rot_status = 2;
        break;
      case 1:
        renc_rot_status = 3;
        rp2040.fifo.push(9);
        break;
      case 2:
      case 3:
      case 4:
      case 5:
      case 6:
      case 7:
      case 8:
        break;
      default:
        renc_rot_status = 0;
    }
  } else {
    switch (renc_rot_status) {
      case 2:
      case 5:
      case 8:
        renc_rot_status = 0;
        break;
      case 3:
        renc_rot_status = 7;
        break;
      case 4:
        renc_rot_status = 6;
        break;
      case 0:
      case 1:
      case 6:
      case 7:
        break;
      default:
        renc_rot_status = 0;
    }
  }
}

// -----------------------------------------------------------------------------
// Everything below should ONLY be accessed from core 1
// -----------------------------------------------------------------------------

// wifi
WiFiClientSecure wfcs;

// info for a track
struct TrackInfo {
  const char* id;
  int duration_ms;
  int progress_ms;
};

// default track info
struct TrackInfo info = { "", 0, 0 };

// info for a token
struct TokenData {
  const char* access_token;
  const char* refresh_token;
  int expires_at;  // milliseconds
};

// default credentials
struct TokenData credentials = { "", REFRESH_TOKEN, 0 };

// buffers for fifo communication from core 0
int renc_click_buffer = 0;
int rotation_buffer = 0;
int blue_click_buffer = 0;
int green_click_buffer = 0;
int yellow_click_buffer = 0;
int red_click_buffer = 0;

// for use with getCurrentTrackInfo()
String track_id = "";
String track_progress_ms = "";
String track_duration_ms = "";

void setup1() {
  delay(200);
  Serial.println("Hello! Booting up Bootleg Spotify Car Thing (core 1)");

  track_id.reserve(64);
  track_progress_ms.reserve(16);
  track_duration_ms.reserve(16);

  DEBUG_PRINTLN("core 1 all set up");
}

void loop1() {
  while (rp2040.fifo.available() <= 0) {
    delay(40);
  }

  update_fifo_bufs();

  bool got_track = false;

  if (renc_click_buffer % 2 == 1) {
    setVolume(0);
  }

  if (rotation_buffer != 0) {
    if (!got_track) {
      getCurrentTrackInfo();
      got_track = true;
    }
    update_fifo_bufs();
    // if positive, then rotated cw -> turn volume up
    // if negative, then rotated ccw -> turn volume down
  }

  if (blue_click_buffer > 0) {
    if (!got_track) {
      getCurrentTrackInfo();
      got_track = true;
    }
    update_fifo_bufs();
    int skip_back = 16000 * blue_click_buffer;
    seek(info.progress_ms - skip_back);
  }

  if (green_click_buffer > 0) {
    if (!got_track) {
      getCurrentTrackInfo();
      got_track = true;
    }
    update_fifo_bufs();
    likeTrack();
  }

  if (yellow_click_buffer > 0) {
    reconnectWifi();
    refresh();
  }

  renc_click_buffer = 0;
  rotation_buffer = 0;
  blue_click_buffer = 0;
  green_click_buffer = 0;
  yellow_click_buffer = 0;
  red_click_buffer = 0;
}

// read from inter-core fifo, update all x_click_buffer variables
void update_fifo_bufs() {
  int msg;
  while (rp2040.fifo.available() > 0) {
    msg = rp2040.fifo.pop();
    switch (msg) {
      case 1:
        renc_click_buffer += 1;
        break;
      case 2:
        blue_click_buffer += 1;
        break;
      case 3:
        green_click_buffer += 1;
        break;
      case 4:
        yellow_click_buffer += 1;
        break;
      case 5:
        red_click_buffer += 1;
        break;
      case 8:
        rotation_buffer -= 1;
        break;
      case 9:
        rotation_buffer += 1;
        break;
    }
  }
}

void reconnectWifi() {
  WiFi.disconnect();

  DEBUG_PRINT("Connecting to WiFi ");
  DEBUG_PRINTLN(SSID);

  WiFi.begin(SSID, PASSWORD);
  delay(50);

  // below does not work as expected, so ignore for now
  // while (WiFi.status() != WL_CONNECTED) {
  //   delay(500);
  //   DEBUG_PRINT(".");
  // }

  // don't use a root cert
  wfcs.setInsecure();
  delay(10);

  DEBUG_PRINT("addr: ");
  DEBUG_PRINTLN(WiFi.localIP());
}

// input: a string like "access_token":"...","refresh_token":"...","expires_in":3600}
void setCredentials(String buffer) {
  static char access_token_buf[MAX_TOKEN_LENGTH];
  static char refresh_token_buf[MAX_TOKEN_LENGTH];

  int start = buffer.indexOf("access_token");
  String access_str = buffer.substring(start + 15);
  int end = access_str.indexOf("\"");
  access_str = access_str.substring(0, end);
  access_str.toCharArray(access_token_buf, MAX_TOKEN_LENGTH);

  start = buffer.indexOf("refresh_token");
  if (start >= 0) {
    String refresh_str = buffer.substring(start + 16);
    end = refresh_str.indexOf("\"");
    refresh_str = refresh_str.substring(0, end);
    refresh_str.toCharArray(refresh_token_buf, MAX_TOKEN_LENGTH);
  } else {
    strcpy(refresh_token_buf, REFRESH_TOKEN);
  }

  int expires_start = buffer.indexOf("expires_in");
  String expires_str = buffer.substring(expires_start + 12);
  int expires_end = expires_str.indexOf("\"");
  if (expires_end < 0) {
    expires_end = expires_str.indexOf("}");
  }
  expires_str = expires_str.substring(0, expires_end);

  // expires at now + expires_in - 5 minutes
  int expires_at = millis() + (expires_str.toInt() * 1000) - 300000;

  credentials.access_token = access_token_buf;
  credentials.refresh_token = refresh_token_buf;
  credentials.expires_at = expires_at;

  return;
}

// refresh credentials (get a new access_token, and possibly also refresh_token)
void refresh() {
  DEBUG_PRINTLN("refreshing...");

  if (WiFi.status() != WL_CONNECTED) {
    DEBUG_PRINTLN("refresh() failed: Wifi not connected");
    return;
  }

  // can test POST to https://www.httpbin.org/post, but must include a body (add `http.println("key=value");` after `wfcs.println();` and update Content-Length)

  if (wfcs.connect("accounts.spotify.com", 443)) {

    wfcs.write("POST /api/token?grant_type=refresh_token&client_id=");
    wfcs.write(CLIENT_ID);
    wfcs.write("&client_secret=");
    wfcs.write(CLIENT_SECRET);
    wfcs.write("&refresh_token=");
    wfcs.write(credentials.refresh_token);
    wfcs.write(" HTTP/1.1\r\n");
    wfcs.write("Host: accounts.spotify.com\r\n");
    wfcs.write("Content-Type: application/x-www-form-urlencoded\r\n");
    wfcs.write("Content-Length: 0\r\n\r\n");

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

    setCredentials(payload);

    return;
  } else {
    DEBUG_PRINTLN("refresh() failed: couldn't connect");
  }

  return;
}

// call refresh() if credentials expired
void refresh_if_expired() {
  if (credentials.expires_at < millis()) {
    refresh();
  }
}

// get info related to the currently playing track (if any)
void getCurrentTrackInfo() {
  static char track_id_buf[MAX_TRACK_ID_LENGTH];

  DEBUG_PRINTLN("track info...");

  if (WiFi.status() != WL_CONNECTED) {
    DEBUG_PRINTLN("getCurrentTrackInfo() failed: Wifi not connected");
    info.id = "";
    return;
  }

  refresh_if_expired();

  if (wfcs.connect("api.spotify.com", 443)) {
    wfcs.println("GET /v1/me/player/currently-playing HTTP/1.1");
    wfcs.println("Host: api.spotify.com");
    wfcs.print("Authorization: Bearer ");
    wfcs.println(credentials.access_token);
    wfcs.println();

    int step_id = 0;
    int step_pm = 0;
    int step_dm = 0;
    unsigned long startMillis = millis();
    int count = 0;

    track_id = "";
    track_progress_ms = "";
    track_duration_ms = "";

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
              track_id += c;
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
              track_progress_ms += c;
              step_pm = 14;
            }
            break;
          case 14:
            if (c == '0' || c == '1' || c == '2' || c == '3' || c == '4' || c == '5' || c == '6' || c == '7' || c == '8' || c == '9') {
              track_progress_ms += c;
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
              track_duration_ms += c;
              step_dm = 14;
            }
            break;
          case 14:
            if (c == '0' || c == '1' || c == '2' || c == '3' || c == '4' || c == '5' || c == '6' || c == '7' || c == '8' || c == '9') {
              track_duration_ms += c;
            } else {
              step_dm = -1;
            }
            break;
        }
      }
    }

    wfcs.stop();

    track_id.toCharArray(track_id_buf, MAX_TRACK_ID_LENGTH);

    info.id = track_id_buf;
    info.duration_ms = track_duration_ms.toInt();
    info.progress_ms = track_progress_ms.toInt();
  } else {
    DEBUG_PRINTLN("getCurrentTrackInfo() failed: couldn't connect");
  }

  info.id = 0;
  return;
}

// go to a specific ms in the current track
void seek(int ms) {
  DEBUG_PRINT("seek ");
  DEBUG_PRINT(ms);
  DEBUG_PRINTLN("...");

  if (WiFi.status() != WL_CONNECTED) {
    DEBUG_PRINTLN("seek() failed: Wifi not connected");
    return;
  }

  refresh_if_expired();

  if (ms < 0) {
    ms = 0;
  }

  if (wfcs.connect("api.spotify.com", 443)) {
    char ms_buf[16];  // if the song is longer than this, good luck

    wfcs.write("PUT /v1/me/player/seek?position_ms=");
    wfcs.write(itoa(ms, ms_buf, 10));
    wfcs.write(" HTTP/1.1\r\n");
    wfcs.write("Host: api.spotify.com\r\n");
    wfcs.write("Authorization: Bearer ");
    wfcs.write(credentials.access_token);
    wfcs.write("\r\n");
    wfcs.write("Content-Length: 0\r\n\r\n");
    // ignore any response

    wfcs.stop();
  } else {
    DEBUG_PRINTLN("seek() failed: couldn't connect");
  }
}

// true = current track is already liked
// false = current track is not already liked
bool trackAlreadyLiked() {
  DEBUG_PRINTLN("track already liked...");

  if (info.id == "") {
    DEBUG_PRINTLN("trackAlreadyLiked() failed: no track id");
    return false;
  }

  if (WiFi.status() != WL_CONNECTED) {
    DEBUG_PRINTLN("trackAlreadyLiked() failed: Wifi not connected");
    return false;
  }

  refresh_if_expired();

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
    DEBUG_PRINTLN("trackAlreadyLiked() failed: couldn't connect (0)");
  }

  return alreadyLiked;
}

// unlike the current track
void unlikeTrack() {
  DEBUG_PRINTLN("unlike track...");

  if (info.id == "") {
    DEBUG_PRINTLN("unlikeTrack() failed: no track id");
    return;
  }

  if (WiFi.status() != WL_CONNECTED) {
    DEBUG_PRINTLN("unlikeTrack() failed: Wifi not connected");
    return;
  }

  refresh_if_expired();

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
    DEBUG_PRINTLN("unlikeTrack() failed: couldn't connect (1)");
  }
}

// like the current track
void likeTrack() {
  DEBUG_PRINTLN("like track...");

  if (info.id == "") {
    DEBUG_PRINTLN("likeTrack() failed: no track id");
    return;
  }

  if (WiFi.status() != WL_CONNECTED) {
    DEBUG_PRINTLN("likeTrack() failed: Wifi not connected");
    return;
  }

  refresh_if_expired();

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
    DEBUG_PRINTLN("likeTrack() failed: couldn't connect (2)");
  }
}

// set playback volume
void setVolume(int vp) {
  DEBUG_PRINT("set volume ");
  DEBUG_PRINT(vp);
  DEBUG_PRINTLN("...");

  if (vp < 0) {
    vp = 0;
  } else if (vp > 100) {
    vp = 100;
  }

  if (WiFi.status() != WL_CONNECTED) {
    DEBUG_PRINTLN("setVolume() failed: Wifi not connected");
    return;
  }

  refresh_if_expired();

  // :(
  /*
  {
    "error": {
        "status": 403,
        "message": "Player command failed: Cannot control device volume",
        "reason": "VOLUME_CONTROL_DISALLOW"
    }
  }
  */

  if (wfcs.connect("api.spotify.com", 443)) {
    wfcs.println(String("PUT /v1/me/player/volume?volume_percent=") + vp + " HTTP/1.1");
    wfcs.println("Host: api.spotify.com");
    wfcs.print("Authorization: Bearer ");
    wfcs.println(credentials.access_token);
    wfcs.println("Content-Length: 0");
    wfcs.println();
    // ignore response
    wfcs.stop();
  } else {
    DEBUG_PRINTLN("setVolume() failed: couldn't connect (2)");
  }
}

// can get current volume from playback state
// https://api.spotify.com/v1/me/player
