/* onoff - 2017-05-07 - Willy Tarreau - public domain.
 *
 * Measures servo pulses received on PB2 (pin7), and sets PB0 (pin5) on or
 * or off depending on the pulses width, and PB1 (pin6) to the opposite
 * value. PB0 is set on short pulses, PB1 on long pulses.
 *
 * Build : make onoff.hex
 */
#include <avr/io.h>

/* ratio to turn microseconds to cycles. Avoid integer overflow by multiplying
 * by kilohertz only
 */
#define US * (F_CPU / 1000) / 1000

int main(void)
{
	uint16_t width;

	/* PB0 and PB1 are the two outputs */
	DDRB = 1<<DDB1 | 1<<DDB0;

	while (1) {
		while (!(PINB & (1 << PB2)));

		/* Note: this loop was measured to take 5 cycles per loop */
		width = 0;
		while (PINB & (1 << PB2))
			width += 5;

		if (width <= 1400 US)
			PORTB = 1 << PB0;
		else if (width >= 1600 US)
			PORTB = 1 << PB1;
	}
}
