#include <Arduino.h>
#include <nrf.h>

#define LED_PIN 13

volatile bool led_state = false;

extern "C" void TIMER3_IRQHandler(void);

void setup()
{
  NRF_P0->PIN_CNF[LED_PIN] = (1 << 0);

  NRF_TIMER3->TASKS_STOP = 1;
  NRF_TIMER3->TASKS_CLEAR = 1;
  NRF_TIMER3->MODE = 0;
  NRF_TIMER3->BITMODE = 3;

  NRF_TIMER3->PRESCALER = 4;
  NRF_TIMER3->CC[0] = 2000000;

  NRF_TIMER3->SHORTS = (1 << 0);
  NRF_TIMER3->INTENSET = (1 << 16);

  NVIC_SetVector(TIMER3_IRQn, (uint32_t)TIMER3_IRQHandler);
  NVIC_EnableIRQ(TIMER3_IRQn);

  NRF_TIMER3->TASKS_START = 1;
}

void loop()
{
  delay(10);
}

extern "C" void TIMER3_IRQHandler(void)
{
  if (NRF_TIMER3->EVENTS_COMPARE[0])
  {
    NRF_TIMER3->EVENTS_COMPARE[0] = 0;

    led_state = !led_state;
    if (led_state)
    {
      NRF_P0->OUTSET = (1 << LED_PIN);
    }
    else
    {
      NRF_P0->OUTCLR = (1 << LED_PIN);
    }
  }
}
