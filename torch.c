/* Torch implementation */

#include <assert.h>
#include <err.h>
#include <pthread.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sysexits.h>
#include <unistd.h>
#include <ccan/ciniparser/ciniparser.h>

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

static pthread_mutex_t torch_mtx = PTHREAD_MUTEX_INITIALIZER;

static void	setColourDimmed(uint16_t, uint8_t, uint8_t, uint8_t, uint8_t);
static int	sendLEDs(void);
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
static void	setVal(struct config_t *conf, const char *, const char *);
static void	dumpVals(struct config_t *conf);

#define TORCH_PASSIVE		0 // Just environment, glow from nearby radiation
#define TORCH_NOP		1 // No processing
#define TORCH_SPARK		2 // Slowly loses energy, moves up
#define TORCH_SPARK_TEMP	3 // A spark still getting energy from the level below

/* Set default config */
void
default_conf(struct config_t *conf)
{

	memset(conf, 0, sizeof(*conf));
	conf->leds_per_level = -1;
	conf->torch_levels = -1;
	conf->wound_cwise = -1;
	conf->torch_chan = -1;
	conf->brightness = 255;
	conf->fade_base = 140;
	conf->text_intensity = 200;
	conf->text_cycles_per_px = 5;
	conf->fade_per_repeat = 50;
	conf->text_base_line = 3;
	conf->text_red = 255;
	conf->text_green = 40;
	conf->text_blue = 40;
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

/* Update conf based on ini file */
#define INI_GET_INT(name)	do {					\
	    if ((i = ciniparser_getint(ini, "torch:" #name, -1)) != -1) \
		    conf->name = i;					\
	} while(0)
#define INI_GET_INT8(name)	do {					\
	    if ((i = ciniparser_getint(ini, "torch:" #name, -1)) != -1 && \
		i >= 0 && i <= 255)					\
		    conf->name = i;					\
	} while(0)
#define INI_GET_BOOL(name)	do {					\
	    if ((i = ciniparser_getboolean(ini, "torch:" #name, -1)) != -1) \
		    conf->name = i;					\
	} while(0)
int
ini2conf(dictionary *ini, struct config_t *conf)
{
	int i;

	/* Look for parameters */
	INI_GET_INT(leds_per_level);
	INI_GET_INT(torch_levels);
	INI_GET_BOOL(wound_cwise);
	INI_GET_INT(torch_chan);
	INI_GET_INT8(brightness);
	INI_GET_INT8(fade_base);
	INI_GET_INT8(text_intensity);
	INI_GET_INT8(text_cycles_per_px);
	INI_GET_INT8(fade_per_repeat);
	INI_GET_INT8(text_base_line);
	INI_GET_INT8(text_red);
	INI_GET_INT8(text_green);
	INI_GET_INT8(text_blue);
	INI_GET_INT8(flame_min);
	INI_GET_INT8(flame_max);
	INI_GET_INT8(rnd_spark_prob);
	INI_GET_INT8(spark_min);
	INI_GET_INT8(spark_max);
	INI_GET_INT8(spark_tfr);
	INI_GET_INT8(spark_cap);
	INI_GET_INT8(up_rad);
	INI_GET_INT8(side_rad);
	INI_GET_INT8(heat_cap);
	INI_GET_INT8(red_bg);
	INI_GET_INT8(green_bg);
	INI_GET_INT8(blue_bg);
	INI_GET_INT8(red_bias);
	INI_GET_INT8(green_bias);
	INI_GET_INT8(blue_bias);
	INI_GET_INT8(red_energy);
	INI_GET_INT8(green_energy);
	INI_GET_INT8(blue_energy);
	INI_GET_BOOL(upside_down);
	INI_GET_INT(update_rate);

	/* Validate config */
	if (conf->leds_per_level == -1) {
		fprintf(stderr, "Must specify leds_per_level in configuration\n");
		return(1);
	}
	if (conf->torch_levels == -1) {
		fprintf(stderr, "Must specify torch_levels in configuration\n");
		return(1);
	}
	if (conf->wound_cwise == -1) {
		fprintf(stderr, "Must specify wound_cwise in configuration\n");
		return(1);
	}
	if (conf->torch_chan == -1) {
		fprintf(stderr, "Must specify torch_chan in configuration\n");
		return(1);
	}
	if (conf->rnd_spark_prob < 0 || conf->rnd_spark_prob > 100){
		fprintf(stderr, "rnd_spark_prob must be between 0 and 100\n");
		return(1);
	}
	if (conf->update_rate <= 0) {
		fprintf(stderr, "update_rate must be greater than 0\n");
		return(1);
	}
	if (conf->text_base_line + ROWS_PER_GLYPH > conf->torch_levels) {
		fprintf(stderr, "text_base_line is too high, text will be truncated\n");
		return(1);
	}

	return 0;
}

/* Allocate memory and setup ready to run */
int
create_torch(int s, struct config_t *conf)
{

	sock = s;

	numleds = conf->leds_per_level * conf->torch_levels;
	assert(numleds > 0);
	pixDataSz = sizeof(*pixData) + numleds * sizeof(pixData->pixels[0]);
	if ((pixData = malloc(pixDataSz)) == NULL)
		goto err;
	if ((currentEnergy = malloc(numleds * sizeof(currentEnergy[0]))) == NULL)
		goto err;
	if ((nextEnergy = malloc(numleds * sizeof(nextEnergy[0]))) == NULL)
		goto err;
	if ((energyMode = malloc(numleds * sizeof(energyMode[0]))) == NULL)
		goto err;
	textPixels = conf->leds_per_level * ROWS_PER_GLYPH;
	assert(textPixels > 0);
	if ((textLayer = malloc(textPixels * sizeof(textLayer[0]))) == NULL)
		goto err;

	pixData->header[0] = conf->torch_chan;
	pixData->header[1] = 0; // Command: set LEDs
	pixData->header[2] = (numleds * sizeof(pixData->pixels[0])) >> 8; // Length MSB
	pixData->header[3] = (numleds * sizeof(pixData->pixels[0])) & 0xff; // Length LSB

	resetEnergy();
	resetText();

	return(0);

 err:
	free_torch();
	return(-1);
}

int
run_torch(struct config_t *conf)
{
	int sl, t, frame;
	struct timeval then, now;

	frame = 0;
	while (1) {
		assert(pthread_mutex_lock(&torch_mtx) == 0);

		frame++;
		gettimeofday(&then, NULL);
		renderText(conf);
		injectRandom(conf);
		calcNextEnergy(conf);
		calcNextColours(conf);

		if (sendLEDs() != 0)
			return(-1);

		gettimeofday(&now, NULL);
		timersub(&now, &then, &now);
		t = now.tv_sec * 1000000 + now.tv_usec;
		sl = (1000000 / conf->update_rate) - t;
		if (frame % 10 == 0)
			printf("%5d: Loop took %5d usec, sleeping %5d usec\n", frame, t, sl);
		assert(pthread_mutex_unlock(&torch_mtx) == 0);

		usleep(sl);
	}

	return(0);
}

void
free_torch(void)
{
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

void
splitargs(char *cmd, char **argv, int nargv, int *argc)
{

	assert(nargv > 0);

	argv[0] = cmd;
	for (*argc = 1; *argc < nargv; (*argc)++) {
		argv[*argc] = strchr(argv[*argc - 1], ' ');
		if (argv[*argc] == NULL)
			break;
		argv[*argc][0] = '\0';
		argv[*argc]++;
	}
}

void
cmd_torch(struct config_t *conf, const char *from, char *cmd)
{	
	char *argv[10], *origline, *tmp;
	int argc;

	origline = strdup(cmd);
	splitargs(cmd, argv, sizeof(argv) / sizeof(argv[0]), &argc);
	
	assert(pthread_mutex_lock(&torch_mtx) == 0);

	fprintf(stderr, "Command from %s: %s\n", from, argv[0]);
	if (!strcmp(argv[0], "message")) {
		if (argc < 2) {
			warnx("message: bad usage");
			goto out;
		}
		tmp = strchr(origline, ' ');
		tmp++;
		newMessage(conf, tmp);
	} else if (!strcmp(argv[0], "set")) {
		if (argc == 3)
			setVal(conf, argv[1], argv[2]);
		else
			warnx("Bad usage for set command");
	} else if (!strcmp(argv[0], "dump")) {
		dumpVals(conf);
	}

  out:
	free(origline);
	assert(pthread_mutex_unlock(&torch_mtx) == 0);
}

static int
sendLEDs(void)
{
	int rtn;

	if ((rtn = send(sock, pixData, pixDataSz, 0)) < 0) {
		warn("Unable to send data");
		return(-1);
	}

	return(0);
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
	return(r);
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
	uint8_t e, m, e2, tmp;

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
				if (i < numleds - 1)
					tmp = currentEnergy[i + 1];
				else
					tmp = 0;
				sat8add(&e, ((((int)currentEnergy[i - 1] + (int)tmp) * conf->side_rad) >> 9) +
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
	msg[sizeof(text) - 1] = '\0';
	strncpy(text, msg, sizeof(text) - 1);
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

static void
setVal(struct config_t *conf, const char *key, const char *val)
{
	int tmp;

	tmp = atoi(val);
	
	if (!strcmp(key, "brightness"))
		conf->brightness = tmp;
	else if (!strcmp(key, "fade_base"))
		conf->fade_base = tmp;
	else if (!strcmp(key, "text_intensity"))
		conf->text_intensity = tmp;
	else if (!strcmp(key, "text_cycles_per_px"))
		conf->text_cycles_per_px = tmp;
	else if (!strcmp(key, "text_repeats"))
		conf->text_repeats = tmp;
	else if (!strcmp(key, "fade_per_repeat"))
		conf->fade_per_repeat = tmp;
	else if (!strcmp(key, "text_base_line"))
		conf->text_base_line = tmp;
	else if (!strcmp(key, "text_red"))
		conf->text_red = tmp;
	else if (!strcmp(key, "text_green"))
		conf->text_green = tmp;
	else if (!strcmp(key, "text_blue"))
		conf->text_blue = tmp;
	else if (!strcmp(key, "flame_min"))
		conf->flame_min = tmp;
	else if (!strcmp(key, "flame_max"))
		conf->flame_max = tmp;
	else if (!strcmp(key, "rnd_spark_prob"))
		conf->rnd_spark_prob = tmp;
	else if (!strcmp(key, "spark_min"))
		conf->spark_min = tmp;
	else if (!strcmp(key, "spark_max"))
		conf->spark_max = tmp;
	else if (!strcmp(key, "spark_tfr"))
		conf->spark_tfr = tmp;
	else if (!strcmp(key, "spark_cap"))
		conf->spark_cap = tmp;
	else if (!strcmp(key, "up_rad"))
		conf->up_rad = tmp;
	else if (!strcmp(key, "side_rad"))
		conf->side_rad = tmp;
	else if (!strcmp(key, "heat_cap"))
		conf->heat_cap = tmp;
	else if (!strcmp(key, "red_bg"))
		conf->red_bg = tmp;
	else if (!strcmp(key, "green_bg"))
		conf->green_bg = tmp;
	else if (!strcmp(key, "blue_bg"))
		conf->blue_bg = tmp;
	else if (!strcmp(key, "red_bias"))
		conf->red_bias = tmp;
	else if (!strcmp(key, "green_bias"))
		conf->green_bias = tmp;
	else if (!strcmp(key, "blue_bias"))
		conf->blue_bias = tmp;
	else if (!strcmp(key, "red_energy"))
		conf->red_energy = tmp;
	else if (!strcmp(key, "green_energy"))
		conf->green_energy = tmp;
	else if (!strcmp(key, "blue_energy"))
		conf->blue_energy = tmp;
	else if (!strcmp(key, "upside_down"))
		conf->upside_down = tmp;
	else if (!strcmp(key, "update_rate"))
		conf->update_rate = tmp;
	else
		warnx("Unknown key %s", key);
}

static void
dumpVals(struct config_t *conf)
{

	fprintf(stderr, "=============\n");
	fprintf(stderr, "Configuration\n");
	fprintf(stderr, "=============\n");
	fprintf(stderr, "%-20s: %d\n", "brightness", conf->brightness);
	fprintf(stderr, "%-20s: %d\n", "fade_base", conf->fade_base);
	fprintf(stderr, "%-20s: %d\n", "text_intensity", conf->text_intensity);
	fprintf(stderr, "%-20s: %d\n", "text_cycles_per_px", conf->text_cycles_per_px);
	fprintf(stderr, "%-20s: %d\n", "text_repeats", conf->text_repeats);
	fprintf(stderr, "%-20s: %d\n", "fade_per_repeat", conf->fade_per_repeat);
	fprintf(stderr, "%-20s: %d\n", "text_base_line", conf->text_base_line);
	fprintf(stderr, "%-20s: %d\n", "text_red", conf->text_red);
	fprintf(stderr, "%-20s: %d\n", "text_green", conf->text_green);
	fprintf(stderr, "%-20s: %d\n", "text_blue", conf->text_blue);
	fprintf(stderr, "%-20s: %d\n", "flame_min", conf->flame_min);
	fprintf(stderr, "%-20s: %d\n", "flame_max", conf->flame_max);
	fprintf(stderr, "%-20s: %d\n", "rnd_spark_prob", conf->rnd_spark_prob);
	fprintf(stderr, "%-20s: %d\n", "spark_min", conf->spark_min);
	fprintf(stderr, "%-20s: %d\n", "spark_max", conf->spark_max);
	fprintf(stderr, "%-20s: %d\n", "spark_tfr", conf->spark_tfr);
	fprintf(stderr, "%-20s: %d\n", "spark_cap", conf->spark_cap);
	fprintf(stderr, "%-20s: %d\n", "up_rad", conf->up_rad);
	fprintf(stderr, "%-20s: %d\n", "side_rad", conf->side_rad);
	fprintf(stderr, "%-20s: %d\n", "heat_cap", conf->heat_cap);
	fprintf(stderr, "%-20s: %d\n", "red_bg", conf->red_bg);
	fprintf(stderr, "%-20s: %d\n", "green_bg", conf->green_bg);
	fprintf(stderr, "%-20s: %d\n", "blue_bg", conf->blue_bg);
	fprintf(stderr, "%-20s: %d\n", "red_bias", conf->red_bias);
	fprintf(stderr, "%-20s: %d\n", "green_bias", conf->green_bias);
	fprintf(stderr, "%-20s: %d\n", "blue_bias", conf->blue_bias);
	fprintf(stderr, "%-20s: %d\n", "red_energy", conf->red_energy);
	fprintf(stderr, "%-20s: %d\n", "green_energy", conf->green_energy);
	fprintf(stderr, "%-20s: %d\n", "blue_energy", conf->blue_energy);
	fprintf(stderr, "%-20s: %d\n", "upside_down", conf->upside_down);
	fprintf(stderr, "%-20s: %d\n", "update_rate", conf->update_rate);
	fprintf(stderr, "\n");
}
