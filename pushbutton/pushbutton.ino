const int BTN = 13;

void setup() {
  Serial.begin(9600);

  while (!Serial);

  pinMode(LEDR, OUTPUT);
  pinMode(LEDG, OUTPUT);
  pinMode(LEDB, OUTPUT);
}

void loop() {
  pinMode(BTN, OUTPUT);
  digitalWrite(BTN, HIGH);
  pinMode(BTN, INPUT);

  if (digitalRead(BTN) == !HIGH) {
    digitalWrite(LEDR, !HIGH);
    digitalWrite(LEDG, !HIGH);
    digitalWrite(LEDB, !HIGH);    
  } else {
    digitalWrite(LEDR, !LOW);
    digitalWrite(LEDG, !LOW);
    digitalWrite(LEDB, !LOW);
  }

  delay(10);
}
