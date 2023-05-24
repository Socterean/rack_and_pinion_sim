#include <SPI.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#include <Stepper.h>

#include <EEPROM.h>

// Display properties ------
#define SCREEN_WIDTH 128 // OLED display width, in pixels
#define SCREEN_HEIGHT 64 // OLED display height, in pixels
// -------------------------

// Declaration for an SSD1306 display connected to I2C (SDA, SCL pins)
#define OLED_RESET     -1 // Reset pin # (or -1 if sharing Arduino reset pin)
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);
// -------------------------

// Stepper motor properties
const int motorPin = 4; 
const int motorSteps = 200;
const int motorSpeed = 200; // motor speed in RPM
Stepper myStepper(motorSteps, 8, 9, 10, 11); // stepper allocation using 8, 9, 10, 11 as pins for motor control
int targetSteps = 0;
// ------------------------

const float pinionDiameter = 10;// in mm
const float pinionCircumference = PI * pinionDiameter;
const float stepTravel = pinionCircumference / motorSteps;

// Keyboard properties-----
uint8_t keyboardPin = A0;
uint8_t newKey = 0; // flag to check if a new key was pressed
char key = ' '; // variable to store the pressed key

char keyboardValues[4][4] = {
                              {'1', '2', '3', 'A'}, 
							                {'4', '5', '6', 'B'},
  							              {'7', '8', '9', 'C'},
  							              {'*', '0', '#', 'D'}
                             };//array with each key of the keypad

int keyboardReadings[4][4] = {
                              {1022, 511, 340, 255}, 
							                {179, 152, 132, 117},
  							              {98,  89,  82,  76},
  							              {67,  63,  59,  56}
                             };//array with each analog reading between 0-1023 for each key from the keypad
// ------------------------


const String initialErrorText = "                     ";
String errorText = initialErrorText;

// Pinion properties ------
const float pinionTravel = 500; // total shaft travel in mm
const float minPinionTravel = -(pinionTravel/2);
char pinionDirection = '>';
int pinionTarget = 0;
int pinionVelocity = 0;
float pinionPosition = 0;
// ------------------------

// EEPROM properties -----
int eeAdress = 0;
float savedValue = 0.0;
float lastSave = millis();
float saveTime = 100; // the default EEPROM save time is 3.3 ms according to Arduino docs but in order to save write cycles we will save once every 100 ms 
// ----------------------

void updateDisplay(){
  display.clearDisplay();

  display.setTextSize(1);      // Normal 1:1 pixel scale
  display.setTextColor(SSD1306_WHITE); // Draw white text
  display.setCursor(0, 5);     // Start at top-left corner
  display.cp437(true);         // Use full 256 char 'Code Page 437' font

  display.print(" Direction: ");
  display.print(pinionDirection);
  display.print("\n");
  display.print(" Target: ");
  display.print(pinionTarget);
  display.print(" mm\n");
  display.print(" Position: ");
  display.print(pinionPosition);
  display.print(" mm\n");
  display.print(" Velocity: ");
  display.print(pinionVelocity);
  display.print(" RPM\n");
  display.print(errorText);
  display.print("\n");

  display.setCursor(((pinionPosition-minPinionTravel)*121)/pinionTravel, 50);
  display.print("v");
  display.setCursor(1, 59);
  display.print("|---------^---------|");

  display.display();

  display.availableForWrite();
}

void manageApp(char key){
  if(key >= '0' && key <= '9'){
    int tempValue = (abs(pinionTarget) * 10) + (key - '0');
    if(tempValue <= (pinionTravel/2)){
      pinionTarget = tempValue;
      if(pinionDirection == '<'){
        pinionTarget = -pinionTarget;
      }
    }
    else{
      errorText = " Err: Cannot   exceed travel limit";
    }
    updateDisplay();
  }

  if(key == 'A'){
    if(errorText != initialErrorText && abs(pinionTarget) <= (pinionTravel/2)){
      errorText = initialErrorText;
    }
    float newTarget = pinionTarget - pinionPosition;
    targetSteps = newTarget / stepTravel;
  }

  if(key == 'B'){
    pinionTarget /= 10;
    errorText = initialErrorText;
    updateDisplay();
  }

  if(key == '*'){
    pinionDirection = '<';
    if(pinionTarget >= 0){
      pinionTarget = -pinionTarget;
    }
    updateDisplay();
  }

  if(key == '#'){
    pinionDirection = '>';
    if(pinionTarget <= 0){
      pinionTarget = -pinionTarget;
    }
    updateDisplay();
  }

}

int readKey(int input){
  for (int i = 0; i < 4; i++){
    for (int j = 0; j < 4; j++){
      if(keyboardReadings[i][j] == input){
        key = keyboardValues[i][j];
        newKey = 1;
      }
    }
  }
  return 0;
}

void setup(){
  Serial.begin(9600);

  // Display setup ---------
  // SSD1306_SWITCHCAPVCC = generate display voltage from 3.3V internally
  if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) { // Address 0x3D for 128x64
    Serial.println(F("SSD1306 allocation failed"));
    for(;;); // Don't proceed, loop forever
  }
  // Show initial display buffer contents on the screen --
  // the library initializes this with an Adafruit splash screen.
  display.display();
  delay(2000); // Pause for 2 seconds

  // Clear the buffer
  display.clearDisplay();
  //-----------------------

  pinMode(keyboardPin, INPUT);
  pinMode(motorPin, OUTPUT);

  digitalWrite(motorPin, HIGH);
  myStepper.setSpeed(motorSpeed);

  EEPROM.get(eeAdress, savedValue);
  if(savedValue != 0){
    pinionPosition = savedValue;
  }

  updateDisplay();
}

void loop(){

  int input = 0;
  if(input = analogRead(keyboardPin)){
    readKey(input);
    if(newKey){
      manageApp(key);
    }
  }

  if(targetSteps != 0){
    if(millis() - lastSave >= saveTime){
      if(EEPROM.get(eeAdress, savedValue) != pinionPosition){
        EEPROM.put(eeAdress, pinionPosition);
      }
    }

    pinionVelocity = motorSpeed;
    if(targetSteps < 0){
      myStepper.step(1);
      pinionPosition -= stepTravel;
      targetSteps++;
    }
    else{
      myStepper.step(-1);
      pinionPosition += stepTravel;
      targetSteps--; 
    }
    updateDisplay();
  }

  if(targetSteps == 0 && pinionVelocity == motorSpeed){
    pinionVelocity = 0;
  }
}