
#include <avr/io.h>
#include <util/delay.h>
#include <avr/interrupt.h>
#include <avr/pgmspace.h>
#include <avr/eeprom.h>
#include <stdlib.h>
#include <math.h>

#define byte unsigned char
#define true 1
#define false 0

#define DISPLAY_SEGMENT_0_PIN_OUTPUT   DDRB |= (1 << DDB2)
#define DISPLAY_SEGMENT_0_PIN_HIGH   PORTB |= (1 << PORTB2)
#define DISPLAY_SEGMENT_0_PIN_LOW   PORTB &= ~(1 << PORTB2)

#define DISPLAY_SEGMENT_1_PIN_OUTPUT   DDRA |= (1 << DDA5)
#define DISPLAY_SEGMENT_1_PIN_HIGH   PORTA |= (1 << PORTA5)
#define DISPLAY_SEGMENT_1_PIN_LOW   PORTA &= ~(1 << PORTA5)

#define DISPLAY_SEGMENT_2_PIN_OUTPUT   DDRB |= (1 << DDB1)
#define DISPLAY_SEGMENT_2_PIN_HIGH   PORTB |= (1 << PORTB1)
#define DISPLAY_SEGMENT_2_PIN_LOW   PORTB &= ~(1 << PORTB1)

#define DISPLAY_SEGMENT_3_PIN_OUTPUT   DDRA |= (1 << DDA4)
#define DISPLAY_SEGMENT_3_PIN_HIGH   PORTA |= (1 << PORTA4)
#define DISPLAY_SEGMENT_3_PIN_LOW   PORTA &= ~(1 << PORTA4)

#define DISPLAY_SEGMENT_4_PIN_OUTPUT   DDRA |= (1 << DDA6)
#define DISPLAY_SEGMENT_4_PIN_HIGH   PORTA |= (1 << PORTA6)
#define DISPLAY_SEGMENT_4_PIN_LOW   PORTA &= ~(1 << PORTA6)

#define DISPLAY_SEGMENT_5_PIN_OUTPUT   DDRB |= (1 << DDB0)
#define DISPLAY_SEGMENT_5_PIN_HIGH   PORTB |= (1 << PORTB0)
#define DISPLAY_SEGMENT_5_PIN_LOW   PORTB &= ~(1 << PORTB0)

#define DISPLAY_SEGMENT_6_PIN_OUTPUT   DDRA |= (1 << DDA7)
#define DISPLAY_SEGMENT_6_PIN_HIGH   PORTA |= (1 << PORTA7)
#define DISPLAY_SEGMENT_6_PIN_LOW   PORTA &= ~(1 << PORTA7)

#define BUTTON_PIN_INPUT   DDRA &= ~(1 << DDA3)
#define BUTTON_PIN_ANALOG 3

#define PORT_0_PIN_OUTPUT   DDRA |= (1 << DDA0)
#define PORT_0_PIN_INPUT   DDRA &= ~(1 << DDA0)
#define PORT_0_PIN_HIGH   PORTA |= (1 << PORTA0)
#define PORT_0_PIN_LOW   PORTA &= ~(1 << PORTA0)
#define PORT_0_PIN_READ   (PINA & (1 << PINA0))
#define PORT_0_PIN_ANALOG 0

#define PORT_1_PIN_OUTPUT   DDRA |= (1 << DDA1)
#define PORT_1_PIN_INPUT   DDRA &= ~(1 << DDA1)
#define PORT_1_PIN_HIGH   PORTA |= (1 << PORTA1)
#define PORT_1_PIN_LOW   PORTA &= ~(1 << PORTA1)
#define PORT_1_PIN_READ   (PINA & (1 << PINA1))
#define PORT_1_PIN_ANALOG 1

#define PORT_2_PIN_OUTPUT   DDRA |= (1 << DDA2)
#define PORT_2_PIN_INPUT   DDRA &= ~(1 << DDA2)
#define PORT_2_PIN_HIGH   PORTA |= (1 << PORTA2)
#define PORT_2_PIN_LOW   PORTA &= ~(1 << PORTA2)
#define PORT_2_PIN_READ   (PINA & (1 << PINA2))
#define PORT_2_PIN_ANALOG 2

#define TEXT_START_INDEX 340
#define DDNC_COMMAND_START_INDEX 360
#define BRANCH_STACK_START_INDEX 380

const byte FONT_DATA_SET[] PROGMEM = {
	0x7C, 0x77, 0x12, 0x5D, 0x5B, 0x3A, 0x6B, 0x6F,
	0x52, 0x7F, 0x7B, 0x04, 0x08, 0x01, 0x13, 0x6D
};

byte dataBlock[400];
long currentTime = 0;
int currentTimeMicroseconds = 0;
int timeCorrectionFactor = -200;
byte currentKeystroke = 0;
byte pendingKeystroke = 0;
long keystrokeHoldTime;
// Machine state may be 3 for viewing text
// or 4 for editing text.
byte machineState = 0;
byte *requestText;
byte requestSelectedCharacter;
int requestIndex;
int ddncCommandAddress;
byte branchStackLevel;
byte branchStackIgnoreLevel;
int lastButtonAnalogOutput = 0;
int ddncEndAddress;

long longModulus(long number1, long number2)
{
	return number1 % number2;
}

void drawCharacterFromFontData(byte fontData)
{
	if (fontData & 0x40)
	{
		DISPLAY_SEGMENT_0_PIN_LOW;
	} else {
		DISPLAY_SEGMENT_0_PIN_HIGH;
	}
	if (fontData & 0x20)
	{
		DISPLAY_SEGMENT_1_PIN_LOW;
	} else {
		DISPLAY_SEGMENT_1_PIN_HIGH;
	}
	if (fontData & 0x10)
	{
		DISPLAY_SEGMENT_2_PIN_LOW;
	} else {
		DISPLAY_SEGMENT_2_PIN_HIGH;
	}
	if (fontData & 0x08)
	{
		DISPLAY_SEGMENT_3_PIN_LOW;
	} else {
		DISPLAY_SEGMENT_3_PIN_HIGH;
	}
	if (fontData & 0x04)
	{
		DISPLAY_SEGMENT_4_PIN_LOW;
	} else {
		DISPLAY_SEGMENT_4_PIN_HIGH;
	}
	if (fontData & 0x02)
	{
		DISPLAY_SEGMENT_5_PIN_LOW;
	} else {
		DISPLAY_SEGMENT_5_PIN_HIGH;
	}
	if (fontData & 0x01)
	{
		DISPLAY_SEGMENT_6_PIN_LOW;
	} else {
		DISPLAY_SEGMENT_6_PIN_HIGH;
	}
}

void drawCharacter(byte character)
{
	byte tempFontData = pgm_read_byte(FONT_DATA_SET + character);
	drawCharacterFromFontData(tempFontData);
}

int analogReadPin(byte pin)
{
    ADMUX &= 0xF0;
	ADMUX |= pin;
    ADCSRA |= (1 << ADSC);
    while(ADCSRA & (1 << ADSC))
	{
		
	}
	return ADC;
}

byte readKeys()
{
	int tempNumber = lastButtonAnalogOutput;
	while (true)
	{
		lastButtonAnalogOutput = analogReadPin(BUTTON_PIN_ANALOG);
		if (lastButtonAnalogOutput < tempNumber + 15 && lastButtonAnalogOutput > tempNumber - 15)
		{
			break;
		}
		tempNumber = lastButtonAnalogOutput;
		_delay_ms(4);
	}
	// Neither key: 0
	// Left key: 514
	// Right key: 336
	// Both keys: 615
	if (tempNumber < 168)
	{
		return 0;
	}
	if (tempNumber < 425)
	{
		return 1;
	}
	if (tempNumber < 564)
	{
		return 2;
	}
	return 3;
}

long getCurrentTime()
{
	long output = currentTime;
	while (output != currentTime)
	{
		output = currentTime;
	}
	return output;
}

// Result is stored in currentKeystroke.
void readKeystroke()
{
	byte tempKey = readKeys();
	byte keyIsHeld0 = tempKey & 0x02;
	byte keyIsHeld1 = tempKey & 0x01;
	if (pendingKeystroke == 0)
	{
		if (keyIsHeld0)
		{
			pendingKeystroke = 1;
			keystrokeHoldTime = getCurrentTime();
		}
		if (keyIsHeld1)
		{
			pendingKeystroke = 2;
			keystrokeHoldTime = getCurrentTime();
		}
	}
	if (pendingKeystroke == 1)
	{
		if (getCurrentTime() - keystrokeHoldTime > 800)
		{
			pendingKeystroke = 3;
			currentKeystroke = 3;
			keystrokeHoldTime = getCurrentTime();
		}
		if (!keyIsHeld0)
		{
			currentKeystroke = 1;
			pendingKeystroke = 0;
		}
		if (keyIsHeld1)
		{
			pendingKeystroke = 10;
			keystrokeHoldTime = getCurrentTime();
		}
	}
	if (pendingKeystroke == 2)
	{
		if (getCurrentTime() - keystrokeHoldTime > 800)
		{
			pendingKeystroke = 4;
			currentKeystroke = 4;
			keystrokeHoldTime = getCurrentTime();
		}
		if (!keyIsHeld1)
		{
			currentKeystroke = 2;
			pendingKeystroke = 0;
		}
		if (keyIsHeld0)
		{
			pendingKeystroke = 11;
			keystrokeHoldTime = getCurrentTime();
		}
	}
	if (pendingKeystroke == 3)
	{
		if (getCurrentTime() - keystrokeHoldTime > 200)
		{
			currentKeystroke = 3;
			keystrokeHoldTime = getCurrentTime();
		}
		if (!keyIsHeld0)
		{
			pendingKeystroke = 0;
		}
	}
	if (pendingKeystroke == 4)
	{
		if (getCurrentTime() - keystrokeHoldTime > 200)
		{
			currentKeystroke = 4;
			keystrokeHoldTime = getCurrentTime();
		}
		if (!keyIsHeld1)
		{
			pendingKeystroke = 0;
		}
	}
	if (pendingKeystroke == 5)
	{
		if (!keyIsHeld0)
		{
			pendingKeystroke = 0;
		}
		if (keyIsHeld1)
		{
			pendingKeystroke = 10;
			keystrokeHoldTime = getCurrentTime();
		}
	}
	if (pendingKeystroke == 6)
	{
		if (!keyIsHeld1)
		{
			pendingKeystroke = 0;
		}
		if (keyIsHeld0)
		{
			pendingKeystroke = 11;
			keystrokeHoldTime = getCurrentTime();
		}
	}
	if (pendingKeystroke == 7)
	{
		if (!keyIsHeld1)
		{
			pendingKeystroke = 0;
			currentKeystroke = 7;
		}
		if (keyIsHeld0)
		{
			pendingKeystroke = 10;
			keystrokeHoldTime = getCurrentTime();
		}
	}
	if (pendingKeystroke == 8)
	{
		if (!keyIsHeld0)
		{
			pendingKeystroke = 0;
			currentKeystroke = 8;
		}
		if (keyIsHeld1)
		{
			pendingKeystroke = 11;
			keystrokeHoldTime = getCurrentTime();
		}
	}
	if (pendingKeystroke == 9)
	{
		if (!keyIsHeld0 && !keyIsHeld1)
		{
			pendingKeystroke = 0;
		}
	}
	// User has pressed left then right.
	if (pendingKeystroke == 10)
	{
		if (getCurrentTime() - keystrokeHoldTime > 2000)
		{
			pendingKeystroke = 9;
			currentKeystroke = 9;
		}
		if (!keyIsHeld0)
		{
			pendingKeystroke = 7;
		}
		if (!keyIsHeld1)
		{
			currentKeystroke = 5;
			pendingKeystroke = 5;
		}
	}
	// User has pressed right then left.
	if (pendingKeystroke == 11)
	{
		if (getCurrentTime() - keystrokeHoldTime > 2000)
		{
			pendingKeystroke = 9;
			currentKeystroke = 9;
		}
		if (!keyIsHeld0)
		{
			currentKeystroke = 6;
			pendingKeystroke = 6;
		}
		if (!keyIsHeld1)
		{
			pendingKeystroke = 8;
		}
	}
}

byte getTextCharacter(byte *text, int index)
{
	byte tempData = *(text + (index >> 1));
	if ((index & 1) == 0)
	{
		return (tempData & 0xF0) >> 4;
	} else {
		return tempData & 0x0F;
	}
}

void setTextCharacter(byte *text, int index, byte character)
{
	int tempIndex = index >> 1;
	byte tempData = *(text + tempIndex);
	if ((index & 1) == 0)
	{
		tempData = (tempData & 0x0F) | (character << 4);
	} else {
		tempData = (tempData & 0xF0) | character;
	}
	*(text + tempIndex) = tempData;
}

float convertTextToFloat(byte *text, byte offset)
{
	float output = 0;
	float tempNumber = 1.0;
	byte hasReachedDecimalPoint = false;
	byte index = 0;
	while (true)
	{
		byte tempCharacter = getTextCharacter(text, index + offset);
		if (tempCharacter == 11)
		{
			hasReachedDecimalPoint = true;
		}
		if (tempCharacter > 12 && tempCharacter < 16)
		{
			break;
		}
		if (tempCharacter > 0 && tempCharacter < 11)
		{
			if (hasReachedDecimalPoint)
			{
				tempNumber *= 0.1;
			}
		}
		index += 1;
	}
	while (index > 0)
	{
		index -= 1;
		byte tempCharacter = getTextCharacter(text, index + offset);
		if (tempCharacter > 0 && tempCharacter < 11)
		{
			output += tempNumber * (tempCharacter - 1);
			tempNumber *= 10.0;
		}
		if (tempCharacter == 12)
		{
			output = 0 - output;
		}
	}
	return output;
}

void convertFloatToText(byte *text, float number)
{
	number += 0.0005;
	byte index = 0;
	if (number < 0)
	{
		setTextCharacter(text, index, 12);
		index += 1;
		number = 0 - number;
	}
	byte hasWrittenDecimalPoint = false;
	float tempNumber = 1.0;
	if (number < 1.0)
	{
		setTextCharacter(text, index, 1);
		index += 1;
		setTextCharacter(text, index, 11);
		index += 1;
		hasWrittenDecimalPoint = true;
		tempNumber *= 0.1;
		while (tempNumber > number)
		{
			setTextCharacter(text, index, 1);
			index += 1;
			tempNumber *= 0.1;
		}
	} else {
		while (tempNumber < number)
		{
			tempNumber *= 10.0;
		}
		tempNumber *= 0.1;
	}
	while (tempNumber > 0.0005 && index < 35)
	{
		if (tempNumber < 0.5 && !hasWrittenDecimalPoint)
		{
			setTextCharacter(text, index, 11);
			index += 1;
			hasWrittenDecimalPoint = true;
		}
		byte tempCharacter = 1;
		while (number >= 0)
		{
			number -= tempNumber;
			tempCharacter += 1;
		}
		if (number < 0)
		{
			number += tempNumber;
			tempCharacter -= 1;
		}
		setTextCharacter(text, index, tempCharacter);
		tempNumber *= 0.1;
		index += 1;
	}
	setTextCharacter(text, index, 15);
}

void drawRequestCharacter()
{
	if (machineState == 3)
	{
		byte tempCharacter = getTextCharacter(requestText, requestIndex);
		drawCharacter(tempCharacter);
	}
	if (machineState == 4)
	{
		drawCharacter(requestSelectedCharacter);
	}
}

void flashBlank()
{
	drawCharacterFromFontData(0);
	_delay_ms(70);
	drawRequestCharacter();
}

void viewText()
{
	byte clearDataBlockCount = 0;
	machineState = 3;
	while (true)
	{
		byte tempCharacter = getTextCharacter(requestText, requestIndex);
		drawCharacter(tempCharacter);
		while (true)
		{
			readKeystroke();
			if (currentKeystroke == 1 || currentKeystroke == 3)
			{
				if (requestIndex > 0)
				{
					requestIndex -= 1;
					flashBlank();
					currentKeystroke = 0;
				}
			}
			if (currentKeystroke == 2 || currentKeystroke == 4)
			{
				tempCharacter = getTextCharacter(requestText, requestIndex);
				if (tempCharacter != 15)
				{
					requestIndex += 1;
					flashBlank();
					currentKeystroke = 0;
				}
			}
			if (currentKeystroke == 5)
			{
				int tempCount = 0;
				while (tempCount < 400)
				{
					tempCharacter = getTextCharacter(requestText, requestIndex);
					if (tempCharacter == 14)
					{
						requestIndex += 1;
						break;
					}
					if (tempCharacter == 15)
					{
						break;
					}
					requestIndex += 1;
					tempCount += 1;
				}
				if (tempCount > 0)
				{
					flashBlank();
				} else {
					drawRequestCharacter();
				}
				currentKeystroke = 0;
			}
			if (currentKeystroke == 6)
			{
				byte numberOfNewlines = 0;
				int tempCount = 0;
				while (tempCount < 400 && requestIndex > 0)
				{
					tempCharacter = getTextCharacter(requestText, requestIndex);
					if (tempCharacter == 14)
					{
						numberOfNewlines += 1;
						if (numberOfNewlines > 1)
						{
							requestIndex += 1;
							break;
						}
					}
					requestIndex -= 1;
					tempCount += 1;
				}
				if (numberOfNewlines == 0 && requestIndex == 0)
				{
					clearDataBlockCount += 1;
					if (clearDataBlockCount > 4)
					{
						requestText[0] = 0xF0;
						requestIndex = 0;
						clearDataBlockCount = 0;
					}
					drawRequestCharacter();
				} else {
					flashBlank();
					clearDataBlockCount = 0;
				}
				currentKeystroke = 0;
			}
			if (currentKeystroke == 7)
			{
				return;
			}
			if (currentKeystroke == 8 || currentKeystroke == 9)
			{
				drawCharacterFromFontData(0);
				_delay_ms(200);
				return;
			}
		}
	}
}

void editText(byte includesWhitespace)
{
	machineState = 4;
	while (true)
	{
		requestSelectedCharacter = 0;
		drawCharacter(requestSelectedCharacter);
		while (true)
		{
			readKeystroke();
			if (currentKeystroke == 1 || currentKeystroke == 3)
			{
				if (requestSelectedCharacter < 2)
				{
					if (includesWhitespace)
					{
						requestSelectedCharacter = 14;
					} else {
						requestSelectedCharacter = 12;
					}
				} else {
					requestSelectedCharacter -= 1;
				}
				drawCharacter(requestSelectedCharacter);
				currentKeystroke = 0;
			}
			if (currentKeystroke == 2 || currentKeystroke == 4)
			{
				requestSelectedCharacter += 1;
				if (includesWhitespace)
				{
					if (requestSelectedCharacter > 14)
					{
						requestSelectedCharacter = 1;
					}
				} else {
					if (requestSelectedCharacter > 12)
					{
						requestSelectedCharacter = 1;
					}
				}
				drawCharacter(requestSelectedCharacter);
				currentKeystroke = 0;
			}
			if (currentKeystroke == 5)
			{
				int index = requestIndex;
				int tempCount = 0;
				while (getTextCharacter(requestText, index) != 15 && tempCount < 400)
				{
					index += 1;
					tempCount += 1;
				}
				while (index > requestIndex - 1)
				{
					byte tempCharacter = getTextCharacter(requestText, index);
					setTextCharacter(requestText, index + 1, tempCharacter);
					index -= 1;
				}
				setTextCharacter(requestText, requestIndex, requestSelectedCharacter);
				requestIndex += 1;
				flashBlank();
				currentKeystroke = 0;
			}
			if (currentKeystroke == 6)
			{
				if (requestIndex > 0)
				{
					int index = requestIndex - 1;
					int tempCount = 0;
					byte tempCharacter = getTextCharacter(requestText, index);
					while (tempCharacter != 15 && tempCount < 400)
					{
						tempCharacter = getTextCharacter(requestText, index + 1);
						setTextCharacter(requestText, index, tempCharacter);
						index += 1;
						tempCount += 1;
					}
					requestIndex -= 1;
					flashBlank();
				}
				currentKeystroke = 0;
			}
			if (currentKeystroke == 7)
			{
				return;
			}
			if (currentKeystroke == 8 || currentKeystroke == 9)
			{
				drawCharacterFromFontData(0);
				_delay_ms(200);
				return;
			}
		}
	}
}

void openText(byte includesWhitespace, byte shouldEdit)
{
	while (true)
	{
		if (shouldEdit)
		{
			currentKeystroke = 0;
			editText(includesWhitespace);
			if (currentKeystroke == 8)
			{
				currentKeystroke = 0;
				return;
			}
			if (currentKeystroke == 9)
			{
				return;
			}
		}
		shouldEdit = true;
		currentKeystroke = 0;
		viewText();
		if (currentKeystroke == 8)
		{
			currentKeystroke = 0;
			return;
		}
		if (currentKeystroke == 9)
		{
			return;
		}
	}
}

void editCode()
{
	int index = 0;
	while (index < sizeof(dataBlock))
	{
		byte tempData = eeprom_read_byte((byte *)index);
		dataBlock[index] = tempData;
		index += 1;
	}
	requestText = dataBlock;
	requestIndex = 0;
	while (true)
	{
		openText(true, false);
		if (currentKeystroke == 9)
		{
			currentKeystroke = 0;
			return;
		}
		eeprom_write_block(dataBlock, (byte *)0, sizeof(dataBlock));
		eeprom_busy_wait();
	}
}

float getMemoryFloat(int address)
{
	return *(float *)(dataBlock + (address << 2));
}

void setMemoryFloat(int address, float number)
{
	*(float *)(dataBlock + (address << 2)) = number;
}

float getFloatArg(byte which)
{
	return *(float *)(dataBlock + DDNC_COMMAND_START_INDEX + (which << 2));
}

byte getByteArg(byte which)
{
	float tempNumber = getFloatArg(which);
	return (byte)tempNumber;
}

float getFloatAtArg(byte which)
{
	int tempAddress = (int)(getFloatArg(which));
	return getMemoryFloat(tempAddress);
}

byte getByteAtArg(byte which)
{
	float tempNumber = getFloatAtArg(which);
	return (byte)tempNumber;
}

long getLongAtArg(byte which)
{
	float tempNumber = getFloatAtArg(which);
	return (long)tempNumber;
}

void setFloatAtArg(byte which, float number)
{
	int tempAddress = (int)(getFloatArg(which));
	setMemoryFloat(tempAddress, number);
}

void setByteAtArg(byte which, byte number)
{
	setFloatAtArg(which, (float)number);
}

void setLongAtArg(byte which, long number)
{
	setFloatAtArg(which, (float)number);
}

int getBranchStackValue()
{
	return *(int *)(dataBlock + BRANCH_STACK_START_INDEX + (branchStackLevel << 1));
}

void setBranchStackValue(int value)
{
	*(int *)(dataBlock + BRANCH_STACK_START_INDEX + (branchStackLevel << 1)) = value;
}

void runCode()
{
	srand(getCurrentTime());
	ddncCommandAddress = 0;
	branchStackLevel = 0;
	branchStackIgnoreLevel = 1;
	ddncEndAddress = 9001;
	while (true)
	{
		if (ddncCommandAddress >= ddncEndAddress)
		{
			return;
		}
		readKeystroke();
		if (currentKeystroke == 9)
		{
			currentKeystroke = 0;
			return;
		}
		
		int lastDdncCommandAddress = ddncCommandAddress;
		byte textIndex = 0;
		byte tempCount = 0;
		while (tempCount < 40)
		{
			byte tempData = eeprom_read_byte((byte *)((ddncCommandAddress >> 1)));
			byte tempCharacter = getTextCharacter(&tempData, ddncCommandAddress & 1);
			setTextCharacter(dataBlock + TEXT_START_INDEX, textIndex, tempCharacter);
			ddncCommandAddress += 1;
			textIndex += 1;
			if (tempCharacter == 14)
			{
				break;
			}
			if (tempCharacter == 15)
			{
				ddncEndAddress = ddncCommandAddress;
				break;
			}
			tempCount += 1;
		}
		if (!tempCount)
		{
			continue;
		}
		byte commandIndex = 0;
		textIndex = 0;
		tempCount = 0;
		while (true)
		{
			float tempNumber = convertTextToFloat(dataBlock + TEXT_START_INDEX, textIndex);
			*(float *)(dataBlock + DDNC_COMMAND_START_INDEX + (commandIndex << 2)) = tempNumber;
			commandIndex += 1;
			byte hasFoundEndOfCommand = false;
			while (true)
			{
				byte tempCharacter = getTextCharacter(dataBlock + TEXT_START_INDEX, textIndex);
				textIndex += 1;
				if (tempCharacter == 13)
				{
					break;
				}
				if (tempCharacter == 14 || tempCharacter == 15)
				{
					hasFoundEndOfCommand = true;
					break;
				}
				tempCount += 1;
				if (tempCount > 38)
				{
					hasFoundEndOfCommand = true;
					break;
				}
			}
			if (hasFoundEndOfCommand)
			{
				break;
			}
		}
		byte tempOpcode = getByteArg(0);
		if (branchStackLevel > branchStackIgnoreLevel - 1)
		{
			switch(tempOpcode)
			{
				case 62:
				case 63:
				case 65:
				{
					setBranchStackValue(-1);
					branchStackLevel += 1;
					break;
				}
				case 64:
				{
					branchStackLevel -= 1;
					int tempAddress = getBranchStackValue();
					if (tempAddress > -1)
					{
						ddncCommandAddress = tempAddress;
					}
					break;
				}
				default:
				{
					break;
				}
			}
		} else {
			switch (tempOpcode)
			{
				case 0:
				{
					float tempNumber = getFloatArg(1);
					setFloatAtArg(2, tempNumber);
					break;
				}
				case 1:
				{
					float tempNumber = getFloatAtArg(1);
					setFloatAtArg(2, tempNumber);
					break;
				}
				case 2:
				{
					float tempNumber = getMemoryFloat(getLongAtArg(1));
					setFloatAtArg(2, tempNumber);
					break;
				}
				case 3:
				{
					float tempNumber = getFloatAtArg(1);
					setMemoryFloat(getLongAtArg(2), tempNumber);
					break;
				}
				case 10:
				{
					long tempSource = getLongAtArg(1);
					byte tempLength = getByteAtArg(2);
					long tempDestination = getLongAtArg(3);
					byte index = 0;
					while (index < tempLength)
					{
						float tempNumber = getMemoryFloat(tempSource + index);
						setMemoryFloat(tempDestination + index, tempNumber);
						index += 1;
					}
					break;
				}
				case 11:
				{
					long tempSource1 = getLongAtArg(1);
					byte tempLength = getByteAtArg(2);
					long tempSource2 = getLongAtArg(3);
					byte tempResult = true;
					byte index = 0;
					while (index < tempLength)
					{
						float tempNumber1 = getMemoryFloat(tempSource1 + index);
						float tempNumber2 = getMemoryFloat(tempSource2 + index);
						if (tempNumber1 != tempNumber2)
						{
							tempResult = false;
							break;
						}
						index += 1;
					}
					setByteAtArg(4, tempResult);
					break;
				}
				case 12:
				{
					float tempNumber = getFloatAtArg(1);
					long tempSource = getLongAtArg(2);
					byte tempLength = getByteAtArg(3);
					long tempResult = -1;
					byte index = 0;
					while (index < tempLength)
					{
						float tempNumber2 = getMemoryFloat(tempSource + index);
						if (tempNumber == tempNumber2)
						{
							tempResult = index;
							break;
						}
						index += 1;
					}
					setLongAtArg(4, tempResult);
					break;
				}
				case 13:
				{
					float tempNumber = getFloatAtArg(1);
					long tempDestination = getLongAtArg(2);
					byte tempLength = getByteAtArg(3);
					byte index = 0;
					while (index < tempLength)
					{
						setMemoryFloat(tempDestination + index, tempNumber);
						index += 1;
					}
					break;
				}
				case 20:
				{
					float tempNumber = getFloatAtArg(1) + getFloatAtArg(2);
					setFloatAtArg(3, tempNumber);
					break;
				}
				case 21:
				{
					float tempNumber = getFloatAtArg(1) - getFloatAtArg(2);
					setFloatAtArg(3, tempNumber);
					break;
				}
				case 22:
				{
					float tempNumber = getFloatAtArg(1) * getFloatAtArg(2);
					setFloatAtArg(3, tempNumber);
					break;
				}
				case 23:
				{
					float tempNumber = getFloatAtArg(1) / getFloatAtArg(2);
					setFloatAtArg(3, tempNumber);
					break;
				}
				case 24:
				{
					long tempNumber = longModulus(getLongAtArg(1), getLongAtArg(2));
					setLongAtArg(3, tempNumber);
					break;
				}
				case 30:
				{
					float tempNumber = getFloatAtArg(1) + 1;
					setFloatAtArg(1, tempNumber);
					break;
				}
				case 31:
				{
					float tempNumber = getFloatAtArg(1) - 1;
					setFloatAtArg(1, tempNumber);
					break;
				}
				case 32:
				{
					long tempNumber = getLongAtArg(1);
					setLongAtArg(2, tempNumber);
					break;
				}
				case 33:
				{
					long tempNumber = ((long)(rand()) << 16) | rand();
					tempNumber = longModulus(tempNumber, getLongAtArg(1));
					setLongAtArg(2, tempNumber);
					break;
				}
				case 34:
				{
					float tempNumber = sin(getFloatAtArg(1));
					setFloatAtArg(2, tempNumber);
					break;
				}
				case 40:
				{
					float tempNumber1 = getFloatAtArg(1);
					float tempNumber2 = getFloatAtArg(2);
					byte tempResult = (tempNumber1 >= tempNumber2 - 0.001 && tempNumber1 <= tempNumber2 + 0.001);
					setByteAtArg(3, tempResult);
					break;
				}
				case 41:
				{
					byte tempNumber = (getFloatAtArg(1) > getFloatAtArg(2) + 0.001);
					setByteAtArg(3, tempNumber);
					break;
				}
				case 50:
				{
					byte tempNumber = !(getFloatAtArg(1) > 0.001);
					setByteAtArg(2, tempNumber);
					break;
				}
				case 51:
				{
					byte tempNumber = (getFloatAtArg(1) > 0.001 || getFloatAtArg(2) > 0.001);
					setByteAtArg(3, tempNumber);
					break;
				}
				case 52:
				{
					byte tempNumber = (getFloatAtArg(1) > 0.001 && getFloatAtArg(2) > 0.001);
					setByteAtArg(3, tempNumber);
					break;
				}
				case 60:
				{
					setLongAtArg(1, ddncCommandAddress);
					break;
				}
				case 61:
				{
					ddncCommandAddress = getLongAtArg(1);
					break;
				}
				case 62:
				{
					if (getFloatAtArg(1) > 0.001)
					{
						branchStackIgnoreLevel += 1;
					}
					setBranchStackValue(-1);
					branchStackLevel += 1;
					break;
				}
				case 63:
				{
					if (!(getFloatAtArg(1) > 0.001))
					{
						branchStackIgnoreLevel += 1;
					}
					setBranchStackValue(-1);
					branchStackLevel += 1;
					break;
				}
				case 64:
				{
					branchStackIgnoreLevel -= 1;
					branchStackLevel -= 1;
					int tempAddress = getBranchStackValue();
					if (tempAddress > -1)
					{
						ddncCommandAddress = tempAddress;
					}
					break;
				}
				case 65:
				{
					if (getFloatAtArg(1) > 0.001)
					{
						branchStackIgnoreLevel += 1;
						setBranchStackValue(lastDdncCommandAddress);
					} else {
						setBranchStackValue(-1);
					}
					branchStackLevel += 1;
					break;
				}
				case 66:
				{
					branchStackIgnoreLevel -= 1;
					byte *tempPointer = dataBlock + BRANCH_STACK_START_INDEX + ((branchStackLevel - 1) << 1);
					while (*(int *)tempPointer < 0 && branchStackIgnoreLevel > 0)
					{
						branchStackIgnoreLevel -= 1;
						tempPointer -= 2;
					}
					*(int *)tempPointer = -1;
					break;
				}
				case 67:
				{
					setLongAtArg(1, ddncCommandAddress);
					setBranchStackValue(-1);
					branchStackLevel += 1;
					break;
				}
				case 68:
				{
					branchStackIgnoreLevel += 1;
					setBranchStackValue(ddncCommandAddress);
					branchStackLevel += 1;
					ddncCommandAddress = getLongAtArg(1);
					break;
				}
				case 70:
				{
					long tempNumber = getLongAtArg(1);
					while (currentTime != tempNumber)
					{
						currentTime = tempNumber;
					}
					break;
				}
				case 71:
				{
					long tempNumber = getCurrentTime();
					setLongAtArg(1, tempNumber);
					break;
				}
				case 72:
				{
					long wakeTime = getCurrentTime() + getLongAtArg(1);
					while (getCurrentTime() < wakeTime)
					{
						readKeystroke();
						if (currentKeystroke == 9)
						{
							currentKeystroke = 0;
							return;
						}
					}
					break;
				}
				case 73:
				{
					timeCorrectionFactor = getLongAtArg(1);
					break;
				}
				case 80:
				{
					byte tempNumber = getByteAtArg(1);
					drawCharacterFromFontData(tempNumber);
					break;
				}
				case 81:
				{
					byte tempNumber = getByteAtArg(1);
					drawCharacter(tempNumber);
					break;
				}
				case 82:
				{
					byte tempNumber = getByteAtArg(1);
					drawCharacter(tempNumber % 10 + 1);
					break;
				}
				case 83:
				{
					requestText = dataBlock + TEXT_START_INDEX;
					float tempNumber = getFloatAtArg(1);
					convertFloatToText(requestText, tempNumber);
					requestIndex = 0;
					openText(true, false);
					break;
				}
				case 84:
				{
					setByteAtArg(1, readKeys());
					break;
				}
				case 85:
				{
					currentKeystroke = 0;
					while (currentKeystroke == 0)
					{
						readKeystroke();
						if (currentKeystroke == 9)
						{
							currentKeystroke = 0;
							return;
						}
					}
					setByteAtArg(1, currentKeystroke);
					currentKeystroke = 0;
					break;
				}
				case 86:
				{
					requestText = dataBlock + TEXT_START_INDEX;
					*requestText = 0xF0;
					requestIndex = 0;
					openText(false, true);
					float tempNumber = convertTextToFloat(requestText, 0);
					setFloatAtArg(1, tempNumber);
					break;
				}
				case 90:
				{
					byte tempMode = getByteAtArg(1);
					byte tempPin = getByteAtArg(2);
					if (tempPin == 0)
					{
						if (tempMode)
						{
							PORT_0_PIN_OUTPUT;
						} else {
							PORT_0_PIN_INPUT;
						}
					}
					if (tempPin == 1)
					{
						if (tempMode)
						{
							PORT_1_PIN_OUTPUT;
						} else {
							PORT_1_PIN_INPUT;
						}
					}
					if (tempPin == 2)
					{
						if (tempMode)
						{
							PORT_2_PIN_OUTPUT;
						} else {
							PORT_2_PIN_INPUT;
						}
					}
					break;
				}
				case 91:
				{
					byte tempPin = getByteAtArg(1);
					if (tempPin == 0)
					{
						setByteAtArg(2, PORT_0_PIN_READ);
					}
					if (tempPin == 1)
					{
						setByteAtArg(2, PORT_1_PIN_READ);
					}
					if (tempPin == 2)
					{
						setByteAtArg(2, PORT_2_PIN_READ);
					}
					break;
				}
				case 92:
				{
					byte tempPin = getByteAtArg(1);
					if (tempPin == 0)
					{
						long tempNumber = analogReadPin(PORT_0_PIN_ANALOG);
						setLongAtArg(2, tempNumber);
					}
					if (tempPin == 1)
					{
						long tempNumber = analogReadPin(PORT_1_PIN_ANALOG);
						setLongAtArg(2, tempNumber);
					}
					if (tempPin == 2)
					{
						long tempNumber = analogReadPin(PORT_2_PIN_ANALOG);
						setLongAtArg(2, tempNumber);
					}
					break;
				}
				case 93:
				{
					byte tempValue = getByteAtArg(1);
					byte tempPin = getByteAtArg(2);
					if (tempPin == 0)
					{
						if (tempValue)
						{
							PORT_0_PIN_HIGH;
						} else {
							PORT_0_PIN_LOW;
						}
					}
					if (tempPin == 1)
					{
						if (tempValue)
						{
							PORT_1_PIN_HIGH;
						} else {
							PORT_1_PIN_LOW;
						}
					}
					if (tempPin == 2)
					{
						if (tempValue)
						{
							PORT_2_PIN_HIGH;
						} else {
							PORT_2_PIN_LOW;
						}
					}
					break;
				}
				default:
				{
					break;
				}
			}
		}
	}
}

int main(void)
{
	DISPLAY_SEGMENT_0_PIN_OUTPUT;
	DISPLAY_SEGMENT_1_PIN_OUTPUT;
	DISPLAY_SEGMENT_2_PIN_OUTPUT;
	DISPLAY_SEGMENT_3_PIN_OUTPUT;
	DISPLAY_SEGMENT_4_PIN_OUTPUT;
	DISPLAY_SEGMENT_5_PIN_OUTPUT;
	DISPLAY_SEGMENT_6_PIN_OUTPUT;
	BUTTON_PIN_INPUT;
	
	ADCSRA |= (1 << ADPS1);
	ADCSRA |= (1 << ADPS0);
	ADCSRA |= (1 << ADEN);
	
	TIMSK0 |= (1 << OCIE0A);
	TCCR0A |= (1 << WGM01);
	TCCR0B |= (1 << CS02);
	TCCR0B |= (1 << CS00);
	OCR0A = 32;
	sei();
	
	while (true)
	{
		editCode();
		runCode();
	}
	
	return 0;
}

ISR(TIM0_COMPA_vect)
{
	currentTime += 33;
	currentTimeMicroseconds += 792 + timeCorrectionFactor;
	while (currentTimeMicroseconds > 999)
	{
		currentTime += 1;
		currentTimeMicroseconds -= 1000;
	}
	while (currentTimeMicroseconds < 0)
	{
		currentTime -= 1;
		currentTimeMicroseconds += 1000;
	}
}

