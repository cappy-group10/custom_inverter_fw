#include "F28x_Project.h"
#include "driverlib.h"
#include "device.h"

#define M1_EN_GATE_GPIO     124
#define PWM_DELAY           50

// Helper to configure a GPIO pin as a push-pull output
static inline void configOutputPin(uint32_t pin)
{
    GPIO_setPadConfig(pin, GPIO_PIN_TYPE_STD);          // Push-pull output
    GPIO_setDirectionMode(pin, GPIO_DIR_MODE_OUT);      // Set as output
    GPIO_setMasterCore(pin, GPIO_CORE_CPU1);            // Assign to CPU1
    GPIO_setQualificationMode(pin, GPIO_QUAL_SYNC);     // Sync qualification
}

void main(void)
{
    // Step 1: Initialize device clock and peripherals
    Device_init();

    // Step 2: Initialize GPIO — MUST come after Device_init()
    Device_initGPIO();

    // Step 3: Configure PWM pins (GPIO 0–5) as outputs
    configOutputPin(0);   // PWM1A
    configOutputPin(1);   // PWM1B
    configOutputPin(2);   // PWM2A
    configOutputPin(3);   // PWM2B
    configOutputPin(4);   // PWM3A
    configOutputPin(5);   // PWM3B

    // Step 4: Configure gate enable pin
    configOutputPin(M1_EN_GATE_GPIO);

    // Step 5: Set initial PWM pin states (complementary pairs)
    GPIO_writePin(0, 1);   // PWM1A high
    GPIO_writePin(1, 0);   // PWM1B low
    GPIO_writePin(2, 1);   // PWM2A high
    GPIO_writePin(3, 0);   // PWM2B low
    GPIO_writePin(4, 1);   // PWM3A high
    GPIO_writePin(5, 0);   // PWM3B low

    // Step 6: Enable gate driver AFTER pins are in a safe known state
    GPIO_writePin(M1_EN_GATE_GPIO, 1);

    while(true)
    {
        // Toggle complementary pairs together to avoid shoot-through
        GPIO_togglePin(0);   // PWM1A
        GPIO_togglePin(1);   // PWM1B
        DELAY_US(PWM_DELAY);

        GPIO_togglePin(2);   // PWM2A
        GPIO_togglePin(3);   // PWM2B
        DELAY_US(PWM_DELAY);

        GPIO_togglePin(4);   // PWM3A
        GPIO_togglePin(5);   // PWM3B
        DELAY_US(PWM_DELAY);
    }
}