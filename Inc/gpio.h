#ifndef GPIO_H
#define GPIO_H

#include "stm32f4xx.h"      // Include CMSIS register definitions 
#include <stdint.h>

// Bit masks for enabling GPIOx clock 
#define GPIOA_CLK_EN            (1U << 0)
#define GPIOB_CLK_EN            (1U << 1)
#define GPIOC_CLK_EN            (1U << 2)
#define GPIOD_CLK_EN            (1U << 3)
#define GPIOE_CLK_EN            (1U << 4)
#define GPIOF_CLK_EN            (1U << 5)
#define GPIOG_CLK_EN            (1U << 6)
#define GPIOH_CLK_EN            (1U << 7)
#define GPIOI_CLK_EN            (1U << 8)

/* Generic GPIO BSRR Bit Set (BSy) and Bit Rest (BRy) macros
  These macros take a pin number (0-15) and return the correct mask
  to set (mask high) or reset (mask low) that specific pin via BSRR.
*/
#define GPIO_PIN_SET(pin_number)            (1U << (pin_number))            // For bit set [0:15]
#define GPIO_PIN_RESET(pin_number)          (1U << ((pin_number) + 16))     // For bit reset [16:31]

// GPIOx mode value definitions
// Usage example: GPIOD->MODER |= (GPIO_MODE_OUTPUT << (pin_number * 2));
typedef enum {
  GPIO_MODE_INPUT         = 0x00, // 0b00 (reset state)
	GPIO_MODE_OUTPUT        = 0x01, // 0b01
	GPIO_MODE_ALTERNATE     = 0x02, // 0b10
	GPIO_MODE_ANALOG        = 0x03, // 0b11
} GPIO_Mode_TypeDef;

// GPIOx output type value definitions
typedef enum {
    PUSH_PULL               = 0x00, // 0b00 (reset state)
    OPEN_DRAIN              = 0x01, // 0b01
} GPIO_OutType_TypeDef;

// GPIOx output speed value definitions
typedef enum {
    LOW_SPEED               = 0x00, // 0b00
    MEDIUM_SPEED            = 0x01, // 0b01
    HIGH_SPEED              = 0x02, // 0b10
    VERY_HIGH_SPEED         = 0x03, // 0b11
} GPIO_OutSpeed_TypeDef;

// GPIOx pull-up/pull-down value definitions
typedef enum {
    NO_PUPD                 = 0x00, // 0b00
    PULL_UP                 = 0x01, // 0b01
    PULL_DOWN               = 0x02, // 0b10
} GPIO_PuPd_TypeDef;

typedef enum {
    AF0                     = 0x00, // System Functions (MCO, JTAG, TRACE)
    AF1                     = 0x01, // TIM1/TIM2
    AF2                     = 0x02, // TIM3/TIM4/TIM5
    AF3                     = 0x03, // TIM8/TIM9/TIM10/TIM11
    AF4                     = 0x04, // I2C1/I2C2/I2C3
    AF5                     = 0x05, // SPI1/SPI2/I2S2, SPI3/I2S3, SPI4
    AF6                     = 0x06, // SPI3/I2S3, SAI1
    AF7                     = 0x07, // USART1/USART2/USART3
    AF8                     = 0x08, // UART4/UART5/USART6
    AF9                     = 0x09, // CAN1/CAN2, TIM12/TIM13/TIM14
    AF10                    = 0x0A, // OTG_FS/OTG_HS
    AF11                    = 0x0B, // ETH
    AF12                    = 0x0C, // FSMC/SDIO/OTG_HS
    AF13                    = 0x0D, // DCMI
    AF14                    = 0x0E, // (Reserved)
    AF15                    = 0x0F, // EVENTOUT
} GPIO_Af_TypeDef;

typedef enum PIN_STATE {
    OFF,
    ON,
} PinState_TypeDef;

/**
  * @brief  Sets the operating mode for a specific GPIO pin.
  * @param  GPIOx: where x can be (A..I) to select the GPIO peripheral for the STM32F407.
  * @param  pin_number: The pin number (0-15).
  * @param  mode: The operating mode (Input, Output, Alternate, Analog).
  */
static inline void GPIO_SetMode(GPIO_TypeDef* GPIOx, uint8_t pin_number, GPIO_Mode_TypeDef mode)
{
    GPIOx->MODER &= ~(0b11UL << (pin_number * 2));      // Clear the current mode bits [2y:2y+1]
    GPIOx->MODER |= (mode << (pin_number * 2));         // Set the mode to General Purpose Output
}

/**
  * @brief  Sets the output type for a specific GPIO pin.
  * @param  GPIOx: where x can be (A..I) to select the GPIO peripheral for the STM32F407.
  * @param  pin_number: The pin number (0-15).
  * @param  out_type: The output type (Push-Pull or Open-Drain).
  */
static inline void GPIO_SetOutType(GPIO_TypeDef* GPIOx, uint8_t pin_number, GPIO_OutType_TypeDef out_type)
{
    GPIOx->OTYPER &= ~(1UL << pin_number);      // Clear the current Output Type bit
    GPIOx->OTYPER |= (out_type << pin_number);  // Set the new Output Type
}

/**
  * @brief  Sets the pull-up/pull-down resistor for a specific GPIO pin.
  * @param  GPIOx: where x can be (A..I) to select the GPIO peripheral for the STM32F407.
  * @param  pin_number: The pin number (0-15).
  * @param  pu_pd: The pull-up/pull-down configuration (No PUPD, Pull-up, Pull-down).
  */
static inline void GPIO_SetPuPd(GPIO_TypeDef* GPIOx, uint8_t pin_number, GPIO_PuPd_TypeDef pu_pd)
{
    GPIOx->PUPDR &= ~(0b11 << (pin_number * 2));   // Clear the current pull-up/pull-down bits [2y:2y+1]
    GPIOx->PUPDR |= (pu_pd << (pin_number * 2)); // Set  to No Pull-up/Pull-down
}

/**
  * @brief  Sets the output speed for a specific GPIO pin.
  * @param  GPIOx: where x can be (A..I) to select the GPIO peripheral for the STM32F407.
  * @param  pin_number: The pin number (0-15).
  * @param  out_speed: The output speed (Low, Medium, High, Very High).
  */
static inline void GPIO_SetOutSpeed(GPIO_TypeDef* GPIOx, uint8_t pin_number, GPIO_OutSpeed_TypeDef out_speed)
{
    GPIOx->OSPEEDR &= ~(0b11 << (pin_number * 2));     // Clear the current speed bits [2y:2y+1]
    GPIOx->OSPEEDR |= (out_speed << (pin_number * 2)); // Set to Low-Speed
}

/**
  * @brief  Configures the alternate function for a specific GPIO pin.
  * @note   This function should be called only after setting the pin mode to GPIO_MODE_ALTERNATE.
  * @param  GPIOx: where x can be (A..I) to select the GPIO peripheral for the STM32F407.
  * @param  pin_number: The pin number (0-15).
  * @param  af_value: The alternate function value (AF0-AF15) from GPIO_Af_TypeDef.
  */
static inline void GPIO_SetAlternateFunction(GPIO_TypeDef* GPIOx, uint8_t pin_number, GPIO_Af_TypeDef af_value)
{
    if (pin_number < 8)
    {
        // Low register (AFRL) for pins 0-7
        uint32_t shift = pin_number * 4;
        GPIOx->AFR[0] &= ~(0xFUL << shift); // Clear the 4 bits for the pin
        GPIOx->AFR[0] |= ((uint32_t)af_value << shift); // Set the new AF value
    }
    else
    {
        // High register (AFRH) for pins 8-15
        uint32_t shift = (pin_number - 8) * 4;
        GPIOx->AFR[1] &= ~(0xFUL << shift); // Clear the 4 bits for the pin
        GPIOx->AFR[1] |= ((uint32_t)af_value << shift); // Set the new AF value
    }
}

#endif // GPIO_H
