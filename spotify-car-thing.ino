#define RED_PIN 5

// detect state change of button values by comparing to previous
int previous_red = LOW;

// placeholder variable for button state
int button_state = 0;

void setup(void) {
  Serial.begin(115200);
  delay(1000);

  // hello!
  Serial.println();
  Serial.println(F("Hello! Booting up Bootleg Spotify Car Thing"));

  // set input button pins
  pinMode(RED_PIN, INPUT_PULLUP);

  Serial.println("all set up, nothing more");
}

void loop() {
  button_state = digitalRead(RED_PIN);
  if (button_state != previous_red) {
    if (button_state == HIGH) {
      digitalWrite(LED_BUILTIN, LOW);
    } else {
      // button is pressed
      digitalWrite(LED_BUILTIN, HIGH);
      Serial.println("red click");
    }
    previous_red = button_state;
  }

  delay(50);  // debounce
}
