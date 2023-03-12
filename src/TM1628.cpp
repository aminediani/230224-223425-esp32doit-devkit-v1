
#if defined(ARDUINO) && ARDUINO >= 100
	#include "Arduino.h"
#else
	#include "WProgram.h"
#endif

#include "TM1628.h"
byte _curpos = 0x00;

byte buffer[14] = {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00};
const byte seg_addr[7]={0x00,0x01,0x07,0x02,0x03,0x04,0x05};//no bit of digital segments
//                       SG0  SG1  SG2  SG3  SG4  SG5  SG6  SG7  DVD  VCD  MP3  PLY  PAU  PBC  RET  DTS  DDD  CL1  CL2   //name   -|
const byte led_addr[19]={0x03,0x0B,0x0D,0x07,0x05,0x09,0x0D,0x01,0x01,0x03,0x05,0x09,0x0b,0x07,0x00,0x08,0x06,0x02,0x04};//adress -| for the signs's and disc's leds
const byte led_val[19]= {0x01,0x01,0x01,0x01,0x01,0x01,0x02,0x01,0x02,0x02,0x02,0x02,0x02,0x02,0x40,0x40,0x40,0x40,0x40};//byte   -|

const int delay_max1 = 1;
int ij =0;

TM1628::TM1628(byte _dio_pin, byte _clk_pin, byte _stb_pin) {
  this->_dio_pin = _dio_pin;
  this->_clk_pin = _clk_pin;
  this->_stb_pin = _stb_pin;

  pinMode(_dio_pin, OUTPUT);
  pinMode(_clk_pin, OUTPUT);
  pinMode(_stb_pin, OUTPUT);

  digitalWrite(_stb_pin, HIGH);
  digitalWrite(_clk_pin, HIGH);

  sendCommand(0x40);
  sendCommand(0x80);

  digitalWrite(_stb_pin, LOW);
  send(0xC0);
  clear();
  digitalWrite(_stb_pin, HIGH);
}

void TM1628::begin(boolean active = true, byte intensity = 1) {
  sendCommand(0x80 | (active ? 8 : 0) | _min(7, intensity));
}

void TM1628::update() {
  for (int i=0; i<14; i++)
    sendData(i, buffer[i]);
}

void TM1628::setTime(int hour,int min,int sec) {

	if (hour >= 100) setSeg(0, (hour/100));
	if (hour >= 10) setSeg(1, (hour%100)/10);
	setSeg(2, (hour%100)%10);
	setSeg(3, (min/10));
	setSeg(4, (min%10));
	setSeg(5, (sec/10));
	setSeg(6, (sec%10));
	update();
}
void TM1628::displayNumber(int num){
  clearDisplay();
  int j=0;
  if (num==0){
    sendData(0x06, NUMBER_FONT[0]);///Serial.println("null");
  }else{
    while(num>0 && j< 7){
      int digit = num%10;
      num/=10;//Serial.println(digit);
      sendData(0x06-j, NUMBER_FONT[digit]);
      j=j+2;
    }
  }
    
}

void TM1628::displayNumberDot(double numd){
  //99.99
  clearDisplay();
  sendData(0x08, 0b11111111);
  if(numd < 1.00){
    //sendData(0x06, NUMBER_FONT[0]);
    sendData(0x04, NUMBER_FONT[0]);
    sendData(0x02, NUMBER_FONT[0]);
  }
  numd=numd*100;
  int num=000;
  num=numd;
  int j=0;
  if (num==0){
    sendData(0x06, NUMBER_FONT[0]);///Serial.println("null");
  }else{
    while(num>0 && j< 7){
      int digit = num%10;
      num/=10;//Serial.println(digit);
      sendData(0x06-j, NUMBER_FONT[digit]);
      j=j+2;
    }
  }
}
void TM1628::clearDisplay(){
  sendData(0x08, 0b00000000);//dote

  sendData(0x00, 0b00000000);
  sendData(0x02, 0b00000000);
  sendData(0x04, 0b00000000);
  sendData(0x06, 0b00000000);
}


void TM1628::clear() {
	for (int i=0; i<14; i++)
		buffer[i]=0x00;
	_curpos=0x00;
	update();
}

byte TM1628::getButtons() {
  byte keys = 0;

  digitalWrite(_stb_pin, LOW);
  send(0x42);
  for (int i = 0; i < 5; i++) {
    keys = keys|receive() << i;
	//Serial.print("keys bin: "); Serial.println(keys,BIN);
  }
  digitalWrite(_stb_pin, HIGH);

  return keys;
}

byte TM1628::getButtonB(){	// Keyscan data on the TM1628 is 2x10 keys, received as an array of 5 bytes (same as TM1668).
	// Of each byte the bits B0/B3 and B1/B4 represent status of the connection of K1 and K2 to KS1-KS10
	// Byte1[0-1]: KS1xK1, KS1xK2
	// The return value is a 32 bit value containing button scans for both K1 and K2, the high word is for K2 and the low word for K1.
  word keys_K1 = 0;
  word keys_K2 = 0;
  byte received;

  digitalWrite(_stb_pin, LOW);
  send(0x42	);		// send read buttons command
  for (int i = 0; i < 5; i++) {
  	received=receive();
	//_BV(bit) (1<<(bit))
    keys_K1 |= (( (received&_BV(0))     | ((received&_BV(3))>>2)) << (2*i));			// bit 0 for K1/KS1 and bit 3 for K1/KS2
    keys_K2 |= ((((received&_BV(1))>>1) | ((received&_BV(4))>>3)) << (2*i));			// bit 1 for K2/KS1 and bit 4 for K2/KS2
  }
  digitalWrite(_stb_pin, HIGH);
  //Serial.print("KA"); Serial.println(keys_K1,BIN);
  //Serial.print("KB"); Serial.print(keys_K2,BIN);

  return(keys_K2<<4 | keys_K1);
}

size_t TM1628::write(byte chr){
	if(_curpos<0x07) {
		setChar(_curpos, chr);
		_curpos++;
	}
}

void TM1628::setCursor(byte pos){
	_curpos = pos;
}
/*********** mid level  **********/
void TM1628::sendData(byte addr, byte data) {
  sendCommand(0x44);
  digitalWrite(_stb_pin, LOW);
  send(0xC0 | addr);
  send(data);
  digitalWrite(_stb_pin, HIGH);
}

void TM1628::sendCommand(byte data) {
  digitalWrite(_stb_pin, LOW);
  send(data);
  digitalWrite(_stb_pin, HIGH);
}

void TM1628::setSeg(byte addr, byte num) {
  for(int i=0; i<7; i++){
      bitWrite(buffer[i*2], seg_addr[addr], bitRead(NUMBER_FONT[num],i));
    }
}

void TM1628::setChar(byte addr, byte chr) {
  for(int i=0; i<7; i++){
      bitWrite(buffer[i*2], seg_addr[addr], bitRead(FONT_DEFAULT[chr - 0x20],i));
    }
	update();
}
/************ low level **********/
void TM1628::send(byte data) {
  for (int i = 0; i < 8; i++) {
	  		//delay(1);
		delayMicroseconds(delay_max1);
    digitalWrite(_clk_pin, LOW);
		//delay(1);***
		delayMicroseconds(delay_max1);
    digitalWrite(_dio_pin, data & 1 ? HIGH : LOW);
		//delay(1);
		delayMicroseconds(delay_max1);
    data >>= 1;
    digitalWrite(_clk_pin, HIGH);
		//delay(1);
		delayMicroseconds(delay_max1);
  }
}

byte TM1628::receive() {
  byte temp = 0;

  // Pull-up on
  pinMode(_dio_pin, INPUT);
  digitalWrite(_dio_pin, HIGH);
  	//delay(1);
	delayMicroseconds(delay_max1);

  for (int i = 0; i < 8; i++) {
    temp >>= 1;
		//delay(1);
		delayMicroseconds(delay_max1);
    digitalWrite(_clk_pin, LOW);
		//delay(1);
		delayMicroseconds(delay_max1);
    if (digitalRead(_dio_pin)) {
      temp |= 0x80;
    }
		//delay(1);
		delayMicroseconds(delay_max1);
    digitalWrite(_clk_pin, HIGH);
		//delay(1);
		delayMicroseconds(delay_max1);
  }

  // Pull-up off
  pinMode(_dio_pin, OUTPUT);
		//delay(1);
		delayMicroseconds(delay_max1);
  digitalWrite(_dio_pin, LOW);
		//delay(1);
		delayMicroseconds(delay_max1);
//Serial.print("temp bin: "); Serial.println(temp,BIN);
  return temp;
}

