#ifndef I2C_LCD_H_
#define I2C_LCD_H_

void I2C1_Init(void);
void I2C1_BurstWrite(char saddr, uint8_t *data, int size);

void LCD_Send4Bit(uint8_t data);
void LCD_Cmd(uint8_t cmd);
void LCD_Data(uint8_t data);
void LCD_Init(void);

void Str_LCD(char *str);
void U32_Lcd(int num);
void S32_Lcd(int32_t num);
void F32_Lcd(float fnum, uint32_t nDP);
void LCD_SetCursor(uint8_t row, uint8_t col);

#endif /* I2C_LCD_H_ */
