

// code includes birthday messages and also automatic daylight saving calculations.
#include <RTClib.h>
#include <FastLED.h>
#include <DS3232RTC.h>      // https://github.com/JChristensen/DS3232RTC
#include <SoftwareSerial.h>

FASTLED_USING_NAMESPACE

#define LED_DATA_PIN        6
#define LED_TYPE        WS2811
#define COLOR_ORDER     GRB
#define NUM_LEDS        121
#define PHOTO_RESISTOR  A0
#define BUTTON_PIN      2
#define SET_TIME       "settime"
#define ADD_BDAY       "addbday"
#define REMOVE_BDAY    "removebday"
#define LIST_BDAY      "listbday"
#define BLE_BUFFER_LENGTH 100

RTC_DS3231 RTC;

CRGB leds[NUM_LEDS];
SoftwareSerial BT(8, 9); //  pin D8 to RXD p-in D9 purple to TXD

//Global Variables
uint8_t animate_speed = 100;
uint8_t Hour_DST;
uint8_t colour_hue = 35;
uint8_t gHue = 0; // rotating "base color" used by many of the patterns

int timeout = 1500;          // Wait 800ms each time for BLE to response, depending on your application, adjust this value accordingly

char buffer[BLE_BUFFER_LENGTH];       // Buffer to store response

const byte numChars = 35;
char receivedData[numChars];   // an array to store the received data

bool changingTime = false;
bool addingBday = false;
bool removingBday = false;
bool listingBday = false;
bool newData = false;

long previousMillis = 0;
long interval = 500; //how long in milli seconds to update clock


// arrays for the LEDs
enum wordLEDs {
  ITS,
  FIVE,
  TEN,
  QUARTER,
  TWENTY,
  TWENTY_FIVE,
  HALF,
  PAST,
  TO,
  OCLOCK,
  HAPPY,
  BIRTHDAY
};

uint8_t wordArray[][11] = {
  {119,118,116,115},
  {83,84,85,86},
  {91,90,89},
  {102,103,104,105,106,107,108},
  {97,96,95,94,93,92},
  {83,84,85,86,92,93,94,95,96,97},
  {78,79,80,81},
  {70,69,68,67},
  {56,57},
  {5,4,3,2,1,0},
  {55,76,77,98,99},
  {43,44,65,66,87,88,109,110}
};

uint8_t wordLengths[] = {4,4,3,7,6,10,4,4,2,6,5,8}; // this is the size of each of the above arrays for indexing the hourarray later in script                       

uint8_t hourArray[][7] = { 
  {59,60,61,62,63,64}, // this outlnes which LEDs to light up, 0-24 arrays, 0 being twleve 1=one 2=two... 24=twelve
  {36,37,38},
  {34,35,36},
  {31,30,29,28,27},
  {13,14,15,16},
  {54,53,52,51},
  {9,8,7},
  {17,18,19,20,21},
  {51,50,49,48,47},
  {39,40,41,42},
  {47,46,45},
  {27,26,25,24,23,22},
  {59,60,61,62,63,64},
  {36,37,38},
  {34,35,36},
  {31,30,29,28,27},
  {13,14,15,16},
  {54,53,52,51},
  {9,8,7},
  {17,18,19,20,21},
  {51,50,49,48,47},
  {39,40,41,42},
  {47,46,45},
  {27,26,25,24,23,22},
  {59,60,61,62,63,64}
};
                        
uint8_t hourLengths[] = {6,3,3,5,4,4,3,5,5,4,3,6,6,3,3,5,4,4,3,5,5,4,3,6,6}; // this is the size of each of the above arrays for indexing the hourarray later in script                       


// settings to implement on startup

void setup() { 
  //Pin Settings
  pinMode(PHOTO_RESISTOR, INPUT);// Set photo resistor
  pinMode(BUTTON_PIN, INPUT); //set a button - for changing colour
    
  //LED settings
  FastLED.addLeds<LED_TYPE,LED_DATA_PIN,COLOR_ORDER>(leds, NUM_LEDS).setCorrection(TypicalLEDStrip);
    
  //RTC Clock settings
  if (! RTC.begin()) {
    Serial.println("Couldn't find RTC");
    while(1) {};
  }
  if (RTC.lostPower()) {
    Serial.println("RTC lost power, lets set the time!");
    // following line sets the RTC to the date & time this sketch was compiled
    RTC.adjust(DateTime(F(__DATE__), F(__TIME__)));
    // This line sets the RTC with an explicit date & time, for example to set
    // January 21, 2014 at 3am you would call:
    // rtc.adjust(DateTime(2014, 1, 21, 3, 0, 0));
  }

  setSyncProvider(getExternalTime());   // the function to get the time from the RTC

  //****settings to edit the time on the real time clock ******
  //setTime(12,59, 55, 05, 01, 2020);   //this sets the system time set to GMT without the daylight saving added. 
  //RTC.set(now());   //sets time onto the RTC. Make sure to comment this out and then re-upload after setting the RTC. -- (setTime() and now() are Part of the Time Library)
  //**** Un-comment above 2 lines to set the time and load onto chip then comment out the lines and re-load onto the chip********
  long baudrate = BLEAutoBaud();

  if (baudrate > 0) {
    Serial.print("Found BLE baud rate ");
    Serial.println(baudrate);
  } else {
    Serial.println("No BLE detected.");
    while (1) {};         // No BLE found, just going to stop here
  }

  //BT.begin(baudrate);
  BT.println("Connected to WordClock");
  BLECmd(timeout,"AT+NAMEWordClock",buffer); // Set the name of the module to HM10

}

long BLEAutoBaud() {
  long bauds[] = {9600, 57600, 115200, 38400, 2400, 4800, 19200}; // common baud rates, when using HM-10 module with SoftwareSerial, try not to go over 57600
  int baudcount = sizeof(bauds) / sizeof(long);
  for (int i = 0; i < baudcount; i++) {
    for (int x = 0; x < 3; x++) { // test at least 3 times for each baud
      
      Serial.print("Testing baud ");
      Serial.println(bauds[i]);
      BT.begin(bauds[i]);
      if (BLEIsReady()) {
        return bauds[i];
      }
    }
  }
  return -1;
}

bool BLEIsReady() {
  BLECmd(timeout, "AT" , buffer);   // Send AT and store response to buffer
  if (strcmp(buffer, "OK") == 0) {
    return true;
  } else {
    return false;
  }
}

bool BLECmd(long timeout, char* command, char* temp) {
  long endtime;
  bool found = false;
  endtime = millis() + timeout; //
  memset(temp, 0, 100);       // clear buffer
  found = true;
  Serial.print("Arduino send = ");
  Serial.println(command);
  BT.print(command);

  // The loop below wait till either a response is received or timeout
  // The problem with this BLE Shield is most of the HM-10 modules do not response with CR LF at the end of the response,
  // so a timeout is required to detect end of response and also prevent the loop locking up.

  while (!BT.available()) {
    if (millis() > endtime) {   // timeout, break
      found = false;
      break;
    }
  }

  if (found) {            // response is available
    int i = 0;
    while (BT.available()) {   // loop and read the data
      char a = BT.read();
      // Serial.print((char)a); // Uncomment this to see raw data from BLE
      temp[i] = a;        // save data to buffer
      i++;
      if (i >= BLE_BUFFER_LENGTH) break; // prevent buffer overflow, need to break
      delay(1);           // give it a 2ms delay before reading next character
    }
    Serial.print("BLE reply    = ");
    Serial.println(temp);
    while ((strlen(temp) > 0) && ((temp[strlen(temp) - 1] == 10) || (temp[strlen(temp) - 1] == 13)))
    {
      temp[strlen(temp) - 1] = 0;
    }
    return true;
  } else {
    Serial.println("BLE timeout!");
    return false;
  }
}

void loop() {
  //bluetooth settings
  bluetoothGetInput();
  bluetoothCheckInput();
  bluetoothChangeTime();
  
  // run the Clock face LEDs every 500ms
  unsigned long currentMillis = millis();
    
  if(currentMillis - previousMillis > interval) {
    // save the last time you blinked the LED 
    previousMillis = currentMillis;
       
    Clockset();
  }
  
  hourAnimation(); //hour animataion to run at full speed
  brightnessSet();  // changing the brightness
    
  if (digitalRead(BUTTON_PIN) == HIGH) {
     colour_hue++;
     delay(100);
  }  
}


void Clockset(){
  // ********************* Calculate offset for Daylight saving hours (UK) *********************
  int y = year();                          // year in 4 digit format
  uint8_t Mar_x = 31 - (4 + 5*y/4) % 7;      // will find the day of the last sunday in march
  uint8_t Oct_x = 31 - (1 + 5*y/4) % 7;       // will find the day of the last sunday in Oct
  uint8_t DST;
                                             
  // *********** Test DST: BEGINS on last Sunday of March @ 2:00 AM *********
  if(month() == 3 && day() == Mar_x && hour() >= 2) {                                   
      DST = 1;                           // Daylight Savings Time is TRUE (add one hour)
     }
     
  if((month() == 3 && day() > Mar_x) || month() > 3) {
      DST = 1;
     }
     
  // ************* Test DST: ENDS on Last Sunday of Oct @ 2:00 AM ************  
  if(month() == 10 && day() == Oct_x && hour() >= 2) {
      DST = 0;                            // daylight savings time is FALSE (Standard time)
     }
     
  if((month() == 10 && day() > Oct_x) || month() > 10 || month() < 3 || month() == 3 && day() < Mar_x || month() == 3 && day() == Mar_x && hour() < 2) {
      DST = 0;
     }
  
  Hour_DST = hour()+DST; //Add the DST to the hour to get correct DST
  
  if(hour() == 23 && DST == 1) {
    Hour_DST = 00;
  }
  
  // ********************************************************************************
  // the first 8 seconds of the hour is a special animation if its past this time set time as normal
  
  if (minute() ==00  && second() > 8 || minute() >00)  {     
    FastLED.clear (); // reset the LEDs prevents old times staying lit up 
    
    lightWordLEDs(ITS); // light up Its LEDs
    
    if (minute() >= 5 && minute() < 10 || minute() >= 55 && minute() < 60) {
      lightWordLEDs(FIVE);    // if functions to set the time minutes
    }
    
    if (minute() >= 10 && minute() < 15 || minute() >= 50 && minute() < 55) { 
      lightWordLEDs(TEN);
    }
    
    if (minute() >= 15 && minute() < 20 || minute() >= 45 && minute() < 50 ) { 
      lightWordLEDs(QUARTER);
    }
    
    if (minute() >= 20 && minute() < 25 || minute() >= 40 && minute() < 45) {
      lightWordLEDs(TWENTY);
    }
    
    if (minute() >= 25 && minute() < 30 || minute() >= 35 && minute() < 40) {
      lightWordLEDs(TWENTY_FIVE);
    }
    
    if (minute() >= 30 && minute() < 35){
      lightWordLEDs(HALF);
    }
    
    if (minute() >= 5 && minute() < 35) { // this sets the 'past' light so it only lights up for first 30 mins
      lightWordLEDs(PAST);
      lightHourLEDs(Hour_DST);
    }
    
    if (minute() >= 35 && minute() < 60 && hour() <= 23) {
      lightWordLEDs(TO);
      lightHourLEDs(Hour_DST+1);
    }
    
    if (minute() >= 0 && minute() < 5) { // sets the 'oclock' light if the time is on the hour
      lightHourLEDs(Hour_DST);
      lightWordLEDs(OCLOCK);
    }
    
    
    //*************** light up the Happy birthday on birthdays ******************
    
    if(month() == 1 && day() ==6 || month() == 2 && day() ==14){
      lightBirthdayLEDs(HAPPY);
      lightBirthdayLEDs(BIRTHDAY);
      FastLED.show();
    }
    
    FastLED.show();
      
  }
  
  // **************animations for quarter to qurter past and half past the hour *******************
  if(minute() == 45 && second() ==00){
    FastLED.clear ();
    animateWordLEDs(ITS);
    delay(500);
    animateWordLEDs(QUARTER);
    animateWordLEDs(TO);
    delay(500);
    animateHourLEDs(Hour_DST+1);
    FastLED.show();
  }
  
  if(minute() == 15 && second() ==00){
    FastLED.clear ();
    animateWordLEDs(ITS);
    delay(500);
    animateWordLEDs(QUARTER);
    animateWordLEDs(PAST);
    delay(500);
    animateHourLEDs(Hour_DST);
    FastLED.show();
  }
  
  if(minute() == 30 && second() ==00){
    FastLED.clear ();
    animateWordLEDs(ITS);
    delay(500);
    animateWordLEDs(HALF);
    animateWordLEDs(PAST);
    delay(500);
    animateHourLEDs(Hour_DST);
    FastLED.show();
  }
}   //end of clock set



//******* functions to turn LEDs on ******************

void lightWordLEDs(enum wordLEDs word) {
  for (uint8_t i = 0; i < wordLengths[word]; i++) {
    leds[wordArray[word][i]] = CRGB::Black; 
    leds[wordArray[word][i]] += CHSV(colour_hue, 250, 250);
  }
}

void lightHourLEDs(uint8_t Hour) {
  for(uint8_t i = 0; i < hourLengths[Hour]; i++) {
    leds[hourArray[Hour][i]] = CRGB::Black; 
    leds[hourArray[Hour][i]] += CHSV( colour_hue, 250, 250);
  }
}

void lightBirthdayLEDs(enum wordLEDs word) {
  for (uint8_t i = 0; i < wordLengths[word]; i++) {
    leds[wordArray[word][i]] += CHSV( gHue+(i*10), 250, 200);
    EVERY_N_MILLISECONDS( 10 ) { gHue++; } //change the colour of the animation
  }
}

void animateWordLEDs(enum wordLEDs word) {
  for (uint8_t i = 0; i < wordLengths[word]; i++) { 
    leds[wordArray[word][i]] += CHSV(colour_hue, 250, 250);
    FastLED.show();
    delay(animate_speed);
  }
}

void animateHourLEDs(uint8_t Hour) { 
  for(uint8_t i = 0; i < hourLengths[Hour]; i++) {
    leds[hourArray[Hour][i]] += CHSV( colour_hue, 250, 250);
    FastLED.show();
    delay(animate_speed);
  }
}

void Animation(){ /// animation to run on the hour
  // random colored speckles that blink in and fade smoothly 
  fadeToBlackBy( leds, NUM_LEDS, 15);
  uint8_t pos = random8(NUM_LEDS);
  leds[pos] += CHSV( gHue + random8(64), 200, 250);
  EVERY_N_MILLISECONDS( 20) { gHue++; } //change the colour of the animation
  delay(10);
}

void hourAnimation() {
//animation for on the hour
  if(minute() == 00 && second() >= 00 && second() <=8){
    Animation();
  }
  
  if(minute() == 00 && second() == 01){    // do a 10sec animation when it hits the hour
    lightWordLEDs(ITS); 
    FastLED.show();
  }
  
  if(minute() == 00 && second() >1 && second() <=2){   
    lightWordLEDs(ITS);
    lightHourLEDs(Hour_DST);
    FastLED.show();
  }
  
  if(minute() == 00 && second() >2 && second() <=8){    // do a 8sec animation when it hits the hour
    lightWordLEDs(ITS);
    lightHourLEDs(Hour_DST);
    lightWordLEDs(OCLOCK);
    FastLED.show();
  }
}



//******* Brightness settings to work with the light dependent resistor ******************

void brightnessSet () {
  int ambient_Light = analogRead(PHOTO_RESISTOR);    // read the value of the photoresisor
  int Brightness_Set = map(ambient_Light, 0, 500, 10, 200); // map the ambiant light value to a LED brightness value
  
  if (ambient_Light >= 500){ 
    FastLED.setBrightness(200);
  }
  if (ambient_Light < 500){
    FastLED.setBrightness(Brightness_Set);  // set the brightness of the LEDs
  }
}



// ******************* bluetooth implementation to change date and time ********************************

void bluetoothGetInput() { //take the message set by bluetooth and then add all the characters together in a char array
  static byte ndx = 0;
  char endMarker = '\n';
  char rc;
  
  while (BT.available() > 0 && newData == false) {    //create a char array untill you get a /n signal from bluetooth
    rc = BT.read();
  
    if (rc != endMarker) {
      receivedData[ndx] = rc;
      ndx++;
      if (ndx >= numChars) {
        ndx = numChars - 1;
      }
    }
    else {
      receivedData[ndx] = '\0'; // terminate the string
      ndx = 0;
      newData = true;
    }
  }
}

void bluetoothCheckInput() { //If the message sent is the same as the trigger word "settime" then ask for user to enter date and time
  if (newData == true && (strcasecmp(SET_TIME,receivedData) == 0)) {
    BT.println("Set the Time & Date as: hh,mm,ss,dd,mm,yyyy");
    newData = false;
    changingTime = true; // set a switch to true that time is going to be changed
  }

  BT.println(String("'") + String(receivedData) + String("'"));
  
  if (newData == true && (strcasecmp(ADD_BDAY,receivedData) == 0)){
    newData = false;
    addingBday = true; // set a switch to true that time is going to be changed
  }
  
  if (newData == true && (strcasecmp(REMOVE_BDAY,receivedData) == 0)){
    newData = false;
    removingBday = true; // set a switch to true that time is going to be changed
  }

  if (newData == true && (strcasecmp(LIST_BDAY,receivedData) == 0)){
    newData = false;
    listingBday = true; // set a switch to true that time is going to be changed
  }
  
  if (newData == true && 
      strcasecmp(SET_TIME,receivedData) != 0 && changingTime == false &&
      strcasecmp(ADD_BDAY,receivedData) != 0 && addingBday == false &&
      strcasecmp(REMOVE_BDAY,receivedData) != 0 && removingBday == false &&
      strcasecmp(LIST_BDAY,receivedData) != 0 && listingBday == false) {
    newData = false;
    String Cmd = (String)"Command not recognised ("+ receivedData + ")"; // if the user input isnt same as trigger word then inform user command not recognised
    BT.println(String("Command not recognised: ") + String(receivedData));
    BT.println(String("Avaliable commands: "));
    BT.println(String(SET_TIME));
    BT.println(String(ADD_BDAY));
    BT.println(String(REMOVE_BDAY));
    BT.println(String(LIST_BDAY));
  }
}

 void bluetoothChangeTime(){  
  if (newData == true && changingTime == true){ // if a new message has been recieved and the Change time switch is active change the time
    char *strings[10];    //following code parses out the date based on being delimited by commas fullstops etc. this gives 
    char *ptr = NULL;
    byte index = 0;
    
    ptr = strtok(receivedData, " :/,.");  // takes a list of delimiters 
    
    while(ptr != NULL){
        strings[index] = ptr;
        index++;
        ptr = strtok(NULL, " :/,.");  // takes a list of delimiters
    }
  
    long hr = atol(strings[0]); // take the parsed date from array which corresponds to hour minute seconds ect. 
    long mm = atol(strings[1]);
    long ss = atol(strings[2]);
    long dd = atol(strings[3]);
    long mth = atol(strings[4]);
    long yyyy = atol(strings[5]);
  
    setTime(hr,mm,ss,dd,mth,yyyy);   //this sets the system time set to GMT without the daylight saving added. 
    RTC.adjust(now());
    
    String Dateset =  (String)dd+"/"+mth+"/"+yyyy;  //create a string to update user interface to bluetooth 
    String Timeset = (String)hr+":"+mm+":"+ss;
    
    BT.println("Time set as: " + Timeset);
    BT.println("Date set as: " + Dateset);
        
    newData = false;
    changingTime = false;
  }
}

// TO ADD setting the bday and saving it to the eprom memory
    
