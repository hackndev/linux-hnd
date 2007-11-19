

struct ipaq_asset_driver {
	char name[DEVICE_NAME_SIZE];
	struct device_driver driver;
	int (*read) (struct ipaq_asset *asset );
};

struct ipaq_asset_device {
	char name[DEVICE_NAME_SIZE];
	struct device device;
};

struct ipaq_audio_driver {
	char name[DEVICE_NAME_SIZE];
	struct device_driver driver;
	int (*set_power) (int);
	int (*set_clock) (int samplerate);
	int (*set_mute)  (int mute);
};

struct ipaq_audio_device {
	char name[DEVICE_NAME_SIZE];
	struct device device;
};

struct ipaq_spi_driver {
	char name[DEVICE_NAME_SIZE];
	struct device_driver driver;
	int (*read)( unsigned short address, unsigned char *data, unsigned short len );
	int (*write)( unsigned short address, unsigned char *data, unsigned short len );
};

struct ipaq_spi_device {
	char name[DEVICE_NAME_SIZE];
	struct device device;
};

struct ipaq_battery_driver {
	char name[DEVICE_NAME_SIZE];
	struct device_driver driver;
};

struct ipaq_battery_device {
	char name[DEVICE_NAME_SIZE];
	struct device device;
};

struct ipaq_thermal_sensor_driver {
	char name[DEVICE_NAME_SIZE];
	int (*read) (unsigned char *result);
	struct device_driver driver;
};

struct ipaq_thermal_sensor_device {
	char name[DEVICE_NAME_SIZE];
	struct device device;
};

struct ipaq_light_sensor_driver {
	char name[DEVICE_NAME_SIZE];
	int (*read) (unsigned char *result);
	struct device_driver driver;
};

struct ipaq_light_sensor_device {
	char name[DEVICE_NAME_SIZE];
	struct device device;
};
