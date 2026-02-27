#ifndef LCD_DEFINES_H_
#define LCD_DEFINES_H_

#define CLEAR_LCD 			0x01
#define SHIFT_CUR_RIGHT 	0x06
#define SHIFT_CUR_LEFT 		0x07
#define DSP_OFF 			0x08
#define DSP_ON_CUR_OFF 		0x0C
#define DSP_OFF_CUR_ON 		0x0E
#define DSP_ON_CUR_BLINK  	0x0F

#define MODE_8BIT_1LINE 	0x30
#define MODE_4BIT_1LINE 	0x20

#define MODE_8BIT_2LINE 	0x38
#define MODE_4BIT_2LINE 	0x28

#define GOTO_LINE1_POS0 	0x80
#define GOTO_LINE2_POS0 	0xC0
#define GOTO_LINE3_POS0 	0x94
#define GOTO_LINE4_POS0 	0xD4

#define GOTO_CGRAM_START 	0x40


// Common PCF8574 mapping (most 0x27 modules)
#define EN  0x04
#define RS  0x01
#define BL  0x08

#endif /* LCD_DEFINES_H_ */
