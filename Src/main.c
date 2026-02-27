#include "stm32f4xx.h"
#include <stdio.h>
#include <string.h>
#include "delay.h"
#include "I2C_Lcd.h"
#include "Lcd_defines.h"

#define UART_BUFFER_SIZE 128
volatile uint8_t uart1_rx_buffer[UART_BUFFER_SIZE];
volatile uint16_t uart1_rx_head = 0;
volatile uint16_t uart1_rx_tail = 0;

#define LED_ON_CMD  "/ledon"
#define LED_OFF_CMD "/ledoff"

volatile uint8_t led_state = 0;   // 0 = OFF, 1 = ON
volatile uint8_t led_changed = 0; // flag to update LCD

void LED_Init(void)
{
    RCC->AHB1ENR |= (1U << 0);   // GPIOA clock

    //GPIOA->MODER &= ~(3U << 10); // Clear PA5
    //GPIOA->MODER |=  (1U << 10); // Output mode

    GPIOA->MODER &= ~(3U << 12); // Clear PA6
    GPIOA->MODER |=  (1U << 12); // Output mode

    GPIOA->ODR |= (1U << 6); // Initially Relay off
}

void UART1_Init(void)
{
	//GPIOA clock access PA9->TX PA10-> RX
	RCC->AHB1ENR |= (1U << 0);

	//UART1 clock access for PA9->TX PA10-> RX
	RCC->APB2ENR |= (1U << 4);

	//GPIO_MODER configuration
	GPIOA->MODER &= ~((1U << 18) | (1U << 20));
	GPIOA->MODER |= (1U << 19) | (1U << 21);

	//GPIO alternate function high register
	GPIOA->AFR[1] |= (1U << 4) | (1U << 5) | (1U << 6);
	GPIOA->AFR[1] |= (1U << 8) | (1U << 9) | (1U << 10);
	GPIOA->AFR[1] &= ~((1U << 7) | (1U << 11));
	//USART1 baud rate
	USART1->BRR |= 16000000/115200;
	//USART1 configuration
	USART1->CR1 |= (1U << 2);
	USART1->CR1 |= (1U << 3);
	USART1->CR1 |= (1U << 13);

	USART1->CR1 |= USART_CR1_RXNEIE;    // RX interrupt
	NVIC_EnableIRQ(USART1_IRQn);
}

int Uart1_ReadChar()
{
	if(uart1_rx_head == uart1_rx_tail)
	{
		return -1;
	}
	uint8_t c = uart1_rx_buffer[uart1_rx_tail];
	uart1_rx_tail = (uart1_rx_tail+1) % UART_BUFFER_SIZE;
	return c;
}

void Uart1_Write(char data)
{
	while(!(USART1->SR & (1U << 7))){}
	USART1->DR = data;
}

void Uart1_SendString(const char *str)
{
	while(*str)
	{
		Uart1_Write(*str++);
	}
}

// ================= UART1 IRQ =================
void USART1_IRQHandler(void)
{
	if(USART1->SR & (1U << 5))
	{
		uint8_t c = USART1->DR;
		uint16_t i = (uart1_rx_head + 1) % UART_BUFFER_SIZE;

		if(i != uart1_rx_tail)
		{
			uart1_rx_buffer[uart1_rx_head] = c;
			uart1_rx_head = i;
		}
	}
}

// ================= UART2 (PC Debug) =================
void UART2_Init(void)
{
    RCC->AHB1ENR |= RCC_AHB1ENR_GPIOAEN;
    RCC->APB1ENR |= RCC_APB1ENR_USART2EN;

    // PA2 TX, PA3 RX
    GPIOA->MODER |= (2<<4) | (2<<6); // AF
    GPIOA->AFR[0] |= (7<<8) | (7<<12);

    USART2->BRR = 16000000/9600;    // PC baud = 9600
    USART2->CR1 |= USART_CR1_TE | USART_CR1_RE | USART_CR1_UE;
}

void UART2_WriteChar(char c)
{
    while(!(USART2->SR & USART_SR_TXE));
    USART2->DR = c;
}

void UART2_SendString(const char *str)
{
    while(*str) UART2_WriteChar(*str++);
}

// ================= Wait for ESP Response =================
char ESP_Check_OK(uint32_t timeout_ms)
{
	int c;
	char last2[2] = {0,0};
	uint32_t t = 0;

	while(t < timeout_ms)
	{
		c = Uart1_ReadChar();

		if(c != -1)
		{
			UART2_WriteChar((char)c);

			last2[0] = last2[1];
			last2[1] = (char)c;

			if(last2[0] == 'O' && last2[1] == 'K')
			{
				return 't';
			}

			t = 0;
		}
		else
		{
			delay_ms(1);
			t++;
		}
	}

	return 'f';
}

char ESP_GetIP(char *ip, uint32_t timeout_ms)
{
    int c;
    int i = 0;
    char keyword[] = "STAIP,\"";
    int k = 0;
    uint32_t t = 0;

    while(t < timeout_ms)
    {
        c = Uart1_ReadChar();

        if(c != -1)
        {
            UART2_WriteChar((char)c);   // optional debug

            t = 0;  // reset timeout when data received

            // Match keyword
            if(c == keyword[k])
            {
                k++;
                if(k == 7)
                {
                    // Now read IP
                    while(t < timeout_ms)
                    {
                        c = Uart1_ReadChar();

                        if(c != -1)
                        {
                            UART2_WriteChar((char)c);
                            t = 0;

                            if(c == '"')
                            {
                                ip[i] = '\0';
                                return 't';
                            }

                            if(i < 15)   // max IPv4 length
                                ip[i++] = (char)c;
                        }
                        else
                        {
                            delay_ms(1);
                            t++;
                        }
                    }
                    return 'f';  // timeout while reading IP
                }
            }
            else
            {
                k = 0;
            }
        }
        else
        {
            delay_ms(1);
            t++;
        }
    }

    return 'f';  // timeout waiting for STAIP
}

char ESP_SendCommand(const char* cmd, uint32_t timeout_ms, int retries)
{
	for(int i = 0; i < retries; i++)
	{
		Uart1_SendString(cmd);
		if(ESP_Check_OK(timeout_ms) == 't')
		{
			return 't';
		}
	}

	return 'f';
}

char ESP_Init(char *SSID, char *PASSWD)
{
    char cmd[128];
    char ip[20];

    // Reset
    if(ESP_SendCommand("AT+RST\r\n", 3000, 2) != 't')
    {
    	return 'f';
    }
    // Basic AT check
    if(ESP_SendCommand("AT\r\n", 1000, 2) != 't')
    {
        return 'f';
    }
    // Set Station mode
    if(ESP_SendCommand("AT+CWMODE=1\r\n", 1000, 2) != 't')
    {
        return 'f';
    }
    // Connect to WiFi
    sprintf(cmd,"AT+CWJAP=\"%s\",\"%s\"\r\n", SSID, PASSWD);
    if(ESP_SendCommand(cmd, 10000, 2) != 't')
    {
        return 'f';
    }
    // ================= GET IP (SPECIAL HANDLING) =================
    Uart1_SendString("AT+CIFSR\r\n");
    if(ESP_GetIP(ip, 3000) != 't')
    {
        return 'f';
    }

    // Display IP on LCD
    LCD_Cmd(CLEAR_LCD);
    Str_LCD("IP Address:");
    LCD_SetCursor(1,0);
    Str_LCD(ip);
    delay_ms(30000);

    // Enable multiple connections
    if(ESP_SendCommand("AT+CIPMUX=1\r\n", 1000, 2) != 't')
    {
        return 'f';
    }
    // Start server on port 80
    if(ESP_SendCommand("AT+CIPSERVER=1,80\r\n", 1000, 2) != 't')
    {
        return 'f';
    }

    UART2_SendString("\r\nESP Ready. Check IP in terminal\r\n");

    return 't';
}

void Server_Start(void)
{
    int c;
    static int idx = 0;
    static char buffer[200];

    while((c = Uart1_ReadChar()) != -1)
    {
        // Forward to PC terminal
        UART2_WriteChar((char)c);

        // Store character in buffer
        buffer[idx++] = (char)c;

        if(idx >= sizeof(buffer)-1)
            idx = 0;

        buffer[idx] = '\0';   // ⭐ REQUIRED for strstr()

        // ================= WIFI DISCONNECT DETECT =================
        if(strstr(buffer, "WIFI DISCONNECT"))
        {
            LCD_Cmd(CLEAR_LCD);
            Str_LCD("WiFi Lost");
            delay_ms(2000);

            UART2_SendString("\r\nWiFi Lost! Reconnecting...\r\n");

            idx = 0;
            memset(buffer, 0, sizeof(buffer));

            ESP_Init("S23","12345678");

            LCD_Cmd(CLEAR_LCD);
            Str_LCD("Server Ready");
            LCD_SetCursor(1,0);
            Str_LCD("Waiting...");

            return;
        }

        // ================= CONNECTION CLOSED =================
        if(strstr(buffer, "CLOSED"))
        {
            idx = 0;
            memset(buffer, 0, sizeof(buffer));
        }

        // Check for LED ON
        if(strstr(buffer, LED_ON_CMD))
        {
            if(led_state == 0)
            {
                led_state = 1;
                led_changed = 1;
            }

            //GPIOA->ODR |= (1 << 5);     // LED ON
            GPIOA->ODR &= ~(1 << 6);     // Relay ON
            Uart1_SendString("LED ON\r\n");

            idx = 0;
            memset(buffer, 0, sizeof(buffer));
        }
        // Check for LED OFF
        else if(strstr(buffer, LED_OFF_CMD))
        {
            if(led_state == 1)
            {
                led_state = 0;
                led_changed = 1;
            }

            //GPIOA->ODR &= ~(1 << 5);    // LED OFF
            GPIOA->ODR |= (1 << 6); // Relay off
            Uart1_SendString("LED OFF\r\n");

            idx = 0;
            memset(buffer, 0, sizeof(buffer));
        }
    }
}

int main(void)
{
    UART1_Init();
    UART2_Init();
    I2C1_Init();
    LCD_Init();
    LED_Init();

    UART2_SendString("ESP Starting...\r\n");

    LCD_Cmd(CLEAR_LCD);
    Str_LCD("ESP Starting...");
    delay_ms(2000);

    delay_ms(5000);  // wait ESP boot

    //ESP_Init("S23","12345678");"HOMESTAY 3B_5g","7207851852"

    if(ESP_Init("S23","12345678") == 't')
    {
        UART2_SendString("WiFi Connected\r\n");

        LCD_Cmd(CLEAR_LCD);
        Str_LCD("WiFi Connected");
        delay_ms(2000);

        LCD_Cmd(CLEAR_LCD);
        Str_LCD("Server Ready");
        LCD_SetCursor(1,0);
        Str_LCD("Waiting...");
    }
    else
    {
        UART2_SendString("WiFi Failed\r\n");

        LCD_Cmd(CLEAR_LCD);
        Str_LCD("WiFi Failed");
        delay_ms(2000);

        while(ESP_Init("S23","12345678") != 't')
        {
            LCD_Cmd(CLEAR_LCD);
            Str_LCD("Retrying WiFi");
            delay_ms(5000);
        }

        LCD_Cmd(CLEAR_LCD);
        Str_LCD("Connected!");
        delay_ms(2000);
    }

    // ===== Main Loop =====

    // Show idle message ONCE
    LCD_Cmd(CLEAR_LCD);
    LCD_SetCursor(0,0);
    Str_LCD("Server Running");
    LCD_SetCursor(1,0);
    Str_LCD("Waiting Cmd");

    while(1)
    {
        Server_Start();

        if(led_changed)
        {
            LCD_Cmd(CLEAR_LCD);

            if(led_state)
            {
                LCD_SetCursor(0,0);
                Str_LCD("LED IS ON");
            }
            else
            {
                LCD_SetCursor(0,0);
                Str_LCD("LED IS OFF");
            }

            LCD_SetCursor(1,0);
            Str_LCD("Cmd Received");
            led_changed = 0;
        }
    }
}

