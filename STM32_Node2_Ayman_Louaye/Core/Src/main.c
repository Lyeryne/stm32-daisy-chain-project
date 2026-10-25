#include "main.h"
#include "protocol_icd.h"
#include <string.h>

UART_HandleTypeDef huart1;

volatile uint8_t message_received_flag = 0;
ProtocolMessage_t rx_message;
ProtocolMessage_t tx_msg;
uint8_t rx_byte;

typedef enum {
    ROLE_UNKNOWN = 0,
    ROLE_MASTER = 1,
    ROLE_SLAVE = 2
} NodeRole_t;

typedef enum {
    STATE_UNINITIALIZED = 0,
    STATE_SYNCING = 1,
    STATE_SYNCED = 2
} NodeState_t;

NodeRole_t currentNodeRole = ROLE_UNKNOWN;
NodeState_t currentNodeState = STATE_UNINITIALIZED;
uint8_t currentNodePosition = 0;
uint32_t currentCryptoKey = 0;

void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_USART1_UART_Init(void);

// Chiffrement / Déchiffrement symétrique par XOR avec la clé de 32 bits (4 octets)
void Cipher_Payload(uint8_t *payload, uint8_t length, uint32_t key)
{
    uint8_t *key_bytes = (uint8_t *)&key;
    for (uint8_t i = 0; i < length; i++)
    {
        payload[i] ^= key_bytes[i % 4];
    }
}

// Calcul d'un CRC-16 sur le Header + le Payload (24 premiers octets de la trame)
uint16_t Calculate_CRC16(const ProtocolMessage_t *msg)
{
    const uint8_t *data = (const uint8_t *)msg;
    uint16_t crc = 0xFFFF;
    size_t len = sizeof(ProtocolMessage_t) - sizeof(uint16_t);

    for (size_t i = 0; i < len; i++)
    {
        crc ^= (uint16_t)data[i] << 8;
        for (uint8_t j = 0; j < 8; j++)
        {
            if (crc & 0x8000)
                crc = (crc << 1) ^ 0x1021;
            else
                crc <<= 1;
        }
    }
    return crc;
}

int main(void)
{
  HAL_Init();
  SystemClock_Config();

  MX_GPIO_Init();
  MX_USART1_UART_Init();

  /* 1. Initialiser la LED et le Bouton en mode GPIO */
  BSP_LED_Init(LED2);
  BSP_PB_Init(BUTTON_USER, BUTTON_MODE_GPIO);

  /* Petit flash de 300ms au démarrage */
  BSP_LED_On(LED2);
  HAL_Delay(300);
  BSP_LED_Off(LED2);

  /* 2. Lancer l'écoute par interruption */
  HAL_UART_Receive_IT(&huart1, &rx_byte, 1);

  while (1)
  {
      // --- 1. GESTION VISUELLE DE L'ÉTAT (LED) ---
      switch(currentNodeState) {
          case STATE_UNINITIALIZED:
              BSP_LED_Off(LED2);
              break;
          case STATE_SYNCING:
              BSP_LED_Toggle(LED2);
              HAL_Delay(200);
              break;
          case STATE_SYNCED:
              BSP_LED_On(LED2);
              break;
      }

      // --- 2. TRAITEMENT D'UN NOUVEAU MESSAGE REÇU ---
      if (message_received_flag == 1)
      {
          message_received_flag = 0;

          if (rx_message.header.start_byte == ICD_START_BYTE &&
              rx_message.crc == Calculate_CRC16(&rx_message))
          {
              // Cas 1 : Réception de CMD_INIT
              if (rx_message.header.command_type == CMD_INIT && currentNodeState == STATE_UNINITIALIZED)
              {
                  currentNodeRole = ROLE_SLAVE;
                  currentNodePosition = rx_message.header.id_src + 1; // Position 2
                  memcpy(&currentCryptoKey, rx_message.payload, 4);
                  currentNodeState = STATE_SYNCED;

                  ProtocolMessage_t tx_ack_msg;
                  memset(&tx_ack_msg, 0, sizeof(ProtocolMessage_t));
                  tx_ack_msg.header.start_byte = ICD_START_BYTE;
                  tx_ack_msg.header.command_type = CMD_ACK;
                  tx_ack_msg.header.length = 0;
                  tx_ack_msg.header.id_src = currentNodePosition;
                  tx_ack_msg.crc = Calculate_CRC16(&tx_ack_msg);

                  HAL_Delay(10);
                  HAL_UART_Transmit(&huart1, (uint8_t*)&tx_ack_msg, sizeof(ProtocolMessage_t), 1000);
              }
              // Cas 2 : Réception d'un message chiffré CMD_MSG
              else if (rx_message.header.command_type == CMD_MSG && currentNodeState == STATE_SYNCED)
              {
                  // Déchiffrement du payload avec la clé partagée
                  Cipher_Payload(rx_message.payload, rx_message.header.length, currentCryptoKey);

                  // Vérification que le texte déchiffré correspond bien à "CYTECH_SECRET_26"
                  if (memcmp(rx_message.payload, "CYTECH_SECRET_26", 16) == 0)
                  {
                      // Succès ! On fait clignoter la LED 3 fois rapidement
                      for (int i = 0; i < 6; i++) {
                          BSP_LED_Toggle(LED2);
                          HAL_Delay(80);
                      }

                      // Et on renvoie CMD_ACK au Maître
                      ProtocolMessage_t tx_ack_msg;
                      memset(&tx_ack_msg, 0, sizeof(ProtocolMessage_t));
                      tx_ack_msg.header.start_byte = ICD_START_BYTE;
                      tx_ack_msg.header.command_type = CMD_ACK;
                      tx_ack_msg.header.length = 0;
                      tx_ack_msg.header.id_src = currentNodePosition;
                      tx_ack_msg.crc = Calculate_CRC16(&tx_ack_msg);

                      HAL_Delay(10);
                      HAL_UART_Transmit(&huart1, (uint8_t*)&tx_ack_msg, sizeof(ProtocolMessage_t), 1000);
                  }
              }
          }
      }
  }
}

void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
    if (huart->Instance == USART1)
    {
        static uint8_t rx_index = 0;
        uint8_t *rx_buffer = (uint8_t *)&rx_message;

        if (rx_index == 0 && rx_byte != ICD_START_BYTE)
        {
            HAL_UART_Receive_IT(&huart1, &rx_byte, 1);
            return;
        }

        rx_buffer[rx_index++] = rx_byte;

        if (rx_index >= sizeof(ProtocolMessage_t))
        {
            message_received_flag = 1;
            rx_index = 0;
        }

        HAL_UART_Receive_IT(&huart1, &rx_byte, 1);
    }
}

static void MX_USART1_UART_Init(void)
{
  __HAL_RCC_USART1_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();

  huart1.Instance = USART1;
  huart1.Init.BaudRate = 115200;
  huart1.Init.WordLength = UART_WORDLENGTH_8B;
  huart1.Init.StopBits = UART_STOPBITS_1;
  huart1.Init.Parity = UART_PARITY_NONE;
  huart1.Init.Mode = UART_MODE_TX_RX;
  huart1.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  huart1.Init.OverSampling = UART_OVERSAMPLING_16;
  if (HAL_UART_Init(&huart1) != HAL_OK)
  {
    Error_Handler();
  }

  HAL_NVIC_SetPriority(USART1_IRQn, 0, 0);
  HAL_NVIC_EnableIRQ(USART1_IRQn);
}

static void MX_GPIO_Init(void)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};

  __HAL_RCC_GPIOC_CLK_ENABLE();
  __HAL_RCC_GPIOH_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();

  GPIO_InitStruct.Pin = GPIO_PIN_9 | GPIO_PIN_10;
  GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
  GPIO_InitStruct.Pull = GPIO_PULLUP;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
  GPIO_InitStruct.Alternate = GPIO_AF7_USART1;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);
}

void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  __HAL_RCC_PWR_CLK_ENABLE();
  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE3);

  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSI;
  RCC_OscInitStruct.PLL.PLLM = 16;
  RCC_OscInitStruct.PLL.PLLN = 336;
  RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV4;
  RCC_OscInitStruct.PLL.PLLQ = 2;
  RCC_OscInitStruct.PLL.PLLR = 2;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_2) != HAL_OK)
  {
    Error_Handler();
  }
}

void Error_Handler(void)
{
  __disable_irq();
  while (1) {}
}
