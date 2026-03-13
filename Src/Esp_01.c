#include "stm32f4xx.h"
#include <stdio.h>
#include <string.h>
#include "delay.h"
#include "I2C_Lcd.h"
#include "Lcd_defines.h"
#include "Esp_01.h"

#define BUFFER_SIZE 1024
#define PAGE_SIZE   1024

const char *html_template =
"<!DOCTYPE html><html>"
"<head>"
"<meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">"
"<meta http-equiv=\"refresh\" content=\"5\">"
"</head>"
"<body style=\"font-family:Arial,sans-serif; text-align:center; background-color:#f0f0f0;\">"
"<center><h2>LED Control</h2></center>"
"<form action=\"/ledon\" method=\"get\"><button type=\"submit\" style=\"margin:10px; padding:10px 20px;\">ON</button></form>"
"<form action=\"/ledoff\" method=\"get\"><button type=\"submit\" style=\"margin:10px; padding:10px 20px;\">OFF</button></form>"
"<p>Status: <span style=\"color:%s;\">%s</span></p>"
"</body></html>";

#define UART_BUFFER_SIZE 512
volatile uint8_t uart1_rx_buffer[UART_BUFFER_SIZE];
volatile uint16_t uart1_rx_head = 0;
volatile uint16_t uart1_rx_tail = 0;

volatile uint8_t led_state = 0;     // 0 = OFF, 1 = ON
volatile uint8_t led_changed = 0;   // flag to update LCD


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

void ESP_Connect(void)
{
    // Try to connect until successful
    while(ESP_Init(WIFI_SSID, WIFI_PASSWD) != 't')
    {
        // Wi-Fi failed message
        LCD_Cmd(CLEAR_LCD);
        Str_LCD("WiFi Connection");
        LCD_SetCursor(1,0);
        Str_LCD("Failed!");
        UART2_SendString("WiFi connection failed!\r\n");
        delay_ms(2000);

        // Retry message
        LCD_Cmd(CLEAR_LCD);
        Str_LCD("Retrying WiFi...");
        LCD_SetCursor(1,0);
        Str_LCD("Please Wait...");      // Line 1
        UART2_SendString("Retrying WiFi...\r\n");
        delay_ms(2000);  // wait before next attempt
    }

    // Successfully connected
    LCD_Cmd(CLEAR_LCD);
    Str_LCD("WiFi Connected!");
    LCD_SetCursor(1,0);
    Str_LCD("IP assigned");
    UART2_SendString("WiFi Connected!\r\n");
    delay_ms(2000);

    // Show server ready message
    LCD_Cmd(CLEAR_LCD);
    Str_LCD("Server Ready");
    LCD_SetCursor(1,0);
    Str_LCD("Waiting Cmd...");
    UART2_SendString("Server Ready, waiting commands...\r\n");
    delay_ms(2000);
}


void Server_Start(void)
{
    static char buffer[BUFFER_SIZE];
    static int idx = 0;

    int c = Uart1_ReadChar();

    if (c != -1)
    {
        /* Store character in buffer */
        if (idx < BUFFER_SIZE - 1)
        {
            buffer[idx++] = (char)c;
            buffer[idx] = '\0';
        }
        else
        {
            idx = 0;
            memset(buffer,0,sizeof(buffer));
        }

        /* Debug output */
        UART2_WriteChar((char)c);

        /* Detect WiFi disconnect immediately */
        if (strstr(buffer,"WIFI DISCONNECT"))
        {
            idx = 0;
            memset(buffer,0,sizeof(buffer));

            LCD_Cmd(CLEAR_LCD);
            Str_LCD("WiFi Lost");
            LCD_SetCursor(1,0);
            Str_LCD("Reconnecting");

            UART2_SendString("\r\nWiFi Lost! Reconnecting...\r\n");

            ESP_Connect();
            return;
        }

        return;
    }

    /* Wait until HTTP request complete */
    if (idx < 4)
        return;

    if (strstr(buffer,"\r\n\r\n") == NULL)
        return;

    /* Copy request safely */
    char request[BUFFER_SIZE];
    memcpy(request,buffer,idx);
    request[idx] = '\0';

    idx = 0;
    memset(buffer,0,sizeof(buffer));

    /* Extract connection ID */
    int conn_id = 0;
    int len = 0;

    char *ipd = strstr(request,"+IPD,");
    if(ipd)
        sscanf(ipd,"+IPD,%d,%d:",&conn_id,&len);

    /* LED ON */
    if(strstr(request,"GET /ledon"))
    {
        led_state = 1;
        led_changed = 1;
        GPIOA->ODR &= ~(1<<6);
    }

    /* LED OFF */
    else if(strstr(request,"GET /ledoff"))
    {
        led_state = 0;
        led_changed = 1;
        GPIOA->ODR |= (1<<6);
    }

    /* Serve webpage */
    if(strstr(request,"GET /") && !strstr(request,"favicon"))
    {
        char page[PAGE_SIZE];

        snprintf(page,PAGE_SIZE,html_template,
                 led_state ? "green":"red",
                 led_state ? "LED is ON":"LED is OFF");

        char cmd[32];
        snprintf(cmd,sizeof(cmd),
                 "AT+CIPSEND=%d,%d\r\n",
                 conn_id,strlen(page));

        Uart1_SendString(cmd);

        /* Wait for '>' prompt */
        int wait = 0;
        while(wait < 5000)
        {
            int ch = Uart1_ReadChar();
            if(ch == '>')
                break;

            delay_ms(1);
            wait++;
        }

        /* Send page */
        Uart1_SendString(page);

        /* Wait SEND OK */
        char resp[32] = {0};
        int i = 0;
        int timeout = 0;

        while(timeout < 5000)
        {
            int ch = Uart1_ReadChar();

            if(ch != -1)
            {
                resp[i++] = (char)ch;

                if(i >= sizeof(resp)-1)
                    i = sizeof(resp)-2;

                resp[i] = '\0';

                if(strstr(resp,"SEND OK"))
                    break;
            }
            else
            {
                delay_ms(1);
                timeout++;
            }
        }

        delay_ms(20);

        char close_cmd[20];
        snprintf(close_cmd,sizeof(close_cmd),
                 "AT+CIPCLOSE=%d\r\n",conn_id);

        Uart1_SendString(close_cmd);
    }
}
