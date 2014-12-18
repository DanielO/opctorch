/* Torch implementation */

#include <err.h>
#include <stdint.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <sysexits.h>
#include <unistd.h>

#include "config.h"
#include "torch.h"

typedef struct {
	uint8_t	red;
	uint8_t	green;
	uint8_t	blue;
} __attribute((packed)) RGBPixel;

typedef struct {
	uint8_t		header[4];
	RGBPixel	pixels[0];
} __attribute((packed)) pixData_t;

static pixData_t *pixData = NULL;
static int pixDataSz;
static uint16_t	numleds;
static int	sock;

uint8_t *currentEnergy = NULL; // current energy level
uint8_t *nextEnergy = NULL; // next energy level
uint8_t *energyMode = NULL; // mode how energy is calculated for this point

static const uint8_t energymap[32] = {0, 64, 96, 112, 128, 144, 152, 160, 168, 176, 184, 184, 192, 200, 200, 208, 208, 216, 216, 224, 224, 224, 232, 232, 232, 240, 240, 240, 240, 248, 248, 248};

static void	setColour(uint16_t lednum, uint8_t red, uint8_t green, uint8_t blue);
static void	getColour(uint16_t lednum, uint8_t *red, uint8_t *green, uint8_t *blue);
static void	setColourDimmed(uint16_t lednum, uint8_t red, uint8_t green, uint8_t blue, uint8_t bright);
static void	sendLEDs(void);
static uint16_t	random16(uint16_t aMinOrMax, uint16_t aMax);
static void	reduce(uint8_t *aByte, uint8_t aAmount, uint8_t aMin);
static void	increase(uint8_t *aByte, uint8_t aAmount, uint8_t aMax);
static void	resetEnergy(void);
static void	calcNextEnergy(void);
static void	calcNextColours(void);
static void	injectRandom(void);

#define TORCH_PASSIVE 0 // Just environment, glob from nearby radiation
#define TORCH_NOP 1 // No processing
#define TORCH_SPARK 2 // Slowly loses energy, moves up
#define TORCH_SPARK_TEMP 3 // a spark still getting energy from the level below

/* Run forever sending messages to socket */
int
run_torch(int s)
{
	int err = 0;

	sock = s;

	numleds = LEDS_PER_LEVEL * TORCH_LEVELS;
	pixDataSz = sizeof(*pixData) + numleds * sizeof(pixData->pixels[0]);
	if ((pixData = malloc(pixDataSz)) == NULL) {
		err = -1;
		goto out;
	}
	if ((currentEnergy = malloc(numleds * sizeof(currentEnergy[0]))) == NULL) {
		err = -1;
		goto out;
	}
	if ((nextEnergy = malloc(numleds * sizeof(nextEnergy[0]))) == NULL) {
		err = -1;
		goto out;
	}
	if ((energyMode = malloc(numleds * sizeof(energyMode[0]))) == NULL) {
		err = -1;
		goto out;
	}

	pixData->header[0] = TORCH_CHAN;
	pixData->header[1] = 0; // Command: set LEDs
	pixData->header[2] = (numleds * sizeof(pixData->pixels[0])) >> 8; // Length MSB
	pixData->header[3] = (numleds * sizeof(pixData->pixels[0])) & 0xff; // Length LSB

	resetEnergy();
	//resetText();

	while (1) {
		// XXX: text handling
		injectRandom();
		calcNextEnergy();
		calcNextColours();

		sendLEDs();

		usleep(100000);
	}

 out:
	if (pixData != NULL) {
		free(pixData);
		pixData = NULL;
	}
	if (currentEnergy != NULL) {
		free(currentEnergy);
		currentEnergy = NULL;
	}
	if (nextEnergy != NULL) {
		free(nextEnergy);
		nextEnergy = NULL;
	}
	if (energyMode != NULL) {
		free(energyMode);
		energyMode = NULL;
	}

	return(err);
}

static void
setColour(uint16_t lednum, uint8_t red, uint8_t green, uint8_t blue)
{

	if (lednum >= numleds)
		return;
	pixData->pixels[lednum].red = red;
	pixData->pixels[lednum].green = green;
	pixData->pixels[lednum].blue = blue;
}

static void
setColourDimmed(uint16_t lednum, uint8_t red, uint8_t green, uint8_t blue, uint8_t bright)
{

	if (lednum >= numleds)
		return;
	pixData->pixels[lednum].red = (red * bright) >> 8;
	pixData->pixels[lednum].green = (green * bright) >> 8;
	pixData->pixels[lednum].blue = (blue * bright) >> 8;
}

static void
getColour(uint16_t lednum, uint8_t *red, uint8_t *green, uint8_t *blue)
{

	if (lednum >= numleds)
		return;
	*red = pixData->pixels[lednum].red;
	*green = pixData->pixels[lednum].green;
	*blue = pixData->pixels[lednum].blue;
}

static void
sendLEDs(void)
{
	int rtn;

	if ((rtn = send(sock, pixData, pixDataSz, 0)) < 0)
		err(EX_PROTOCOL, "Unable to send data");
}

static uint16_t
random16(uint16_t aMinOrMax, uint16_t aMax)
{
	uint32_t r;

	if (aMax == 0) {
	  aMax = aMinOrMax;
	  aMinOrMax = 0;
	}
	r = aMinOrMax;
	aMax = aMax - aMinOrMax + 1;
	r += rand() % aMax;
	return r;
}

static void
reduce(uint8_t *aByte, uint8_t aAmount, uint8_t aMin)
{
	int r;

	r = *aByte - aAmount;
	if (r < aMin)
		*aByte = aMin;
	else
		*aByte = (uint8_t)r;
}

static void
increase(uint8_t *aByte, uint8_t aAmount, uint8_t aMax)
{
	int r;

	r = *aByte + aAmount;
	if (r > aMax)
		*aByte = aMax;
	else
		*aByte = (uint8_t)r;
}

/* Input a value 0 to 255 to get a color value.
 * The colours are a transition r - g - b - back to r.
 */
static void
wheel(uint8_t WheelPos, uint8_t *red, uint8_t *green, uint8_t *blue) {
	if (WheelPos < 85) {
		*red = WheelPos * 3;
		*green = 255 - WheelPos * 3;
		*blue = 0;
	} else if(WheelPos < 170) {
		WheelPos -= 85;
		*red = 255 - WheelPos * 3;
		*green = 0;
		*blue = WheelPos * 3;
	} else {
		WheelPos -= 170;
		*red = 0;
		*green = WheelPos * 3;
		*blue = 255 - WheelPos * 3;
	}
}

static void
resetEnergy()
{
	int i;

	for (i = 0; i < numleds; i++) {
		currentEnergy[i] = 0;
		nextEnergy[i] = 0;
		energyMode[i] = TORCH_PASSIVE;
	}
}


static void
calcNextEnergy(void)
{
	int x, y, i;
	uint8_t e, m, e2;

	i = 0;
	for (y = 0; y < TORCH_LEVELS; y++) {
		for (x = 0; x < LEDS_PER_LEVEL; x++) {
			e = currentEnergy[i];
			m = energyMode[i];
			switch (m) {
			case TORCH_SPARK:
				// lose transfer up energy as long as there is any
				reduce(&e, SPARK_TFR, 0);
				// cell above is temp spark, sucking up energy from this cell until empty
				if (y < TORCH_LEVELS - 1) {
					energyMode[i + LEDS_PER_LEVEL] = TORCH_SPARK_TEMP;
				}
				break;

			case TORCH_SPARK_TEMP:
				// just getting some energy from below
				e2 = currentEnergy[i - LEDS_PER_LEVEL];
				if (e2 < SPARK_TFR) {
					// cell below is exhausted, becomes passive
					energyMode[i - LEDS_PER_LEVEL] = TORCH_PASSIVE;
					// gobble up rest of energy
					increase(&e, e2, 255);
					// loose some overall energy
					e = ((int)e * SPARK_CAP) >>8;
					// this cell becomes active spark
					energyMode[i] = TORCH_SPARK;
				} else {
					increase(&e, SPARK_TFR, 255);
				}
				break;
			case TORCH_PASSIVE:
				e = ((int)e * HEAT_CAP) >> 8;
				increase(&e, ((((int)currentEnergy[i - 1] + (int)currentEnergy[i + 1]) * SIDE_RAD) >> 9) +
				    (((int)currentEnergy[i - LEDS_PER_LEVEL] * UP_RAD) >> 8), 255);

			default:
				break;
			}
			nextEnergy[i++] = e;
		}
	}
}

static void
calcNextColours(void)
{
	int i, ei;
	uint16_t e;
	uint8_t eb, r, g, b;
	// XXX: add text overlay

	for (i = 0; i < numleds; i++) {
		if (UPSIDE_DOWN)
			ei = numleds - i;
		else
			ei = i;
		e = nextEnergy[ei];
		currentEnergy[ei] = e;
		if (e > 250)
			setColourDimmed(i, 170, 170, e, BRIGHTNESS); // blueish extra-bright spark
		else {
			if (e > 0) {
				// energy to brightness is non-linear
				eb = energymap[e >> 3];
				r = RED_BIAS;
				g = GREEN_BIAS;
				b = BLUE_BIAS;
				increase(&r, (eb * RED_ENERGY) >> 8, 255);
				increase(&g, (eb * GREEN_ENERGY) >> 8, 255);
				increase(&b, (eb * BLUE_ENERGY) >> 8, 255);
				setColourDimmed(i, r, g, b, BRIGHTNESS);
			} else {
				// background, no energy
				setColourDimmed(i, RED_BG, GREEN_BG, BLUE_BG, BRIGHTNESS);
			}
		}
	}
}

static void
injectRandom(void)
{
	int i;

	// random flame energy at bottom row
	for (i = 0; i < LEDS_PER_LEVEL; i++) {
		currentEnergy[i] = random16(FLAME_MIN, FLAME_MAX);
		energyMode[i] = TORCH_NOP;
	}
	// random sparks at second row
	for (i = LEDS_PER_LEVEL; i < 2 *LEDS_PER_LEVEL; i++) {
		if (energyMode[i] != TORCH_SPARK && random16(100, 0) < RND_SPARK_PROB) {
			currentEnergy[i] = random16(SPARK_MIN, SPARK_MAX);
			energyMode[i] = TORCH_SPARK;
		}
	}
}
