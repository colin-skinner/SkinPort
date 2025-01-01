//ADS1256 cpp file
/*
 Name:		ADS1256.cpp
 Created:	2022/07/14
 Author:	Curious Scientist
 Editor:	Notepad++
 Comment: Visit https://curiousscientist.tech/blog/ADS1256-custom-library
 Special thanks to Abraão Queiroz for spending time on the code and suggesting corrections for ESP32 microcontrollers!
*/


#include "ADS1256.h"
#include <SPI.h>

//----
volatile bool DRDY_value = false; //It is chosen to be false (HIGH) because that's the default state when a conversion is completed

#if defined(ESP32) //If the code is compiled for an ESP32 chip, we need a different definition for the ISR()
#pragma message "Compilation for ESP32" //This line will print the message in the terminal for the compilation
void IRAM_ATTR DRDY_ISR() 
{		
	DRDY_value = true; //Change the DRDY_value to true (LOW). (The ISR was fired by a FALLING signal)
}
#else
#pragma message "Compilation for NON-ESP32"
void DRDY_ISR() 
{
	DRDY_value = true; //Change the DRDY_value to true (LOW). (The ISR was fired by a FALLING signal)
}
#endif

void waitForDRDY() //A function that waits for the DRDY status to change via the ISR()
{		
	while (DRDY_value == false) {}
	noInterrupts();
	DRDY_value = false; //Reset the DRDY_value manually to false while disabling the interrupts
	interrupts();	
}

//----

//Constructor
ADS1256_Base::ADS1256_Base() {}

void ADS1256_Base::setup(const byte DRDY_pin, const byte RESET_pin, const byte SYNC_pin, const byte CS_pin, float VREF)
{	
	
	_DRDY_pin = DRDY_pin; 
	pinMode(_DRDY_pin, INPUT);	
	 
	if(RESET_pin !=0)
	{
	_RESET_pin = RESET_pin;
	pinMode(_RESET_pin, OUTPUT);
	}
	
	if(SYNC_pin != 0)
	{
	_SYNC_pin = SYNC_pin;
	pinMode(_SYNC_pin, OUTPUT);
	}
	
	_CS_pin = CS_pin;
	pinMode(_CS_pin, OUTPUT);
	
	_VREF = VREF;
}
	

//Initialization
void ADS1256_Base::InitializeADC(SPIClass* ads_spi)
{

  //Chip select LOW  
  digitalWrite(_CS_pin, LOW);
  
  //We do a manual chip reset on the ADS1256 - Datasheet Page 27/ RESET
  if(_RESET_pin != 0)
  {
  digitalWrite(_RESET_pin, LOW);
  delay(200);
  digitalWrite(_RESET_pin, HIGH); //RESET is set to high
  delay(1000);
  }
  
  //Sync pin is also treated if it is defined
  if(_SYNC_pin != 0)
  {
  digitalWrite(_SYNC_pin, HIGH); //RESET is set to high  
  }

  ADS_SPI = ads_spi;

  attachInterrupt(digitalPinToInterrupt(_DRDY_pin), DRDY_ISR, FALLING); //Make the interrupt fire on FALLING signal	

  //Applying arbitrary default values to speed up the starting procedure if the user just want to get quick readouts
  delay(200);
  writeRegister(STATUS_REG, B00110110); //BUFEN and ACAL enabled, Order is MSB, rest is read only
  delay(200);
  writeRegister(MUX_REG, B00000001); //MUX AIN0+AIN1
  delay(200);
  writeRegister(ADCON_REG, B00000000); //ADCON - CLK: OFF, SDCS: OFF, PGA = 0 (+/- 5 V);
  delay(200);
  writeRegister(DRATE_REG, B10000010); //100SPS 
  delay(200);
  sendDirectCommand(B11110000); //Offset and self-gain calibration
  delay(200);
  _isAcquisitionRunning = false; //MCU will be waiting to start a continuous acquisition
}

void ADS1256_Base::stopConversion() //Sending SDATAC to stop the continuous conversion
{	
	waitForDRDY(); //SDATAC should be called after DRDY goes LOW (p35. Figure 33)
	ADS_SPI->transfer(B00001111); //Send SDATAC to the ADC	
	digitalWrite(_CS_pin, HIGH); //We finished the command sequence, so we switch it back to HIGH
	ADS_SPI->endTransaction();
	
	_isAcquisitionRunning = false; //Reset to false, so the MCU will be able to start a new conversion
}

void ADS1256_Base::setDRATE(uint8_t drate) //Setting DRATE (sampling frequency)
{		
	writeRegister(DRATE_REG, drate);
	_DRATE = drate;
	delay(200);
}

void ADS1256_Base::setMUX(uint8_t mux) //Setting MUX (input channel)
{	
	writeRegister(MUX_REG, mux);
	_MUX = mux;
	delay(200);
}

void ADS1256_Base::setPGA(uint8_t pga) //Setting PGA (input voltage range)
{	
	_PGA = pga;
	_ADCON = _ADCON | 0b00000111; //Change bit 2-0 to 1, leave rest unchanged 
	_ADCON = _ADCON & _PGA; //Combine the new _ADCON with the _PGA (changes bit 0-2)
	writeRegister(ADCON_REG, _ADCON);	
	delay(200);
}

void ADS1256_Base::setCLKOUT(uint8_t clkout) //Setting CLKOUT 
{
	//Values: 0, 1, 2, 3
	
	if(clkout == 0)
	{
		//00
		bitWrite(_ADCON, 6, 0);
		bitWrite(_ADCON, 5, 0);		
	}
	else if(clkout == 1)
	{
		//01 (default)	
		bitWrite(_ADCON, 6, 0);
		bitWrite(_ADCON, 5, 1);			
	}
	else if(clkout == 2)
	{
		//10
		bitWrite(_ADCON, 6, 1);
		bitWrite(_ADCON, 5, 0);		
	}
	else if(clkout == 3)
	{
		//11	
		bitWrite(_ADCON, 6, 1);
		bitWrite(_ADCON, 5, 1);				
	}
	else{}
	
	writeRegister(ADCON_REG, _ADCON);
	delay(100);
}

void ADS1256_Base::setSDCS(uint8_t sdcs) //Setting SDCS
{
	//Values: 0, 1, 2, 3
	
	if(sdcs == 0)
	{
		//00 (default)
		bitWrite(_ADCON, 4, 0);
		bitWrite(_ADCON, 3, 0);		
	}
	else if(sdcs == 1)
	{
		//01	
		bitWrite(_ADCON, 4, 0);
		bitWrite(_ADCON, 3, 1);		
	}
	else if(sdcs == 2)
	{
		//10
		bitWrite(_ADCON, 4, 1);
		bitWrite(_ADCON, 3, 0);		
	}
	else if(sdcs == 3)
	{
		//11
		bitWrite(_ADCON, 4, 1);
		bitWrite(_ADCON, 3, 1);				
	}
	else{}
	
	writeRegister(ADCON_REG, _ADCON);
	delay(100);
}

void ADS1256_Base::setByteOrder(uint8_t byteOrder) //Setting byte order (MSB/LSB)
{
	if(byteOrder == 0)
	{
		//Byte order is MSB (default)
		bitWrite(_STATUS, 3, 0);
		//Set value of _STATUS at the third bit to 0
	}
	else if(byteOrder == 1)
	{
		//Byte order is LSB
		bitWrite(_STATUS, 3, 1);
		//Set value of _STATUS at the third bit to 1
	}
	else{}
	
	writeRegister(STATUS_REG, _STATUS);
	delay(100);
}

void ADS1256_Base::setAutoCal(uint8_t acal) //Setting ACAL (Automatic SYSCAL)
{
	if(acal == 0)
	{
		//Auto-calibration is disabled (default)
		bitWrite(_STATUS, 2, 0);
		//_STATUS |= B00000000;
	}
	else if(acal == 1)
	{
		//Auto-calibration is enabled
		bitWrite(_STATUS, 2, 1);
		//_STATUS |= B00000100;
	}
	else{}
	
	writeRegister(STATUS_REG, _STATUS);
	delay(100);
}

void ADS1256_Base::setBuffer(uint8_t bufen) //Setting input buffer (Input impedance)
{
	if(bufen == 0)
	{
		//Analog input buffer is disabled (default)
		//_STATUS |= B00000000;
		bitWrite(_STATUS, 1, 0);
	}
	else if(bufen == 1)
	{
		//Analog input buffer is enabled (recommended)
		//_STATUS |= B00000010;
		bitWrite(_STATUS, 1, 1);
	}
	else{}
	
	writeRegister(STATUS_REG, _STATUS);
	delay(100);
}

void ADS1256_Base::setGPIO(uint8_t dir0, uint8_t dir1, uint8_t dir2, uint8_t dir3) //Setting GPIO
{
	//Default: 11100000 - DEC: 224 - Ref: p32 I/O section
	//Sets D3-D0 as input or output
	uint8_t GPIO_bit7, GPIO_bit6, GPIO_bit5, GPIO_bit4;
	
	//Bit7: DIR3
	if(dir3 == 1)
	{
		GPIO_bit7 = 1; //D3 is input (default)
	}
	else
	{
		GPIO_bit7 = 0; //D3 is output
	}	
	bitWrite(_GPIO, 7, GPIO_bit7);
	//-----------------------------------------------------
	//Bit6: DIR2
	if(dir2 == 1)
	{
		GPIO_bit6 = 1; //D2 is input (default)
	}
	else
	{
		GPIO_bit6 = 0; //D2 is output
	}
	bitWrite(_GPIO, 6, GPIO_bit6);
	//-----------------------------------------------------
	//Bit5: DIR1 
	if(dir1 == 1)
	{
		GPIO_bit5 = 1; //D1 is input (default) 
	}
	else
	{
		GPIO_bit5 = 0; //D1 is output
	}
	bitWrite(_GPIO, 5, GPIO_bit5);
	//-----------------------------------------------------
	//Bit4: DIR0
	if(dir0 == 1)
	{
		GPIO_bit4 = 1; //D0 is input
	}
	else
	{
		GPIO_bit4 = 0; //D0 is output (default)
	}
	bitWrite(_GPIO, 4, GPIO_bit4);
	//-----------------------------------------------------
	
	writeRegister(IO_REG, _GPIO);
	delay(100);
}

void ADS1256_Base::writeGPIO(uint8_t dir0value, uint8_t dir1value, uint8_t dir2value, uint8_t dir3value) //Writing GPIO
{
	//Sets D3-D0 output values
	//It is important that first one must use setGPIO, then writeGPIO
	
	uint8_t GPIO_bit3, GPIO_bit2, GPIO_bit1, GPIO_bit0;
	
	//Bit3: DIR3 
	if(dir3value == 1)
	{
		GPIO_bit3 = 1;
	}
	else
	{
		GPIO_bit3 = 0;
	}	
	bitWrite(_GPIO, 3, GPIO_bit3);
	//-----------------------------------------------------
	//Bit2: DIR2 
	if(dir2value == 1)
	{
		GPIO_bit2 = 1;
	}
	else
	{
		GPIO_bit2 = 0;
	}	
	bitWrite(_GPIO, 2, GPIO_bit2);
	//-----------------------------------------------------
	//Bit1: DIR1 
	if(dir1value == 1)
	{
		GPIO_bit1 = 1;
	}
	else
	{
		GPIO_bit1 = 0;
	}
	bitWrite(_GPIO, 1, GPIO_bit1);	
	//-----------------------------------------------------
	//Bit0: DIR0 
	if(dir0value == 1)
	{
		GPIO_bit0 = 1;
	}
	else
	{
		GPIO_bit0 = 0;
	}	
	bitWrite(_GPIO, 0, GPIO_bit0);
	//-----------------------------------------------------	
		
	writeRegister(IO_REG, _GPIO);
	delay(100);
}

uint8_t ADS1256_Base::readGPIO(uint8_t gpioPin) //Reading GPIO
{	
	uint8_t GPIO_bit3, GPIO_bit2, GPIO_bit1, GPIO_bit0;
	
	_GPIO = readRegister(IO_REG); //Read the GPIO register
	
	//Save each bit values in a variable
	GPIO_bit3 = bitRead(_GPIO, 3);
	GPIO_bit2 = bitRead(_GPIO, 2);
	GPIO_bit1 = bitRead(_GPIO, 1);
	GPIO_bit0 = bitRead(_GPIO, 0);	
	
	delay(100);
	
	//Return the selected bit value
	//Bit3: DIR3 
	if(gpioPin == 0)
	{
		return(GPIO_bit0);
	}	
	//-----------------------------------------------------
	//Bit2: DIR2 
	if(gpioPin == 1)
	{
		return(GPIO_bit1);
	}	
	//-----------------------------------------------------
	//Bit1: DIR1 
	if(gpioPin == 2)
	{
		return(GPIO_bit2);
	}	
	//-----------------------------------------------------
	//Bit0: DIR0 
	if(gpioPin == 3)
	{
		return(GPIO_bit3);
	}			

    return -1;
}

void ADS1256_Base::sendDirectCommand(uint8_t directCommand)
{
  //Direct commands can be found in the datasheet Page 34, Table 24.
  ADS_SPI->beginTransaction(SPISettings(1920000, MSBFIRST, SPI_MODE1));

  digitalWrite(_CS_pin, LOW); //REF: P34: "CS must stay low during the entire command sequence"
  delayMicroseconds(5);
  ADS_SPI->transfer(directCommand); //Send Command
  delayMicroseconds(5);
  digitalWrite(_CS_pin, HIGH); //REF: P34: "CS must stay low during the entire command sequence"

  ADS_SPI->endTransaction();
}


float ADS1256_Base::convertToVoltage(int32_t rawData) //Converting the 24-bit data into a voltage value
{	
  if (rawData >> 23 == 1) //if the 24th digit (sign) is 1, the number is negative
  {
    rawData = rawData - 16777216;  //conversion for the negative sign
    //"mirroring" around zero
  }  
  
  float voltage = ((2 * _VREF) / 8388608) * rawData / (pow(2, _PGA)); //8388608 = 2^{23} - 1
  //REF: p23, Table 16.
	
  return(voltage);
}

void ADS1256_Base::writeRegister(uint8_t registerAddress, uint8_t registerValueToWrite)
{	
  waitForDRDY();

  ADS_SPI->beginTransaction(SPISettings(1920000, MSBFIRST, SPI_MODE1));
  //SPI_MODE1 = output edge: rising, data capture: falling; clock polarity: 0, clock phase: 1.

  digitalWrite(_CS_pin, LOW); //CS must stay LOW during the entire sequence [Ref: P34, T24]

  delayMicroseconds(5); //see t6 in the datasheet

  ADS_SPI->transfer(0x50 | registerAddress); // 0x50 = 01010000 = WREG

  ADS_SPI->transfer(0x00); //2nd (empty) command byte

  ADS_SPI->transfer(registerValueToWrite); //pass the value to the register
  
  digitalWrite(_CS_pin, HIGH);
  ADS_SPI->endTransaction();
  delay(100);
  
}

long ADS1256_Base::readRegister(uint8_t registerAddress) //Reading a register
{
   waitForDRDY();
	
  ADS_SPI->beginTransaction(SPISettings(1920000, MSBFIRST, SPI_MODE1));
  //SPI_MODE1 = output edge: rising, data capture: falling; clock polarity: 0, clock phase: 1.

  digitalWrite(_CS_pin, LOW); //CS must stay LOW during the entire sequence [Ref: P34, T24]

  ADS_SPI->transfer(0x10 | registerAddress); //0x10 = 0001000 = RREG - OR together the two numbers (command + address)

  ADS_SPI->transfer(0x00); //2nd (empty) command byte

  delayMicroseconds(5); //see t6 in the datasheet

  _registerValuetoRead = ADS_SPI->transfer(0xFF); //read out the register value

  digitalWrite(_CS_pin, HIGH);
  ADS_SPI->endTransaction();
  delay(100);
  return _registerValuetoRead;
}


long ADS1256_Base::readSingle() //Reading a single value ONCE using the RDATA command 
{
	ADS_SPI->beginTransaction(SPISettings(1920000, MSBFIRST, SPI_MODE1));
	digitalWrite(_CS_pin, LOW); //REF: P34: "CS must stay low during the entire command sequence"  
	waitForDRDY();
	ADS_SPI->transfer(B00000001); //Issue RDATA (0000 0001) command
	delayMicroseconds(7); //Wait t6 time (~6.51 us) REF: P34, FIG:30.

	_outputBuffer[0] = ADS_SPI->transfer(0); // MSB
	_outputBuffer[1] = ADS_SPI->transfer(0); // Mid-byte
	_outputBuffer[2] = ADS_SPI->transfer(0); // LSB		

	//Shifting and combining the above three items into a single, 24-bit number
	_outputValue = ((long)_outputBuffer[0]<<16) | ((long)_outputBuffer[1]<<8) | (_outputBuffer[2]);
	
	digitalWrite(_CS_pin, HIGH); //We finished the command sequence, so we set CS to HIGH
	ADS_SPI->endTransaction();
  
	return(_outputValue);
}

long ADS1256_Base::readSingleContinuous() //Reads the recently selected input channel using RDATAC
{		
	if(_isAcquisitionRunning == false)
	{
	  _isAcquisitionRunning = true;
	  ADS_SPI->beginTransaction(SPISettings(1920000, MSBFIRST, SPI_MODE1));
	  digitalWrite(_CS_pin, LOW); //REF: P34: "CS must stay low during the entire command sequence"	  
	  while (digitalRead(_DRDY_pin)) {}
	  ADS_SPI->transfer(B00000011);  //Issue RDATAC (0000 0011) 
	  delayMicroseconds(7); //Wait t6 time (~6.51 us) REF: P34, FIG:30.
	  
	}
	else
	{
		waitForDRDY();
	}	
	
	_outputBuffer[0] = ADS_SPI->transfer(0); // MSB 
	_outputBuffer[1] = ADS_SPI->transfer(0); // Mid-byte
	_outputBuffer[2] = ADS_SPI->transfer(0); // LSB
	
	_outputValue = ((long)_outputBuffer[0]<<16) | ((long)_outputBuffer[1]<<8) | (_outputBuffer[2]);
	
	return _outputValue;
}

long ADS1256_Base::cycleSingle()
{
	if(_isAcquisitionRunning == false)
	{
	  _isAcquisitionRunning = true;
	  _cycle = 0;
	  ADS_SPI->beginTransaction(SPISettings(1920000, MSBFIRST, SPI_MODE1));
	  digitalWrite(_CS_pin, LOW); //CS must stay LOW during the entire sequence [Ref: P34, T24]
	  ADS_SPI->transfer(0x50 | 1); // 0x50 = WREG //1 = MUX
      ADS_SPI->transfer(0x00);
      ADS_SPI->transfer(SING_0); //AIN0+AINCOM
	  digitalWrite(_CS_pin, HIGH);			
	  delay(50);
	  digitalWrite(_CS_pin, LOW); //CS must stay LOW during the entire sequence [Ref: P34, T24]
	}
	else
	{}
  
	if(_cycle < 8)
	{      
	  _outputValue = 0;
	  waitForDRDY();
      //Step 1. - Updating MUX
      switch (_cycle)
      {
        //Channels are written manually
        case 0: //Channel 2
          ADS_SPI->transfer(0x50 | 1); // 0x50 = WREG //1 = MUX
          ADS_SPI->transfer(0x00);
          ADS_SPI->transfer(SING_1);  //AIN1+AINCOM
          break;

        case 1: //Channel 3
          ADS_SPI->transfer(0x50 | 1); // 0x50 = WREG //1 = MUX
          ADS_SPI->transfer(0x00);
          ADS_SPI->transfer(SING_2);  //AIN2+AINCOM
          break;

        case 2: //Channel 4
          ADS_SPI->transfer(0x50 | 1); // 0x50 = WREG //1 = MUX
          ADS_SPI->transfer(0x00);
          ADS_SPI->transfer(SING_3);  //AIN3+AINCOM
          break;

        case 3: //Channel 5
          ADS_SPI->transfer(0x50 | 1); // 0x50 = WREG //1 = MUX
          ADS_SPI->transfer(0x00);
          ADS_SPI->transfer(SING_4);  //AIN4+AINCOM
          break;

        case 4: //Channel 6
          ADS_SPI->transfer(0x50 | 1); // 0x50 = WREG //1 = MUX
          ADS_SPI->transfer(0x00);
          ADS_SPI->transfer(SING_5);  //AIN5+AINCOM
          break;

        case 5: //Channel 7
          ADS_SPI->transfer(0x50 | 1); // 0x50 = WREG //1 = MUX
          ADS_SPI->transfer(0x00);
          ADS_SPI->transfer(SING_6);  //AIN6+AINCOM
          break;

        case 6: //Channel 8
          ADS_SPI->transfer(0x50 | 1); // 0x50 = WREG //1 = MUX
          ADS_SPI->transfer(0x00);
          ADS_SPI->transfer(SING_7);  //AIN7+AINCOM
          break;

        case 7: //Channel 1
          ADS_SPI->transfer(0x50 | 1); // 0x50 = WREG //1 = MUX
          ADS_SPI->transfer(0x00);
          ADS_SPI->transfer(SING_0); //AIN0+AINCOM
          break;
      }
      //Step 2.
      ADS_SPI->transfer(B11111100); //SYNC
      delayMicroseconds(4); //t11 delay 24*tau = 3.125 us //delay should be larger, so we delay by 4 us
      ADS_SPI->transfer(B11111111); //WAKEUP

      //Step 3.
      //Issue RDATA (0000 0001) command
      ADS_SPI->transfer(B00000001);
      delayMicroseconds(7); //Wait t6 time (~6.51 us) REF: P34, FIG:30.

	  _outputBuffer[0] = ADS_SPI->transfer(0x0F); // MSB 
	  _outputBuffer[1] = ADS_SPI->transfer(0x0F); // Mid-byte
	  _outputBuffer[2] = ADS_SPI->transfer(0x0F); // LSB
		
	  _outputValue = ((long)_outputBuffer[0]<<16) | ((long)_outputBuffer[1]<<8) | (_outputBuffer[2]);
		
	  _cycle++; //Increase cycle - This will move to the next MUX input channel
	  if(_cycle == 8)
	  {
		  _cycle = 0; //Reset to 0 - Restart conversion from the 1st input channel
	  }
    }	
  
	return _outputValue;
}

long ADS1256_Base::cycleDifferential() 
{
	if(_isAcquisitionRunning == false)
	{
	  _cycle = 0;
	  _isAcquisitionRunning = true;
	  ADS_SPI->beginTransaction(SPISettings(1920000, MSBFIRST, SPI_MODE1));
	  
	  //Set the AIN0+AIN1 as inputs manually
	  digitalWrite(_CS_pin, LOW); //CS must stay LOW during the entire sequence [Ref: P34, T24]
	  ADS_SPI->transfer(0x50 | 1); // 0x50 = WREG //1 = MUX
	  ADS_SPI->transfer(0x00);
	  ADS_SPI->transfer(DIFF_0_1);  //AIN0+AIN1
	  digitalWrite(_CS_pin, HIGH);	  
	  delay(50);
	  digitalWrite(_CS_pin, LOW); //CS must stay LOW during the entire sequence [Ref: P34, T24]
	}
	else
	{}
	
	if(_cycle < 4)
    {
	  _outputValue = 0;
      //DRDY has to go low
	  waitForDRDY();

      //Step 1. - Updating MUX
      switch (_cycle)
      {
        case 0: //Channel 2
          ADS_SPI->transfer(0x50 | 1); // 0x50 = WREG //1 = MUX
          ADS_SPI->transfer(0x00);
          ADS_SPI->transfer(DIFF_2_3);  //AIN2+AIN3
          break;

        case 1: //Channel 3
          ADS_SPI->transfer(0x50 | 1); // 0x50 = WREG //1 = MUX
          ADS_SPI->transfer(0x00);
          ADS_SPI->transfer(DIFF_4_5); //AIN4+AIN5
          break;

        case 2: //Channel 4
          ADS_SPI->transfer(0x50 | 1); // 0x50 = WREG //1 = MUX
          ADS_SPI->transfer(0x00);
          ADS_SPI->transfer(DIFF_6_7); //AIN6+AIN7
          break;

        case 3: //Channel 1
          ADS_SPI->transfer(0x50 | 1); // 0x50 = WREG //1 = MUX
          ADS_SPI->transfer(0x00);
          ADS_SPI->transfer(DIFF_0_1); //AIN0+AIN1
          break;
      }

      ADS_SPI->transfer(0b11111100); //SYNC
      delayMicroseconds(4); //t11 delay 24*tau = 3.125 us //delay should be larger, so we delay by 4 us
      ADS_SPI->transfer(0b11111111); //WAKEUP

      //Step 3.
      ADS_SPI->transfer(0b00000001); //Issue RDATA (0000 0001) command
      delayMicroseconds(7); //Wait t6 time (~6.51 us) REF: P34, FIG:30.

	
	  _outputBuffer[0] = ADS_SPI->transfer(0); // MSB 
	  _outputBuffer[1] = ADS_SPI->transfer(0); // Mid-byte
	  _outputBuffer[2] = ADS_SPI->transfer(0); // LSB
		
	  _outputValue = ((long)_outputBuffer[0]<<16) | ((long)_outputBuffer[1]<<8) | (_outputBuffer[2]);
		
	  _cycle++;
	  if(_cycle == 4)
	  {
		  _cycle = 0;
		  //After the 4th cycle, we reset to zero so the next iteration reads the 1st MUX again
	  }
    }	
  
	return _outputValue;
}

