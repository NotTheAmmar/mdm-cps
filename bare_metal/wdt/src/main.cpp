#include <Arduino.h>

#define LED_ORANGE       13
#define WDT_TIMEOUT_8S   (8 * 32768) // 8-second safety timeline pool
#define WDT_FEED_KEY     0x6E528035 

// Safe macros utilizing standard API to completely avoid Mbed OS memory bus HardFaults
#define TURN_WHITE_ON()  { digitalWrite(LEDR, LOW); digitalWrite(LEDG, LOW); digitalWrite(LEDB, LOW); }
#define TURN_WHITE_OFF() { digitalWrite(LEDR, HIGH); digitalWrite(LEDG, HIGH); digitalWrite(LEDB, HIGH); }

void setup()
{
    // Configure pin states safely
    pinMode(LED_ORANGE, OUTPUT);
    pinMode(LEDR, OUTPUT);
    pinMode(LEDG, OUTPUT);
    pinMode(LEDB, OUTPUT);
    
    TURN_WHITE_OFF();
    digitalWrite(LED_ORANGE, LOW);

    // Read the pure hardware register directly to see if the WDT just caught a freeze
    uint32_t reset_reason = NRF_POWER->RESETREAS;

    if (reset_reason & 0x02)
    {
        // Clear the watchdog bit
        NRF_POWER->RESETREAS = 0x02;

        // INDICATE RESET: Keep Orange LED solid high for 2 full seconds
        digitalWrite(LED_ORANGE, HIGH);
        delay(2000);
        digitalWrite(LED_ORANGE, LOW);
    }
    else
    {
        // Normal Boot: Strobe Orange slowly 3 times
        for (int i = 0; i < 3; i++) {
            digitalWrite(LED_ORANGE, HIGH);  delay(200);
            digitalWrite(LED_ORANGE, LOW);   delay(200);
        }
    }
    
    delay(500); // Stabilization pause

    // Arm the bare-metal Watchdog Timer peripheral registers safely
    NRF_WDT->CONFIG = 0x01;       
    NRF_WDT->CRV = WDT_TIMEOUT_8S; // 8-second countdown
    NRF_WDT->RREN = 0x01;         
    NRF_WDT->TASKS_START = 1;     
}

void loop()
{
    // 1. Normal Routine: Blink White RGB 5 times smoothly (1 second per full cycle)
    for (int blinks = 0; blinks < 5; blinks++)
    {
        // Pet the dog right at the beginning of the sequence
        NRF_WDT->RR[0] = WDT_FEED_KEY;

        TURN_WHITE_ON();
        delay(500);
        
        TURN_WHITE_OFF();
        delay(500); 
    }

    // 2. Failure Induction: Go dark and trap the CPU
    TURN_WHITE_OFF();

    // Infinite freeze loop simulation. Watchdog will force a reboot in exactly 3 seconds 
    // (since 5 seconds were spent blinking out of the 8-second window pool).
    while (1)
    {
        // Stalled state
    }
}