/* Number of LEDs (only used for test code) */
#define NLEDS 256

/* Number of LEDs around the tube. One too much looks better (italic text look)
 * than one to few (backwards leaning text look)
 * Higher number = diameter of the torch gets larger
 */
#define LEDS_PER_LEVEL	11

/* Number of windings of the strip around the tube */
#define TORCH_LEVELS	21

/* Winding direction
 * 0 = clockwise
 * 1 = counter-clockwise
 */
#define WOUND_CWISE	0

/* OPC channel LEDs are connected to (0 = all) */
#define TORCH_CHAN 1

/* Overall brightness */
#define BRIGHTNESS 255

/* Cross fading brightness level */
#define FADE_BASE 140

/* Text parameters */
#define TEXT_INTENSITY 255
#define TXT_CYCLES_PER_PX 5
#define TEXT_REPEATS 15
#define FADE_PER_REPEAT 15
#define TEXT_BASE_LINE 10
#define TEXT_RED 0
#define TEXT_GREEN 255
#define TEXT_BLUE 180

/* Torch Parameters */
#define FLAME_MIN 100 // 0..255
#define FLAME_MAX 220 // 0..255

#define RND_SPARK_PROB 2 // 0..100
#define SPARK_MIN 200 // 0.255
#define SPARK_MAX 255 // 0.255

#define SPARK_TFR 40 // 0..255 - how much transferred up per cycle
#define SPARK_CAP 200 // 0..200 - how much energy is retained in sparks from previous cycle

#define UP_RAD 40 // upward radiation
#define SIDE_RAD 35 // sideways radiation
#define HEAT_CAP 0 // 0..255 - how much is retained in passive cells from previous cycle

#define RED_BG 0
#define GREEN_BG 0
#define BLUE_BG 0
#define RED_BIAS 10
#define GREEN_BIAS 0
#define BLUE_BIAS 0
#define RED_ENERGY 180
#define GREEN_ENERGY 145
#define BLUE_ENERGY 0

#define UPSIDE_DOWN 0 // if set, flame (or rather: drop) animation is upside down. Text remains as-is
