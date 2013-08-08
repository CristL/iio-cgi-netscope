/* Industrialio buffer test code.
 *
 * Copyright (c) 2008 Jonathan Cameron
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published by
 * the Free Software Foundation.
 *
 * This program is primarily intended as an example application.
 * Reads the current buffer setup from sysfs and starts a short capture
 * from the specified device, pretty printing the result after appropriate
 * conversion.
 *
 * Command line parameters
 * generic_buffer -n <device_name> -t <trigger_name>
 * If trigger name is not specified the program assumes you want a dataready
 * trigger associated with the device and goes looking for it.
 *
 */

#define _GNU_SOURCE
#include <unistd.h>
#include <dirent.h>
#include <fcntl.h>
#include <stdio.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/dir.h>
#include <linux/types.h>
#include <syslog.h>

#include "ndso.h"
#include "iio_utils.h"

#define BINARY_OFFSET		0

unsigned samples_per_scan = 2;

//#define BINARY_OFFSET		0

/**
 * size_from_channelarray() - calculate the storage size of a scan
 * @channels: the channel info array
 * @num_channels: size of the channel info array
 *
 * Has the side effect of filling the channels[i].location values used
 * in processing the buffer output.
 **/
int size_from_channelarray(struct iio_channel_info *channels, int num_channels)
{
	int bytes = 0;
	int i = 0;
	while (i < num_channels) {
		if (bytes % channels[i].bytes == 0)
			channels[i].location = bytes;
		else
			channels[i].location = bytes - bytes % channels[i].bytes
			    + channels[i].bytes;
		bytes = channels[i].location + channels[i].bytes;
		i++;
	}
	return bytes;
}

/**
 * process_scan() - print out the values in SI units
 * @data:		pointer to the start of the scan
 * @infoarray:		information about the channels. Note
 *  size_from_channelarray must have been called first to fill the
 *  location offsets.
 * @num_channels:	the number of active channels
 **/
void process_scan(s_info * info,
		  char *data,
		  struct iio_channel_info *infoarray,
		  int num_channels, unsigned scan_size, int64_t tstmp)
{
	int k;

	for (k = 0; k < num_channels; k++) {

		switch (infoarray[k].bytes) {
			/* only a few cases implemented so far */
		case 2:
			if (infoarray[k].is_signed) {
				int16_t val = *(int16_t *)
				    (data + infoarray[k].location);
				if ((val >> infoarray[k].bits_used) & 1)
					val = (val & infoarray[k].mask) |
					    ~infoarray[k].mask;
				fprintf(info->pFile_samples, "%05f ",
					((float)val +
					 infoarray[k].offset) *
					infoarray[k].scale);
			} else {
				uint16_t val = *(uint16_t *)
				    (data + infoarray[k].location);
				val = (val & infoarray[k].mask);
				fprintf(info->pFile_samples, "%05f ",
					((float)val +
					 infoarray[k].offset) *
					infoarray[k].scale);
			}
			break;
		case 8:
			if (infoarray[k].is_signed) {
				int64_t val = *(int64_t *)
				    (data + infoarray[k].location);
				if ((val >> infoarray[k].bits_used) & 1)
					val = (val & infoarray[k].mask) |
					    ~infoarray[k].mask;
				/* special case for timestamp */
				if (infoarray[k].scale == 1.0f &&
				    infoarray[k].offset == 0.0f)
					fprintf(info->pFile_samples, "%lld ",
						(long long int)(val  - tstmp));
				else
					fprintf(info->pFile_samples, "%05f ",
						((float)val +
						 infoarray[k].offset) *
						infoarray[k].scale);
			}
			break;
		default:
			break;
		}

	}
	fprintf(info->pFile_samples, "\n");

}

int iio_enable_selected_channels(char *dev_dir_name, unsigned long ch_mask)
{
	char *scan_el_dir;
	const struct dirent *ent;
	DIR *dp;
	int ret, i = 0;
	FILE *sysfsfp;
	char *filename;

	ret = asprintf(&scan_el_dir, FORMAT_SCAN_ELEMENTS_DIR, dev_dir_name);
	if (ret < 0) {
		ret = -ENOMEM;
		goto error_ret;
	}
	dp = opendir(scan_el_dir);
	if (dp == NULL) {
		ret = -errno;
		goto error_free_name;
	}
	while (ent = readdir(dp), ent != NULL)
		if ((strcmp(ent->d_name + strlen(ent->d_name) - strlen("_en"),
			   "_en") == 0) && !(strcmp(ent->d_name, "timestamp_en") == 0)) {
			ret = asprintf(&filename,
				       "%s/%s", scan_el_dir, ent->d_name);
			if (ret < 0) {
				ret = -ENOMEM;
				goto error_close_dir;
			}
			sysfsfp = fopen(filename, "w");
			if (sysfsfp == NULL) {
				ret = -errno;
				free(filename);
				goto error_close_dir;
			}
			fprintf(sysfsfp, "%d\n", !!((ch_mask >> i++) & 1));
			fclose(sysfsfp);
			free(filename);
		}

error_close_dir:
	closedir(dp);
error_free_name:
	free(scan_el_dir);
error_ret:
	return ret;
}



int __write_devattr(const char *dir, char *device_name, char *attr, unsigned int value, int type2, unsigned int value2)
{
	char *dev_dir_name;
	int dev_num, ret = 0;

	/* Find the device requested */
	dev_num = find_type_by_name(device_name, "iio:device");
	if (dev_num < 0) {
		syslog(LOG_INFO, "Failed to find the %s\n", device_name);
		ret = -ENODEV;
		goto error_ret;
	}

	asprintf(&dev_dir_name, "%siio:device%d", dir, dev_num);

	/* Setup ring buffer parameters */

	if (type2)
		ret = write_sysfs_int2(attr, dev_dir_name, value, value2);
	else
		ret = write_sysfs_int(attr, dev_dir_name, value);

	if (ret < 0) {
		syslog(LOG_INFO, "write_sysfs_int failed (%d)\n",__LINE__);
	}

	free(dev_dir_name);

error_ret:
	return ret;
}

int __read_devattr(const char *dir, char *device_name, char *attr, unsigned int *value)
{
	char *dev_dir_name;
	int dev_num, ret = 0;

	/* Find the device requested */
	dev_num = find_type_by_name(device_name, "iio:device");
	if (dev_num < 0) {
		syslog(LOG_INFO, "Failed to find the %s\n", device_name);
		ret = -ENODEV;
		goto error_ret;
	}

	asprintf(&dev_dir_name, "%siio:device%d", dir, dev_num);

	/* Setup ring buffer parameters */
	ret = read_sysfs_posint(attr, dev_dir_name);
	if (ret < 0) {
		syslog(LOG_INFO, "read_sysfs_posint failed (%d)\n",__LINE__);
	}
	*value = ret;
	free(dev_dir_name);

error_ret:
	return ret;
}

int iio_write_devattr(char *device_name, char *attr, unsigned int value)
{
	return __write_devattr(iio_dir, device_name, attr, value, 0, 0);
}

int iio_read_devattr(char *device_name, char *attr, unsigned int *value)
{
	return __read_devattr(iio_dir, device_name, attr, value);
}

int debugfs_write_devattr(char *device_name, char *attr, unsigned int value, int type, unsigned int value2)
{
	return __write_devattr(iio_debugfs_dir, device_name, attr, value, type, value2);
}

int debugfs_read_devattr(char *device_name, char *attr, unsigned int *value)
{
	return __read_devattr(iio_debugfs_dir, device_name, attr, value);
}

int iio_read_device_files(char *device_name, unsigned out)
{
	struct dirent **namelist;
	struct stat entrystat;
	char *dev_dir_name;
	int dev_num, n, i, ret = 0;
	FILE *sysfsfp;
	char *filename;
	char buf[4096];

	/* Find the device requested */
	dev_num = find_type_by_name(device_name, "iio:device");
	if (dev_num < 0) {
		syslog(LOG_INFO, "Failed to find the %s\n", device_name);
		ret = -ENODEV;
		goto error_ret;
	}

	asprintf(&dev_dir_name, "%siio:device%d", iio_dir, dev_num);

	printf("<form method=\"POST\" action=\"/cgi-bin/ndso.cgi\" target=\"main\">\n");
	printf("<TABLE BORDER=\"2\" BORDERCOLORLIGHT=\"#66FFFF\">\n");

	n = scandir(dev_dir_name, &namelist, 0, alphasort);
	if (n < 0)
               syslog(LOG_INFO, "scandir failed %d\n", errno);
	else {
		for (i = 0; i < n; i++) {
			ret = asprintf(&filename, "%s/%s", dev_dir_name, namelist[i]->d_name);
			if (ret < 0) {
				ret = -ENOMEM;
				goto error_close_dir;
			}

			stat(filename, &entrystat);
			if (S_ISREG(entrystat.st_mode)) {

				sysfsfp = fopen(filename, "r");
				if (sysfsfp == NULL) {
					strcpy(buf, "");
				} else {
					if (fgets((char *) &buf, 4096, sysfsfp) == NULL)
						strcpy(buf, "");

				}
				if (entrystat.st_mode & S_IWUSR) {
					printf
					("<TR> <TD WIDTH=\"100\"><FONT FACE=\"Arial\" SIZE=\"5\">  %s </FONT></TD> <TD><FONT FACE=\"Arial\" SIZE=\"5\"> %s </FONT></TD> <TD> <input type=\"text\" STYLE=\"background-color: #00BFFF;\" name=\"__%s/%s\" size=\"15\" maxlength=\"24\" value=\"\"> </TD> </TR> \n",
					namelist[i]->d_name, buf, dev_dir_name, namelist[i]->d_name);
					if (sysfsfp != NULL)
						fclose(sysfsfp);
				} else {
					printf("<TR> <TD WIDTH=\"100\"><FONT FACE=\"Arial\" SIZE=\"5\">  %s </FONT></TD> <TD><FONT FACE=\"Arial\" SIZE=\"5\">", namelist[i]->d_name);
					printf("%s <BR>", buf);
					while (fgets((char *) &buf, 4096, sysfsfp) != NULL) {
						printf("%s <BR>", buf);
					}
					printf("</FONT></TD> <TD> Read Only </TD> </TR>\n");
					fclose(sysfsfp);
				}
			}
			free(filename);
			free(namelist[i]);
		}
		free(namelist);
	}

	printf("</TABLE>\n");
	printf("<input type=\"text\" name=\"device\" size=\"15\" maxlength=\"24\" value=\"%s\">", device_name);
	printf("<input type=\"submit\" value=\"Update Values\" name=\"B9\">");
	printf("</from>\n");

error_close_dir:
error_free_name:
	free(dev_dir_name);
error_ret:
	return ret;
}

int iio_stats(s_info * info, int count, short *data)
{
	int i, avg0, min0, max0, avg1, min1, max1, val;

	min0 = data[0];
	max0 = data[0];
	avg0 = 0;

	min1 = data[1];
	max1 = data[1];
	avg1 = 0;

	for (i = 0; i < (count * samples_per_scan); i+=samples_per_scan) {
		val = data[i] - BINARY_OFFSET;

		if (val < min0)
			min0 = val;
		if (val > max0)
			max0 = val;
		avg0 += val;

		val = data[i+1] - BINARY_OFFSET;

		if (val < min1)
			min1 = val;
		if (val > max1)
			max1 = val;
		avg1 += val;
	}

	info->min_ch0 = min0;
	info->min_ch1 = min1;
	info->max_ch0 = max0;
	info->max_ch1 = max1;
	info->avg_ch0 = (float)avg0 / count;
	info->avg_ch1 = (float)avg1 / count;

	return 0;
}

int iio_set_scan_elements(s_info * info, char *dev_dir_name, unsigned mask)
{
	char *scan_el_dir;

	int ret = asprintf(&scan_el_dir, FORMAT_SCAN_ELEMENTS_DIR, dev_dir_name);
	if (ret < 0) {
		ret = -ENOMEM;
	}

	if (info->sdisplay.hw_fft) {
		write_sysfs_int("in_voltage0_frequency_domain_en", scan_el_dir,
				!!(mask & 0x1) && !info->sdisplay.tdom);
		write_sysfs_int("in_voltage1_frequency_domain_en", scan_el_dir,
				!!(mask & 0x2) && !info->sdisplay.tdom);
	} else {
		write_sysfs_int("in_voltage0_en", scan_el_dir,
				!!(mask & 0x1));
		write_sysfs_int("in_voltage1_en", scan_el_dir,
				!!(mask & 0x2));
	}

	samples_per_scan = 0;

	ret = read_sysfs_posint("in_voltage0_en", scan_el_dir);
	if (ret >= 0)
		samples_per_scan += ret;

	ret = read_sysfs_posint("in_voltage1_en", scan_el_dir);
	if (ret >= 0)
		samples_per_scan += ret;

	free(scan_el_dir);


	return ret;
}


int iio_sample(int form_method, char **getvars, char **postvars, s_info * info,
	       char *device_name, char *device_name_slave)
{
	int ret, i, buf_len;
	int fp;
	int cnt;
	char *dev_dir_name, *buf_dir_name, *saved_device_name = NULL, *pFILENAME_T_OUT;
	FILE *file_samples;
#if BINARY_OFFSET > 0
	unsigned short *data;
#else
	short *data;
#endif
	size_t read_size;
	int dev_num;
	char *buffer_access;

//	syslog(LOG_INFO, "device_name = %s, device_name_slave %s\n", device_name, device_name_slave);

	if (device_name == NULL)
		return -1;

	if (device_name_slave && (device_name_slave[0] != 0)) {
		/* We are first, so start with the slave */
		info->has_slave = 1;
		saved_device_name = device_name;
		device_name = device_name_slave;
	} else {
		pFILENAME_T_OUT = info->pFILENAME_T_OUT;
	}


	/* Find the device requested */
	dev_num = find_type_by_name(device_name, "iio:device");
	if (dev_num < 0) {
		syslog(LOG_INFO, "Failed to find the %s\n", device_name);
		ret = -ENODEV;
		goto error_ret;
	}

	asprintf(&dev_dir_name, "%siio:device%d", iio_dir, dev_num);

	ret = asprintf(&buf_dir_name, "%siio:device%d/buffer", iio_dir, dev_num);
	if (ret < 0) {
		ret = -ENOMEM;
		syslog(LOG_INFO, "asprintf failed (%d)\n",__LINE__);
		goto error_ret;
	}

	iio_set_scan_elements(info, dev_dir_name,
			(info->id == ID_AD9250) ? 0x3 : info->channel_en_mask);

	buf_len = info->stime_s.samples * sizeof(short) * samples_per_scan;

	/* Setup ring buffer parameters */
	ret = write_sysfs_int("length", buf_dir_name, buf_len);
	if (ret < 0) {
		syslog(LOG_INFO, "write_sysfs_int failed (%d) (%d) %s %d\n",
			__LINE__, ret, buf_dir_name, buf_len);
		goto error_free_scan_el_dir_name;
	}

	/* Enable the buffer */
	ret = write_sysfs_int("enable", buf_dir_name, 1);
	if (ret < 0) {
		syslog(LOG_INFO, "write_sysfs_int failed (%d)\n",__LINE__);
//		goto error_free_buf_dir_name;
	}

	if (saved_device_name) {
		/* We've setup the Slave, now trigger the master */
		iio_sample(form_method, getvars, postvars, info,
			     saved_device_name,
			     NULL);
		pFILENAME_T_OUT = info->pFILENAME_T_OUT2;
	}

	data = malloc(buf_len);
	if (!data) {
		syslog(LOG_INFO, "malloc failed (%d)\n",__LINE__);
		ret = -ENOMEM;
		goto error_disable;
	}

	ret = asprintf(&buffer_access,
		       "/dev/iio:device%d", dev_num);
	if (ret < 0) {
		ret = -ENOMEM;
		syslog(LOG_INFO, "asprintf failed (%d)\n",__LINE__);
		goto error_free_data;
	}

	/* Attempt to open the event access dev (blocking this time) */
	fp = open(buffer_access, O_RDONLY | O_NONBLOCK);
	if (fp < 0) {
		syslog(LOG_INFO, "Failed to open %s\n", buffer_access);
		ret = -errno;
		goto error_close_buffer_access;
	}

	file_samples = fopen(pFILENAME_T_OUT, "w");
	if (file_samples == NULL){
		syslog(LOG_INFO, "Failed to open %s\n", pFILENAME_T_OUT);
		goto error_close_buffer_access;

	}

	read_size = read(fp, data, buf_len);
	if (read_size == -EAGAIN) {
		syslog(LOG_INFO, "nothing available\n");
	}

	if (info->sdisplay.tdom) {
		iio_stats(info, info->stime_s.samples, data);
		switch (info->channel_en_mask) {
		case 3:
			for (i = 0; i < (info->stime_s.samples * samples_per_scan); i+=samples_per_scan)
				fprintf(file_samples, "%d %d %d\n", data[i] - BINARY_OFFSET, data[i+1] - BINARY_OFFSET, i >> 1);

			break;
		case 1:
			for (i = 0; i < (info->stime_s.samples * samples_per_scan); i+=samples_per_scan)
				fprintf(file_samples, "%d\n", data[i] - BINARY_OFFSET);
			break;
		case 2:
			for (i = 0; i < (info->stime_s.samples * samples_per_scan); i+=samples_per_scan)
				fprintf(file_samples, "%d\n", data[i+1] - BINARY_OFFSET);
			break;
		}
	} else if (info->sdisplay.hw_fft) {
		short *real;
		short *imag;
		short *amp;

		real = malloc(((2 * info->stime_s.samples) + (info->stime_s.samples / 2)) * sizeof(short));
		if (real == NULL){
			syslog(LOG_INFO, "malloc failed (%d)\n",__LINE__);
			ret = -ENOMEM;
			goto error_close_file_samples;
		}

 		imag = real + info->stime_s.samples;
 		amp = imag + info->stime_s.samples;

		cnt = 0;

		for (i = 0; i < (info->stime_s.samples * samples_per_scan); i+=samples_per_scan) {
			real[cnt] = data[i];
			imag[cnt] = data[i + 1];
			cnt++;
		}

//		fix_loud (amp, real, imag, info->stime_s.samples/2, 0); /* scale 14->16 bit */

		for (i = info->sdisplay.fftexludezero;
       			i < (info->stime_s.samples / 2); i++) {
       				fprintf (file_samples, "%d %d\n", i, real[i]);
		}

		free(real);

	} else {
		short *real;
		short *imag;
		short *amp;

		real = malloc(((2 * info->stime_s.samples) + (info->stime_s.samples / 2)) * sizeof(short));
		if (real == NULL){
			syslog(LOG_INFO, "malloc failed (%d)\n",__LINE__);
			ret = -ENOMEM;
			goto error_close_file_samples;
		}

 		imag = real + info->stime_s.samples;
 		amp = imag + info->stime_s.samples;

		cnt = 0;
		switch (info->channel_en_mask) {
		case 1:
			for (i = 0; i < (info->stime_s.samples * samples_per_scan); i+=samples_per_scan) {
				real[cnt] = data[i] - BINARY_OFFSET;
				imag[cnt] = 0;
				cnt++;
			}
			break;
		case 2:
			for (i = 0; i < (info->stime_s.samples * samples_per_scan); i+=samples_per_scan) {
				real[cnt] = data[i+1] - BINARY_OFFSET;
				imag[cnt] = 0;
				cnt++;
			}
			break;
		case 3:
			for (i = 0; i < (info->stime_s.samples * samples_per_scan); i+=samples_per_scan) {
				real[cnt] = data[i] - BINARY_OFFSET;
				imag[cnt] = data[i + 1] - BINARY_OFFSET;
				cnt++;
			}
				if (info->sdisplay.window)
					window(imag, info->stime_s.samples);
			break;
		}

		if (info->sdisplay.window)
			window(real, info->stime_s.samples);

		fix_fft (real, imag, info->stime_s.fsamples, 0);
		fix_loud (amp, real, imag, info->stime_s.samples/2, 2); /* scale 14->16 bit */

		for (i = info->sdisplay.fftexludezero;
       			i < (info->stime_s.samples / 2); i++) {
       				fprintf (file_samples, "%d %d\n", i, amp[i]);
		}

		free(real);
	}

error_close_file_samples:
	fclose(file_samples);

	ret = 3;

error_close_buffer_access:
	close(fp);
error_free_data:
	free(data);
	free(buffer_access);
error_disable:
	/* Stop the ring buffer */
	ret = write_sysfs_int("enable", buf_dir_name, 0);
	if (ret < 0) {
		syslog(LOG_INFO, "write_sysfs_int failed (%d)\n",__LINE__);
	}
error_free_scan_el_dir_name:


error_free_buf_dir_name:
	free(buf_dir_name);
error_ret:
	return ret;
}

int iio_test(int form_method, char **getvars, char **postvars, s_info * info,
	       char *device_name, char *device_name_slave, s_test *test)
{
	int ret, i, buf_len, err;
	int fp;
	int cnt;
	char *dev_dir_name, *buf_dir_name, *saved_device_name = NULL, *pFILENAME_T_OUT;
// #if BINARY_OFFSET > 0
// 	unsigned *data;
// #else
	int *data;
// #endif
	size_t read_size;
	int dev_num;
	char *buffer_access;
	unsigned fail1, fail2;

//	syslog(LOG_INFO, "device_name = %s, device_name_slave %s\n", device_name, device_name_slave);

	if (device_name == NULL)
		return -1;

	if (device_name_slave && (device_name_slave[0] != 0)) {
		/* We are first, so start with the slave */
		info->has_slave = 1;
		saved_device_name = device_name;
		device_name = device_name_slave;
	} else {

	}

	/* Find the device requested */
	dev_num = find_type_by_name(device_name, "iio:device");
	if (dev_num < 0) {
		syslog(LOG_INFO, "Failed to find the %s\n", device_name);
		ret = -ENODEV;
		goto error_ret;
	}

	asprintf(&dev_dir_name, "%siio:device%d", iio_dir, dev_num);

	ret = asprintf(&buf_dir_name, "%siio:device%d/buffer", iio_dir, dev_num);
	if (ret < 0) {
		ret = -ENOMEM;
		syslog(LOG_INFO, "asprintf failed (%d)\n",__LINE__);
		goto error_ret;
	}

	iio_set_scan_elements(info, dev_dir_name, 0x3);

	buf_len = info->stime_s.samples * sizeof(short) * samples_per_scan;

	write_sysfs_string("in_voltage0_test_mode", dev_dir_name, test->iiotestname);
	write_sysfs_string("in_voltage1_test_mode", dev_dir_name, test->iiotestname);

	/* Setup ring buffer parameters */
	ret = write_sysfs_int("length", buf_dir_name, buf_len);
	if (ret < 0) {
		syslog(LOG_INFO, "write_sysfs_int failed (%d) (%d) %s %d\n",
			__LINE__, ret, buf_dir_name, buf_len);
		goto error_free_buf_dir_name;
	}

	/* Enable the buffer */
	ret = write_sysfs_int("enable", buf_dir_name, 1);
	if (ret < 0) {
		syslog(LOG_INFO, "write_sysfs_int failed (%d)\n",__LINE__);
//		goto error_free_buf_dir_name;
	}

	if (saved_device_name) {
		/* We've setup the Slave, now trigger the master */
		iio_test(form_method, getvars, postvars, info,
			     saved_device_name,
			     NULL, test);
	}

	data = malloc(buf_len);
	if (!data) {
		syslog(LOG_INFO, "malloc failed (%d)\n",__LINE__);
		ret = -ENOMEM;
		goto error_disable;
	}

	ret = asprintf(&buffer_access,
		       "/dev/iio:device%d", dev_num);
	if (ret < 0) {
		ret = -ENOMEM;
		syslog(LOG_INFO, "asprintf failed (%d)\n",__LINE__);
		goto error_free_data;
	}

	/* Attempt to open the event access dev (blockinin_voltage0_eng this time) */
	fp = open(buffer_access, O_RDONLY | O_NONBLOCK);
	if (fp < 0) {
		syslog(LOG_INFO, "Failed to open %s\n", buffer_access);
		ret = -errno;
		goto error_close_buffer_access;
	}

	read_size = read(fp, data, buf_len);
	if (read_size == -EAGAIN) {
		syslog(LOG_INFO, "nothing available\n");
	}

	printf("<p><font face=\"Courier New\" size=\"3\">%s: Running test: %s [%d Samples]\n</font></p>",
	       device_name, test->testname, info->stime_s.samples);

	for (i = 0, err = 0; i < info->stime_s.samples; i++)
		if (!((data[i] == test->pat1) || (data[i] == test->pat2))) {
			if (!((data[i] == ((test->pat1 >> 16) | (test->pat2 << 16))) ||
				(data[i] == ((test->pat2 >> 16) | (test->pat1 << 16))))) {
				err++;
				fail1 = data[i];
				fail2 = data[i + 1];
			}
		}
	if (err) {
		printf("<p><font face=\"Courier New\" size=\"3\">Found %d Errors in test\n</font></p>", err);
		printf("<p><font face=\"Courier New\" size=\"3\">0x%X 0x%X\n</font></p>", fail1, fail2);
		printf("<p><font face=\"Arial Black\" color=\"red\" size=\"5\">FAILED\n</font></p><hr>");
		err = 1;
	} else {
		printf("<p><font face=\"Arial Black\" size=\"5\">PASSED\n</font></p><hr>");
		err = 0;
	}

//error_close_file_samples:
error_close_buffer_access:
	close(fp);
error_free_data:
	free(data);
	free(buffer_access);
error_disable:
	/* Stop the ring buffer */
	ret = write_sysfs_int("enable", buf_dir_name, 0);
	if (ret < 0) {
		syslog(LOG_INFO, "write_sysfs_int failed (%d)\n",__LINE__);
	}
error_free_buf_dir_name:
	free(buf_dir_name);
error_ret:
	write_sysfs_string("in_voltage0_test_mode", dev_dir_name, "off");
	write_sysfs_string("in_voltage1_test_mode", dev_dir_name, "off");
	return ret < 0 ? ret : err;
}
