

// code includes birthday messages and also automatic daylight saving calculations.
#include <RTClib.h>
#include <FastLED.h>
#include <DS3232RTC.h>      // https://github.com/JChristensen/DS3232RTC
#include <SoftwareSerial.h>
#include <Timezone.h>    // https://github.com/JChristensen/Timezone

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
#define NUM_CHARS         35

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

char receivedData[NUM_CHARS];   // an array to store the received data

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
  HALF,
  PAST,
  TO,
  OCLOCK,
  HAPPY,
  BIRTHDAY
};

uint8_t wordArray[][11] = {
  {111,112,114,115}, // it is
  {81,80,79,78},     // five
  {95,96,97},        // ten
  {100,101,102,103,104,105,106}, // quarter
  {89,90,91,92,93,94}, // twenty
  {83,84,85,86},    // half
  {72,73,74,75},    // past
  {63,64},          // to
  {5,6,7,8,9,10},   // oclock
  {55,76,77,98,99}, // happy
  {43,44,65,66,87,88,109,110} // birthday
};

uint8_t wordLengths[] = {4,4,3,7,6,10,4,4,2,6,5,8}; // this is the size of each of the above arrays for indexing the hourarray later in script                       

// this outlnes which LEDs to light up, 0-24 arrays, 0 being twleve 1=one 2=two... 24=twelve
uint8_t hourArray[][7] = { 
  {61,60,59,58,57,56}, // 12
  {40,39,38},       // 1
  {40,41,42},       // 2
  {23,24,25,26,27}, // 3
  {16,17,18,19},    // 4
  {44,45,46,47},    // 5
  {1,2,3},          // 6
  {11,12,13,14,15}, // 7
  {51,50,49,48,47}, // 8
  {37,36,35,34},    // 9
  {51,52,53},       // 10
  {27,28,29,30,31,32}, // 11
  {61,60,59,58,57,56}, // 12
  {40,39,38},       // 1
  {40,41,42},       // 2
  {23,24,25,26,27}, // 3
  {16,17,18,19},    // 4
  {44,45,46,47},    // 5
  {1,2,3},          // 6
  {11,12,13,14,15}, // 7
  {51,50,49,48,47}, // 8
  {37,36,35,34},    // 9
  {51,52,53},       // 10
  {27,28,29,30,31,32}, // 11
  {61,60,59,58,57,56} // 12
};
                    //  12 1 2 3 4 5 6 7 8 9 0 11
uint8_t hourLengths[] = {6,3,3,5,4,4,3,5,5,4,3,6,
                         6,3,3,5,4,4,3,5,5,4,3,6,6}; // this is the size of each of the above arrays for indexing the hourarray later in script
                    
TimeChangeRule usCDT = {"CDT", Second, Sun, Mar, 2, -300};
TimeChangeRule usCST = {"CST", First, Sun, Nov, 2, -360};
Timezone usCT(usCDT, usCST);

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


  setSyncProvider(getExternalTime());   // the function to get the time from the RTC

  //****settings to edit the time on the real time clock ******
  //setTime(12,59, 55, 05, 01, 2020);   //this sets the system time set to GMT without the daylight saving added. 
  //RTC.set(now());   //sets time onto the RTC. Make sure to comment this out and then re-upload after setting the RTC. -- (setTime() and now() are Part of the Time Library)
  //**** Un-comment above 2 lines to set the time and load onto chip then comment out the lines and re-load onto the chip********

  BT.begin(9600);
  BT.println("Connected to WordClock");
  BLECmd(timeout,"AT+NAMEWordClock",buffer); // Set the name of the module to HM10

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
  DateTime now = usCT.toLocal(RTC.now().unixtime());
  int y = now.year();                          // year in 4 digit format
    
  // ********************************************************************************
  // the first 8 seconds of the hour is a special animation if its past this time set time as normal
  
  FastLED.clear (true); // reset the LEDs prevents old times staying lit up 
  if (now.minute() == 00  && now.second() > 8 || now.minute() > 00)  {     
    
    lightWordLEDs(ITS); // light up Its LEDs
    
    if (now.minute() >= 5 && now.minute() < 10 || now.minute() >= 55 && now.minute() < 60) {
      lightWordLEDs(FIVE);    // if functions to set the time minutes
    }
    
    if (now.minute() >= 10 && now.minute() < 15 || now.minute() >= 50 && now.minute() < 55) { 
      lightWordLEDs(TEN);
    }
    
    if (now.minute() >= 15 && now.minute() < 20 || now.minute() >= 45 && now.minute() < 50 ) { 
      lightWordLEDs(QUARTER);
    }
    
    if (now.minute() >= 20 && now.minute() < 25 || now.minute() >= 40 && now.minute() < 45) {
      lightWordLEDs(TWENTY);
    }
    
    if (now.minute() >= 25 && now.minute() < 30 || now.minute() >= 35 && now.minute() < 40) {
      lightWordLEDs(TWENTY);
      lightWordLEDs(FIVE);
    }
    
    if (now.minute() >= 30 && now.minute() < 35){
      lightWordLEDs(HALF);
    }
    
    if (now.minute() >= 5 && now.minute() < 35) { // this sets the 'past' light so it only lights up for first 30 mins
      lightWordLEDs(PAST);
      lightHourLEDs(now.hour());
    }
    
    if (now.minute() >= 35 && now.minute() < 60 && now.hour() <= 23) {
      lightWordLEDs(TO);
      lightHourLEDs(now.hour()+1);
    }
    
    if (now.minute() >= 0 && now.minute() < 5) { // sets the 'oclock' light if the time is on the hour
      lightHourLEDs(now.hour());
      lightWordLEDs(OCLOCK);
    }
    
    
    //*************** light up the Happy birthday on birthdays ******************
    
    if(now.month() == 1 && now.day() ==6 || now.month() == 2 && now.day() ==14){
      lightBirthdayLEDs(HAPPY);
      lightBirthdayLEDs(BIRTHDAY);
      FastLED.show();
    }
    
    FastLED.show();
      
  }
  
  // **************animations for quarter to qurter past and half past the hour *******************
  if(now.minute() == 45 && now.second() ==00){
    animateWordLEDs(ITS);
    delay(500);
    animateWordLEDs(QUARTER);
    animateWordLEDs(TO);
    delay(500);
    animateHourLEDs(now.hour()+1);
    FastLED.show();
  }
  
  if(now.minute() == 15 && now.second() ==00){
    animateWordLEDs(ITS);
    delay(500);
    animateWordLEDs(QUARTER);
    animateWordLEDs(PAST);
    delay(500);
    animateHourLEDs(now.hour());
    FastLED.show();
  }
  
  if(now.minute() == 30 && now.second() ==00){
    animateWordLEDs(ITS);
    delay(500);
    animateWordLEDs(HALF);
    animateWordLEDs(PAST);
    delay(500);
    animateHourLEDs(now.hour());
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
  DateTime now = RTC.now();
//animation for on the hour
  if(now.minute() == 00 && now.second() >= 00 && now.second() <=8){
    Animation();
  }
  
  if(now.minute() == 00 && now.second() == 01){    // do a 10sec animation when it hits the hour
    lightWordLEDs(ITS); 
    FastLED.show();
  }
  
  if(now.minute() == 00 && now.second() >1 && now.second() <=2){   
    lightWordLEDs(ITS);
    lightHourLEDs(Hour_DST);
    FastLED.show();
  }
  
  if(now.minute() == 00 && now.second() >2 && now.second() <=8){    // do a 8sec animation when it hits the hour
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
  bool foundAllChars = false;
  
  while (BT.available() > 0 && newData == false) {    //create a char array untill you get a /n signal from bluetooth
    rc = BT.read();
  
    if (rc != endMarker) {
      receivedData[ndx] = rc;
      ndx++;
      if (ndx >= NUM_CHARS) {
        ndx = NUM_CHARS - 1;
        foundAllChars = true;
      }
    }

    if (rc == endMarker || foundAllChars) {
      receivedData[ndx] = '\0'; // terminate the string
      ndx = 0;
      newData = true;
    }
  }
}

void bluetoothCheckInput() { //If the message sent is the same as the trigger word "settime" then ask for user to enter date and time
  String data = String(receivedData);
  data.trim();

  if (newData == true) {
    BT.println(String("'") + data + String("'"));
  }

  if (newData == true && String(SET_TIME).equalsIgnoreCase(data)) {
    BT.println("Set the Time & Date as: hh,mm,ss,dd,mm,yyyy");
    newData = false;
    changingTime = true; // set a switch to true that time is going to be changed
  }

  if (newData == true && String(ADD_BDAY).equalsIgnoreCase(data)) {
    newData = false;
    addingBday = true; // set a switch to true that time is going to be changed
  }
  
  if (newData == true && String(REMOVE_BDAY).equalsIgnoreCase(data)) {
    newData = false;
    removingBday = true; // set a switch to true that time is going to be changed
  }

  if (newData == true && String(LIST_BDAY).equalsIgnoreCase(data)) {
    newData = false;
    listingBday = true; // set a switch to true that time is going to be changed
  }
  
  if (newData == true && changingTime == false && addingBday == false &&
      removingBday == false && listingBday == false) {
    newData = false;
    String Cmd = (String)"Command not recognised ("+ data + ")"; // if the user input isnt same as trigger word then inform user command not recognised
    BT.println(Cmd);
    BT.println(String("Avaliable commands: "));
    BT.println(String(SET_TIME));
    BT.println(String(ADD_BDAY));
    BT.println(String(REMOVE_BDAY));
    BT.println(String(LIST_BDAY));
  }
}

 void bluetoothChangeTime(){  
  if (newData == true && changingTime == true){ // if a new message has been recieved and the Change time switch is active change the time

    String timeStrings[6];
    int StringCount = 0;
    String data = String(receivedData);
    data.trim();

    while (data.length() > 0)
    {
      int index = data.indexOf(',');
      if (index == -1) // No comma found
      {
        timeStrings[StringCount++] = data;
        break;
      }
      else
      {
        timeStrings[StringCount++] = data.substring(0, index);
        data = data.substring(index+1);
      }
    }
  
    int hour = timeStrings[0].toInt(); // take the parsed date from array which corresponds to hour minute seconds ect. 
    int minute = timeStrings[1].toInt();
    int second = timeStrings[2].toInt();
    int day = timeStrings[3].toInt();
    int month = timeStrings[4].toInt();
    int year = timeStrings[5].toInt();
  
    setTime(hour,minute,second,day,month,year);   //this sets the system time set to GMT without the daylight saving added. 
    RTC.adjust(now());
    
    BT.println("Success");

    newData = false;
    changingTime = false;
  }
}

// TO ADD setting the bday and saving it to the eprom memory
    
