#ifndef ESP_01_H_
#define ESP_01_H_

void LED_Init(void);
void UART1_Init(void);
int Uart1_ReadChar();
void Uart1_Write(char data);
void Uart1_SendString(const char *str);
void USART1_IRQHandler(void);

// ================= UART2 (PC Debug) =================
void UART2_Init(void);
void UART2_WriteChar(char c);
void UART2_SendString(const char *str);

// ================= Wait for ESP Response =================
char ESP_Check_OK(uint32_t timeout_ms);
char ESP_GetIP(char *ip, uint32_t timeout_ms);
char ESP_SendCommand(const char* cmd, uint32_t timeout_ms, int retries);
char ESP_Init(char *SSID, char *PASSWD);
void Server_Start(void);
void ESP_Connect(void);

extern volatile uint8_t led_state;   // 0 = OFF, 1 = ON
extern volatile uint8_t led_changed; // flag to update LCD

#define LED_ON_CMD  "/ledon"
#define LED_OFF_CMD "/ledoff"

// Wi-Fi credentials
#define WIFI_SSID     "S23"
#define WIFI_PASSWD   "12345678"

#endif /* ESP_01_H_ */
