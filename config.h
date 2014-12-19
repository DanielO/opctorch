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
	int	torch_chan;

	/* Overall brightness */
	int	brightness;

	/* Cross fading brightness level */
	int	fade_base;

	/* Text parameters */
	int	text_intensity;
	int	text_cycles_per_px;
	int	text_repeats;
	int	fade_per_repeat;
	int	text_base_line;
	int	text_red;
	int	text_green;
	int	text_blue;

	/* Torch Parameters */
	int	flame_min;	// Bounds of random flame energy (0-255)
	int	flame_max;

	int	rnd_spark_prob;	// Percentage chance of a spark in the second row
	int	spark_min;	// Bounds of random spark energy (0-255)
	int	spark_max;

	int	spark_tfr;	// How much transferred up per cycle (0-255)
	int	spark_cap;	// How much energy is retained in sparks from previous cycle (0-255)

	int	up_rad;		// Upward radiation
	int	side_rad;	// Sideways radiation
	int	heat_cap;	// How much is retained in passive cells from previous cycle (0-255)

	int	red_bg;		// Colour of background cells
	int	green_bg;
	int	blue_bg;
	int	red_bias;	// Base offset applied to energy mapping
	int	green_bias;
	int	blue_bias;
	int	red_energy;	// Base valie applied to energy mapping
	int	green_energy;
	int	blue_energy;

	int	upside_down;	// If set, flame animation is upside down. Text remains as-is

	int	update_rate;	// Update rate target (FPS)
};

