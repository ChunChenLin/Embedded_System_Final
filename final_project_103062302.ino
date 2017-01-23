#include <Arduino_FreeRTOS.h>
#include <Stepper.h>
#include <Wire.h>
#include <IRremote.h>
#include <LiquidCrystal_I2C.h>

#define STEPS 2048
const int trigPin = 12;
const int echoPin = 13;
const int soundOut = 3;
const int remoteOut = 4;
const int button_pin = 2;
const int redPin = 11;
const int greenPin = 10;
const int half = 1024;
const int MAX_CAR = 3; // allow MAX_CAR in parking lot

int carNum[4] = {0,0,0,0}; // 4 digits
int recordNum[MAX_CAR] ; // record numID of car in parking lot
int money[2];
int cm; // counter for money
int counter; // counter for 4 digits
int avail;
int flag; // 0 for EMERGENCY_OPEN; 1 for EMERGENCY_CLOSE
int duration, distance;
int tmp;
int charge;
bool found;
enum Status{
  NONE, 
  EMERGENCY_OPEN, EMERGENCY_CLOSE,
  ABOUT_IN, ABOUT_OUT, 
  CARIN_OPEN, CARIN_CLOSE,
  CAROUT_OPEN, CAROUT_CLOSE,
  ABOUT_CLOSE_IN, ABOUT_CLOSE_OUT,
  ABOUT_PAY
};
Status STATUS = NONE;

IRrecv irrecv(remoteOut);
decode_results results;
Stepper stepper(STEPS,6,8,7,9);
LiquidCrystal_I2C lcd(0x3F, 2, 1, 0, 4, 5, 6, 7, 3, POSITIVE);

void setup() {
  stepper.setSpeed(10);
  Serial.begin(9600);
  lcd.begin(16,2);
  lcd.off();
  lcd.setCursor(0, 0);
  pinMode(trigPin,OUTPUT);
  pinMode(echoPin,INPUT);
  pinMode(soundOut,OUTPUT);
  pinMode(button_pin,INPUT);
  pinMode(A0,INPUT);
  irrecv.enableIRIn();
  lcd.clear();
  lcd.on();
  lcd.setCursor(0,0);
  distance = 100;
  avail = MAX_CAR;
  counter = 0;
  tmp = 0;
  charge = 0;
  found=false;
  /* Initialization */
  for(int i=0;i<MAX_CAR;i++) {
    recordNum[i]=-1; // empty
  }
} 
void loop() {
  int button;
  int pr; // photoresister
  pr = analogRead(A0);
  button = digitalRead(button_pin);
  digitalWrite(trigPin,LOW);
  delayMicroseconds(20);
  digitalWrite(trigPin,HIGH);
  delayMicroseconds(100);
  digitalWrite(trigPin,LOW);
  duration = pulseIn(echoPin, HIGH);
  distance = duration*0.034/2;
  //Serial.println(distance);
  //Serial.println(pr);

  /* Highest priority*/
  if(button==HIGH /*&& STATUS==NONE*/) {
    //Serial.println("BUTTON HIGH");
    if(!flag) {
      STATUS = EMERGENCY_OPEN;
      flag = !flag;
    }
    else {
      STATUS = EMERGENCY_CLOSE;
      flag = !flag;
    }
  }
  /* Detect Car Approach*/
  else if(distance < 10 && STATUS==NONE) {
     if(avail>0) {
       //Serial.println("Distance < 10 and ABOUT_IN");
       STATUS = ABOUT_IN;
     }
     else {
       //Serial.println("No enough spaces");
       STATUS = NONE;
     }
  }
  /* If about to IN but end up leaving*/ 
  /* Note that if start entering number(counter>0) then ignore*/
  else if(distance >= 10 && STATUS==ABOUT_IN && counter==0) {
    //Serial.println("Distance >= 10 and LEAVING");
    STATUS = NONE;
  }
  /* The car has entered and about to close*/
  else if(distance >= 10 && STATUS==ABOUT_CLOSE_IN) {
    //Serial.println("distance >= 10 && STATUS==ABOUT_CLOSE_IN");
    STATUS = CARIN_CLOSE;
  }
  /* The car has left and about to close*/
  else if(distance < 10 && STATUS==ABOUT_CLOSE_OUT) {
    //Serial.println("distance >= 10 && STATUS==ABOUT_CLOSE_OUT");
    STATUS = CAROUT_CLOSE;
  }
  /* The car is about to leaving */
  else if(pr < 600 && STATUS==NONE) {
    //Serial.println("pr < 600 and CAROUT_OPEN");
    STATUS = ABOUT_OUT;
  }

/**************************Finite State Mechine**************************************/
  if(STATUS==EMERGENCY_OPEN) {
    //Serial.println("EMERGENCY_OPEN");
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Emergency");
    tone(soundOut,50);
    analogWrite(redPin,255);
    analogWrite(greenPin,0);
    stepper.step(half);
    noTone(soundOut);
    STATUS=NONE;
  }
  else if(STATUS==EMERGENCY_CLOSE) {
    //Serial.println("EMERGENCY_CLOSE");
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Emergency");
    tone(soundOut,50);
    analogWrite(redPin,255);
    analogWrite(greenPin,0);
    stepper.step(-half);
    noTone(soundOut);
    STATUS=NONE;
  }
  else if(STATUS==ABOUT_IN){
    if(counter==0) {
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("Welcome! Enter");
      lcd.setCursor(0, 1);
      lcd.print("your number:");
      counter=0;
    }
    if (irrecv.decode(&results)) {
      if(counter==0) {
        counter=0;
        lcd.clear();
        lcd.setCursor(0, 0);
        translateIR();
        irrecv.resume();
      }
      else if(counter>=4) {
        counter=0;
        STATUS=CARIN_OPEN; // go next state
      }
      else {
        translateIR();
        irrecv.resume();
      }
    }
  }
  else if(STATUS==CARIN_OPEN) {
    //avail = avail - 1; // car in
    lcd.clear();
    lcd.setCursor(0,0);
    tmp=0;
    for(int i=0; i<4; i++) {
      lcd.print(carNum[i]);
      tmp = tmp*10 + carNum[i];
    }
    for(int i=0;i<MAX_CAR;i++) {
      if(recordNum[i]==tmp) {
        found=true;
        break;
      }
    }
    if(found) {
      found=false;
      noTone(soundOut);
      lcd.print(" already");
      lcd.setCursor(0,1);
      lcd.print("exists");
      delay(3000);
      STATUS=ABOUT_IN;
    }
    else {
      avail = avail - 1;
      for(int i=0;i<MAX_CAR;i++) {
        if(recordNum[i]==-1) {
          recordNum[i]=tmp;
          break;
        }
      }
      lcd.print(" is coming");
      tone(soundOut,50);
      analogWrite(redPin,255);
      analogWrite(greenPin,0);
      stepper.step(half);
      STATUS=ABOUT_CLOSE_IN;
    }
  }
  else if(STATUS==CARIN_CLOSE) {
    delay(2000);
    tone(soundOut,50);
    stepper.step(-half);
    analogWrite(redPin,255);
    analogWrite(greenPin,0);
    STATUS=NONE;
  }
  
  else if(STATUS==ABOUT_OUT) {
    noTone(soundOut);
    if(counter==0) {
      counter=0;
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("Please Enter");
      lcd.setCursor(0, 1);
      lcd.print("your number:");
    }
    if (irrecv.decode(&results)) {
      if(counter==0) {
        counter=0;
        lcd.clear();
        lcd.setCursor(0, 0);
        translateIR();
        irrecv.resume();
      }
      else if(counter>=4) {
        counter=0;
        STATUS=CAROUT_OPEN;
      }
      else {
        translateIR();
        irrecv.resume();
      }
    }
  }
  else if(STATUS==CAROUT_OPEN) {
    lcd.clear();
    lcd.setCursor(0,0);
    tmp=0;
    for(int i=0; i<4; i++) {
      lcd.print(carNum[i]);
      tmp = tmp*10 + carNum[i];
    }
    for(int i=0; i<MAX_CAR; i++) {
      if(recordNum[i]==tmp) {
        Serial.print(recordNum[i]);
        Serial.println(" is out");
        recordNum[i]=-1;
        found=true;
        break;
      }
    }
    if(found) {
      charge = random(10,99);
      STATUS = ABOUT_PAY;
      found=false;
    }
    else {
      noTone(soundOut);
      lcd.print(" Not Found");
      delay(3000);
      STATUS = ABOUT_OUT;
    }
  }
  else if(STATUS==ABOUT_PAY) {
    if(counter==0) {
      lcd.clear();
      lcd.setCursor(0,0);
      lcd.print("Parking fee $");
      lcd.print(charge);
    }
    if (irrecv.decode(&results)) {
      if(counter==0) {
        lcd.clear();
        lcd.setCursor(0, 0);
        translateIR();
        irrecv.resume();
      }
      else if(counter>=2) {
        counter=0;
        tmp=0;
        for(int i=0;i<2;i++) {
          tmp=tmp*10+carNum[i];
        }
        if(tmp>=charge) {
          lcd.clear();
          lcd.setCursor(0,0);
          lcd.print("Return ");
          lcd.print(tmp-charge);
          lcd.print(" dollars");
          delay(3000);
          lcd.clear();
          lcd.setCursor(0,0);
          lcd.print("GoodBye Thanks");
          avail = avail + 1; // car out
          tone(soundOut,50);
          analogWrite(redPin,255);
          analogWrite(greenPin,0);
          stepper.step(half);
          STATUS=ABOUT_CLOSE_OUT;
          //delay(3000);
          //STATUS=CAROUT_CLOSE;
        }
        else {
          lcd.clear();
          lcd.setCursor(0,0);
          lcd.print("Not enough paid");
          delay(3000);
          STATUS=ABOUT_PAY;
        }
      }
      else {
        translateIR();
        irrecv.resume();
      }
    }
  }
  else if(STATUS==CAROUT_CLOSE) {
    tone(soundOut,50);
    stepper.step(-half);
    analogWrite(redPin,255);
    analogWrite(greenPin,0);
    STATUS=NONE;
  }
  else if(STATUS==NONE){
    noTone(soundOut);
    counter=0;
    lcd.clear();
    lcd.setCursor(0,0);
    if(avail > 0) {
      lcd.print("Remaining ");
      lcd.print(avail);
    }
    else {
      lcd.print("Sorry! No Spaces");
    }
    analogWrite(greenPin,255);
    analogWrite(redPin,0);
  }

  /*********** Checking ***********/
  for(int i=0;i<MAX_CAR;i++) {
    if(recordNum[i]!=-1) {
      Serial.print("Car: ");
      Serial.println(recordNum[i]);
    }
  }
  Serial.println("----------------------------");
  delay(500);
}

void translateIR() // takes action based on IR code received describing Car MP3 IR codes 
{
  switch(results.value) {
  case 0xFFC23D:
    Serial.println(" PLAY/PAUSE   ~ ");
    //counter++;
    break;
   case 0xBE90A873:  
    Serial.println(" PLAY/PAUSE     ");
    break;
  case 0xFFE01F:  
    Serial.println(" VOL-           "); 
    break;
   case 0xF076C13B:  
    Serial.println(" VOL-           "); 
    break;
  case 0xFFA857:  
    Serial.println(" VOL+           "); 
    break;
  case 0xA3C8EDDB:  
    Serial.println(" VOL+           "); 
    break;
  case 0xFF906F:  
    Serial.println(" EQ             "); 
    break;
  case 0xE5CFBD7F:  
    Serial.println(" EQ             "); 
    break;
  case 0xFF6897:  
    Serial.println(" 0              ");
    lcd.print("0");
    carNum[counter] = 0;
    counter++;
    break;
  case 0xC101E57B:  
    Serial.println(" 0              ");
    lcd.print("0");
    carNum[counter] = 0;
    counter++;
    break;
  case 0xFF9867:  
    Serial.println(" 100+           "); 
    break;
  case 0x97483BFB:  
    Serial.println(" 100+           "); 
    break;
  case 0xFFB04F:  
    Serial.println(" 200+           "); 
    break;
  case 0xF0C41643:  
    Serial.println(" 200+           "); 
    break;
  case 0xFF30CF:  
    Serial.println(" 1              ");
    lcd.print("1");
    carNum[counter] = 1;
    counter++;
    break;
  case 0x9716BE3F:  
    Serial.println(" 1              ");
    lcd.print("1");
    carNum[counter] = 1;
    counter++;
    break;
  case 0xFF18E7:  
    Serial.println(" 2              "); 
    lcd.print("2");
    carNum[counter] = 2;
    counter++;
    break;
  case 0x3D9AE3F7:  
    Serial.println(" 2              ");
    lcd.print("2");
    carNum[counter] = 2;
    counter++;
    break;
  case 0xFF7A85:  
    Serial.println(" 3              ");
    lcd.print("3");
    carNum[counter] = 3;
    counter++;
    break;
  case 0x6182021B:  
    Serial.println(" 3              ");
    lcd.print("3");
    carNum[counter] = 3;
    counter++;
    break;
  case 0xFF10EF:  
    Serial.println(" 4              ");
    lcd.print("4");
    carNum[counter] = 4;
    counter++;
    break;
  case 0x8C22657B:  
    Serial.println(" 4              ");
    lcd.print("4");
    carNum[counter] = 4;
    counter++;
    break;
  case 0xFF38C7:  
    Serial.println(" 5              ");
    lcd.print("5");
    carNum[counter] = 5;
    counter++;
    break;
  case 0x488F3CBB:  
    Serial.println(" 5              ");
    lcd.print("5");
    carNum[counter] = 5;
    counter++;
    break;
  case 0xFF5AA5:  
    Serial.println(" 6              ");
    lcd.print("6");
    carNum[counter] = 6;
    counter++;
    break;
  case 0x449E79F:  
    Serial.println(" 6              ");
    lcd.print("6");
    carNum[counter] = 6;
    counter++;
    break;
  case 0xFF42BD:  
    Serial.println(" 7              ");
    lcd.print("7");
    carNum[counter] = 7;
    counter++;
    break;
  case 0x32C6FDF7:  
    Serial.println(" 7              ");
    lcd.print("7");
    carNum[counter] = 7;
    counter++;
    break;
  case 0xFF4AB5:  
    Serial.println(" 8              ");
    lcd.print("8");
    carNum[counter] = 8;
    counter++;
    break;
  case 0x1BC0157B:  
    Serial.println(" 8              ");
    lcd.print("8");
    carNum[counter] = 8;
    counter++;
    break;
  case 0xFF52AD:  
    Serial.println(" 9              ");
    lcd.print("9");
    carNum[counter] = 9;
    counter++;
    break;
  case 0x3EC3FC1B:  
    Serial.println(" 9              ");
    lcd.print("9");
    carNum[counter] = 9;
    counter++;
    break;
  case 0xFFFFFFFF:  
    Serial.println("               ");  //ignore the buffer error
    break;
  case 0xFF:  
    Serial.println("               "); // ignore buffer error
    break;
  default: 
    Serial.print(" unknown button   ");
    Serial.println(results.value, HEX);
  }
  delay(100);
}
