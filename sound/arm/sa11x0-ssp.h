struct sa11x0_snd_ops {
	int	(*open)(struct device *dev, snd_pcm_runtime_t *runtime);
	void	(*close)(struct device *dev);
	int	(*samplerate)(struct device *dev, unsigned int rate);
	void	(*suspend)(struct device *dev);
	void	(*resume)(struct device *dev);
};


