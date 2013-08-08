/*
 * Copyright 2004-2011 Analog Devices Inc.
 *
 * Licensed under the GPL-2.
 */

/* ------------ Defines ------------ */

#define DEBUG 				0

#define CALL_GNUPLOT "/usr/bin/gnuplot /var/www/data/cgi-bin/gnu.plt_"
#define FILENAME_T_OUT "/var/www/data/cgi-bin/t_samples.txt_"
#define FILENAME_T_OUT2 "/var/www/data/cgi-bin/t_samples2.txt_"
#define FILENAME_F_OUT "/var/www/data/cgi-bin/f_samples.txt_"
#define FILENAME_GNUPLT "/var/www/data/cgi-bin/gnu.plt_"

#define VALUE_FRAME "\n<html>\n<head>\n<meta http-equiv=\"Content-Type\" content=\"text/html; charset=windows-1252\">\n<title></title></head><body> <p><font face=\"Tahoma\" size=\"10\">%4.3f Volt</font></p>\n"


#define ID_AD9643		1
#define ID_AD9467		2
#define ID_AD9250		3

#define MINSAMPLERATE 		1
#define MAXSAMPLERATE 		300000000
#define MAXNUMSAMPLES 		65536
#define MINNUMSAMPLES 		1
#define MAXSIZERATIO		4
#define TIMEOUT			10

#define OUT_DEC 1
#define OUT_BIN 2
#define OUT_HEX 3

#ifndef fixed
#define fixed short
#endif

#define ARRAY_SIZE(a) (sizeof(a) / sizeof((a)[0]))


/* ------------ Structs ------------ */

typedef struct {
	unsigned short vdiv;
} vertical;

typedef struct {
	unsigned short set_grid;
	unsigned short axis;
	unsigned short style;
	unsigned short linestyle;
	unsigned short color;
	unsigned short logscale;
	unsigned short size_ratio;
	unsigned short xrange;
	unsigned short xrange1;
	unsigned short smooth;
	unsigned short tdom;
	unsigned short fftscaled;
	unsigned short fftexludezero;
	unsigned short window;
	unsigned short hw_fft;
} display;

typedef struct {
	unsigned short device;
	unsigned short slaveadc;

} input;

typedef struct {
	unsigned int sps;
	unsigned int samples;
	unsigned int fsamples;
} time_set;

typedef struct {
	display sdisplay;
	vertical svertical;
	input sinput;
	time_set stime_s;
	unsigned short num_channels;
	unsigned long channel_en_mask;
	unsigned short run;
	unsigned has_slave;
	int fd0;
	int framebuffer;
	FILE *pFile_samples;
	FILE *pFile_init;
	char *pFILENAME_T_OUT;
	char *pFILENAME_T_OUT2;
	char *pFILENAME_GNUPLT;
	char *pGNUPLOT;
	char *pREMOTE_ADDR;
	unsigned long reg;
	unsigned long val;
	float avg_ch0;
	float avg_ch1;
	int min_ch0;
	int min_ch1;
	int max_ch0;
	int max_ch1;
	unsigned id;
} s_info;

typedef struct {
	char testname[32];
	char iiotestname[32];
	unsigned int pat1;
	unsigned int pat2;
} s_test;

/* ------------ Enums ------------ */

enum {
	ACQUIRE, SAVE, SHOWDEVATTR, GNUPLOT_FILES, WRITEREG, READREG, WSYSFS, TEST,
};				/* what program we want to run */

enum {
	IIO_OPEN, FILE_OPEN, SAMPLE_RATE, SAMPLE_DEPTH, SIZE_RATIO, RANGE, TIME_OUT
};

/* ------------ function prototypes ------------ */

extern int gettimeofday(struct timeval *, void *);

int iio_sample(int form_method, char **getvars, char **postvars, s_info * info,
	       char *device_name, char *trigger_name);
int iio_read_device_files(char *device_name, unsigned out);
int iio_write_devattr(char *device_name, char *attr, unsigned int value);
int iio_read_devattr(char *device_name, char *attr, unsigned int *value);
int debugfs_write_devattr(char *device_name, char *attr, unsigned int value, int type, unsigned int value2);
int debugfs_read_devattr(char *device_name, char *attr, unsigned int *value);


extern int fix_fft (fixed *, fixed *, int, int);
extern int iscale (int, int, int);
extern void window (fixed *, int);
extern void fix_loud (fixed loud[], fixed fr[], fixed fi[], int n, int scale_shift);

