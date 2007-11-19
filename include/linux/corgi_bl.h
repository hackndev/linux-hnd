/*
 * Generic Backlight, from sharpsl.h
 */
struct corgibl_machinfo {
	int max_intensity;
	int default_intensity;
	int limit_mask;
	void (*set_bl_intensity)(int intensity);
};
extern void corgibl_limit_intensity(int limit);
