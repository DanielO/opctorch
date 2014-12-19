/* Number of LEDs (only used for test code) */
#define NLEDS 256

struct config_t {
	/* Number of LEDs around the tube. One too much looks better (italic text look)
	 * than one to few (backwards leaning text look)
	 * Higher number = diameter of the torch gets larger
	 */
	int	leds_per_level;

	/* Number of windings of the strip around the tube */
	int	torch_levels;

	/* Winding direction
	 * 0 = clockwise
	 * 1 = counter-clockwise
	 */
	int	wound_cwise;

	/* OPC channel LEDs are connected to (0 = all) */
	int torch_chan;

	/* Overall brightness */
	int brightness;

	/* Cross fading brightness level */
	int fade_base;

	/* Text parameters */
	int text_intensity;
	int text_cycles_per_px;
	int text_repeats;
	int fade_per_repeat;
	int text_base_line;
	int text_red;
	int text_green;
	int text_blue;

	/* Torch Parameters */
	int flame_min; // 0..255
	int flame_max; // 0..255

	int rnd_spark_prob; // 0..100
	int spark_min; // 0.255
	int spark_max; // 0.255

	int spark_tfr; // 0..255 - how much transferred up per cycle
	int spark_cap; // 0..200 - how much energy is retained in sparks from previous cycle

	int up_rad; // upward radiation
	int side_rad; // sideways radiation
	int heat_cap; // 0..255 - how much is retained in passive cells from previous cycle

	int red_bg;
	int green_bg;
	int blue_bg;
	int red_bias;
	int green_bias;
	int blue_bias;
	int red_energy;
	int green_energy;
	int blue_energy;

	int upside_down; // if set, flame (or rather: drop) animation is upside down. Text remains as-is

	int update_rate; // Frames per second
};

