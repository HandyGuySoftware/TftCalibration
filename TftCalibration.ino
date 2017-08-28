/*
 Name:		TftCalibration.ino
 Created:	11/23/2016 8:26:03 PM
 Author:	parents
*/

#if defined(ARDUINO) && ARDUINO >= 100
#include <RA8875.h>
#include <SPI.h>
#include <EEPROM.h>
#include "arduino.h"
#include "TftCalibration.h"
#else
#include "WProgram.h"
#endif

// Definitions for connecting the RA8875 Board
// Library only supports hardware SPI at this time
// Connect SCLK to UNO Digital #13 (Hardware SPI clock)
// Connect MISO to UNO Digital #12 (Hardware SPI MISO)
// Connect MOSI to UNO Digital #11 (Hardware SPI MOSI)
#define RA8875_INT 2
#define RA8875_CS 10
#define RA8875_RESET 9

#define EEPROM_CALIBRATION_LOCATION	100	// Calibration settings

RA8875 tft = RA8875(RA8875_CS, RA8875_RESET);  // 800x600 TFT Display 

//This function will write a 4 byte (32bit) long to the eeprom at
//the specified address to address + 3.
void EEPROMWritelong(int address, int32_t value)
{
	//Decomposition from a long to 4 bytes by using bitshift.
	//One = Most significant -> Four = Least significant byte

	byte four = (value & 0xFF);
	byte three = ((value >> 8) & 0xFF);
	byte two = ((value >> 16) & 0xFF);
	byte one = ((value >> 24) & 0xFF);

	// Write the 4 bytes into the eeprom memory. Update function only writes if value was updated
	EEPROM.update(address, one);
	EEPROM.update(address + 1, two);
	EEPROM.update(address + 2, three);
	EEPROM.update(address + 3, four);
}

/**************************************************************************
@brief Calculates the difference between the touch screen and the actual screen co-ordinates, taking into account misalignment
and any physical offset of the touch screen.
**************************************************************************/
int setCalibrationMatrix(tsPoint_t * displayPtr, tsPoint_t * screenPtr, tsMatrix_t * matrixPtr)
{
	int  retValue = 0;

	matrixPtr->Divider = ((screenPtr[0].x - screenPtr[2].x) * (screenPtr[1].y - screenPtr[2].y)) -
		((screenPtr[1].x - screenPtr[2].x) * (screenPtr[0].y - screenPtr[2].y));

	if (matrixPtr->Divider == 0)
	{
		retValue = -1;
	}
	else
	{
		matrixPtr->An = ((displayPtr[0].x - displayPtr[2].x) * (screenPtr[1].y - screenPtr[2].y)) -
			((displayPtr[1].x - displayPtr[2].x) * (screenPtr[0].y - screenPtr[2].y));
		matrixPtr->Bn = ((screenPtr[0].x - screenPtr[2].x) * (displayPtr[1].x - displayPtr[2].x)) -
			((displayPtr[0].x - displayPtr[2].x) * (screenPtr[1].x - screenPtr[2].x));
		matrixPtr->Cn = (screenPtr[2].x * displayPtr[1].x - screenPtr[1].x * displayPtr[2].x) * screenPtr[0].y +
			(screenPtr[0].x * displayPtr[2].x - screenPtr[2].x * displayPtr[0].x) * screenPtr[1].y +
			(screenPtr[1].x * displayPtr[0].x - screenPtr[0].x * displayPtr[1].x) * screenPtr[2].y;
		matrixPtr->Dn = ((displayPtr[0].y - displayPtr[2].y) * (screenPtr[1].y - screenPtr[2].y)) -
			((displayPtr[1].y - displayPtr[2].y) * (screenPtr[0].y - screenPtr[2].y));
		matrixPtr->En = ((screenPtr[0].x - screenPtr[2].x) * (displayPtr[1].y - displayPtr[2].y)) -
			((displayPtr[0].y - displayPtr[2].y) * (screenPtr[1].x - screenPtr[2].x));
		matrixPtr->Fn = (screenPtr[2].x * displayPtr[1].y - screenPtr[1].x * displayPtr[2].y) * screenPtr[0].y +
			(screenPtr[0].x * displayPtr[2].y - screenPtr[2].x * displayPtr[0].y) * screenPtr[1].y +
			(screenPtr[1].x * displayPtr[0].y - screenPtr[0].x * displayPtr[1].y) * screenPtr[2].y;
	}
	return(retValue);
}


/**************************************************************************/
/*!
@brief  Waits for a touch event
*/
/**************************************************************************/
void waitForTouchEvent(RA8875* disp, tsPoint_t * point)
{
	uint16_t x, y;

	/* Clear the touch data object and placeholder variables */
	memset(point, 0, sizeof(tsPoint_t));

	disp->touchEnable(true);

	/* Clear any previous interrupts to avoid false buffered reads */
	disp->touchReadAdc(&x, &y);
	delay(1);

	/* Wait around for a new touch event (INT pin goes low) */
	while (digitalRead(RA8875_INT)) {}

	/* Make sure this is really a touch event */
	if (disp->touched())
	{
		// We're reading the raw register data, not the Sumotoy calibrated coordinates.
		// The Sumotoy calibration routine was highly inaccurate,
		// So we're using the Adafruit calibration code instead
		disp->touchReadAdc(&x, &y);
		point->x = x;
		point->y = y;
		Serial.print("Raw Point: x="); Serial.print(x); Serial.print(" Y="); Serial.println(y);
	}
	else
	{
		point->x = 0;
		point->y = 0;
	}
}

//**************************************************************************/
/*!
@brief  Renders the calibration screen with an appropriately
placed test point and waits for a touch event
*/
/**************************************************************************/
tsPoint_t renderCalibrationScreen(RA8875* disp, uint16_t x, uint16_t y, uint16_t radius)
{
	tsPoint_t point = { 0, 0 };
	bool valid = false;

	disp->fillWindow(RA8875_BLACK);
	disp->fillWindow(RA8875_WHITE);

	// Draw some explanatory text
	disp->setFont(INT);
	disp->setTextColor(RA8875_BLACK, RA8875_WHITE);
	disp->setFontScale(3);
	disp->setCursor(80, 50); disp->print(F(" Screen Calibration "));
	disp->setCursor(115, 125); disp->print(F(" Press the Circle "));

	// Draw the calibration circle on the screen
	disp->drawCircle(x, y, radius + 2, RA8875_BLACK);
	disp->fillCircle(x, y, radius + 2, RA8875_BLACK);

	/* Keep polling until the TS event flag is valid */
	valid = false;
	while (!valid)
	{
		waitForTouchEvent(disp, &point);
		if (point.x || point.y)
		{
			valid = true;
		}
	}

	return point;
}

/**************************************************************************/
/*!
@brief  Starts the screen calibration process.  Each corner will be
tested, meaning that each boundary (top, left, right and
bottom) will be tested twice and the readings averaged.
*/
/**************************************************************************/
void tsCalibrate(RA8875* disp, tsMatrix_t *matrix)
{
	// Screen Point references used in screen calibration routines
	tsPoint_t       _tsLCDPoints[3];
	tsPoint_t       _tsTSPoints[3];
	//tsMatrix_t      _tsMatrix;

	tsPoint_t data;

	// Screen not yet calibrated. Create new calibration data
	/* --------------- Welcome Screen --------------- */
	data = renderCalibrationScreen(disp, disp->width() / 2, disp->height() / 2, 5);
	delay(250);

	/* ----------------- First Dot ------------------ */
	// 10% over and 10% down
	data = renderCalibrationScreen(disp, disp->width() / 10, disp->height() / 10, 5);
	_tsLCDPoints[0].x = disp->width() / 10;
	_tsLCDPoints[0].y = disp->height() / 10;
	_tsTSPoints[0].x = data.x;
	_tsTSPoints[0].y = data.y;
	delay(250);

	/* ---------------- Second Dot ------------------ */
	// 50% over and 90% down
	data = renderCalibrationScreen(disp, disp->width() / 2, disp->height() - disp->height() / 10, 5);
	_tsLCDPoints[1].x = disp->width() / 2;
	_tsLCDPoints[1].y = disp->height() - disp->height() / 10;
	_tsTSPoints[1].x = data.x;
	_tsTSPoints[1].y = data.y;
	delay(250);

	/* ---------------- Third Dot ------------------- */
	// 90% over and 50% down
	data = renderCalibrationScreen(disp, disp->width() - disp->width() / 10, disp->height() / 2, 5);
	_tsLCDPoints[2].x = disp->width() - disp->width() / 10;
	_tsLCDPoints[2].y = disp->height() / 2;
	_tsTSPoints[2].x = data.x;
	_tsTSPoints[2].y = data.y;
	delay(250);

	/* Clear the screen */
	disp->fillWindow(RA8875_WHITE);

	// Do matrix calculations for calibration and store to EEPROM
	//setCalibrationMatrix(&_tsLCDPoints[0], &_tsTSPoints[0], &_tsMatrix);
	setCalibrationMatrix(&_tsLCDPoints[0], &_tsTSPoints[0], matrix);

	Serial.println("0 Degree Matrix Code:"); Serial.println("");
	Serial.println("typedef struct");
	Serial.println("{");
	Serial.println("int32_t An,");
	Serial.println("\tBn,");
	Serial.println("\tCn,");
	Serial.println("\tDn,");
	Serial.println("\tEn,");
	Serial.println("\tFn,");
	Serial.println("\tDivider;");
	Serial.println("} tsMatrix_t;");
	Serial.print("tsMatrix_t _tsMatrix = {");

	Serial.print(matrix->An); Serial.print(", ");
	Serial.print(matrix->Bn); Serial.print(", ");
	Serial.print(matrix->Cn); Serial.print(", ");
	Serial.print(matrix->Dn); Serial.print(", ");
	Serial.print(matrix->En); Serial.print(", ");
	Serial.print(matrix->Fn); Serial.print(", ");
	Serial.print(matrix->Divider); Serial.println("}; "); Serial.println("");

}

void writeEEPROMData(tsMatrix_t *matrix, int startLocation)
{
	// Store calibration data to EEPROM
	EEPROMWritelong(startLocation, (int32_t)matrix->An);
	EEPROMWritelong(startLocation + 4, (int32_t)matrix->Bn);
	EEPROMWritelong(startLocation + 8, (int32_t)matrix->Cn);
	EEPROMWritelong(startLocation + 12, (int32_t)matrix->Dn);
	EEPROMWritelong(startLocation + 16, (int32_t)matrix->En);
	EEPROMWritelong(startLocation + 20, (int32_t)matrix->Fn);
	EEPROMWritelong(startLocation + 24, (int32_t)matrix->Divider);
}

void setup() {
	Serial.begin(9600);
	tsMatrix_t matrix;

	Serial.println("Begin setup()");

	/* Initialize the TFT display */
	tft.begin(Adafruit_800x480);
	tft.useINT(RA8875_INT);
	tft.touchBegin();
	tft.enableISR(true);

	tft.setRotation(0);
	tsCalibrate(&tft, &matrix);
	writeEEPROMData(&matrix, EEPROM_CALIBRATION_LOCATION);

	tft.setRotation(2);
	tsCalibrate(&tft, &matrix);
	writeEEPROMData(&matrix, EEPROM_CALIBRATION_LOCATION + 28);

	Serial.println("Ending setup()");
}

// the loop function runs over and over again until power down or reset
void loop() {

 
}
