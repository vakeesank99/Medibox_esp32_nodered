#include <PubSubClient.h>
#include <WiFi.h>
#include <WiFiUdp.h>
#include <NTPClient.h>
#include <math.h>
#include <ESP32Servo.h>
// #include <Servo.h>
#include <DHTesp.h>

//if wokwi platform
// #define DHT_PIN 15
// #define LDR_PIN 35
// #define BUZZER_PIN 33
// #define SERVO_PIN 16

//if the physical board
#define DHT_PIN 32//22
#define LDR_PIN 36
#define BUZZER_PIN 33//13
#define SERVO_PIN 13
#define ssid "vakeesan wifi"
#define pass "Va@123456"


Servo servo;
WiFiClient espclient;
WiFiUDP udp;
NTPClient timeClient(udp,"pool.ntp.org");
PubSubClient mqttClient(espclient);
DHTesp dhtsensor;


int hertz=5000; 
int dur=500;
int sch_init_time;
int sch_now_time;
int pos = 0;
bool main_switch = true;
bool is_repeat = false;

char tempArr[6];
char humiArr[6];
char ldrArr[6];
char daysArr[6];
char schArr[11]={0};
void setup() {
  // put your setup code here, to run once:
  Serial.begin(115200);

  setupWiFi();
  setupMqtt();
  timeClient.begin();
  timeClient.setTimeOffset(19800);
  //dhtsensor.setup(DHT_PIN,DHTesp::DHT22); //if wokwi platform
  dhtsensor.setup(DHT_PIN,DHTesp::DHT11); //if physical board

  servo.attach(SERVO_PIN, 500, 2400);
  //setupServo();
  pinMode(BUZZER_PIN, OUTPUT);
  pinMode(LDR_PIN, INPUT);

}

// void setupWiFi(){
//   WiFi.begin ("Wokwi-GUEST","");

//   while(WiFi.status() != WL_CONNECTED){
//     delay(500);
//     Serial.print(".");
//   }
// }

void setupWiFi(){
  WiFi.begin (ssid,pass);

  while(WiFi.status() != WL_CONNECTED){
    delay(500);
    Serial.print(".");
  }
}

void setupMqtt(){
  mqttClient.setServer("test.mosquitto.org",1883);
  mqttClient.setCallback(receiveCallback);
}

void connectToBroker(){
  while(!mqttClient.connected()){
    Serial.println("Attempting MQTT connection...");
    if(mqttClient.connect("ESP32-876968797")){
      Serial.println("connected");
      mqttClient.subscribe("MOT_ANG"); //node red mqtt out label
      mqttClient.subscribe("SCHEDULE");
      mqttClient.subscribe("DELAY");
      mqttClient.subscribe("FREQUENCY");
      mqttClient.subscribe("MAIN-SWITCH");
      mqttClient.subscribe("REPEAT");


    }else{
      Serial.print("failed");
      Serial.print(mqttClient.state());
      delay(5000);
    }
  }
}

void updateTemperature(){
  TempAndHumidity data = dhtsensor.getTempAndHumidity();
  String(data.temperature,2).toCharArray(tempArr,6);
  String(data.humidity,2).toCharArray(humiArr,6);

}

void updateLight(){
  String((int)analogRead(LDR_PIN)).toCharArray(ldrArr,6);
}

void alarm(){
  if (is_repeat){
    tone(BUZZER_PIN, hertz);
    delay(1000*dur);
    noTone(BUZZER_PIN);
    delay(1000*dur);
  }
  else{
    tone(BUZZER_PIN, hertz);
    delay(500);
    noTone(BUZZER_PIN);
  }
}

void checkAlarm(){
  if((schArr[9]==1) && main_switch && schArr[10] ){
    // if((timeClient.getDay()-today==1) || (today-timeClient.getDay()==7)){
    //   today=timeClient.getDay();
    //   days-=1;
    // }
    //Serial.println(sch_now_time-sch_init_time);
    //while the alarm is ringing nothing will update to the node red platform so BE MINDFUL to change after the alarm!
    while((timeClient.getHours() == (int)schArr[1]) && ((timeClient.getMinutes() == (int)schArr[2])) && (schArr[0]==1) ){
      Serial.println("Alarm 1 is ringing! ");
      alarm();
      
    }
    while((timeClient.getHours() == (int)schArr[4]) && ((timeClient.getMinutes() == (int)schArr[5])) && (schArr[3]==1) ){
      Serial.println("Alarm 2 is ringing! ");
      alarm();

    }
    while((timeClient.getHours() == (int)schArr[7]) && ((timeClient.getMinutes() == (int)schArr[8])) && (schArr[6]==1) ){
      Serial.println("Alarm 3 is ringing! ");
      alarm();
    }
  }
}


void receiveCallback(char* topic, byte* payload, unsigned int length){
  Serial.print("Message arrived [ ");
  Serial.print (topic);
  Serial.print(" ]");
  char payloadCharArr[length];
  for (int i = 0; i<length;i++){
    Serial.print((char)payload[i]);
    payloadCharArr[i]=(char)payload[i];
  }
  Serial.println();
  //--------------------------------------------------------------------------------------------
//    Schedule seetings
  if(strcmp(topic, "SCHEDULE") == 0 ){
    /*  
    * our received string get form like below 
    *   "NNF----N1202N2139" 
    *       N:on  F:off 
    *       F---- :schedule is off 
    *       N2139 :schedule is on at time 21.39h
    *   [0]=schedule on/off
    *   [1]=days
    *   [2]=schedule1 on/off
    *   [3-6]=schdule 1 time
    *   [7]=schedule2 on/off
    *   [8-11]=schdule 2 time
    *   [12]=schedule3 on/off
    *   [13-16]=schdule 3 time
    */
    if(payloadCharArr[0]=='N'){
    /*  
    * our derived array like below 
    *   [0]=schedule 1 on/off
    *   [1-2]=schedule 1 time
    *   [3]=schedule 2 on/off
    *   [4-5]=schdule 2 time
    *   [6]=schedule 3 on/off
    *   [7-8]=schdule 3 time
    *   [9]=schdule all on off
    *   [10]=days on off
    */
      sch_init_time = 3600*timeClient.getHours() + 60*timeClient.getMinutes();
      Serial.println("Schedule ALL ON ");
      schArr[9]= 1; //
      //today=timeClient.getDay();
      //schArr[9]= 10 * payloadCharArr[1] + payloadCharArr[2] - 16 ;
      if (payloadCharArr[1]=='N'){
        Serial.println("more Days to go");
        schArr[10]= 1;

      }
      else{
        Serial.println("NO more Days to go! ");
        schArr[10]= 0;
      }
      if (payloadCharArr[2]=='N'){
        Serial.println("Schedule 1 ON ");
        schArr[0]= 1;
        schArr[1]= 10 * payloadCharArr[3] + payloadCharArr[4] - 16 ;
        schArr[2]= 10 * payloadCharArr[5] + payloadCharArr[6] - 16 ;

      }
      if (payloadCharArr[7]=='N'){
        Serial.println("Schedule 2 ON ");
        schArr[3]= 1;
        schArr[4]= 10 * payloadCharArr[8] + payloadCharArr[9] - 16 ;
        schArr[5]= 10 * payloadCharArr[10] + payloadCharArr[11] - 16 ;

      }
      else{
        schArr[3]=0;
      }
      if (payloadCharArr[12]=='N'){
        Serial.println("Schedule 3 ON ");
        schArr[6]= 1;
        schArr[7]= 10 * payloadCharArr[13] + payloadCharArr[14] - 16 ;
        schArr[8]= 10 * payloadCharArr[15] + payloadCharArr[16] - 16 ;

      }
      else{
        schArr[6] = 0;
      }

    }
  }
//--------------------------------------------------------------------------------------------
//    motor seetings
  if(strcmp(topic,"MOT_ANG")==0){
  /*
  *motor angle for the sliding window is given by this call!
  */
    //Serial.print(payload);
    pos=0;
    //Serial.println(pos);
    for (int i = 0; i<length;i++){
      //Serial.println((int)payload[i]);
      pos += pow(10,(length-i-1))*((int)payloadCharArr[i]-48);
      //Serial.print(length);
    }

    if (pos!=3520){ 
      //to ignore the NaN value getting from the node red 
      Serial.println(pos);
      servo.write(pos);
      delay(15);
    }

  }  
//--------------------------------------------------------------------------------------------
//    Buzzer seetings

  if(strcmp(topic,"DELAY")==0){
  /*
  *delay time at the repeated on mode of the buzzer defined here!
  */
    dur=0;
    //Serial.println(dur);
    for (int i = 0; i<length;i++){
      //Serial.println((int)payload[i]);
      dur += pow(10,(length-i-1))*((int)payload[i]-48);
      //Serial.print(length);
    }
    //Serial.println(dur);
  }

  if(strcmp(topic,"FREQUENCY")==0){
  /*
  *frequency of the buzzer defined here!
  */
    hertz=0;
    //Serial.println(dur);
    for (int i = 0; i<length;i++){
      //Serial.println((int)payload[i]);
      hertz += pow(10,(length-i-1))*((int)payload[i]-48);
      //Serial.print(length);
    }
    //Serial.println(hertz);
  }

  if(strcmp(topic,"MAIN-SWITCH")==0){
  /*
  *switch on the buzzer or off by default its on but in the node red it wil show like its off
  *inorder ro switch it off you have to 1.first ON the main switch  2. off the switch
  */
    //Serial.println(payloadCharArr[0]);
    if (payloadCharArr[0]=='1'){
      main_switch=true;
      Serial.println("Buzzer is on");
    }
    if(payloadCharArr[0]=='0'){
      main_switch=false;
      Serial.println("Buzzer is off");
    }
  }

    if(strcmp(topic,"REPEAT")==0){
    /*
    *for the buzzer repeated on off mode and contniuous mode
    *by default it's repeated on 
    *for repeated off or continuos mode you have to change in node red
    */
      //Serial.println(payloadCharArr[0]);
      if (payloadCharArr[0]=='1'){
        is_repeat=true;
        Serial.println("Repeated mode on");
      }
      if(payloadCharArr[0]=='0'){
        is_repeat=false;
        Serial.println("Continuous mode on");
      }
  }
}

void loop() {

  if (!mqttClient.connected()){
    connectToBroker();
  }
  mqttClient.loop();

  timeClient.update();
  checkAlarm();
  updateTemperature();
  updateLight();
  //Serial.println(tempArr);
  //Serial.println(humiArr);
  //Serial.println(ldrArr);

  mqttClient.publish("TEMPERATURE", tempArr); //node red mqtt in lable
  mqttClient.publish("HUMIDITY", humiArr); //node red mqtt in lable
  mqttClient.publish("LIGHT", ldrArr); //node red mqtt in lable

  delay(1000);

}


