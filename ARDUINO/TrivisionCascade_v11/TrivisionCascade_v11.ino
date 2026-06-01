/*
 * TrivisionCascade_v11 — 4-Motor Pin Diagnostic
 * 
 * Tests each motor individually: 200 steps CW, pause, 200 steps CCW.
 * Announces each motor over Serial so you can see which ones respond.
 * Simple blocking code — no overlap, just raw diagnosis.
 */

#define NUM_MOTORS 4

const uint8_t stepPin[NUM_MOTORS] = {35, 36, 37, 38};
const uint8_t dirPin[NUM_MOTORS]  = {18, 19, 20, 21};

void setup() {
  Serial.begin(115200);
  while (!Serial) { ; }
  
  for (int i = 0; i < NUM_MOTORS; i++) {
    pinMode(stepPin[i], OUTPUT);
    pinMode(dirPin[i], OUTPUT);
    digitalWrite(stepPin[i], LOW);
    digitalWrite(dirPin[i], LOW);
  }
  
  Serial.println(F("=== v11 — 4-Motor Pin Diagnostic ==="));
  Serial.println(F("Each motor: 200 steps CW, pause, 200 steps CCW"));
  Serial.println();
  delay(2000);
}

void loop() {
  for (int m = 0; m < NUM_MOTORS; m++) {
    Serial.print(F("Motor "));
    Serial.print(m + 1);
    Serial.print(F(" (STEP="));
    Serial.print(stepPin[m]);
    Serial.print(F(", DIR="));
    Serial.print(dirPin[m]);
    Serial.println(F(") — 200 steps CW..."));
    
    digitalWrite(dirPin[m], HIGH);
    delayMicroseconds(5);
    for (int s = 0; s < 200; s++) {
      digitalWrite(stepPin[m], HIGH);
      delayMicroseconds(2);
      digitalWrite(stepPin[m], LOW);
      delayMicroseconds(1500);
    }
    
    delay(1000);
    
    Serial.print(F("Motor "));
    Serial.print(m + 1);
    Serial.println(F(" — 200 steps CCW..."));
    
    digitalWrite(dirPin[m], LOW);
    delayMicroseconds(5);
    for (int s = 0; s < 200; s++) {
      digitalWrite(stepPin[m], HIGH);
      delayMicroseconds(2);
      digitalWrite(stepPin[m], LOW);
      delayMicroseconds(1500);
    }
    
    Serial.print(F("Motor "));
    Serial.print(m + 1);
    Serial.println(F(" — DONE"));
    Serial.println();
    delay(2000);
  }
  
  Serial.println(F("=== All 4 tested. Restarting in 5 sec... ==="));
  Serial.println();
  delay(5000);
}
