struct ghi270_nand_data {
	int		     rnb_gpio;     /* gpio no. of Ready/Busy signal */
	int		     n_partitions; /* number of partitions */
	struct mtd_partition *partitions;
};
