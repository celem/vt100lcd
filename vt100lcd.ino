/*
 *      vt100lcd.ino
 * 
 *      Copyright 2012 Edward I. Comer <remocmail-1shot@yahoo.com>
 *      
 *      This program is free software; you can redistribute it and/or modify
 *      it under the terms of the GNU General Public License Version 3, as 
 * 		published by the Free Software Foundation. http://www.fsf.org
 *      
 *      This program is distributed in the hope that it will be useful,
 *      but WITHOUT ANY WARRANTY; without even the implied warranty of
 *      MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *      GNU General Public License for more details.
 *      
 *      You should have received a copy of the GNU General Public License
 *      along with this program; if not, write to the Free Software
 *      Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 *      MA 02110-1301, USA or browse to http://www.fsf.org
 * 
 * WARNING - WARNING - WARNING - WARNING - WARNING - WARNING - WARNING
 * WARNING: Pin assignments are per the Arduino-Tiny open source set of
 * ATtiny "cores". http://code.google.com/p/arduino-tiny/
 * Use of a different core may have different pin assignments.
 * WARNING - WARNING - WARNING - WARNING - WARNING - WARNING - WARNING
 * 
 * ATMEL ATTINY84 / ARDUINO
 *
 *                           +-\/-+
 *                     VCC  1|    |14  GND
 *        XTAL (D 10)  PB0  2|    |13  PA0  (D  0) AREF
 *        XTAL (D  9)  PB1  3|    |12  PA1  (D  1) 
 *       RESET         PB3  4|    |11  PA2  (D  2) 
 *  PWM  INT0  (D  8)  PB2  5|    |10  PA3  (D  3) 
 *  PWM        (D  7)  PA7  6|    |9   PA4  (D  4) 
 *  PWM        (D  6)  PA6  7|    |8   PA5  (D  5) PWM
 *                           +----+
 * 
 *  ATMEL ATTINY85 / ARDUINO
 *
 *                           +-\/-+
 *         RESET (D 5) PB5  1|    |8  Vcc
 *          Ain3 (D 3) PB3  2|    |7  PB2 (D 2) Ain1
 *          Ain2 (D 4) PB4  3|    |6  PB1 (D 1) pwm1
 *                     GND  4|    |5  PB0 (D 0) pwm0
 *                           +----+
 * 
 * 		Employs a state machine to parse special escape sequences. If
 * 		the character passed to parse is not special and the state
 * 		machine is not in a "special" state, then the character is returned
 * 		as passed. Otherwise, a zero value is returned.
 */
#include "vt100lcd.h"

#ifdef	ATTINY85
#include <LiquidCrystal_SR.h>
#else
#include <LiquidCrystal.h>
#endif

#ifdef SOFTSERIAL
#include <SoftwareSerial.h>
#define	SerialPort	SoftSerial
#else
#define	SerialPort	Serial
#endif
#include <ctype.h>

int parsechar(unsigned char current_char);

#ifdef ATTINY84
#define rxPin 		0	//ATtiny84 PA0 physical pin 13
#define txPin 		1	//ATtiny84 PA1 physical pin 12
#define	ledPin		2	//ATtiny84 PA2 physical pin 11
#define	LCD_RS_Pin	3	//ATtiny84 PA3 physical pin 10
#define	LCD_E_Pin	4	//ATtiny84 PA4 physical pin 9
#define	LCD_d4_Pin	5	//ATTiny84 PA5 physical pin 8
#define	LCD_d5_Pin	6	//ATTiny84 PA6 physical pin 7
#define	LCD_d6_Pin	7	//ATTiny84 PA7 physical pin 6
#define	LCD_d7_Pin	8	//ATTiny84 PA2 physical pin 5
#elif ATTINY85
#define rxPin 	0		//ATtiny85 PB0 physical pin 5
#define txPin 	1		//ATtiny85 PB1 physical pin 6
#define	LCDdataPin  2	//ATtiny85 PB2 physical pin 7
#define	LCDclockPin 3	//ATtiny85 PB3 physical pin 2
#define	ledPin	4		//ATtiny85 PB4 physical pin 3
#else
#define	ledPin		2	//ArduinoNano D2 physical pin 5
#define	LCD_RS_Pin	3	//ArduinoNano D3 physical pin 6
#define	LCD_E_Pin	4	//ArduinoNano D4 physical pin 7
#define	LCD_d4_Pin	5	//ArduinoNano D5 physical pin 8
#define	LCD_d5_Pin	6	//ArduinoNano D6 physical pin 9
#define	LCD_d6_Pin	7	//ArduinoNano D7 physical pin 10
#define	LCD_d7_Pin	8	//ArduinoNano D8 physical pin 11

#endif

// Software default is 16X2 HJ1602 LCD display
// lcd.begin() default is 16X4 JHD539 LCD display
#define	MAXCOLUMNS  16
#define	MAXLINES  	4	// Default Hardware to 4 Lines, software to 2

#define	NOTSPECIAL	1
#define	GOTESCAPE	2
#define	GOTBRACKET	3
#define	INNUM	4

// 0 based defines
#define LEFT_EDGE0 0
#define RIGHT_EDGE16COL_0 15
#define TOP_EDGE0 0
#define BOTTOM_EDGE2LINE_0 1
#define BOTTOM_EDGE4LINE_0 3

#define	DEFBAUD	9600

struct cursor { unsigned int row; unsigned int col; };

int current_state = NOTSPECIAL;
int previous_state = NOTSPECIAL;
int tmpnum;							// number accumulator
int n, c;
unsigned int num, row, col;
int bottom_edge0 = BOTTOM_EDGE2LINE_0;	//Startup default
int right_edge0 = RIGHT_EDGE16COL_0;	//Startup default
struct cursor cursor_pos = { 0, 0 }; // VT100 is 1 based but
struct cursor cursor_sav = { 0, 0 }; // cursor values 0 based

#ifdef ATTINY85
LiquidCrystal_SR lcd(LCDdataPin,LCDclockPin,TWO_WIRE);
#else
LiquidCrystal lcd(LCD_RS_Pin,LCD_E_Pin,LCD_d4_Pin, LCD_d5_Pin, LCD_d6_Pin, LCD_d7_Pin);
#endif

#ifdef SOFTSERIAL
SoftwareSerial SoftSerial =  SoftwareSerial(rxPin, txPin);
#endif

// Write char to LCD and update col position
void writelcd(int c) {
  lcd.write(c);
  cursor_pos.col = (right_edge0 > (col = cursor_pos.col + 1) ? col : right_edge0);
}

void setup() {
  pinMode(ledPin, OUTPUT);
  digitalWrite(ledPin, LOW);   		      // low activates LED
  lcd.begin(MAXCOLUMNS, MAXLINES);	      // set up the LCD's number of rows and columns:
  lcd.print(FILENME);		      		  // Print a message to the LCD.
  // set the cursor to column 0, line 1
  // (note: line 1 is the second row, 
  // since counting begins with 0):
  lcd.setCursor(0, 1);				//lcd.setCursor(column, row);
  lcd.print(COPYRIGHT);				// Print a message to the LCD.
  lcd.setCursor(0, 0);				//lcd.setCursor(column, row);

  // set the data rate for the Serial port
  SerialPort.begin(DEFBAUD);
  SerialPort.println("Init Serial Port");
}


void loop() {
#ifdef SOFTSERIAL
	SoftSerial.listen();
#endif
  // fetch softserial and send/parse to LCD
  if (SerialPort.available())
  {
    if((c = SerialPort.read()) > 0)
    {
      if(parsechar(c) > 0)
      {
        writelcd(c);
      }
    }
  }
}

void switchstate(int newstate) {	// TODO not sure prev-state needed
	previous_state = current_state;
	current_state = newstate;
}

void cursorDown()
{
	if(cursor_pos.row < bottom_edge0){
		n = cursor_pos.row++;
		lcd.setCursor(cursor_pos.col, cursor_pos.row);
	}
}

/*
 * call with char to parse for VT100 escape sequence.
 * Returns 0 for chars within a parse or the
 * passed char if the char is not special
 */
int parsechar(unsigned char current_char) {
	switch(current_state)
	{
		case NOTSPECIAL:
		if(current_char == 033) {	// Escape
			switchstate(GOTESCAPE);
			return(0);
		}
		else if(current_char == 015) {	// CR
			lcd.setCursor(cursor_pos.col=0, cursor_pos.row);
			return(0);
		}
		else if(current_char == 012) {	// LF
			cursorDown();
			return(0);
		}
		else {
			switchstate(NOTSPECIAL);
			return(current_char);
		}
		break;
		
		case GOTESCAPE:
		switch(current_char)
		{
			case '[':	// Bracket
			switchstate(GOTBRACKET);
			return(0);
			
			case 'D':	// Cursor Down
			cursorDown();
			break;
			
			case 'M':	// Cursor Up
			if(cursor_pos.row == TOP_EDGE0){
				break;	// already at top
			}
			lcd.setCursor(cursor_pos.col, cursor_pos.row -= 1);
			break;
						
			case 'E':	// Cursor down to col 1
			cursor_pos.col = 0;
			cursorDown();
			switchstate(NOTSPECIAL);
			return(0);
			
			case 'c':	// Reset
			lcd.clear();
			lcd.noCursor();
			cursor_pos.col = cursor_pos.row =0;
			switchstate(NOTSPECIAL);
			return(0);
			
			default:
			switchstate(NOTSPECIAL);
			return(current_char);
		}
		switchstate(NOTSPECIAL);
		return(0);
		
		/*
		 * Previous ESC[ should be followed by a decimal number,
		 * for curdor movement, keyboard arrow or a row number
		 */
		case GOTBRACKET:
		if(isdigit(current_char)){
			switchstate(INNUM);
			// accumulate number
			tmpnum = 0;
			tmpnum = tmpnum*10 + (current_char-'0');
			return(0);
		}
		else {	// Here if non-numeric char after bracket
			// Check for Keyboard Arrows
			switch(current_char) {
				case 'A':	// Keyboard UP Arrow
				if(cursor_pos.row == TOP_EDGE0){
				    break;	// already at top
				}
				lcd.setCursor(cursor_pos.col, cursor_pos.row -= 1);
				break;
				
				case 'B':	// Keyboard Down Arrow
				cursorDown();
				break;
				
				case 'C':	// Keyboard Right Arrow
				col = (right_edge0 > (col = cursor_pos.col + 1)) ? col : right_edge0;
				lcd.setCursor(cursor_pos.col=col, cursor_pos.row=row);
				break;
				
				case 'D':	// Keyboard Left Arrow
				col = (LEFT_EDGE0 < (col = cursor_pos.col - 1)) ? col : LEFT_EDGE0;
				lcd.setCursor(cursor_pos.col=col, cursor_pos.row=row);
				break;
				
				case 'H':	// Cursor to Home
				lcd.setCursor(cursor_pos.col=0, cursor_pos.row=0);
				break;
				
				case 'm':	// turn off attributes
				lcd.noCursor();
				break;
				
				case 's':	// Save cursor pos
				cursor_sav.col = cursor_pos.col;
				cursor_sav.row = cursor_pos.row;
				break;
				
				case 'u':	// Restore cursor pos
				cursor_pos.col = cursor_sav.col;
				cursor_pos.row = cursor_sav.row;
				lcd.setCursor(cursor_pos.col, cursor_pos.row);
				break;
				
				case '=':	// Set screen size
				switchstate(INNUM);
				// accumulate screen size number
				tmpnum = 0;
				return(0);	// discard '=' char
				
				
				default:	// ESC[ was fluke
				break;
			}
			switchstate(NOTSPECIAL); 
			return(0);
		}
		break;
		
		case INNUM: // intermediate number accumulation
		if(isdigit(current_char)){
			// accumulate number
			tmpnum = tmpnum*10 + (current_char-'0');
			return(0);	// stay in INNUM state
		}
		else {	// must be a post numeric delimiter or command
			switch(current_char){
				case ';':	//Delimiter between row, col
				tmpnum = (tmpnum > 0) ? tmpnum-1 : 0;	//set base 0
				row = (tmpnum > bottom_edge0) ? bottom_edge0 : tmpnum;
				tmpnum = 0;
				return(0);	// stay in INNUM state
				
				// Case for ESC [ num1 ; num2 H|f
				case 'H':	// Move cursor to r,c
				case 'f':	// ditto
				tmpnum = (tmpnum > 0) ? tmpnum-1 : 0;	//set base 0
				col = (tmpnum > right_edge0) ? right_edge0 : tmpnum;
				lcd.setCursor(cursor_pos.col=col, cursor_pos.row=row);
				break;
				
				// Case for ESC [ num A|F
				case 'A':	// Cursor up n lines
				case 'F':	// Cursor up n lines to col 1
				tmpnum = (tmpnum > 0) ? tmpnum : 1;	// min val of 1
				row = (TOP_EDGE0 < (row = cursor_pos.row - tmpnum)) ? row : TOP_EDGE0;
				lcd.setCursor(cursor_pos.col=(current_char=='A')?col:LEFT_EDGE0, cursor_pos.row=row);
				break;
				
				// Case for ESC [ num B|E
				case 'B':	// Cursor down n lines
				case 'E':	// Cursor down n lines to col 1
				tmpnum = (tmpnum > 0) ? tmpnum : 1;	// min val of 1
				row = (bottom_edge0 > (row = cursor_pos.row + tmpnum)) ? row : bottom_edge0;
				lcd.setCursor(cursor_pos.col=(current_char=='B')?col:LEFT_EDGE0, cursor_pos.row=row);
				break;
				
				// Case for ESC [ num C
				case 'C':	// Cursor right n chars
				tmpnum = (tmpnum > 0) ? tmpnum : 1;	// min val of 1
				col = (right_edge0 > (col = cursor_pos.col + tmpnum)) ? col : right_edge0;
				lcd.setCursor(cursor_pos.col=col, cursor_pos.row=row);
				break;
				
				// Case for ESC [ num D
				case 'D':	// Cursor left n chars
				tmpnum = (tmpnum > 0) ? tmpnum : 1;	// min val of 1
				col = (LEFT_EDGE0 < (col = cursor_pos.col - tmpnum)) ? col : LEFT_EDGE0;
				lcd.setCursor(cursor_pos.col=col, cursor_pos.row=row);
				break;
				
				// Case for ESC [ num G
				case 'G':	// Cursor to pos n on cur line
				tmpnum = (tmpnum > 0) ? tmpnum-1 : 0;	// base 0
				col = (tmpnum > right_edge0) ? right_edge0 : tmpnum;		
				lcd.setCursor(cursor_pos.col=col, cursor_pos.row);
				break;
				
				// Case for ESC [ 0|4 m
				case 'm':	// turn off attributes
				if(tmpnum == 0)
					lcd.noCursor();
				else if(tmpnum == 4)
					lcd.cursor();
				break;
				
				// Case for ESC [ num c
				case 'c':	// report terminal type
				SerialPort.print(FILENME);
				SerialPort.println(VERSION);
				break;
				
				// Case for ESC [ 2 J
				case 'J':	// Erase screen and home cursor
				if(tmpnum == 2){
					lcd.clear();
					lcd.noCursor();
					cursor_pos.col = cursor_pos.row =0;
				}
				break;
				
				// Case for ESC [ 0|1 q
				case 'q':	// LED Operation
				if(tmpnum == 0) {
					digitalWrite(ledPin, HIGH);
				} else {
					digitalWrite(ledPin, LOW); // Low activates LED
				}
				break;
				
				// case for ESC [ 5|6 n
				// NOTE: DSR response differs in format from VT100
				case 'n':	// Device Status Report  
				if(tmpnum == 5){	// Request DSR
					SerialPort.print("lines=");
					SerialPort.println(bottom_edge0+1, DEC);
				}
				else if(tmpnum == 6){	// Request active position
					SerialPort.print("col=");
					SerialPort.print(cursor_pos.col+1,DEC);
					SerialPort.print(", row=");
					SerialPort.println(cursor_pos.row+1, DEC);
				}
				break;
				
				// case for ESC [ = num h
				case 'h':	// Set screen size
				if(tmpnum == 4){
					bottom_edge0 = 3;
				}
				else
					bottom_edge0 = BOTTOM_EDGE2LINE_0;
				break;
				
				default:	// not supposed to happen
				switchstate(NOTSPECIAL);
				return(current_char);
				
			}
			switchstate(NOTSPECIAL);
			return(0);
		}
		
		default:	// not supposed to happen
		switchstate(NOTSPECIAL);
		return(current_char);
		break;
	}
	
}
