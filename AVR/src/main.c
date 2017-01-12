#include <avr/io.h>
#include <util/delay.h>
#include <Arduino.h>
#include "rosnode.h"

#define MOTOR_MIN 50
#define MOTOR_MAX 950
#define ANALOG_READ_MIN 0
#define ANALOG_READ_MAX 1023

void arduino_init(void)
{
    init();
    #if defined(USBCON)
        USB.attach();
    #endif
}

long map(long x, long in_min, long in_max, long out_min, long out_max)
{
    return (x - in_min)*(out_max - out_min)/(in_max - in_min) + out_min;
}

int main(void)
{
    arduino_init();
    init_node();

    int pot = A0;
    pinMode(pot, INPUT);

    // Pin PD3 will be our output (pin 3 on arduino)
    DDRD |= _BV(PD3);

    // TCCR = Timer Counter Control Register. It is generally used for
    // prescaling the timer.
    // COM2A1 and COM2B1 means that OC2A and OC2B will be cleared on compare match.
    // WGM21 and WGM20 means that waveform generation mode is set to fast PWM
    TCCR2A = _BV(COM2A1) | _BV(COM2B1) | _BV(WGM21) | _BV(WGM20);

    // by setting WGM22, the fast PWM is now set to mode 7. This means that the
    // counter will reset on the OCR2A value rather than the default 255.
    // The prescaler divisor is set to 1 by setting CS20 only. This means that
    // the counter will run at the clock's full frequency (16 MHz).
    TCCR2B = _BV(WGM22) | _BV(CS20);

    OCR2A = 63;

    // This is the duty cycle. It is the last value of the counter our output
    // will remain high for. Can't be greater than OCR2A. A value of 0 means a
    // duty cycle of 1/64 in this case.
    OCR2B = 8;

    // flip direction back in forth and delay?
    DDRB |= _BV(PB5);

    int64_t pot_position;
    while (1) {
        pot_position = analogRead(pot);
        pot_position = map(pot_position, ANALOG_READ_MIN, ANALOG_READ_MAX,
                           MOTOR_MIN, MOTOR_MAX);
        publish_data(pot_position);
        _delay_ms(3000);
        PORTB ^= _BV(PORTB5); // flip the bit
    }
}