#include <SoftwareSerial.h>     //  device baud 9600 currently
#include <RTClib.h>
#include <Wire.h>
#include <Servo.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#define ONE_WIRE_BUS 4
#define MINTNKLVL   1
#define MAXTNKLVL   90
#define FEEDHOUR    8           // 24 hour
#define FEEDMINUTE  0
#define SensorPin A3            //pH meter Analog output to Arduino Analog Input 0
#define Offset 0.00             //deviation compensate
#define samplingInterval 20     // ph sampling Interval
#define printInterval 800       // ph print interval 
#define ArrayLenth 40           //times of collection
#define APIKEY   "WFPRNDZ9PACU9AXN"
SoftwareSerial wifiSerial(2,3); //Define ESP8266 software serial  connections
OneWire oneWire(ONE_WIRE_BUS);  // Setup a oneWire instance to communicate with any OneWire device
DallasTemperature sensors(&oneWire);// Pass oneWire reference to DallasTemperature library
RTC_DS1307 RTC;                 // initialize RTC object sda a4 scl a5
Servo myservo;                  //Servo object
DateTime now;                   //Date and Time object
bool motor_State ;              // motor state  
int pHArray[ArrayLenth];        //Store the average value of the sensor feedback
int pHArrayIndex=0;
int ff =8;                      // filter tank float
int gbf=9;                      // grow bed float
int relay=10;                   // relay pin
int servo =11;                  // servo pin
int trigPin = 12;               // Ultra_sonic trigger
int echoPin = 13;               // Ultra_sonic  echo
int distance;                   // calculated distance  
int tank_lvl=0;                 // tank water lvl
int pos = 0;                    // store fish feed servo position
int upload_count=0;             // things_speak upload count
float tempC = 0;                // temperature celcius
long duration;                  // Ultra_sonic pulse time
String cipstart="AT+CIPSTART=\"TCP\",\"184.106.153.149\",80";
String const_getStr = "GET /update?api_key=";
String getStr;
void uploaddataesp(float volt,float ph);// upload data to things speak channel
void change_motor_condition(int x);     // turn motor on and off
void clockdisplay();                    // Print Date and time on serial monitor                   
void startfishfeed();                   // Drop Fishfeed to tank
int ultrasonic_read();                  // Water level
float temperature();                    // temperature reading from ds18b20
double avergearray(int* arr, int number);///function to calculate ph value after sampling


void setup(){
    
    Serial.begin(9600);
    wifiSerial.begin(9600);
    Wire.begin();
    RTC.begin();
    sensors.begin();	// Start up the dellastemperture library
    pinMode(ff, INPUT_PULLUP);// sets float to input , default high
    pinMode(gbf, INPUT_PULLUP);// sets float to input , default high
    pinMode(relay,OUTPUT);
    pinMode(servo,OUTPUT);
    pinMode(trigPin, OUTPUT); // Sets the trigPin as an Output
    pinMode(echoPin, INPUT); // Sets the echoPin as an Input
    myservo.attach(servo);
    bool motor_State = false;
    
    if (! RTC.isrunning()) {              // Check to see if the RTC is keeping time.
    Serial.println("RTC is NOT running!");//If it is, load the time from your computer.
    RTC.adjust(DateTime(__DATE__, __TIME__));
    }
}

void loop()
{
    tank_lvl=ultrasonic_read();
    if (tank_lvl <= MINTNKLVL){ 
        Serial.println("tank water low"); tank_lvl=5;}
    else if (tank_lvl >= MAXTNKLVL){
         Serial.println("tank water high"); tank_lvl=95;
    }

    if (digitalRead(ff)==HIGH && digitalRead(gbf)==HIGH){ // both  floats are down  --start motor
        change_motor_condition(1);  // start motor
        Serial.println("motor on");
        motor_State=true;
    }

    if (digitalRead(ff)==LOW || digitalRead(gbf)==LOW){ // one of the floats are up  --stop motor
        change_motor_condition(0);
        Serial.println("motor  off");
        motor_State=false;
    }
     
    now = RTC.now();  // current date and time reading
    clockdisplay();
    if (now.hour()==FEEDHOUR && now.minute()==FEEDMINUTE && now.second()==0)// check fish feed time
    startfishfeed();
    tempC=temperature();
    //ph
    static unsigned long samplingTime = millis();
    static unsigned long printTime = millis();
    static float pHValue,voltage;
    if(millis()-samplingTime > samplingInterval){
    pHArray[pHArrayIndex++]=analogRead(SensorPin);
    if(pHArrayIndex==ArrayLenth)pHArrayIndex=0;
    voltage = avergearray(pHArray, ArrayLenth)*5.0/1024;
    pHValue = 3.5*voltage+Offset;
    samplingTime=millis();
    }
    if(millis() - printTime > printInterval){ //Every 800 milliseconds, print a numerical, convert the state of the LED indicator
    Serial.print("Voltage:");
    Serial.print(voltage,2);
    Serial.print(" pH value: ");
    pHValue=pHValue;    
    Serial.println(pHValue,2);          //PHVALUE
    printTime=millis();
    } 
 if (millis()% 15000 > upload_count){ //  loop ends here by uploading data every 15 seconds 
      uploaddataesp(voltage,pHValue);upload_count++;
     Serial.println("upload count=");Serial.print(upload_count++);Serial.println();Serial.println();
    } 
}


//iot/// 
void uploaddataesp(float volt,float ph){
  getStr=const_getStr;
  wifiSerial.println(cipstart);
  delay(2000);
  getStr +=APIKEY;          // create get string
  getStr +="&field1=";
  getStr += String(tank_lvl);// adding values
  getStr +="&field2=";
  getStr += String(motor_State);
  getStr +="&field3=";
  getStr += String(volt);
  getStr +="&field4=";
  getStr += String(ph);
  getStr +="&field5=";
  getStr += String(tempC);
  getStr +="\r\n";
  Serial.println(getStr);
  wifiSerial.println("AT+CIPSEND="+String((getStr.length()+2)));
  delay(250);
  wifiSerial.println(getStr); // send command
}

int ultrasonic_read()
{
digitalWrite(trigPin, LOW);
delayMicroseconds(2);
digitalWrite(trigPin, HIGH);   // Sets the trigPin on HIGH state for 10 micro seconds
delayMicroseconds(10);
digitalWrite(trigPin, LOW);
duration = pulseIn(echoPin, HIGH);// Reads the echoPin, returns the sound wave travel time in microseconds
distance= duration*0.034/2;// Calculating the distance in cm
Serial.print("Tank_lvl_Distance:");// Prints the distance on the Serial Monitor
Serial.print(distance);Serial.print("cm "); 
return(distance);
}

void change_motor_condition(int x) // 0 to stop else will be onlayon
{if (x==0)
    {digitalWrite(relay,HIGH);Serial.println("relay off");}//relay works on active LOW
    else    { digitalWrite(relay,LOW);Serial.println("relay onn");} //motor on 
}

void clockdisplay(){ // Print data to display
    Serial.print(now.month());
    Serial.print('/');
    Serial.print(now.day());
    Serial.print('/');
    Serial.print(now.year());
    Serial.print(' ');
    Serial.print(now.hour(), DEC);
    Serial.print(':');
    Serial.print(now.minute(), DEC);
    Serial.print(':');
    Serial.print(now.second(), DEC);
    Serial.println();    
}

void startfishfeed()
{
   for(pos = 0; pos <= 180; pos += 1) // goes from 0 degrees to 180 degrees
  { // in steps of 1 degree
    myservo.write(pos); // tell servo to go to position in variable 'pos'
    delay(15); // waits 15ms for the servo to reach the position
  }
  for(pos = 180; pos>=0; pos-=1) // goes from 180 degrees to 0 degrees
  {
    myservo.write(pos); // tell servo to go to position in variable 'pos'
    delay(15); // waits 15ms for the servo to reach the position
  }
}
float temperature()
{ float val;
 // Send the command to get temperatures
  sensors.requestTemperatures(); 
  val=sensors.getTempCByIndex(0);
  //print the temperature in Celsius
  Serial.println("Temperature: ");
  Serial.print(val);
  Serial.print((char)176);//shows degrees character oc
  Serial.println("C");
  return val;
}


double avergearray(int* arr, int number){    // simple math
    int i;
    int max,min;
    double avg;
    long amount=0;
    if(number<=0){
    Serial.println("Error number for the array to avraging!/n");
    return 0;
    }

    if(number<5){ //less than 5, calculated directly statistics
        for(i=0;i<number;i++){
        amount+=arr[i];
        }
        avg = amount/number;
        return avg;
    }else{
         if(arr[0]<arr[1]){
         min = arr[0];max=arr[1];
        }else{min=arr[1];max=arr[0];}
        for(i=2;i<number;i++){
        if(arr[i]<min){
        amount+=min; //arr<min
        min=arr[i];
        }else {
                if(arr[i]>max){
                 amount+=max; //arr>max
                 max=arr[i];
                }else{
                  amount+=arr[i]; //min<=arr<=max
                 }
            }
        }
        avg = (double)amount/(number-2);
    }
    return avg;

}
