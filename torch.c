/* Torch implementation */

#include <err.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sysexits.h>
#include <unistd.h>

#include "config.h"
#include "font.h"
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

static uint8_t *currentEnergy = NULL; // current energy level
static uint8_t *nextEnergy = NULL; // next energy level
static uint8_t *energyMode = NULL; // mode how energy is calculated for this point

static const uint8_t energymap[32] = {0, 64, 96, 112, 128, 144, 152, 160, 168, 176, 184, 184, 192, 200, 200, 208, 208, 216, 216, 224, 224, 224, 232, 232, 232, 240, 240, 240, 240, 248, 248, 248};

static int textPixels;
static uint8_t *textLayer;
static char text[100];
static int textLen;
static int textPixelOffset;
static int textCycleCount;
static int repeatCount;

static void	setColourDimmed(uint16_t, uint8_t, uint8_t, uint8_t, uint8_t);
static void	sendLEDs(void);
static uint16_t	random16(uint16_t, uint16_t);
static void	sat8sub(uint8_t *, uint8_t);
static void	sat8add(uint8_t *, uint8_t);
static void	resetEnergy(void);
static void	resetText(void);
static void	calcNextEnergy(struct config_t *);
static void	calcNextColours(struct config_t *);
static void	injectRandom(struct config_t *);
static void	renderText(struct config_t *);
static void	crossFade(struct config_t *, uint8_t, uint8_t, uint8_t *, uint8_t *);

#define TORCH_PASSIVE		0 // Just environment, glow from nearby radiation
#define TORCH_NOP		1 // No processing
#define TORCH_SPARK		2 // Slowly loses energy, moves up
#define TORCH_SPARK_TEMP	3 // A spark still getting energy from the level below

/* Set default config */
void
default_conf(struct config_t *conf)
{

	memset(conf, 0, sizeof(*conf));
	conf->leds_per_level = 26;
	conf->torch_levels = 9;
	conf->wound_cwise = 0;
	conf->torch_chan = 1;
	conf->brightness = 255;
	conf->fade_base = 140;
	conf->text_intensity = 255;
	conf->text_cycles_per_px = 5;
	conf->fade_per_repeat = 15;
	conf->text_base_line = 3;
	conf->text_red = 0;
	conf->text_green = 255;
	conf->text_blue = 180;
	conf->flame_min = 100;
	conf->flame_max = 220;
	conf->rnd_spark_prob = 2;
	conf->spark_min = 200;
	conf->spark_max = 255;
	conf->spark_tfr = 40;
	conf->spark_cap = 200;
	conf->up_rad = 40;
	conf->side_rad = 35;
	conf->heat_cap = 0;
	conf->red_bg = 0;
	conf->green_bg = 0;
	conf->blue_bg = 0;
	conf->red_bias = 10;
	conf->green_bias = 0;
	conf->blue_bias = 0;
	conf->red_energy = 180;
	conf->green_energy = 145;
	conf->blue_energy = 0;
	conf->upside_down = 0;
	conf->update_rate = 30;
}

/* Run forever sending messages to socket */
int
run_torch(int s, struct config_t *conf)
{
	int err, sl, t, frame;
	struct timeval then, now;
	char tmp[100];

	err = frame = 0;
	sock = s;

	numleds = conf->leds_per_level * conf->torch_levels;
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
	textPixels = conf->leds_per_level * ROWS_PER_GLYPH;
	if ((textLayer = malloc(textPixelOffset * sizeof(textLayer[0]))) == NULL) {
		err = -1;
		goto out;
	}
	pixData->header[0] = conf->torch_chan;
	pixData->header[1] = 0; // Command: set LEDs
	pixData->header[2] = (numleds * sizeof(pixData->pixels[0])) >> 8; // Length MSB
	pixData->header[3] = (numleds * sizeof(pixData->pixels[0])) & 0xff; // Length LSB

	resetEnergy();
	resetText();

	while (1) {
		if (frame % 300 == 0) {
			snprintf(tmp, sizeof(tmp) - 1, "%d", frame);
			tmp[sizeof(tmp) - 1] = 0;
			newMessage(conf, tmp);
		}
		frame++;
		gettimeofday(&then, NULL);
		renderText(conf);
		injectRandom(conf);
		calcNextEnergy(conf);
		calcNextColours(conf);

		sendLEDs();
		gettimeofday(&now, NULL);
		timersub(&now, &then, &now);
		t = now.tv_sec * 1000000 + now.tv_usec;
		sl = (1000000 / conf->update_rate) - t;
		if (frame % 10 == 0)
			printf("%5d: Loop took %5d usec, sleeping %5d usec\n", frame, t, sl);

		usleep(sl);
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
	if (textLayer != NULL) {
		free(textLayer);
		textLayer = NULL;
	}
	return(err);
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
sat8sub(uint8_t *aByte, uint8_t aAmount)
{
	int r;

	r = *aByte - aAmount;
	if (r < 0)
		*aByte = 0;
	else
		*aByte = (uint8_t)r;
}

static void
sat8add(uint8_t *aByte, uint8_t aAmount)
{
	int r;

	r = *aByte + aAmount;
	if (r > 255)
		*aByte = 255;
	else
		*aByte = (uint8_t)r;
}

static void
resetEnergy(void)
{
	int i;

	for (i = 0; i < numleds; i++) {
		currentEnergy[i] = 0;
		nextEnergy[i] = 0;
		energyMode[i] = TORCH_PASSIVE;
	}
}


static void
resetText(void)
{
	int i;

	for(i = 0; i < textPixels; i++) {
		textLayer[i] = 0;
	}
}

static void
calcNextEnergy(struct config_t *conf)
{
	int x, y, i;
	uint8_t e, m, e2;

	i = 0;
	for (y = 0; y < conf->torch_levels; y++) {
		for (x = 0; x < conf->leds_per_level; x++) {
			e = currentEnergy[i];
			m = energyMode[i];
			switch (m) {
			case TORCH_SPARK:
				// lose transfer up energy as long as there is any
				sat8sub(&e, conf->spark_tfr);
				// cell above is temp spark, sucking up energy from this cell until empty
				if (y < conf->torch_levels - 1) {
					energyMode[i + conf->leds_per_level] = TORCH_SPARK_TEMP;
				}
				break;

			case TORCH_SPARK_TEMP:
				// just getting some energy from below
				e2 = currentEnergy[i - conf->leds_per_level];
				if (e2 < conf->spark_tfr) {
					// cell below is exhausted, becomes passive
					energyMode[i - conf->leds_per_level] = TORCH_PASSIVE;
					// gobble up rest of energy
					sat8add(&e, e2);
					// loose some overall energy
					e = ((int)e * conf->spark_cap) >>8;
					// this cell becomes active spark
					energyMode[i] = TORCH_SPARK;
				} else {
					sat8add(&e, conf->spark_tfr);
				}
				break;
			case TORCH_PASSIVE:
				e = ((int)e * conf->heat_cap) >> 8;
				sat8add(&e, ((((int)currentEnergy[i - 1] + (int)currentEnergy[i + 1]) * conf->side_rad) >> 9) +
				    (((int)currentEnergy[i - conf->leds_per_level] * conf->up_rad) >> 8));

			default:
				break;
			}
			nextEnergy[i++] = e;
		}
	}
}

static void
calcNextColours(struct config_t *conf)
{
	int i, ei, textStart, textEnd;
	uint16_t e;
	uint8_t eb, r, g, b;

	textStart = conf->text_base_line * conf->leds_per_level;
	textEnd = textStart + ROWS_PER_GLYPH * conf->leds_per_level;

	for (i = 0; i < numleds; i++) {
		if (i >= textStart && i < textEnd && textLayer[i - textStart] > 0) {
			// overlay with text color
			setColourDimmed(i, conf->text_red, conf->text_green, conf->text_blue,
			    (conf->brightness * textLayer[i - textStart]) >> 8);
		} else {
			if (conf->upside_down)
				ei = numleds - i;
			else
				ei = i;
			e = nextEnergy[ei];
			currentEnergy[ei] = e;
			if (e > 250)
				setColourDimmed(i, 170, 170, e, conf->brightness); // blueish extra-bright spark
			else {
				if (e > 0) {
					// energy to brightness is non-linear
					eb = energymap[e >> 3];
					r = conf->red_bias;
					g = conf->green_bias;
					b = conf->blue_bias;
					sat8add(&r, (eb * conf->red_energy) >> 8);
					sat8add(&g, (eb * conf->green_energy) >> 8);
					sat8add(&b, (eb * conf->blue_energy) >> 8);
					setColourDimmed(i, r, g, b, conf->brightness);
				} else {
					// background, no energy
					setColourDimmed(i, conf->red_bg, conf->green_bg, conf->blue_bg, conf->brightness);
				}
			}
		}
	}
}

static void
injectRandom(struct config_t *conf)
{
	int i;

	// random flame energy at bottom row
	for (i = 0; i < conf->leds_per_level; i++) {
		currentEnergy[i] = random16(conf->flame_min, conf->flame_max);
		energyMode[i] = TORCH_NOP;
	}
	// random sparks at second row
	for (i = conf->leds_per_level; i < 2 *conf->leds_per_level; i++) {
		if (energyMode[i] != TORCH_SPARK && random16(100, 0) < conf->rnd_spark_prob) {
			currentEnergy[i] = random16(conf->spark_min, conf->spark_max);
			energyMode[i] = TORCH_SPARK;
		}
	}
}

void
newMessage(struct config_t *conf, char *msg)
{
	strlcpy(text, msg, sizeof(text));
	textLen = strlen(text);
	textPixelOffset = -conf->leds_per_level;
	textCycleCount = 0;
	repeatCount = 0;
}

static
void crossFade(struct config_t *conf, uint8_t aFader, uint8_t aValue, uint8_t *aOutputA, uint8_t *aOutputB)
{
	uint8_t baseBrightness, varBrightness, fade;

	baseBrightness = (aValue * conf->fade_base) >> 8;
	varBrightness = aValue - baseBrightness;
	fade = (varBrightness * aFader) >> 8;
	*aOutputB = baseBrightness + fade;
	*aOutputA = baseBrightness + (varBrightness - fade);
}

static void
renderText(struct config_t *conf)
{
	uint8_t maxBright, thisBright, nextBright, column;
	int pixelsPerChar, activeCols, totalTextPixels, x, rowPixelOffset, charIndex, glyphOffset, glyphRow;
	int i, leftstep;
	char c;

	// fade between rows
	maxBright = conf->text_intensity - conf->text_repeats * conf->fade_per_repeat;

	crossFade(conf, 255 * textCycleCount / conf->text_cycles_per_px, maxBright, &thisBright, &nextBright);

	// generate vertical rows
	pixelsPerChar = BYTES_PER_GLYPH + GLYPH_SPACING;
	activeCols = conf->leds_per_level - 2;
	totalTextPixels = textLen * pixelsPerChar;
	for (x = 0; x < conf->leds_per_level; x++) {
		column = 0;
		// determine font row
		if (x < activeCols) {
			rowPixelOffset = textPixelOffset + x;
			if (rowPixelOffset >= 0) {
				// visible row
				charIndex = rowPixelOffset / pixelsPerChar;
				if (textLen > charIndex) {
					// visible char
					c = text[charIndex];
					glyphOffset = rowPixelOffset % pixelsPerChar;
					if (glyphOffset < BYTES_PER_GLYPH) {
						// fetch glyph column
						c -= 0x20;
						if (c >= NUM_GLYPHS)
							c = 95; // ASCII 0x7F-0x20
						column = fontBytes[c * BYTES_PER_GLYPH + glyphOffset];
					}
				}
			}
		}
		// now render columns
		for (glyphRow = 0; glyphRow < ROWS_PER_GLYPH; glyphRow++) {
			if (conf->wound_cwise) {
				i = (glyphRow + 1) * conf->leds_per_level - 1 - x; // LED index, x-direction mirrored
				leftstep = 1;
			} else {
				i = glyphRow * conf->leds_per_level + x; // LED index
				leftstep = -1;
			}
			if (glyphRow < ROWS_PER_GLYPH) {
				if (column & (0x40 >> glyphRow)) {
					textLayer[i] = thisBright;
					// also adjust pixel left to this one
					if (x > 0) {
						sat8add(&textLayer[i + leftstep], nextBright);
						if (textLayer[i + leftstep] > maxBright)
							textLayer[i + leftstep] = maxBright;
					}
					continue;
				}
			}
			textLayer[i] = 0; // no text
		}
	}
	// increment
	textCycleCount++;
	if (textCycleCount >= conf->text_cycles_per_px) {
		textCycleCount = 0;
		textPixelOffset++;
		if (textPixelOffset > totalTextPixels) {
			// text shown, check for repeats
			repeatCount++;
			if (conf->text_repeats != 0 && repeatCount >= conf->text_repeats) {
				// done
				strcpy(text, ""); // remove text
				textLen = 0;
			}
			else {
				// show again
				textPixelOffset = -conf->leds_per_level;
				textCycleCount = 0;
			}
		}
	}
}
