#include <stdint.h>
#include <stdio.h>
#include "stm32f4xx.h"
#include "delay.h"
#include "Lcd_defines.h"

#define LCD_ADDR 0x27

#define SR2_BUSY (1U<<1)
#define SR1_SB   (1U<<0)
#define SR1_ADDR (1U<<1)
#define SR1_BTF  (1U<<2)

#define CR1_START (1U<<8)
#define CR1_STOP  (1U<<9)


/* ================= I2C INIT (FOR 84MHz SYSCLK) ================= */
void I2C1_Init(void)
{
	// Enable GPIOB clock
	RCC->AHB1ENR |= RCC_AHB1ENR_GPIOBEN;

	// PB8, PB9 Alternate function
	GPIOB->MODER &= ~(3U<<16);
	GPIOB->MODER |=  (2U<<16);
	GPIOB->MODER &= ~(3U<<18);
	GPIOB->MODER |=  (2U<<18);

	 //SET OUTTYPE AS OPEN DRAIN
	GPIOB->OTYPER |= (1U<<8);
	GPIOB->OTYPER |= (1U<<9);

	//SET PULLUP FOR BOTH PB8 AND PB9
	GPIOB->PUPDR |=(1U<<16);
	GPIOB->PUPDR &=~(1U<<17);
	GPIOB->PUPDR |=(1U<<18);
	GPIOB->PUPDR &=~(1U<<19);

	//SET THE ALTERNATE FUNCTION TYPE FOR I2C PB8 AND PB9
	GPIOB->AFR[1] &=~(1U<<0);
	GPIOB->AFR[1] &=~(1U<<1);
	GPIOB->AFR[1] |= (1U<<2);
	GPIOB->AFR[1] &=~(1U<<3);

	GPIOB->AFR[1] &=~(1U<<4);
	GPIOB->AFR[1] &=~(1U<<5);
	GPIOB->AFR[1] |= (1U<<6);
	GPIOB->AFR[1] &=~(1U<<7);

	// Enable I2C1 clock
	RCC->APB1ENR |= RCC_APB1ENR_I2C1EN;

	//ENTER TO RESET MODE
	I2C1->CR1 |= (1U<<15);

	//COME OUT OF THE RESET MODE
	I2C1->CR1 &= ~(1U<<15);

	// APB1 = 50 MHz, standard mode 100 kHz
	I2C1->CR2   = 50;  // Peripheral clock in MHz

	//Set the Standard Mode (I2C_100KHZ)
	I2C1->CCR   = 250; // 100 kHz
	I2C1->TRISE = 51;  // Maximum rise time

    //Set the I2C Peripheral Enable
	I2C1->CR1 |= (1U << 0);

}

/* ================= I2C BRUST WRITE ================= */
void I2C1_BurstWrite(char saddr, uint8_t *data, int size)
{
	//Wait Until Bus is not busy
	while(I2C1->SR2 & SR2_BUSY);
	//Generate Start Condition
	I2C1->CR1 |= CR1_START;
	//Wait Until the start flag is set
	while(!(I2C1->SR1 & SR1_SB));
	//Transmit Slave address + write
	I2C1->DR = saddr << 1;
	//Wait until the Address Flag is set
	while(!(I2C1->SR1 & SR1_ADDR));
	//Clear the Address flag by reading SR1 and SR2
	(void)I2C1->SR2;

	for(int i = 0; i < size; i++)
	{
		//Send the data byte
		I2C1->DR = data[i];
		//Wait until Byte Transfer Finished (BTF)
		while(!(I2C1->SR1 & SR1_BTF));
	}

	//Generate stop after data received
	I2C1->CR1 |= CR1_STOP;
}

/* ================= LCD LOW LEVEL ================= */

void LCD_Send4Bit(uint8_t data)
{
	uint8_t buffer[2];

	buffer[0] = data | EN;      // EN = 1
	buffer[1] = data & ~EN;     // EN = 0

	I2C1_BurstWrite(LCD_ADDR, buffer, 2);

	for(int i=0;i<500;i++);     // Small pulse delay
}

/* ================= LCD COMMAND & DATA ================= */

void LCD_Cmd(uint8_t cmd)
{
	char high = (cmd & 0xF0) | BL;
	char low  = ((cmd<<4) & 0xF0) | BL;

	LCD_Send4Bit(high);
	LCD_Send4Bit(low);

	delay_ms(2);
}

void LCD_Data(uint8_t data)
{
	char high = (data & 0xF0) | RS | BL;
	char low  = ((data<<4) & 0xF0) | RS | BL;

	LCD_Send4Bit(high);
	LCD_Send4Bit(low);

	delay_ms(2);
}

/* ================= LCD INIT ================= */

void LCD_Init(void)
{
	//4-bit initialization
	delay_ms(50); // Wait for LCD to power up

    // Force 8-bit mode (3 times)
    LCD_Send4Bit(MODE_8BIT_1LINE);  // 0X30
    delay_ms(5);

    LCD_Send4Bit(MODE_8BIT_1LINE);
    delay_ms(5);

    LCD_Send4Bit(MODE_8BIT_1LINE);
    delay_ms(5);

    LCD_Send4Bit(MODE_4BIT_1LINE); // Switch to 4-bit mode 0x20
    delay_ms(5);


    //Display initialization

    LCD_Cmd(MODE_4BIT_2LINE);   // 0x28 Function set: 4-bit, 2 lines, 5x8 dots
    delay_ms(5);
    // Display ON,OFF Control ---> DISPLAY OFF
    LCD_Cmd(DSP_OFF); //0x08
    delay_ms(5);
    // Clear display
    LCD_Cmd(CLEAR_LCD); //0X01
    delay_ms(5);
    delay_ms(5);
    // 0x06 Entry mode: increment cursor, no display shift
    LCD_Cmd(SHIFT_CUR_RIGHT);
    delay_ms(5);
    // 0x0C Display ON, cursor OFF, blink OFF
    LCD_Cmd(DSP_ON_CUR_OFF);
    delay_ms(5);
}



void Str_LCD(char *str)
{
	while(*str)
	{
		LCD_Data(*str++);
	}
}

void U32_Lcd(int num)
{
	int i = 0;
	uint8_t buff[10];

	if(num == 0)
	{
		LCD_Data('0');
	}
	else
	{
		while(num > 0)
		{
			buff[i++] = (num % 10) + '0'; // convert digit to ASCII
			num /= 10;
		}

		for(--i; i >= 0; i--)
		{
			LCD_Data(buff[i]);
		}

	}
}

void S32_Lcd(int32_t num)
{
	if(num<0)
	{
		LCD_Data('-');
		num=-num;
	}
	U32_Lcd(num);
}

void F32_Lcd(float fnum, uint32_t nDP)
{
	 uint32_t num,i;
	if(fnum<0.0)
	{
		LCD_Data('-');
		fnum=-fnum;
	}
	num=fnum;
	U32_Lcd(num);
	LCD_Data('.');
	for(i=0;i<nDP;i++)
	{
		fnum=(fnum-num)*10;
		num=fnum;
		LCD_Data(num+48);
	}
}


void LCD_SetCursor(uint8_t row, uint8_t col)
{
	uint8_t address;


	if(row == 0)
	{
		address = 0x80 + col;
	}
	else
	{
		address = 0xC0 + col;
	}

	LCD_Cmd(address);
}






