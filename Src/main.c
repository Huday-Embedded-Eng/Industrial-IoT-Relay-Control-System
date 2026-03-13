#include "stm32f4xx.h"
#include <stdio.h>
#include <string.h>
#include "delay.h"
#include "I2C_Lcd.h"
#include "Lcd_defines.h"
#include "Esp_01.h"

int main(void)
{
    /* Peripheral Initialization */
    UART1_Init();       // ESP UART
    UART2_Init();       // Debug UART
    I2C1_Init();        // LCD I2C
    LCD_Init();
    LED_Init();

    UART2_SendString("ESP Starting...\r\n");

    LCD_Cmd(CLEAR_LCD);
    Str_LCD("ESP Starting...");
    delay_ms(2000);

    /* Wait ESP Boot */
    delay_ms(5000);

    /* Connect WiFi */
    ESP_Connect();

    LCD_Cmd(CLEAR_LCD);
    Str_LCD("Server Ready");

    /* Main Loop */
    while(1)
    {
        /* Handle ESP HTTP server */
        Server_Start();

        /* Update LCD when LED changes */
        if(led_changed)
        {
            LCD_Cmd(CLEAR_LCD);

            if(led_state)
                Str_LCD("LED IS ON");
            else
                Str_LCD("LED IS OFF");

            LCD_SetCursor(1,0);
            Str_LCD("Cmd Received");

            led_changed = 0;
        }
    }
}
