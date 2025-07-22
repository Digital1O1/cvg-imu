// Pin definitions
const int ADC_PIN = A0;        // Potentiometer wiper (analog input)
const int VOLTAGE_PIN = 13;    // Voltage supply for potentiometer
const int GROUND_PIN = 11;     // Ground for potentiometer
const int SIGNAL_PIN = 9;      // Digital input signal

// Threshold and state variables
const int ADC_THRESHOLD = 630; // ADC threshold value
int adcValue;
int counter = 0;
int previousAdcValue = 0;
bool thresholdCrossed = false;
bool pin9High = false;

// Timing variables
unsigned long thresholdTime = 0;
unsigned long pin9Time = 0;
unsigned long timeDifference = 0;

void setup() {
  // Initialize serial communication
  Serial.begin(9600);
  while (!Serial) {
    ; // Wait for serial port to connect (needed for native USB)
  }
  
  // Configure pins
  pinMode(VOLTAGE_PIN, OUTPUT);
  pinMode(GROUND_PIN, OUTPUT);
  pinMode(ADC_PIN, INPUT);
  pinMode(SIGNAL_PIN, INPUT);
  
  // Set up power supply for potentiometer
  digitalWrite(VOLTAGE_PIN, HIGH);  // Provide 5V (or 3.3V depending on board)
  digitalWrite(GROUND_PIN, LOW);    // Provide ground reference
  
  // Allow time for voltage to stabilize
  delay(100);
}

void loop() {
  // Read ADC value
  adcValue = analogRead(ADC_PIN);
  
  // Check for threshold crossing (from below 631 to at or above 631)
  if (previousAdcValue < ADC_THRESHOLD && adcValue >= ADC_THRESHOLD && !thresholdCrossed) {
    thresholdTime = micros();  // Record timestamp in microseconds
    thresholdCrossed = true;
  }
  
  // Check for pin 9 HIGH
  if (digitalRead(SIGNAL_PIN) == HIGH && !pin9High && thresholdCrossed) {
    pin9Time = micros();  // Record timestamp in microseconds
    pin9High = true;
    
    // Calculate and output time difference
    timeDifference = pin9Time - thresholdTime;
    if(counter < 150){
    Serial.println(timeDifference);
    counter++;
    }
    if(counter == 150){
      Serial.println("DONE !!");
    }    
    // Reset for next measurement
    thresholdCrossed = false;
    pin9High = false;
  }
  
  // Reset if ADC goes back below threshold
  if (adcValue < ADC_THRESHOLD) {
    thresholdCrossed = false;
    pin9High = false;
  }
  
  // Update previous ADC value
  previousAdcValue = adcValue;
  
  // Optional: Print current ADC value for monitoring
  // Uncomment the next 4 lines if you want to see continuous ADC readings
  // Serial.print("ADC: ");
  // Serial.println(adcValue);
  // Serial.print(" | Pin 9: ");
  // Serial.println(digitalRead(SIGNAL_PIN));
  
  // Small delay to avoid overwhelming the serial output
  delay(1);
}