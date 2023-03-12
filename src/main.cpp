#include <Arduino.h>
#include <WiFi.h> //import for wifi functionality
#include <WebSocketsServer.h> //import for websocket
#include "TM1628.h"


const char *ssid =  "ESPWIFI";   //Wifi SSID (Name)   
const char *pass =  "1122334400"; //wifi password

// define - data pin 13, clock pin 25 and strobe pin 33
TM1628 disp(13, 0, 33); //arduino uno version :12-11-10
int button_tm=0;//button value recived from interface

//pins setup
const int pwm_out = 32;
const int adcCurrentPin = 34;//A2 uno//current
const int adc_voltage = 36;//A0 uno
const int green_LED = 15;

//time constants
const  int keyWait = 200;
const int microd = 10;// *10

int sensorValue = 0;
int adcValue =0;

const float alfa = 0.4426;//facteur of current 
double Iout=0;
double Vout=0;

int valu1=0,valu2=0, valu3=0,valu4=0,valu5=0;
int valu6=0,valu7=0, valu8=0,valu9=0,valu10=0;
int avg=0;

int valu1Ic=0, valu2Ic=0,valu3Ic=0,valu4Ic=0,valu5Ic=0;
int avgIc=0;

double current = 0.00;

//compensator variables
int currentRef = 0 ;
double errorCurrent=0.00;
int delta =0, reg = 0, shift=0;
const int downLimit=8000;//10k

bool manual = false;
int viewMode=10;//10 is for Iref display
int count=20;

//PWM sensor var
const int freq = 1000;
const int pwm_channel = 0;
const int resolution = 16;
const int upLimit= (int)(pow(2, resolution) - 1);

bool EventPwmChange = false;

int hours= 0,minutes= 0,secondes = 0;
unsigned long time1= 0,time2= 0;
bool restart_count = false;

bool chargingState=true;

//dac 
int dacPin= 25;
	
	double ViewVoltage (double );
	double ViewCurrent(int );
WebSocketsServer webSocket = WebSocketsServer(81); //websocket init with port 81

		// 		*************************************
		// 		*									*
		// 		*	   SentTXT must be String  		*
		// 		*									*
		//      *************************************

void webSocketEvent(uint8_t num, WStype_t type, uint8_t * payload, size_t length) {
//webscket event method
    String cmd = "";
    int cmdInt = 0;
	//bool connectionws = false;
	//Serial.println(type);
    switch(type) {
        case WStype_DISCONNECTED:
            //Serial.println("Websocket is disconnected");
            //connectionws=false;
            //case when Websocket is disconnected
            break;
        case WStype_CONNECTED:{
            //wcase when websocket is connected
            //Serial.println("Websocket is connected");
            //Serial.println(webSocket.remoteIP(num).toString());
            webSocket.sendTXT(num, "connected");}
            //connectionws=true;
            break;
        case WStype_TEXT:
            cmd = "";
            for(int i = 0; i < length; i++) {
                cmd = cmd + (char) payload[i]; 
            } //merging payload to single string
			//Serial.print("command from Client: ");Serial.println(cmd);
			//send data to client (app)
			if (cmd == "getpwmdata" || (cmd == "getalldata" && EventPwmChange)){
				String txt = "sendpwm"+String(currentRef);
				webSocket.sendTXT(num, txt);
				//Serial.println("send actual pwm");
				EventPwmChange = false;
			} else if (cmd == "getalldata"){
				//Serial.println("getalldata message");
				//delay(10);
				int ioutInt= (int) Iout;
				String txt = "dti"+String(ioutInt)+"v"+String(Vout)+"h"+String(hours)+"m"+String(minutes)+"s"+String(secondes)+"e";
				//delay(50);
				//Serial.println(txt);
				webSocket.sendTXT(num, txt);
				//power 
				if (chargingState) webSocket.sendTXT(num, "poweron");
				else webSocket.sendTXT(num, "poweroff");
				//delay(10);
				
			}else if (cmd.startsWith("pwm")){
				//Serial.println("PWM CMD ACTIVE");
				String tmp = cmd.substring(3,cmd.length());
				currentRef = tmp.toInt();
			}else if (cmd.startsWith("power")){
				//Serial.println("Power CMD ACTIVE");
				String tmp = cmd.substring(5,cmd.length());
				if (tmp == "on"){chargingState = true;}
				else if (tmp == "off"){chargingState = false;}
			}else if (cmd.startsWith("dac")) {
				//Serial.println("DAC CMD ACTIVE");
				String tmp = cmd.substring(3,cmd.length());
				dacWrite(dacPin,tmp.toInt());
			}
			
			delay(1);
			//webSocket.sendTXT(num, cmd+":success");
            break;
        case WStype_FRAGMENT_TEXT_START:
            break;
        case WStype_FRAGMENT_BIN_START:
            break;
        case WStype_BIN:
			//hexdump(payload, length);
            break;
        default:
            break;
    }

}


void setup() {
		//adc setup
	ledcSetup(pwm_channel,freq,resolution);
	ledcAttachPin(pwm_out,pwm_channel);

	//TM1628 
	disp.begin(ON,7);
	
	//green LED stup
	pinMode(green_LED, OUTPUT);
	digitalWrite(green_LED, LOW);   

	Serial.begin(115200);
	Serial.println("Start");
	
	//PWM intialation 
	currentRef=0;
	reg=downLimit;
	ledcWrite(pwm_channel,currentRef);
	
	//ADC Atenuation setup
	//analogSetAttenuation(ADC_0db);
	//adcAttachPin(34);

   Serial.println("Connecting to wifi");
   IPAddress apIP(192, 168, 0, 1);   //Static IP for wifi gateway
   WiFi.softAPConfig(apIP, apIP, IPAddress(255, 255, 255, 0)); //set Static IP gateway on NodeMCU
   WiFi.softAP(ssid, pass); //turn on WIFI

   webSocket.begin(); //websocket Begin
   webSocket.onEvent(webSocketEvent); //set Event for websocket
   Serial.println("Websocket is started");
   time1=millis();
   //dac initial for 4.1V voltage 
   //dacWrite(dacPin,184);
   //4.0V: 177
   //4.2V: 191
   //4.3V: 198
   //4.4V: 255
   
}

void loop() {
	
   webSocket.loop(); //keep this line on loop method
   
   // if (Serial.available()){
		// String data=Serial.readStringUntil('\n');
		// Serial.println(data);
		// dacWrite(dacPin,data.toInt());
	// }
   if(currentRef==0){
	restart_count=true;}
   else {
	restart_count=false;}
   
   
   if(restart_count){
	   hours=0;
	   minutes=0;
	   secondes=0;
	   //time1=millis();
	   //restart_count=false;
	} else {
	   time2=millis();
	   
	   if(time2-time1 > 1000){
		   secondes++;
		   time1=millis();
		}
		
	   if(secondes == 60){
		   minutes++;
		   secondes=0;
	   }
	   
	   if(minutes==60){
		   hours++;
		   minutes=0;
		}
	}
	// if(count>10){
	// Serial.print(hours);Serial.print("h");Serial.print(minutes);
    // Serial.print("m");Serial.print(secondes);Serial.println("s");
	// }
   
   button_tm = disp.getButtonB();

  //three modes view : current & voltage & Iref_indi commande
  if(button_tm == 32){
	//viewMode value : 10 for Iref, 11 for Voltage display and 12 for Current display
    viewMode = (viewMode == 12 ? 10 : viewMode+1);
	count=50;//Display immediatly
    delay(keyWait);
  }

  //auto manual toggle commande
  if(button_tm == 8){
	  //chargingState=!chargingState; //default
    // if(manual == true){manual=false;disp.displayNumber(1111);}
    // else {manual=true;disp.displayNumber(8888);}  
    if(1){
		ledcWrite(pwm_channel,0);
		delay(50);
		ledcWrite(pwm_channel,17000);
		delay(200);
		ledcWrite(pwm_channel,0);
		delay(200);
		ledcWrite(pwm_channel,18600);
		delay(300);
		ledcWrite(pwm_channel,0);
		delay(300);
		ledcWrite(pwm_channel,19000);
		delay(400);
		ledcWrite(pwm_channel,0);
		delay(400);
		ledcWrite(pwm_channel,20000);
		delay(500);
		ledcWrite(pwm_channel,0);
		delay(600);
		ledcWrite(pwm_channel,21000);
		delay(600);
		ledcWrite(pwm_channel,0);
		delay(700);
		ledcWrite(pwm_channel,22000);
		delay(700);
		ledcWrite(pwm_channel,0);
		delay(800);
		ledcWrite(pwm_channel,27000);
		delay(700);
		ledcWrite(pwm_channel,0);
		delay(800);
		ledcWrite(pwm_channel,33000);
		delay(700);
		ledcWrite(pwm_channel,0);
		delay(800);
		ledcWrite(pwm_channel,50000);
		delay(700);
		ledcWrite(pwm_channel,0);
		delay(300);
		

  
  }
	
	count=50;//Display immediatly
    delay(keyWait); 
	
  }

  //value adjust commande
  if( button_tm == 2|| button_tm == 16|| button_tm == 4|| button_tm == 1 || button_tm == 64){
    count=4;//little improvment
    switch (button_tm){
      case 2:
        currentRef=currentRef+100;
        break;
      case 16:
        if (currentRef>99) currentRef=currentRef-100;
        else currentRef=0;
        break;
      case 4:
        currentRef=currentRef+10;
        break;
      case 1:
        if (currentRef>9) currentRef=currentRef-10;
        break;
      case 64:
        currentRef=0;
        break;
    }
	
	EventPwmChange = true;
    if(currentRef==0) {//fast current shutdown :: compensator bypassing
    reg=downLimit;}
	
	count=50;//Display immediatly
	delay(keyWait); 
  }
  
  //voltage digital filtre
  adcValue = analogRead(adc_voltage);
  valu1=valu2;
  valu2=valu3;
  valu3=valu4;
  valu4=valu5;
  valu5=valu6;
  valu6=valu7;
  valu7=valu8;
  valu8=valu9;
  valu9=valu10;
  valu10=adcValue;
  avg=(valu1+valu2+valu3+valu4+valu5+valu6+valu7+valu8+valu9+valu10)/10;
  //Serial.print("Vout adc = " );Serial.println(avg );

  //current digital filtre :: sampling
  valu1Ic = analogRead(adcCurrentPin);delay(microd);
  valu2Ic = analogRead(adcCurrentPin);delay(microd);
  valu3Ic = analogRead(adcCurrentPin);delay(microd);
  //valu1Ic=valu2Ic;
  //valu2Ic=valu3Ic;
  //valu3Ic=valu4Ic;
  //valu4Ic=valu5Ic;//i
  //valu5Ic=sensorValue;//i
  avgIc=(valu1Ic+valu2Ic+valu3Ic+valu4Ic+valu5Ic+avgIc)/4;
  
  //ESP32 ADC OFFSET ISSUE FIXING
  int Ioffset_2 = 56;
  if(avgIc < 5) Ioffset_2=0;
  current=(avgIc*alfa+Ioffset_2);

  errorCurrent=current-currentRef;
  shift = 10;
  
  //compensator:
  if(errorCurrent < shift && errorCurrent > -1*shift){
	//digitalWrite(green_LED, LOW);   
	delta = 1;}
	
  if(errorCurrent <= 1 && errorCurrent >= -1){//the minimal :: no shift 
	//digitalWrite(green_LED, LOW);   
	delta = 0;}

  if(errorCurrent >= shift || errorCurrent <= -1*shift){
	//digitalWrite(green_LED, HIGH);  
	delta = 25;}
	
  if(errorCurrent >= 4*shift || errorCurrent <= -4*shift){
	//digitalWrite(green_LED, HIGH);   
	delta = 50;}

  if(current > currentRef && reg > (downLimit + delta)){
	reg=reg-delta;}

  if(current < currentRef && reg < (upLimit-delta)){
	reg=reg+delta;}
	
  if(currentRef==0){
	reg=downLimit;}
	
  //IDLE
  if (!chargingState) reg=downLimit;
  
  if(chargingState)digitalWrite(green_LED, LOW); 
  else digitalWrite(green_LED, HIGH); 
  
  // if (manual)ledcWrite(pwm_channel,currentRef*10);
  // else ledcWrite(pwm_channel,reg);

  
  ledcWrite(pwm_channel,reg);

  //Display Section :: current and  voltage 
  count++;
 
  if(count > 10 ){
    count=0;

    Iout=ViewCurrent(avgIc);
	Vout=ViewVoltage(avg)-Iout*0.001*0.39;
    Serial.println(currentRef);
    Serial.print("reg : ");Serial.print(reg);Serial.print("@@ delta: ");Serial.println(delta);
    //Serial.print("Ouput Power = ");Serial.print(Iout*Vout);Serial.println(" W");
    //Serial.print("PNP Power = ");Serial.print(Iout*(Vcpnp-Vout));Serial.println(" W");
    //Serial.print("battery voltage = ");Serial.print(Vout-Iout*0.001*0.39);Serial.println(" V");
	
    if (viewMode == 10){
      disp.displayNumber(Iout);
    }
    
    if(Vout < 99.99 && viewMode == 11) {
    disp.displayNumberDot(Vout);
    }

    if (viewMode == 12){//Iref
      disp.displayNumber(currentRef);
    }
  }
  
}

double ViewVoltage (double val){
	double tmpv;
	//Serial.print("Vout = " );Serial.print(val*1.203*0.001);Serial.print(" V / " );Serial.println(val);
	if(val< 20){ 
		tmpv = 0;
	}else if( val<= 3089){
		tmpv = val*0.881*0.001+1.162;
	}else{
		tmpv = val*0.881*0.001+1.162;
	}
	return tmpv;
}
double ViewCurrent (int val0){
	int Ioffset = 56;
	if(val0<5) Ioffset=0;
	//Serial.print("Iout = " );Serial.print(val0*alfa+Ioffset);Serial.print(" mA / ");Serial.println(val0);
	//Serial.print("adc current: ");Serial.println(analogRead(adcCurrentPin));
	return (val0*alfa+Ioffset);
}
