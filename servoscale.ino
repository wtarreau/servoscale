// Measures servo pulses received on PB4 (pin3), determines the current
// state (INI, STP, FWD, REV, BRK), applies a factor and sends the new
// pulse on PB3 (pin2). Useful to reduce the amplitude of incoming commands
// for training, without removing ability to brake or to drift a little
// bit.
//
// needs wiring_pulse.c and wiring.c

#define LED 1
#define PIN_IN 4
#define PIN_OUT 3

// pulse margin around center, in microseconds
#define MARGIN 40

// detect full throttle in microseconds
#define FWDFULL  300

// 20 * 20ms = 400ms max burst duration and cancellation delay
#define MAXBURST 20

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
int duration = 0;
int nobst = 0;
int offset = 0;

void setup()
{
  pinMode(PIN_IN, INPUT);
  pinMode(PIN_OUT, OUTPUT);
  pinMode(LED, OUTPUT);
}

void loop()
{
  int len;

  // we read such a pulse every 20 ms
  len = pulseIn(PIN_IN, HIGH); // time in microseconds

  // center is at 1500 microseconds.
  len -= 1500;
  if (state != CTR)
    len += offset;

  // calibration: turn on the LED for non-rest positions
  digitalWrite(LED, (state == CTR || len >= -1500 && len <= -MARGIN || len >= MARGIN && len <= 1500) ? HIGH : LOW);

  switch (state) {
    case CTR :
      if (len < -1500 || len > 1500) {
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

  // debug: show state changes
  //digitalWrite(LED, (duration < 5) ? HIGH : LOW);

  // scale pulse width depending on direction. It may also be useful
  // to increase the braking strength.
  switch (state) {
    case FWD :
      // support short bursts at full speed, but disable them as soon as
      // we start.
      if (++nobst >= MAXBURST)
          nobst = 2 * MAXBURST;

      // limit forward speed unless we're exceptionally tolerating a burst
      if (len < FWDFULL || nobst >= MAXBURST)
        len = len * 2 / 5;
      break;

    case REV :
      len = len * 2 / 4; // backward scaling

      if (--nobst < 0)
        nobst = 0;
      break;

    default:
      if (--nobst < 0)
        nobst = 0;
      break;
  }

  // send new pulse
  len += 1500;
  digitalWrite(PIN_OUT, HIGH);
  delayMicroseconds(len);
  digitalWrite(PIN_OUT, LOW);

  // update measure of current state duration
  duration++;
  if (duration > 1000)
      duration = 1000;
}

