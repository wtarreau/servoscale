/* Servoscale - 2017-05-06 - Willy Tarreau - public domain.
 *
 * Measures servo pulses received on PB4 (pin3), determines the current
 * state (INI, STP, FWD, REV, BRK), applies a factor and sends the new
 * pulse on PB3 (pin2). Useful to reduce the amplitude of incoming commands
 * for training, without removing ability to brake or to drift a little
 * bit.
 * PB2 can be connected to 3 leds to power the REAR light and the brakes
 * like this by setting it to input, outlow, outhigh :
 *                  + VCC                  //
 *                  |                      // or safer for the leds, place
 *         100      V D1 (white) REAR      // two 100 ohms resistors between
 *  PB2 --vvvvv-----+                      // D1 and D2, and connect PB2 in
 *                  |                      // the middle. It will ensure that
 *                  V D2 (red)  BRAKE      // voltage peaks don't destroy the
 *                  T                      // leds.
 *                  V D3 (red)             //
 *                  T
 *                 /// GND
 *
 * A second function is implemented : PB0 (pin 5) can be used to power front
 * lights when connected to the 3rd channel. It only discriminates the pulse
 * width.
 */

#ifndef F_CPU
#define F_CPU 8000000
#endif

#include <avr/io.h>
#include <stdlib.h>

// pulse margin around center, in microseconds
#define MARGIN 40

// detect full throttle in microseconds
#define FWDFULL  400

// 15 * 20ms = 300ms max burst duration and cancellation delay
#define MAXBURST 15

/*
 * state transitions :
 *
 *   CTR = auto centering at boot
 *   CTR ====(dur=1s)=======> INI
 *
 *   INI = initial state
 *   INI ====(pulse < 0)====> REV
 *   INI ====(pulse > 0)====> FWD
 *
 *   REV = reverse
 *   REV ====(pulse = 0)====> INI
 *   REV ====(pulse > 0)====> FWD
 *
 *   FWD = forward
 *   FWD ====(pulse < 0)====> BRK
 *   FWD ====(pulse = 0)====> STP
 *
 *   STP = stop (stop accelerating), can't go backwards but can brake
 *   STP ====(pulse < 0)====> BRK
 *   STP ====(pulse > 0)====> FWD
 *
 *   BRK = braking : waiting for the trigger to be released
 *   BRK ====(pulse = 0)====> INI
 *   BRK ====(pulse > 0)====> FWD
 *
 * Scaling is only applied to FWD and REV states.
 *
 */
enum {
	CTR,
	INI,
	REV,
	FWD,
	STP,
	BRK,
} state = CTR;

// duration in pulses in the current state (used to allow some burst periods)
uint8_t duration = 0;
uint8_t led = 1;
int8_t nobst = 0;
int16_t offset = 0;

/* wait for a positive pulse on PB4 and return its width in microseconds */
static inline uint16_t pulse_width(void)
{
	uint16_t width = 0;

	/* 4 phases :  XXXX___---___
	 *               0  1  2  3
	 */
	while (PINB & (1 << PB4));
	/* now low, phase 1 */
	while (!(PINB & (1 << PB4)));
	/* high, phase 2, measure it.
	 * Note: this loop was measured to take 5 cycles per loop */
	while (PINB & (1 << PB4))
		width += 5;
	/* low again, phase 3 */

	/* <width> is the number of cycles, turn it into microseconds */
	if (F_CPU % 1000000 == 0)
		width = width / (F_CPU / 1000000);
	else
		width = (uint32_t)width * 10 / (F_CPU / 100000);

	return width;
}

/* send a positive pulse of <width> microseconds on PB3 */
static inline void send_pulse(uint16_t width)
{
	/* each loop takes 4 cycles (measured) */
	width = width * (F_CPU / 1000000) / 4 + 1;

	PORTB |= 1 << PB3;
	while (--width) asm volatile("");
	PORTB &= ~(1 << PB3);
}

static inline void show_led(void)
{
	PORTB = (led << PB1) | (PORTB & ~(1 << PB1));
}

int main(void)
{
	int16_t len;

	/* PB#  pin  dir  role
	 * PB3   2   out  (pulse-out)
	 * PB4   3    in  (pulse-in)
	 * PB0   5   out  (front light)
	 * PB1   6   out  (debug)
	 * PB2   7   i/o  (brake/rear)
	 */
	DDRB = 1<<DDB3 | 1<<DDB1 | 1<<DDB0;

#ifdef DEBUG_BIT_PASSTHROUGH
	while (1) {
		if (PINB & (1 << PB4))
			PORTB |= (1 << PB3) | (1 << PB1);
		else
			PORTB &= ~((1 << PB3) | (1 << PB1));
	}
#endif

#ifdef DEBUG_PULSE_PASSTHROUGH
	while (1) {
		len = pulse_width();
		if (len < 1500)
			PORTB &= ~(1 << PB1);
		else
			PORTB |= (1 << PB1);

		send_pulse(len);
	}
#endif

	while (1) {
		/* wait for a pulse. It usually lasts 20ms except during pairing
		 * at boot where it takes forever. We turn the LED on during
		 * this time, it helps seeing we've paired. During normal ops
		 * the led status is controlled by debugging states.
		 */
		show_led();
		len = pulse_width();
		led = 0;
		show_led();

		/* front light on PB0 */
		if (len < 1400)
			PORTB &= ~(1 << PB0);
		else
			PORTB |= (1 << PB0);

		// center is at 1500 microseconds.
		len -= 1500;
		if (state != CTR)
			len += offset;

		switch (state) {
		case CTR :
			if (len < -500 || len > 500) {
				/* wait for a valid signal to start measuring */
				duration--;
				break;
			}
			if (duration >= 10) {
				/* cumulate imprecision over the last 10 samples */
				if (len < 0)
					offset -= len;
				else
					offset += len;
			}
			if (duration >= 20) {
				offset /= 10;
				state = INI;
				duration = 0;
			}
			break;
		case INI :
			if (len >= MARGIN) {
				state = FWD;
				duration = 0;
			}
			else if (len <= -MARGIN) {
				state = REV;
				duration = 0;
			}
			break;
		case FWD :
			if (len <= -MARGIN) {
				state = BRK;
				duration = 0;
			}
			else if (len > -MARGIN && len < MARGIN) {
				if (duration >= 4) { // avoid jitter during throttle manipulation
					state = STP;
					duration = 0;
				}
			}
			break;
		case STP :
			if (len >= MARGIN) {
				state = FWD;
				duration = 0;
			}
			else if (len <= -MARGIN) {
				state = BRK;
				duration = 0;
			}
			break;
		case BRK :
		case REV :
			if (len >= MARGIN) {
				state = FWD;
				duration = 0;
			}
			else if (len > -MARGIN && len < MARGIN) {
				if (duration >= 4) { // avoid jitter during throttle manipulation
					state = INI;
					duration = 0;
				}
			}
			break;
		}

		if (state == BRK) {
			/* PB2 as output to VCC => red leds */
			DDRB |= 1 << DDB2;
			PORTB |= 1 << PB2;
		}
		else if (state == REV) {
			/* PB2 as output to GND => white led */
			DDRB |= 1 << DDB2;
			PORTB &= ~(1 << PB2);
		}
		else {
			/* configure as input, the pull-up is too weak to light
			 * two red lights in series.
			 */
			DDRB &= ~(1<<DDB2);
		}

		// scale pulse width depending on direction. It may also be useful
		// to increase the braking strength.
		switch (state) {
		case FWD :
			// support short bursts at full speed, but disable them as soon as
			// we start.
			if (++nobst >= MAXBURST)
				nobst = 2 * MAXBURST;

			// limit forward speed unless we're exceptionally tolerating a burst
			if (len < FWDFULL || nobst >= MAXBURST) {
				len = len * 2 / 5;
				led = 1; // show we're limited
			}
			break;

		case REV :
			len = len * 2 / 3; // backward scaling
			led = 1; // show we're limited
			if (--nobst < 0)
				nobst = 0;
			break;

		case CTR :
			led = 1; // show we're syncing.
			break;

		default:
			led = 0;
			if (--nobst < 0)
				nobst = 0;
			break;
		}

		// send new pulse
		len += 1500;
		send_pulse(len);

		// update measure of current state duration
		if (duration < 255)
			duration++;
	}
}
