/*
 * Copyright 2004-2012 Analog Devices Inc.
 *
 * Licensed under the GPL-2.
 */

#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/poll.h>
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <syslog.h>

#ifdef TM_IN_SYS_TIME
#include <sys/time.h>
#else
#include <time.h>
#endif

#include "cgivars.h"
#include "htmllib.h"
#include "ndso.h"

static s_info sinfo;

static const s_test stest[] = {{
	.testname = "Midscale Short",
	.iiotestname = "midscale_short",
	.pat1 = 0x00000000,
	.pat2 = 0x00000000,
	}, {
	.testname = "Positive Full Scale",
	.iiotestname = "pos_fullscale",
	.pat1 = 0x1FFF1FFF,
	.pat2 = 0x1FFF1FFF,
	}, {
	.testname = "Negative Full Scale",
	.iiotestname = "neg_fullscale",
	.pat1 = 0xE000E000,
	.pat2 = 0xE000E000,
	}, {
	.testname = "Alternating Checkerboard",
	.iiotestname = "checkerboard",
	.pat1 = 0x2AAA1555,
	.pat2 = 0x15552AAA,
	}, {
	.testname = "One Zero Word Toggle",
	.iiotestname = "one_zero_toggle",
	.pat1 = 0xFFFF0000,
	.pat2 = 0x0000FFFF,
	},
};

static const s_test stest_9467[] = {{
	.testname = "Midscale Short",
	.iiotestname = "midscale_short",
	.pat1 = 0x00000000,
	.pat2 = 0x00000000,
	}, {
	.testname = "Positive Full Scale",
	.iiotestname = "pos_fullscale",
	.pat1 = 0x7FFF7FFF,
	.pat2 = 0x7FFF7FFF,
	}, {
	.testname = "Negative Full Scale",
	.iiotestname = "neg_fullscale",
	.pat1 = 0x80008000,
	.pat2 = 0x80008000,
	}, {
	.testname = "Alternating Checkerboard",
	.iiotestname = "checkerboard",
	.pat1 = 0xAAAA5555,
	.pat2 = 0x5555AAAA,
	}, {
	.testname = "One Zero Word Toggle",
	.iiotestname = "one_zero_toggle",
	.pat1 = 0xFFFF0000,
	.pat2 = 0x0000FFFF,
	},
};

static const s_test stest_9250[] = {{
	.testname = "Midscale Short",
	.iiotestname = "midscale_short",
	.pat1 = 0x20002000,
	.pat2 = 0x20002000,
	}, {
	.testname = "Positive Full Scale",
	.iiotestname = "pos_fullscale",
	.pat1 = 0x3FFF3FFF,
	.pat2 = 0x3FFF3FFF,
	}, {
	.testname = "Negative Full Scale",
	.iiotestname = "neg_fullscale",
	.pat1 = 0x00000000,
	.pat2 = 0x00000000,
	}, {
	.testname = "Alternating Checkerboard",
	.iiotestname = "checkerboard",
	.pat1 = 0x2AAA1555,
	.pat2 = 0x15552AAA,
	}, {
	.testname = "One Zero Word Toggle",
	.iiotestname = "one_zero_toggle",
	.pat1 = 0x3FFF0000,
	.pat2 = 0x00003FFF,
	},
};


void display_on_framebuffer(s_info * info)
{
	if (!info->framebuffer)
		return;

	if (vfork() == 0) {
		char img[256];
		snprintf(img, sizeof(img), "/home/httpd/img%s.png",
			 info->pREMOTE_ADDR);
		execlp("pngview", "pngview", "-q", img, NULL);
		printf("<br>Hmm, could not run pngview, that's odd ...<br>\n");
		_exit(-1);
	}
}

int getrand(void)
{

	struct timeval tv;

	if (gettimeofday(&tv, NULL) != 0) {
		printf("Error getting time\n");
	}

	srand(tv.tv_sec * rand());

	return rand();
};

/* str2num */
int str2num(char *str)
{
	int num = 0;
	int i = 0, ilen;

	if (str == NULL)
		return -1;
	ilen = strlen(str);

	if (str[0] == '*' && str[1] == 0)
		return 1;

	for (i = 0; i < ilen; i++) {
		if (str[i] == '.' || str[i] == '-')
			i++;	// ignore dot and sign

		if (str[i] > 57 || str[i] < 48)
			return -1;
		num = num * 10 + (str[i] - 48);
	}
	return (str[0] == '-' ? (-1) * num : num);
};

void make_session_files(s_info * info)
{
	char str[80];

/* Generate File Names Based on the REMOTE IP ADDR */
	info->pREMOTE_ADDR = strdup(getRemoteAddr());

	info->pGNUPLOT =
	    strdup(strcat(strcpy(str, CALL_GNUPLOT), info->pREMOTE_ADDR));
	info->pFILENAME_T_OUT =
	    strdup(strcat(strcpy(str, FILENAME_T_OUT), info->pREMOTE_ADDR));
	info->pFILENAME_T_OUT2 =
	    strdup(strcat(strcpy(str, FILENAME_T_OUT2), info->pREMOTE_ADDR));
	info->pFILENAME_GNUPLT =
	    strdup(strcat(strcpy(str, FILENAME_GNUPLT), info->pREMOTE_ADDR));

	return;
};

void free_session_files(s_info * info)
{
	free(info->pREMOTE_ADDR);
	free(info->pFILENAME_T_OUT);
	free(info->pFILENAME_T_OUT2);
	free(info->pFILENAME_GNUPLT);
	free(info->pGNUPLOT);

	return;
};

void do_files(s_info * info)
{
	printf("<hr>\n<menu>\n");

	info->pFile_samples = fopen(info->pFILENAME_T_OUT, "r");
	if (info->pFile_samples) {
		fclose(info->pFile_samples);
		printf
		    ("  <li><font face=\"Arial Black\"><a href=\"t_samples.txt_%s\">Time Samples</a></font></li>\n",
		     info->pREMOTE_ADDR);
	}

	info->pFile_samples = fopen(info->pFILENAME_T_OUT2, "r");
	if (info->pFile_samples) {
		fclose(info->pFile_samples);
		printf
		    ("  <li><font face=\"Arial Black\"><a href=\"t_samples2.txt_%s\">Time Samples</a></font></li>\n",
		     info->pREMOTE_ADDR);
	}

	info->pFile_init = fopen(info->pFILENAME_GNUPLT, "r");
	if (info->pFile_init) {
		fclose(info->pFile_init);
		printf
		    ("  <li><font face=\"Arial Black\"><a href=\"gnu.plt_%s\">Gnuplot File</a></font></li>\n",
		     info->pREMOTE_ADDR);
	}

	if ((info->pFile_samples == NULL) && (info->pFile_init == NULL))
		printf
		    ("  <li><font face=\"Arial Black\">No Files available from %s</font></li>\n",
		     info->pREMOTE_ADDR);

	printf("</menu>\n<hr>\n");

	return;
};

int do_html(int form_method, char **getvars, char **postvars, s_info * info)
{
	int fd, n = 0;
	unsigned val;
	char buf[256];

	switch (info->run) {
	case ACQUIRE:
		htmlHeader("NDSO Demo Web Page");
		htmlBody();
		printf
		    ("\n<img border=\"0\" src=\"/img%s.png?id=%u\" align=\"left\">\n",
		     info->pREMOTE_ADDR, getrand());

		if (info->sdisplay.tdom) {
			if (info->channel_en_mask & (1 << 0)) {
				printf("<p style=\"margin-top: 0; margin-bottom: 0\"><font face=\"Courier new\"> CH0 Min:%d</font></p>\n", info->min_ch0);
				printf("<p style=\"margin-top: 0; margin-bottom: 0\"><font face=\"Courier new\"> CH0 Max:%d</font></p>\n", info->max_ch0);
				printf("<p style=\"margin-top: 0; margin-bottom: 0\"><font face=\"Courier new\"> CH0 Avg:%4.3f</font></p>\n", info->avg_ch0);
				printf("<p style=\"margin-top: 0; margin-bottom: 0\"><font face=\"Courier new\"> ______________</font></p>\n");
			}
			if (info->channel_en_mask & (1 << 1)) {
				printf("<p style=\"margin-top: 0; margin-bottom: 0\"><font face=\"Courier new\"> CH1 Min:%d</font></p>\n", info->min_ch1);
				printf("<p style=\"margin-top: 0; margin-bottom: 0\"><font face=\"Courier new\"> CH1 Max:%d</font></p>\n", info->max_ch1);
				printf("<p style=\"margin-top: 0; margin-bottom: 0\"><font face=\"Courier new\"> CH1 Avg:%4.3f</font></p>\n", info->avg_ch1);
			}
		}
		htmlFooter();
		break;
	case SAVE:
		fd = open(info->pFILENAME_T_OUT, 0);
		if (fd < 0)
			break;

		printf("Content-type: application/octet-stream\n");
		printf("Content-Transfer-Encoding: binary\n");
		printf("Content-Disposition: attachment; filename=\"samples_%s.txt\"\n\n", info->pREMOTE_ADDR);
		do {
			fwrite(buf, n, 1, stdout);
			n = read(fd, buf, 256);
		} while(n!=0);
		close(fd);
		break;
	case SHOWDEVATTR:
	case WSYSFS:
		htmlHeader("NDSO Demo Web Page");
		htmlBody();
		iio_read_device_files(postvars[info->sinput.device], 1);
		htmlFooter();
		if (info->id == ID_AD9643)
			system_sync(form_method, getvars, postvars, info);
		break;
	case GNUPLOT_FILES:
		htmlHeader("NDSO Demo Web Page");
		htmlBody();
		do_files(info);
		htmlFooter();
		break;
	case WRITEREG:
		htmlHeader("NDSO Demo Web Page");
		htmlBody();
		debugfs_write_devattr(postvars[info->sinput.device],
			"direct_reg_access", info->reg, 1, info->val);
		printf("<p><font face=\"Tahoma\" size=\"7\">Wrote REG[0x%X] = 0x%X</font></p>",
			(unsigned) info->reg, (unsigned) info->val);
		htmlFooter();
		break;
	case READREG:
		htmlHeader("NDSO Demo Web Page");
		htmlBody();
		debugfs_write_devattr(postvars[info->sinput.device],
			"direct_reg_access", info->reg, 0, 0);
		debugfs_read_devattr(postvars[info->sinput.device], "direct_reg_access", &val);
		printf("<p><font face=\"Tahoma\" size=\"7\">Read REG[0x%X] = 0x%X</font></p>",
			(unsigned) info->reg, (unsigned) val);
		htmlFooter();
		break;
	case TEST:
		htmlHeader("NDSO Demo Web Page");
		htmlBody();
		do_test(form_method, getvars, postvars, info);
		htmlFooter();
		break;
	default:

		break;
	}

	cleanUp(form_method, getvars, postvars);

	fflush(stdout);

	return 0;
}

int do_error(int errnum, int form_method, char **getvars, char **postvars,
	   s_info * info)
{
	htmlHeader("NDSO Demo Web Page");
	htmlBody();

	switch (errnum) {

	case IIO_OPEN:
		printf
		    ("<p><font face=\"Tahoma\" size=\"7\">ERROR[%d]:\n</font></p>",
		     IIO_OPEN);
		printf
		    ("<p><font face=\"Tahoma\" size=\"7\">Can't open %s IIO Device.\n</font></p>",
		    postvars[info->sinput.device]);
		break;
	case FILE_OPEN:
		printf
		    ("<p><font face=\"Tahoma\" size=\"7\">ERROR[%d]:\n</font></p>",
		     FILE_OPEN);
		printf
		    ("<p><font face=\"Tahoma\" size=\"7\">Can't open FILE.\n</font></p>");
		break;
	case SAMPLE_RATE:
		printf
		    ("<p><font face=\"Tahoma\" size=\"7\">ERROR[%d]:\n</font></p>",
		     SAMPLE_RATE);
		printf
		    ("<p><font face=\"Tahoma\" size=\"7\">Sample Rate outside specified range: [%d] < Rate < [%d] \n</font></p>",
		     MINSAMPLERATE, MAXSAMPLERATE);
		break;
	case SAMPLE_DEPTH:
		printf
		    ("<p><font face=\"Tahoma\" size=\"7\">ERROR[%d]:\n</font></p>",
		     SAMPLE_DEPTH);
		printf
		    ("<p><font face=\"Tahoma\" size=\"7\">Sample Depth outside specified range: [%d] < Depth < [%d] \n</font></p>",
		     MINNUMSAMPLES, MAXNUMSAMPLES);
		break;
	case SIZE_RATIO:
		printf
		    ("<p><font face=\"Tahoma\" size=\"7\">ERROR[%d]:\n</font></p>",
		     SIZE_RATIO);
		printf
		    ("<p><font face=\"Tahoma\" size=\"7\">Size Ratio contains invalid characters r exceeds maximum Size Ratio < [%d]\n</font></p>",
		     MAXSIZERATIO);
		break;
	case RANGE:
		printf
		    ("<p><font face=\"Tahoma\" size=\"7\">ERROR[%d]:\n</font></p>",
		     RANGE);
		printf
		    ("<p><font face=\"Tahoma\" size=\"7\">Specified Range is invalid or out of range.\n</font></p>");
		break;
	case TIME_OUT:
		printf
		    ("<p><font face=\"Tahoma\" size=\"7\">ERROR[%d]:\n</font></p>",
		     TIME_OUT);
		printf
		    ("<p><font face=\"Tahoma\" size=\"7\">Ratio between Sample Depth and Sample Rate will exceed Timeout criteria [%d sec].\n</font></p>",
		     TIMEOUT);
		break;
	default:
		printf
		    ("<p><font face=\"Tahoma\" size=\"7\">ERROR[UNDEF]:\n</font></p>");
		printf
		    ("<p><font face=\"Tahoma\" size=\"7\">undefined ERROR: \n</font></p>");
		break;
	}

	htmlFooter();
	cleanUp(form_method, getvars, postvars);
	free_session_files(info);
	fflush(stdout);

	exit(1);
};

char * delspace(char *Str)
{
	int Pntr = 0;
	int Dest = 0;

	while (Str [Pntr])
	{
		if (Str [Pntr] != ' ')
			Str [Dest++] = Str [Pntr];
		Pntr++;
	}

	Str [Dest] = 0;

	return Str;
}

int parse_request(int form_method, char **getvars, char **postvars, s_info * info)
{
	int i;

	/*Preset checkbox settings */
	info->sdisplay.set_grid = 0;
	info->sdisplay.axis = 0;
	info->framebuffer = 0;
	info->sinput.slaveadc = 0xFFFF;


	if (form_method == POST) {
		/* Parse Request */
		for (i = 0; postvars[i]; i += 2) {
			if (strncmp(postvars[i], "D5", 2) == 0) {
				info->svertical.vdiv = i + 1;
			} else if (strncmp(postvars[i], "T2", 2) == 0) {
				info->stime_s.sps = str2num(postvars[i + 1]);
			} else if (strncmp(postvars[i], "T3", 2) == 0) {
				info->stime_s.samples =
				    str2num(postvars[i + 1]);
			} else if (strncmp(postvars[i], "set_grid", 8) == 0) {
				info->sdisplay.set_grid = 1;
			} else if (strncmp(postvars[i], "axis", 4) == 0) {
				info->sdisplay.axis = 1;
			} else if (strncmp(postvars[i], "linestyle", 9) == 0) {
				info->sdisplay.style = i + 1;
			} else if (strncmp(postvars[i], "color", 5) == 0) {
				info->sdisplay.color = i + 1;
			} else if (strncmp(postvars[i], "xrangeS", 7) == 0) {
				info->sdisplay.xrange = i + 1;
			} else if (strncmp(postvars[i], "xrangeE", 7) == 0) {
				info->sdisplay.xrange1 = i + 1;
			} else if (strncmp(postvars[i], "logscale", 8) == 0) {
				info->sdisplay.logscale = i + 1;
			} else if (strncmp(postvars[i], "size_ratio", 10) == 0) {
				info->sdisplay.size_ratio = i + 1;
			} else if (strncmp(postvars[i], "smooth", 6) == 0) {
				info->sdisplay.smooth = i + 1;
			} else if (strncmp(postvars[i], "device", 6) == 0) {
				info->sinput.device = i + 1;
			} else if (strncmp(postvars[i], "slaveadc", 7) == 0) {
				info->sinput.slaveadc = i + 1;
			} else if (strncmp(postvars[i], "REG", 3) == 0) {
				info->reg = strtoul(postvars[i + 1], NULL, 16);
			} else if (strncmp(postvars[i], "VAL", 3) == 0) {
				info->val = strtoul(postvars[i + 1], NULL, 16);
			} else if (strncmp(postvars[i], "C10", 3) == 0) {
				info->channel_en_mask |= (1 << 0);
			} else if (strncmp(postvars[i], "C11", 3) == 0) {
				info->channel_en_mask |= (1 << 1);
//			} else if (strncmp(postvars[i], "C12", 3) == 0) {
//				info->channel_en_mask |= (1 << 2);
//			} else if (strncmp(postvars[i], "C13", 3) == 0) {
//				info->channel_en_mask |= (1 << 3);
//			} else if (strncmp(postvars[i], "C14", 3) == 0) {
//				info->channel_en_mask |= (1 << 4);
//			} else if (strncmp(postvars[i], "C15", 3) == 0) {
//				info->channel_en_mask |= (1 << 5);
//			} else if (strncmp(postvars[i], "C16", 3) == 0) {
//				info->channel_en_mask |= (1 << 6);
//			} else if (strncmp(postvars[i], "C17", 3) == 0) {
//				info->channel_en_mask |= (1 << 7);
			} else if (strncmp(postvars[i], "R3", 2) == 0) {
				info->sdisplay.tdom = str2num(postvars[i + 1]);
			} else if (strncmp (postvars[i], "C7", 2) == 0) {
	      			info->sdisplay.fftexludezero = 1;
			} else if (strncmp (postvars[i], "C12", 2) == 0) {
	      			info->sdisplay.hw_fft = 1;
			} else if (strncmp (postvars[i], "C9", 2) == 0) {
	      			info->sdisplay.window = 1;
	      		} else if (strncmp (postvars[i], "D8", 2) == 0) {
	      			info->stime_s.fsamples = str2num (postvars[i + 1]);
	    		} else if (strncmp (postvars[i], "C8", 2) == 0) {
	    			info->sdisplay.fftscaled = 1;
			} else if (strncmp(postvars[i], "B1", 2) == 0) {
				info->run = ACQUIRE;
			} else if (strncmp(postvars[i], "B3", 2) == 0) {
				info->run = SAVE;
			} else if (strncmp(postvars[i], "B5", 2) == 0) {
				info->run = SHOWDEVATTR;
			} else if (strncmp(postvars[i], "B6", 2) == 0) {
				info->run = GNUPLOT_FILES;
			} else if (strncmp(postvars[i], "B4", 2) == 0) {
				info->run = WRITEREG;
			} else if (strncmp(postvars[i], "B7", 2) == 0) {
				info->run = READREG;
			} else if (strncmp(postvars[i], "B9", 2) == 0) {
				info->run = WSYSFS;
			} else if (strncmp(postvars[i], "B8", 2) == 0) {
				info->run = TEST;
			} else if (strncmp(postvars[i], "FB", 2) == 0) {
				info->framebuffer = str2num(postvars[i + 1]);
			} else if (strncmp(postvars[i], "__", 2) == 0) {
				FILE *sysfsfp;
				char *filename;
				filename = postvars[i];
				filename += 2;
//				syslog(LOG_INFO, "write_sysfs %s = %s\n",filename, postvars[i+1]);
				if (postvars[i+1][0]) {
					sysfsfp = fopen(filename, "w");
//					syslog(LOG_INFO, "sysfsfp %p\n",sysfsfp);
					if (sysfsfp != NULL) {
//						syslog(LOG_INFO, "write_sysfs %s = %s\n",filename, postvars[i+1]);
						fprintf(sysfsfp, "%s\n", delspace(postvars[i + 1]));
						fclose(sysfsfp);
					}
				}
			}
		}
	}

	/* FIXME this should be build from scan_elements */
	if (strncmp(postvars[info->sinput.device], "cf-ad9643", 9) == 0)
		info->id = ID_AD9643;
	else if (strncmp(postvars[info->sinput.device], "cf-ad9467", 9) == 0)
		info->id = ID_AD9467;
	else if (strncmp(postvars[info->sinput.device], "cf-ad9265", 9) == 0)
		info->id = ID_AD9467;
	else if (strncmp(postvars[info->sinput.device], "cf-ad9250", 9) == 0)
		info->id = ID_AD9250;
	else
		info->id = 0;

	if (!info->sdisplay.tdom)
		info->stime_s.samples = 1 << info->stime_s.fsamples;

	return 0;
};

int check_request(int form_method, char **getvars, char **postvars, s_info * info)
{
	if ((info->sinput.slaveadc != 0xFFFF) &&
		(strcmp(postvars[info->sinput.device],
			postvars[info->sinput.slaveadc]) == 0) &&
			(info->run == ACQUIRE || info->run == SAVE))
		do_error(1234, form_method, getvars, postvars, info);

	if (!info->sdisplay.tdom)
		if (!info->sdisplay.hw_fft && (info->stime_s.fsamples > 10)) {
			info->stime_s.fsamples = 10;
			info->stime_s.samples = 1 << info->stime_s.fsamples;
		}

// 	if (info->stime_s.sps > (MAXSAMPLERATE)
// 	    || (info->stime_s.sps <= MINSAMPLERATE))
// 		do_error(SAMPLE_RATE, form_method, getvars, postvars, info);

	if (info->stime_s.samples > MAXNUMSAMPLES
	    || info->stime_s.samples <= MINNUMSAMPLES)
		do_error(SAMPLE_DEPTH, form_method, getvars, postvars, info);

// 	if ((info->stime_s.samples / info->stime_s.sps) > TIMEOUT)
// 		do_error(TIME_OUT, form_method, getvars, postvars, info);

	if (atof(postvars[info->sdisplay.size_ratio]) <= 0
	    || atof(postvars[info->sdisplay.size_ratio]) >= MAXSIZERATIO)
		do_error(SIZE_RATIO, form_method, getvars, postvars, info);

	if (str2num(postvars[info->sdisplay.xrange]) < 0)
		do_error(RANGE, form_method, getvars, postvars, info);

	if (str2num(postvars[info->sdisplay.xrange1]) < 0)
		do_error(RANGE, form_method, getvars, postvars, info);

	if (!(postvars[info->sdisplay.xrange][0] == '*' &&
	      postvars[info->sdisplay.xrange1][0] == '*'))
		if (atof(postvars[info->sdisplay.xrange1]) <=
		    atof(postvars[info->sdisplay.xrange]))
			do_error(RANGE, form_method, getvars, postvars, info);

	return 0;
}

int
do_test(int form_method, char **getvars, char **postvars, s_info * info)
{
	int i, ret;
	static const s_test *test;

	if (info->id == ID_AD9250)
		test = stest_9250;
	else if (info->id == ID_AD9467)
		test = stest_9467;
	else
		test = stest;

	for (i = 0; i < ARRAY_SIZE(stest); i++) {
		ret = iio_test(form_method, getvars, postvars, info,
				postvars[info->sinput.device],
				(info->sinput.slaveadc == 0xFFFF) ? NULL : postvars[info->sinput.slaveadc], &test[i]);

		if (ret < 0) {
			do_error(IIO_OPEN, form_method, getvars, postvars, info);
		}
	}
	return ret;
}

// int
// do_test(int form_method, char **getvars, char **postvars, s_info * info)
// {
// 	int i, k, ret, err;
// 	unsigned char field[32];
//
// 	for (k = 0; k < 32 ; k++) {
// 		debugfs_write_devattr(postvars[info->sinput.device],
// 			"direct_reg_access", 0x17, 1, (k > 0) ? (k | 0x80) : 0);
//
// 		for (i = 0, err = 0; i < ARRAY_SIZE(stest); i++) {
// 			ret = iio_test(form_method, getvars, postvars, info,
// 					postvars[info->sinput.device],
// 					(info->sinput.slaveadc == 0xFFFF) ? NULL : postvars[info->sinput.slaveadc], &stest[i]);
//
// 			if (ret == 1)
// 				err++;
//
// 			if (ret < 0) {
// 				do_error(IIO_OPEN, form_method, getvars, postvars, info);
// 			}
// 		}
// 		field[k] = err;
// 	}
// 	for (k = 0; k < 32; k++)
// 		printf("<p><font face=\"Arial Black\" size=\"5\">[%d] = %d\n</font></p><hr>", k, field[k]);
//
// 	return ret;
// }


int
get_sample_freq(int form_method, char **getvars, char **postvars, s_info * info)
{
	int ret;

	ret = iio_read_devattr(postvars[info->sinput.device], "in_voltage_sampling_frequency", &info->stime_s.sps);

// 	if (strncmp (postvars[info->sinput.device], "cf-ad9643-core-lpc",
// 		strlen("cf-ad9643-core-lpc")) == 0) {
// 		ret = iio_read_devattr("ad9523-lpc", "out_altvoltage2_ADC_CLK_frequency", &info->stime_s.sps);
// 	} else if (strncmp (postvars[info->sinput.device], "cf-ad9643-core-hpc",
// 		strlen("cf-ad9643-core-hpc")) == 0) {
// 		ret = iio_read_devattr("ad9523-hpc", "out_altvoltage2_ADC_CLK_frequency", &info->stime_s.sps);
// 	}

	if (ret < 0) {
		do_error(IIO_OPEN, form_method, getvars, postvars, info);
	}

	return ret;
}

int
system_sync(int form_method, char **getvars, char **postvars, s_info * info)
{

	unsigned int val;
	int ret;

	return 0;

// 	if (strncmp (postvars[info->sinput.device], "ad9523-lpc", strlen("ad9523-lpc")) == 0) {
// 		ret = iio_read_devattr("ad9523-lpc", "out_altvoltage5_TX_LO_REF_CLK_frequency", &val);
// 		if (ret >= 0)
// 			iio_write_devattr("adf4351-tx-lpc", "out_altvoltage0_refin_frequency", val);
//
// 		ret = iio_read_devattr("ad9523-lpc", "out_altvoltage9_RX_LO_REF_CLK_frequency", &val);
// 		if (ret >= 0)
// 			iio_write_devattr("adf4351-rx-lpc", "out_altvoltage0_refin_frequency", val);
//
// 		ret = iio_read_devattr("ad9523-lpc", "out_altvoltage12_DAC_CLK_frequency", &val);
// 		if (ret >= 0)
// 			iio_write_devattr("cf-ad9122-core-lpc", "in_altvoltage0_DAC_CLK_frequency", val);
//
// 	} else if (strncmp (postvars[info->sinput.device], "ad9523-hpc", strlen("ad9523-hpc")) == 0) {
// 		ret = iio_read_devattr("ad9523-hpc", "out_altvoltage5_TX_LO_REF_CLK_frequency", &val);
// 		if (ret >= 0)
// 			iio_write_devattr("adf4351-tx-hpc", "out_altvoltage0_refin_frequency", val);
//
// 		ret = iio_read_devattr("ad9523-hpc", "out_altvoltage9_RX_LO_REF_CLK_frequency", &val);
// 		if (ret >= 0)
// 			iio_write_devattr("adf4351-rx-hpc", "out_altvoltage0_refin_frequency", val);
//
// 		ret = iio_read_devattr("ad9523-hpc", "out_altvoltage12_DAC_CLK_frequency", &val);
// 		if (ret >= 0)
// 			iio_write_devattr("cf-ad9122-core-hpc", "in_altvoltage0_DAC_CLK_frequency", val);
// 	}
//
// 	return ret;
}

int
make_file_samples(int form_method, char **getvars, char **postvars, s_info * info)
{
	int ret = iio_sample(form_method, getvars, postvars, info,
			     postvars[info->sinput.device],
			     (info->sinput.slaveadc == 0xFFFF) ? NULL : postvars[info->sinput.slaveadc]);

	if (ret < 0) {
		do_error(IIO_OPEN, form_method, getvars, postvars, info);
	}

	return ret;
}

int
make_file_init(int form_method, char **getvars, char **postvars, s_info * info)
{
//	int i, j;
	/* open file for write */
	unsigned has_slave = info->has_slave;

	info->pFile_init = fopen(info->pFILENAME_GNUPLT, "w");

	if (info->pFile_init == NULL) {
		do_error(FILE_OPEN, form_method, getvars, postvars, info);
	}

	/* print header information */

	fprintf(info->pFile_init, "#GNUPLOT File generated by NDSO\n");
	fprintf(info->pFile_init, "set term png\nset output \"../img%s.png\"\n",
		info->pREMOTE_ADDR);

	/* print commands */

	if (info->sdisplay.set_grid)
		fprintf(info->pFile_init, "set grid\n");

	if (info->sdisplay.axis)
		fprintf(info->pFile_init, "set xzeroaxis lt 2 lw 4\n");

	if (info->sdisplay.logscale)
		fprintf(info->pFile_init, "set %s\n",
			postvars[info->sdisplay.logscale]);

	if (info->sdisplay.style)
		fprintf(info->pFile_init, "set style data %s\n",
			postvars[info->sdisplay.style]);
//		fprintf(info->pFile_init, "set data style %s\n",
//
	if (info->sdisplay.size_ratio)
		fprintf(info->pFile_init, "set size %s\n",
			postvars[info->sdisplay.size_ratio]);

	fprintf(info->pFile_init, "set xrange [%s:%s]\n",
		postvars[info->sdisplay.xrange],
		postvars[info->sdisplay.xrange1]);

	if (info->sdisplay.tdom) {
		if (postvars[info->svertical.vdiv][0] != 'X')
			fprintf(info->pFile_init, "set ytics %s\n",
				postvars[info->svertical.vdiv]);

		fprintf(info->pFile_init,
			"set xlabel \"%d Samples @ %d Samples/s                t->\"\n",
			info->stime_s.samples, info->stime_s.sps);
		fprintf(info->pFile_init, "set ylabel \"ADC Values\" \n");

		switch (info->channel_en_mask) {
		case 3:
			if (has_slave)
				fprintf(info->pFile_init, "plot \"%s\" using 3:1 title \"LPC_CH0\", '' using 3:2 title \"LPC_CH1\", \"%s\" using 3:1 title \"HPC_CH0\", '' using 3:2 title \"HPC_CH1\"\n", info->pFILENAME_T_OUT, info->pFILENAME_T_OUT2);
			else
				fprintf(info->pFile_init, "plot \"%s\" using 3:1 title \"ch0\", '' using 3:2 title \"ch1\"\n", info->pFILENAME_T_OUT);
			break;
		case 2:
			if (has_slave)
				fprintf(info->pFile_init, "plot \"%s\" title \"LPC_CH1\", \"%s\" title \"HPC_CH1\"\n", info->pFILENAME_T_OUT, info->pFILENAME_T_OUT2);
			else
				fprintf(info->pFile_init, "plot \"%s\" title \"ch1\"\n", info->pFILENAME_T_OUT);
			break;
		case 1:
			if (has_slave)
				fprintf(info->pFile_init, "plot \"%s\" title \"LPC_CH0\", \"%s\" title \"HPC_CH0\"\n", info->pFILENAME_T_OUT, info->pFILENAME_T_OUT2);
			else
				fprintf(info->pFile_init, "plot \"%s\" title \"ch0\"\n", info->pFILENAME_T_OUT);
			break;
		}
	} else {
		fprintf (info->pFile_init, "set ylabel \"Magnitude in dB\" \n");

	      if (info->sdisplay.fftscaled)
		{
		  fprintf (info->pFile_init,
			   "set xlabel \"%d point FFT @ %d Samples/s               f/Hz->\"\n",
			   info->stime_s.samples, info->stime_s.sps);
		if (has_slave)
			   fprintf (info->pFile_init,
			   "plot  \"%s\" using ($1*%d/%d):($2) title \"LPC\", \"%s\" using ($1*%d/%d):($2) title \"HPC\" \nexit\n",
			   info->pFILENAME_T_OUT, info->stime_s.sps,
			   info->stime_s.samples,
			   info->pFILENAME_T_OUT2, info->stime_s.sps,
			   info->stime_s.samples);
		else
			   fprintf (info->pFile_init,
			   "plot  \"%s\" using ($1*%d/%d):($2) notitle %s  \nexit\n",
			   info->pFILENAME_T_OUT, info->stime_s.sps,
			   info->stime_s.samples, postvars[info->sdisplay.color]);
		}
	      else
		{
		  fprintf (info->pFile_init,
			   "set xlabel \"%d point FFT @ %d Samples/s               f->\"\n",
			   info->stime_s.samples, info->stime_s.sps);
		if (has_slave)
			   fprintf (info->pFile_init,
			   "plot  \"%s\" using 1:($2) title \"LPC\", \"%s\" using 1:($2) title \"HPC\" \nexit\n",
			   info->pFILENAME_T_OUT, info->pFILENAME_T_OUT2);
		else
			   fprintf (info->pFile_init,
			   "plot  \"%s\" using 1:($2) notitle %s  \nexit\n",
			   info->pFILENAME_T_OUT, postvars[info->sdisplay.color]);
		}
	}


//	fprintf(info->pFile_init, "plot \"%s\" ", info->pFILENAME_T_OUT);

	/* Last channel is the timestamp */

//	for (i = 0, j = 0; i < info->num_channels; i++) {
//		while (!((info->channel_en_mask >> j++) & 1)) ;
//		if (i == (info->num_channels - 1)) {
//			if (str2num(postvars[info->sdisplay.smooth]))
//				fprintf(info->pFile_init,
//					"using %d:%d smooth %s title \"ch%d\"",
//					info->num_channels, i + 1, postvars[info->sdisplay.smooth],
//					j - 1);
//			else
//				fprintf(info->pFile_init,
//					"using %d:%d title \"ch%d\"",info->num_channels, i + 1,
//					j - 1);
//		} else {
//			if (str2num(postvars[info->sdisplay.smooth]))
//				fprintf(info->pFile_init,
//					"using %d:%d smooth %s title \"ch%d\", '' ",
//					info->num_channels, i + 1, postvars[info->sdisplay.smooth],
//					j - 1);
//			else
//				fprintf(info->pFile_init,
//					"using %d:%d title \"ch%d\", '' ",info->num_channels, i + 1,
//					j - 1);
//		}
//	}
	fprintf(info->pFile_init, "exit\n");

	/* close file */

	fclose(info->pFile_init);

	return 0;
};

int main()
{
	char **postvars = NULL;	/* POST request data repository */
	char **getvars = NULL;	/* GET request data repository */
	int form_method;	/* POST = 1, GET = 0 */

	s_info *info = &sinfo;

	form_method = getRequestMethod();

	if (form_method == POST) {
		getvars = getGETvars();
		postvars = getPOSTvars();
	} else if (form_method == GET) {
		getvars = getGETvars();
	}

	make_session_files(info);

	parse_request(form_method, getvars, postvars, info);
	if (info->run != WSYSFS)
		check_request(form_method, getvars, postvars, info);

	switch (info->run) {

	case ACQUIRE:
//		if (info->id == ID_AD9643)
			get_sample_freq(form_method, getvars, postvars, info);
//		else
//			info->stime_s.sps = 250000000;
		info->num_channels =
		    make_file_samples(form_method, getvars, postvars, info);
		make_file_init(form_method, getvars, postvars, info);
		system(info->pGNUPLOT);
		do_html(form_method, getvars, postvars, info);
		display_on_framebuffer(info);
		break;
	case SAVE:
		info->num_channels =
		    make_file_samples(form_method, getvars, postvars, info);
		do_html(form_method, getvars, postvars, info);
		break;

	case SHOWDEVATTR:
	case GNUPLOT_FILES:
	case WRITEREG:
	case READREG:
	case WSYSFS:
	case TEST:
		do_html(form_method, getvars, postvars, info);
		break;

	default:

		break;
	}

	free_session_files(info);
	exit(0);
}
