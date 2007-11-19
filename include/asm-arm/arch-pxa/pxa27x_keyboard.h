enum {
	PXAKBD_MAXROW = 8,
	PXAKBD_MAXCOL = 8,
};

struct pxa27x_keyboard_platform_data {
	int nr_rows, nr_cols;
	int keycodes[PXAKBD_MAXROW][PXAKBD_MAXCOL];
	int gpio_modes[PXAKBD_MAXROW + PXAKBD_MAXCOL];
	int debounce_ms;
};
