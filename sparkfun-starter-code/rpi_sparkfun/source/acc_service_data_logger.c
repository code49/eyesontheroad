// Copyright (c) Acconeer AB, 2018-2021
// All rights reserved
// This file is subject to the terms and conditions defined in the file
// 'LICENSES/license_acconeer.txt', (BSD 3-Clause License) which is part
// of this source code package.

// Needed for clock_gettime, not a part of C99, see "man clock_gettime"
#define _POSIX_C_SOURCE 199309L

#include <complex.h>
#include <getopt.h>
#include <math.h>
#include <signal.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "acc_definitions_common.h"
#include "acc_hal_integration.h"
#include "acc_integration.h"
#include "acc_integration_log.h"
#include "acc_rss.h"
#include "acc_service.h"
#include "acc_service_envelope.h"
#include "acc_service_iq.h"
#include "acc_service_power_bins.h"
#include "acc_service_sparse.h"

#include "acc_version.h"

#define DEFAULT_UPDATE_COUNT                   0
#define DEFAULT_WAIT_FOR_INTERRUPT             true
#define DEFAULT_RANGE_START_M                  0.07f
#define DEFAULT_RANGE_END_M                    0.5f
#define DEFAULT_POWER_SAVE_MODE_INDEX          1
#define DEFAULT_DOWNSAMPLING_FACTOR            1
#define DEFAULT_HW_ACCELERATED_AVERAGE_SAMPLES 10
#define DEFAULT_N_BINS                         10
#define DEFAULT_SWEEPS_PER_FRAME               16
#define DEFAULT_SPARSE_DATA_FORMAT             "f"
#define DEFAULT_SERVICE_PROFILE                0         // Use service default profile
#define DEFAULT_GAIN                           -1.0f     //-1.0 will trigger that the stack default will be used
#define DEFAULT_FREQUENCY                      10.0f
#define DEFAULT_POWER_SAVE_MODE_STRING         "READY"
#define DEFAULT_RUNNING_AVG                    0.7f
#define DEFAULT_INTEGER_IQ                     false
#define DEFAULT_SENSOR                         1
#define DEFAULT_RUNTIME                        false
#define DEFAULT_DATE_TIMESTAMP                 false
#define DEFAULT_DATA_WARNINGS                  false
#define DEFAULT_LOG_LEVEL                      ACC_LOG_LEVEL_ERROR

#define SPARSE_DATA_FORMAT_BUFSIZE 8

volatile sig_atomic_t interrupted = 0;


typedef enum
{
	INVALID_SERVICE = 0,
	POWER_BIN,
	ENVELOPE,
	IQ,
	SPARSE
} service_type_t;

typedef struct
{
	bool runtime;
	bool date_timestamp;
	bool data_warnings;
} metadata_opt_t;

typedef struct
{
	service_type_t        service_type;
	uint16_t              update_count;
	bool                  wait_for_interrupt;
	float                 start_m;
	float                 end_m;
	float                 frequency;
	acc_power_save_mode_t power_save_mode;
	int                   downsampling_factor;
	int                   hwaas;
	int                   n_bins;
	int                   sweeps_per_frame;
	char                  sparse_data_format[SPARSE_DATA_FORMAT_BUFSIZE];
	float                 gain;
	uint32_t              service_profile;
	float                 running_avg;
	bool                  integer_iq;
	int                   sensor;
	metadata_opt_t        metadata_options;
	acc_log_level_t       log_level;
	char                  *file_path;
} input_t;


static bool string_to_power_save_mode(const char *str, acc_power_save_mode_t *power_save_mode);


static void initialize_input(input_t *input)
{
	input->service_type        = INVALID_SERVICE;
	input->update_count        = DEFAULT_UPDATE_COUNT;
	input->wait_for_interrupt  = DEFAULT_WAIT_FOR_INTERRUPT;
	input->start_m             = DEFAULT_RANGE_START_M;
	input->end_m               = DEFAULT_RANGE_END_M;
	input->frequency           = DEFAULT_FREQUENCY;
	input->downsampling_factor = DEFAULT_DOWNSAMPLING_FACTOR;
	input->hwaas               = DEFAULT_HW_ACCELERATED_AVERAGE_SAMPLES;
	input->n_bins              = DEFAULT_N_BINS;
	input->sweeps_per_frame    = DEFAULT_SWEEPS_PER_FRAME;
	input->gain                = DEFAULT_GAIN;
	input->service_profile     = DEFAULT_SERVICE_PROFILE;
	input->running_avg         = DEFAULT_RUNNING_AVG;
	input->integer_iq          = DEFAULT_INTEGER_IQ;
	input->sensor              = DEFAULT_SENSOR;
	input->log_level           = DEFAULT_LOG_LEVEL;
	input->file_path           = NULL;

	string_to_power_save_mode(DEFAULT_POWER_SAVE_MODE_STRING, &input->power_save_mode);

	strncpy(input->sparse_data_format, DEFAULT_SPARSE_DATA_FORMAT, SPARSE_DATA_FORMAT_BUFSIZE);

	input->metadata_options.runtime        = DEFAULT_RUNTIME;
	input->metadata_options.date_timestamp = DEFAULT_DATE_TIMESTAMP;
	input->metadata_options.data_warnings  = DEFAULT_DATA_WARNINGS;
}


static bool parse_options(int argc, char *argv[], input_t *input);


static void set_up_common(acc_service_configuration_t service_configuration, input_t *input);


static acc_service_configuration_t set_up_power_bin(input_t *input);


static bool execute_power_bin(acc_service_configuration_t power_bin_configuration, FILE *file, bool wait_for_interrupt,
                              uint16_t update_count, metadata_opt_t metadata_options);


static acc_service_configuration_t set_up_envelope(input_t *input);


static bool execute_envelope(acc_service_configuration_t envelope_configuration, FILE *file, bool wait_for_interrupt,
                             uint16_t update_count, metadata_opt_t metadata_options);


static acc_service_configuration_t set_up_iq(input_t *input);


static bool execute_iq(acc_service_configuration_t iq_configuration, FILE *file, bool wait_for_interrupt,
                       uint16_t update_count, metadata_opt_t metadata_options);


static acc_service_configuration_t set_up_sparse(input_t *input);


static bool execute_sparse(acc_service_configuration_t envelope_configuration, FILE *file, bool wait_for_interrupt,
                           uint16_t update_count, metadata_opt_t metadata_options, const char *sparse_data_format);


static void print_time(metadata_opt_t metadata_options, struct timespec *first_update_time);

static void print_data_warnings(bool missed_data, bool data_quality_warning, bool data_saturated);


static void print_sparse_data_item(FILE *file, const uint16_t *data, uint16_t sweep_length, uint16_t sweek_count,
                                   char item_selection);


static void interrupt_handler(int signum)
{
	if (signum == SIGINT)
	{
		interrupted = 1;
	}
}


int main(int argc, char *argv[])
{
	input_t input;

	initialize_input(&input);

	signal(SIGINT, interrupt_handler);

	if (!parse_options(argc, argv, &input))
	{
		if (input.file_path != NULL)
		{
			acc_integration_mem_free(input.file_path);
		}

		return EXIT_FAILURE;
	}

	acc_hal_t hal = *acc_hal_integration_get_implementation();

	hal.log.log_level = input.log_level;

	if (!acc_rss_activate(&hal))
	{
		return EXIT_FAILURE;
	}

	FILE *file = stdout;

	if (input.file_path != NULL)
	{
		file = fopen(input.file_path, "w");

		if (file == NULL)
		{
			acc_integration_mem_free(input.file_path);
			printf("opening output file failed\n");
			return EXIT_FAILURE;
		}
	}

	bool service_status = false;

	switch (input.service_type)
	{
		case POWER_BIN:
		{
			acc_service_configuration_t power_bin_configuration = set_up_power_bin(&input);

			if (power_bin_configuration != NULL)
			{
				service_status = execute_power_bin(power_bin_configuration, file, input.wait_for_interrupt,
				                                   input.update_count, input.metadata_options);

				if (!service_status)
				{
					printf("execute_power_bin() failed\n");
				}
			}

			acc_service_power_bins_configuration_destroy(&power_bin_configuration);
			break;
		}

		case ENVELOPE:
		{
			acc_service_configuration_t envelope_configuration = set_up_envelope(&input);

			if (envelope_configuration != NULL)
			{
				service_status = execute_envelope(envelope_configuration, file, input.wait_for_interrupt,
				                                  input.update_count, input.metadata_options);

				if (!service_status)
				{
					printf("execute_envelope() failed\n");
				}
			}

			acc_service_envelope_configuration_destroy(&envelope_configuration);
			break;
		}

		case IQ:
		{
			acc_service_configuration_t iq_configuration = set_up_iq(&input);

			if (iq_configuration != NULL)
			{
				service_status = execute_iq(iq_configuration, file, input.wait_for_interrupt,
				                            input.update_count, input.metadata_options);

				if (!service_status)
				{
					printf("execute_iq() failed\n");
				}
			}

			acc_service_iq_configuration_destroy(&iq_configuration);
			break;
		}

		case SPARSE:
		{
			acc_service_configuration_t sparse_configuration = set_up_sparse(&input);

			if (sparse_configuration != NULL)
			{
				service_status = execute_sparse(sparse_configuration, file, input.wait_for_interrupt,
				                                input.update_count, input.metadata_options, input.sparse_data_format);

				if (!service_status)
				{
					printf("execute_sparse() failed\n");
				}
			}

			acc_service_sparse_configuration_destroy(&sparse_configuration);
			break;
		}

		default:
		{
			printf("Invalid service_type %d\n", input.service_type);
		}
	}

	if (input.file_path != NULL)
	{
		acc_integration_mem_free(input.file_path);
		fclose(file);
	}

	acc_rss_deactivate();

	return service_status ? EXIT_SUCCESS : EXIT_FAILURE;
}


static void print_usage(void)
{
	printf("Usage: data_logger [OPTION]...\n\n");
	printf("-h, --help                this help\n");
	printf("-t, --service-type        service type to be run\n");
	printf("                            0. Power bin\n");
	printf("                            1. Envelope\n");
	printf("                            2. IQ\n");
	printf("                            3. Sparse\n");
	printf("-c, --sweep-count         number of updates, default application continues until interrupt\n");
	printf("-b, --range-start         start measurements at this distance [m], default %" PRIfloat "\n",
	       ACC_LOG_FLOAT_TO_INTEGER(DEFAULT_RANGE_START_M));
	printf("-e, --range-end           end measurements at this distance [m], default %" PRIfloat "\n",
	       ACC_LOG_FLOAT_TO_INTEGER(DEFAULT_RANGE_END_M));
	printf("-f, --frequency           update rate [Hz] or \"max\", default %" PRIfloat "\n",
	       ACC_LOG_FLOAT_TO_INTEGER(DEFAULT_FREQUENCY));
	printf("-p, --power-save-mode     power save mode selection for the sensor between updates,\n");
	printf("                            default %s\n", DEFAULT_POWER_SAVE_MODE_STRING);
	printf("                            ACTIVE    : no power-save, fully powered\n");
	printf("                            READY     : PLL driven clock-gating off\n");
	printf("                            SLEEP     : PLL oscillator off\n");
	printf("                            HIBERNATE : unclocked with memory retention\n");
	printf("                            OFF       : power off\n");
	printf("-g, --gain                gain (default service dependent)\n");
	printf("-d, --downsampling-factor factor for reduction of the hardware sample density (must be\n");
	printf("                            1, 2, or 4 for power_bins, envelope, and iq), default %d.\n", DEFAULT_DOWNSAMPLING_FACTOR);
	printf("-a, --hwaas               number of Hardware Accelerated Average Samples per data point\n");
	printf("                            (1 to 63), default %d.\n", DEFAULT_HW_ACCELERATED_AVERAGE_SAMPLES);
	printf("-n, --number-of-bins      number of bins (powerbins only), default %d.\n", DEFAULT_N_BINS);
	printf("-m, --sweeps-per-frame    number of sweeps per update (sparse only), default %d.\n", DEFAULT_SWEEPS_PER_FRAME);
	printf("-k, --sparse-data-format  format string for a frame (sparse only), default \"%s\"\n", DEFAULT_SPARSE_DATA_FORMAT);
	printf("                            a: average value over sweeps\n");
	printf("                            c: average absolute difference between consecutive sweeps\n");
	printf("                            d: average absolute deviation from average over sweeps\n");
	printf("                            f: full frame shown as consecutive sweeps\n");
	printf("-o, --out                 path to out file, default stdout\n");
	printf("-y, --service-profile     service profile to use (starting at index 1), default %u\n",
	       DEFAULT_SERVICE_PROFILE);
	printf("                            means no profile is set explicitly\n");
	printf("                            but default profile for the service is used\n");
	printf("-r, --running-avg-factor  strength of time domain filering (envelope only), default %" PRIfloat "\n",
	       ACC_LOG_FLOAT_TO_INTEGER(DEFAULT_RUNNING_AVG));
	printf("-i, --integer-iq          select integer output format for IQ service\n");
	printf("-s, --sensor              select sendor id, default %d\n", DEFAULT_SENSOR);
	printf("-U, --date-timestamp      add date (yyyy-mm-dd) and timestamp (hh:mm:ss.ss) to each data row.\n");
	printf("-u, --runtime             add the data collection runtime in seconds to each data row.\n");
	printf("-w, --data-warnings       add data warning status info to each output data row.\n");
	printf("                            The format is \"w:mqs\" with the letter for an active warning\n");
	printf("                            written out or replaced with \"-\" if the warning is inactive.\n");
	printf("                            The warning statuses are \"m\" for missed data, \"q\" for data\n");
	printf("                            quality warning, and \"s\" for data saturated. This output is \"w:---\"\n");
	printf("                            if there are no warnings.\n");
	printf("-v, --verbose             set debug level to verbose\n");
}


static bool string_to_power_save_mode(const char *str, acc_power_save_mode_t *power_save_mode)
{
	if (strcmp("ACTIVE", str) == 0)
	{
		*power_save_mode = ACC_POWER_SAVE_MODE_ACTIVE;
	}
	else if (strcmp("READY", str) == 0)
	{
		*power_save_mode = ACC_POWER_SAVE_MODE_READY;
	}
	else if (strcmp("SLEEP", str) == 0)
	{
		*power_save_mode = ACC_POWER_SAVE_MODE_SLEEP;
	}
	else if (strcmp("HIBERNATE", str) == 0)
	{
		*power_save_mode = ACC_POWER_SAVE_MODE_HIBERNATE;
	}
	else if (strcmp("OFF", str) == 0)
	{
		*power_save_mode = ACC_POWER_SAVE_MODE_OFF;
	}
	else
	{
		return false;
	}

	return true;
}


static bool parse_options(int argc, char *argv[], input_t *input)
{
	static struct option long_options[] =
	{
		{"service-type",        required_argument,  0, 't'},
		{"sweep-count",         required_argument,  0, 'c'},
		{"range-start",         required_argument,  0, 'b'},
		{"range-end",           required_argument,  0, 'e'},
		{"frequency",           required_argument,  0, 'f'},
		{"power-save-mode",     required_argument,  0, 'p'},
		{"gain",                required_argument,  0, 'g'},
		{"downsampling-factor", required_argument,  0, 'd'},
		{"hwaas",               required_argument,  0, 'a'},
		{"number-of-bins",      required_argument,  0, 'n'},
		{"sweeps-per-frame",    required_argument,  0, 'm'},
		{"sparse-data-format",  required_argument,  0, 'k'},
		{"out",                 required_argument,  0, 'o'},
		{"service-profile",     required_argument,  0, 'y'},
		{"running-avg-factor",  required_argument,  0, 'r'},
		{"integer-iq",          no_argument,        0, 'i'},
		{"sensor",              required_argument,  0, 's'},
		{"runtime",             no_argument,        0, 'u'},
		{"date-timestamp",      no_argument,        0, 'U'},
		{"data-warnings",       no_argument,        0, 'w'},
		{"verbose",             no_argument,        0, 'v'},
		{"help",                no_argument,        0, 'h'},
		{NULL,                  0,                  NULL, 0}
	};

	int16_t character_code;
	int32_t option_index = 0;

	while ((character_code = getopt_long(argc, argv, "t:c:b:e:f:p:g:d:a:n:m:k:o:r:is:uUwvh?:y:", long_options, &option_index)) != -1)
	{
		switch (character_code)
		{
			case 't':
			{
				switch (atoi(optarg))
				{
					case 0:
					{
						input->service_type = POWER_BIN;
						break;
					}
					case 1:
					{
						input->service_type = ENVELOPE;
						break;
					}
					case 2:
					{
						input->service_type = IQ;
						break;
					}
					case 3:
					{
						input->service_type = SPARSE;
						break;
					}
					default:
						printf("Invalid service type.\n");
						print_usage();
						return false;
				}

				break;
			}
			case 'c':
			{
				input->update_count       = atoi(optarg);
				input->wait_for_interrupt = false;
				break;
			}
			case 'b':
			{
				input->start_m = strtof(optarg, NULL);
				break;
			}
			case 'e':
			{
				input->end_m = strtof(optarg, NULL);
				break;
			}
			case 'f':
			{
				float f = strtof(optarg, NULL);
				if (f > 0 && f < 100000)
				{
					input->frequency = f;
				}
				else if (strcmp(optarg, "max") == 0)
				{
					input->frequency = INFINITY;
				}
				else
				{
					printf("Frequency out of range.\n");
					print_usage();
					exit(EXIT_FAILURE);
				}

				break;
			}
			case 'p':
			{
				if (!string_to_power_save_mode(optarg, &input->power_save_mode))
				{
					printf("Invalid power save mode: %s\n", optarg);
					print_usage();
					exit(EXIT_FAILURE);
				}

				break;
			}
			case 'g':
			{
				float g = strtof(optarg, NULL);
				if (g >= 0 && g <= 1)
				{
					input->gain = g;
				}
				else
				{
					printf("Gain out of range.\n");
					print_usage();
					exit(EXIT_FAILURE);
				}

				break;
			}
			case 'd':
			{
				int d = atoi(optarg);
				if (d > 0 && d < 200)
				{
					input->downsampling_factor = d;
				}
				else
				{
					printf("Downsampling factor out of range.\n");
					print_usage();
					exit(EXIT_FAILURE);
				}

				break;
			}
			case 'a':
			{
				int a = atoi(optarg);
				if (a > 0 && a < 64)
				{
					input->hwaas = a;
				}
				else
				{
					printf("Hardware accelerated average samples out of range.\n");
					print_usage();
					exit(EXIT_FAILURE);
				}

				break;
			}
			case 'n':
			{
				int n = atoi(optarg);
				if (n > 0 && n <= 32)
				{
					input->n_bins = n;
				}
				else
				{
					printf("Number of bins out of range.\n");
					print_usage();
					exit(EXIT_FAILURE);
				}

				break;
			}
			case 'm':
			{
				int n = atoi(optarg);
				if (n > 0 && n < 2048)
				{
					input->sweeps_per_frame = n;
				}
				else
				{
					printf("Number of sweeps per frame out of range.\n");
					print_usage();
					exit(EXIT_FAILURE);
				}

				break;
			}
			case 'k':
			{
				int i;
				for (i = 0; optarg[i] != '\0'; i++)
				{
					char c = optarg[i];
					if (c != 'a' && c != 'c' && c != 'd' && c != 'f')
					{
						printf("Bad character \"%c\" in sparse format string.\n", c);
						print_usage();
						exit(EXIT_FAILURE);
					}

					if (i >= SPARSE_DATA_FORMAT_BUFSIZE - 1)
					{
						printf("Too long sparse format string.\n");
						print_usage();
						exit(EXIT_FAILURE);
					}

					input->sparse_data_format[i] = c;
				}

				input->sparse_data_format[i] = '\0';

				break;
			}
			case 'y':
			{
				input->service_profile = atoi(optarg);
				break;
			}
			case 'o':
			{
				input->file_path = acc_integration_mem_alloc(sizeof(char) * (strlen(optarg) + 1));
				if (input->file_path == NULL)
				{
					printf("Failed allocating memory\n");
					return false;
				}

				snprintf(input->file_path, strlen(optarg) + 1, "%s", optarg);
				break;
			}
			case 'r':
			{
				float r = strtof(optarg, NULL);
				if (r >= 0 && r <= 1)
				{
					input->running_avg = r;
				}
				else
				{
					printf("Running average factor out of range.\n");
					print_usage();
					exit(EXIT_FAILURE);
				}

				break;
			}
			case 'i':
			{
				input->integer_iq = true;
				break;
			}
			case 's':
			{
				int s = atoi(optarg);
				if (s > 0 && s <= 4)
				{
					input->sensor = s;
				}
				else
				{
					printf("Sensor id out of range.\n");
					print_usage();
					exit(EXIT_FAILURE);
				}

				break;
			}
			case 'u':
			{
				input->metadata_options.runtime = true;
				break;
			}
			case 'U':
			{
				input->metadata_options.date_timestamp = true;
				break;
			}
			case 'w':
			{
				input->metadata_options.data_warnings = true;
				break;
			}
			case 'v':
			{
				input->log_level = ACC_LOG_LEVEL_VERBOSE;
				break;
			}
			case 'h':
			case '?':
			{
				print_usage();
				return false;
			}
		}
	}

	if (input->service_type == INVALID_SERVICE)
	{
		printf("Missing option service type.\n");
		print_usage();
		return false;
	}

	return true;
}


static void set_up_common(acc_service_configuration_t service_configuration, input_t *input)
{
	/*
	 * Numbering of service profiles starts at 1. Setting 0 means don't set profile explicitly
	 * but instead use the default for the service
	 */
	if (input->service_profile > 0)
	{
		acc_service_profile_set(service_configuration, input->service_profile);
	}

	if (input->frequency < INFINITY)
	{
		acc_service_repetition_mode_streaming_set(service_configuration, input->frequency);
	}
	else
	{
		acc_service_repetition_mode_on_demand_set(service_configuration);
	}

	float length_m = input->end_m - input->start_m;

	acc_service_requested_start_set(service_configuration, input->start_m);
	acc_service_requested_length_set(service_configuration, length_m);
	acc_service_power_save_mode_set(service_configuration, input->power_save_mode);
	acc_service_hw_accelerated_average_samples_set(service_configuration, input->hwaas);
	acc_service_sensor_set(service_configuration, input->sensor);

	if (input->gain >= 0)
	{
		acc_service_receiver_gain_set(service_configuration, input->gain);
	}
}


static acc_service_configuration_t set_up_power_bin(input_t *input)
{
	acc_service_configuration_t power_bin_configuration = acc_service_power_bins_configuration_create();

	if (power_bin_configuration == NULL)
	{
		printf("acc_service_power_bin_configuration_create() failed\n");
		return NULL;
	}

	set_up_common(power_bin_configuration, input);

	acc_service_power_bins_requested_bin_count_set(power_bin_configuration, input->n_bins);
	acc_service_power_bins_downsampling_factor_set(power_bin_configuration, input->downsampling_factor);

	return power_bin_configuration;
}


static bool execute_power_bin(acc_service_configuration_t power_bin_configuration, FILE *file, bool wait_for_interrupt,
                              uint16_t update_count, metadata_opt_t metadata_options)
{
	acc_service_handle_t handle = acc_service_create(power_bin_configuration);

	if (handle == NULL)
	{
		printf("acc_service_create failed\n");
		return false;
	}

	acc_service_power_bins_metadata_t power_bins_metadata = { 0 };
	acc_service_power_bins_get_metadata(handle, &power_bins_metadata);

	uint16_t *power_bins_data;

	acc_service_power_bins_result_info_t result_info;
	bool                                 service_status = acc_service_activate(handle);

	if (service_status)
	{
		uint16_t        updates           = 0;
		struct timespec first_update_time = { 0 };

		while ((wait_for_interrupt && interrupted == 0) || updates < update_count)
		{
			service_status = acc_service_power_bins_get_next_by_reference(handle, &power_bins_data, &result_info);

			if (service_status && !result_info.sensor_communication_error)
			{
				print_time(metadata_options, &first_update_time);

				if (metadata_options.data_warnings)
				{
					print_data_warnings(result_info.missed_data, result_info.data_quality_warning,
					                    result_info.data_saturated);
				}

				for (uint16_t index = 0; index < power_bins_metadata.bin_count; index++)
				{
					fprintf(file, "%u\t", (unsigned int)power_bins_data[index]);
				}

				fprintf(file, "\n");

				if (file == stdout)
				{
					fflush(stdout);
				}
			}
			else
			{
				printf("Power bin data not properly retrieved\n");
				fflush(stdout);
				return false;
			}

			if (!wait_for_interrupt)
			{
				updates++;
			}
		}

		service_status = acc_service_deactivate(handle);
	}
	else
	{
		printf("acc_service_activate() failed\n");
	}

	acc_service_destroy(&handle);

	return service_status;
}


static acc_service_configuration_t set_up_envelope(input_t *input)
{
	acc_service_configuration_t envelope_configuration = acc_service_envelope_configuration_create();

	if (envelope_configuration == NULL)
	{
		printf("acc_service_envelope_configuration_create() failed\n");
		return NULL;
	}

	set_up_common(envelope_configuration, input);

	acc_service_envelope_running_average_factor_set(envelope_configuration, input->running_avg);
	acc_service_envelope_downsampling_factor_set(envelope_configuration, input->downsampling_factor);

	return envelope_configuration;
}


static bool execute_envelope(acc_service_configuration_t envelope_configuration, FILE *file, bool wait_for_interrupt,
                             uint16_t update_count, metadata_opt_t metadata_options)
{
	acc_service_handle_t handle = acc_service_create(envelope_configuration);

	if (handle == NULL)
	{
		printf("acc_service_create failed\n");
		return false;
	}

	acc_service_envelope_metadata_t envelope_metadata = { 0 };
	acc_service_envelope_get_metadata(handle, &envelope_metadata);

	uint16_t *envelope_data;

	acc_service_envelope_result_info_t result_info;
	bool                               service_status = acc_service_activate(handle);

	if (service_status)
	{
		uint16_t        updates           = 0;
		struct timespec first_update_time = { 0 };

		while ((wait_for_interrupt && interrupted == 0) || updates < update_count)
		{
			service_status = acc_service_envelope_get_next_by_reference(handle, &envelope_data, &result_info);

			if (service_status && !result_info.sensor_communication_error)
			{
				print_time(metadata_options, &first_update_time);

				if (metadata_options.data_warnings)
				{
					print_data_warnings(result_info.missed_data, result_info.data_quality_warning,
					                    result_info.data_saturated);
				}

				for (uint16_t index = 0; index < envelope_metadata.data_length; index++)
				{
					fprintf(file, "%u\t", (unsigned int)envelope_data[index]);
				}

				fprintf(file, "\n");

				if (file == stdout)
				{
					fflush(stdout);
				}
			}
			else
			{
				printf("Envelope data not properly retrieved\n");
				fflush(stdout);
				return false;
			}

			if (!wait_for_interrupt)
			{
				updates++;
			}
		}

		service_status = acc_service_deactivate(handle);
	}
	else
	{
		printf("acc_service_activate() failed\n");
	}

	acc_service_destroy(&handle);

	return service_status;
}


static acc_service_configuration_t set_up_iq(input_t *input)
{
	acc_service_configuration_t iq_configuration = acc_service_iq_configuration_create();

	if (iq_configuration == NULL)
	{
		printf("acc_service_iq_configuration_create() failed\n");
		return NULL;
	}

	set_up_common(iq_configuration, input);

	if (input->integer_iq)
	{
		acc_service_iq_output_format_set(iq_configuration, ACC_SERVICE_IQ_OUTPUT_FORMAT_INT16_COMPLEX);
	}
	else
	{
		acc_service_iq_output_format_set(iq_configuration, ACC_SERVICE_IQ_OUTPUT_FORMAT_FLOAT_COMPLEX);
	}

	acc_service_iq_downsampling_factor_set(iq_configuration, input->downsampling_factor);

	return iq_configuration;
}


static bool execute_iq(acc_service_configuration_t iq_configuration, FILE *file, bool wait_for_interrupt,
                       uint16_t update_count, metadata_opt_t metadata_options)
{
	acc_service_handle_t handle = acc_service_create(iq_configuration);

	if (handle == NULL)
	{
		printf("acc_service_create failed\n");
		return false;
	}

	acc_service_iq_metadata_t iq_metadata = { 0 };
	acc_service_iq_get_metadata(handle, &iq_metadata);

	float complex       *iq_data_float = NULL;
	acc_int16_complex_t *iq_data_i16   = NULL;

	if (acc_service_iq_output_format_get(iq_configuration) == ACC_SERVICE_IQ_OUTPUT_FORMAT_FLOAT_COMPLEX)
	{
		iq_data_float = acc_integration_mem_alloc(sizeof(float complex) * iq_metadata.data_length);
		if (iq_data_float == NULL)
		{
			acc_service_destroy(&handle);
			printf("Failed allocating memory\n");
			return false;
		}
	}

	acc_service_iq_result_info_t result_info;

	bool service_status = acc_service_activate(handle);

	if (service_status)
	{
		uint16_t        updates           = 0;
		struct timespec first_update_time = { 0 };

		while ((wait_for_interrupt && interrupted == 0) || updates < update_count)
		{
			if (iq_data_float != NULL)
			{
				service_status = acc_service_iq_get_next(handle, iq_data_float, iq_metadata.data_length, &result_info);
			}
			else
			{
				service_status = acc_service_iq_get_next_by_reference(handle, &iq_data_i16, &result_info);
			}

			if (service_status && !result_info.sensor_communication_error)
			{
				print_time(metadata_options, &first_update_time);

				if (metadata_options.data_warnings)
				{
					print_data_warnings(result_info.missed_data, result_info.data_quality_warning,
					                    result_info.data_saturated);
				}

				for (uint16_t index = 0; index < iq_metadata.data_length; index++)
				{
					if (iq_data_float != NULL)
					{
						fprintf(file, "%" PRIfloat "\t%" PRIfloat "\t",
						        ACC_LOG_FLOAT_TO_INTEGER(crealf(iq_data_float[index])),
						        ACC_LOG_FLOAT_TO_INTEGER(cimagf(iq_data_float[index])));
					}
					else
					{
						fprintf(file, "%d\t%d\t", (int)iq_data_i16[index].real, (int)iq_data_i16[index].imag);
					}
				}

				fprintf(file, "\n");

				if (file == stdout)
				{
					fflush(stdout);
				}
			}
			else
			{
				printf("IQ data not properly retrieved\n");
				fflush(stdout);
				return false;
			}

			if (!wait_for_interrupt)
			{
				updates++;
			}
		}

		service_status = acc_service_deactivate(handle);
	}
	else
	{
		printf("acc_service_activate() failed\n");
	}

	acc_integration_mem_free(iq_data_float);
	acc_service_destroy(&handle);

	return service_status;
}


static acc_service_configuration_t set_up_sparse(input_t *input)
{
	acc_service_configuration_t sparse_configuration = acc_service_sparse_configuration_create();

	if (sparse_configuration == NULL)
	{
		printf("acc_service_sparse_configuration_create() failed\n");
		return NULL;
	}

	set_up_common(sparse_configuration, input);

	acc_service_sparse_configuration_sweeps_per_frame_set(sparse_configuration, input->sweeps_per_frame);
	acc_service_sparse_downsampling_factor_set(sparse_configuration, input->downsampling_factor);

	return sparse_configuration;
}


static bool execute_sparse(acc_service_configuration_t sparse_configuration, FILE *file, bool wait_for_interrupt,
                           uint16_t update_count, metadata_opt_t metadata_options, const char *sparse_data_format)
{
	acc_service_handle_t handle = acc_service_create(sparse_configuration);

	if (handle == NULL)
	{
		printf("acc_service_create failed\n");
		return false;
	}

	acc_service_sparse_metadata_t sparse_metadata = { 0 };
	acc_service_sparse_get_metadata(handle, &sparse_metadata);

	uint16_t sweep_count  = acc_service_sparse_configuration_sweeps_per_frame_get(sparse_configuration);
	uint16_t sweep_length = sparse_metadata.data_length / sweep_count;

	uint16_t *sparse_data;

	acc_service_sparse_result_info_t result_info;
	bool                             service_status = acc_service_activate(handle);

	if (service_status)
	{
		uint16_t        updates           = 0;
		struct timespec first_update_time = { 0 };

		while ((wait_for_interrupt && interrupted == 0) || updates < update_count)
		{
			service_status = acc_service_sparse_get_next_by_reference(handle, &sparse_data, &result_info);

			if (service_status && !result_info.sensor_communication_error)
			{
				print_time(metadata_options, &first_update_time);

				if (metadata_options.data_warnings)
				{
					print_data_warnings(result_info.missed_data, false,
					                    result_info.data_saturated);
				}

				for (uint16_t i = 0; sparse_data_format[i] != '\0'; i++)
				{
					print_sparse_data_item(file, sparse_data, sweep_length, sweep_count, sparse_data_format[i]);
				}

				fprintf(file, "\n");

				if (file == stdout)
				{
					fflush(stdout);
				}
			}
			else
			{
				printf("Sparse data not properly retrieved\n");
				fflush(stdout);
				return false;
			}

			if (!wait_for_interrupt)
			{
				updates++;
			}
		}

		service_status = acc_service_deactivate(handle);
	}
	else
	{
		printf("acc_service_activate() failed\n");
	}

	acc_service_destroy(&handle);

	return service_status;
}


static void print_time(metadata_opt_t metadata_options, struct timespec *first_update_time)
{
	struct timespec ts;

	clock_gettime(CLOCK_REALTIME, &ts);

	if (first_update_time->tv_sec == 0)
	{
		*first_update_time = ts;
	}

	if (metadata_options.date_timestamp)
	{
		char buf[24];
		strftime(buf, sizeof(buf), "%Y-%m-%d\t%H:%M:%S", localtime(&ts.tv_sec));
		printf("%s.%02u\t", buf, (unsigned int)(ts.tv_nsec / 10000000));
	}

	if (metadata_options.runtime)
	{
		int secs     = ts.tv_sec - first_update_time->tv_sec;
		int hundreds = 0;

		if (ts.tv_nsec < first_update_time->tv_nsec)
		{
			secs--;
			hundreds = (1000000000 + ts.tv_nsec - first_update_time->tv_nsec) / 10000000;
		}
		else
		{
			hundreds = (ts.tv_nsec - first_update_time->tv_nsec) / 10000000;
		}

		printf("%d.%02d\t", secs, hundreds);
	}
}


static void print_data_warnings(bool missed_data, bool data_quality_warning, bool data_saturated)
{
	char flags[] = {'-', '-', '-', '\0'};

	if (missed_data)
	{
		flags[0] = 'm';
	}

	if (data_quality_warning)
	{
		flags[1] = 'q';
	}

	if (data_saturated)
	{
		flags[2] = 's';
	}

	printf("w:%s\t", flags);
}


static void print_sparse_data_item(FILE *file, const uint16_t *data, uint16_t sweep_length, uint16_t sweep_count,
                                   char item_selection)
{
	uint16_t data_length = sweep_length * sweep_count;

	switch (item_selection)
	{
		case 'a':
		{
			// Average over sweeps
			for (uint16_t index = 0; index < sweep_length; index++)
			{
				int32_t sum = 0;
				for (uint16_t k = 0; k < sweep_count; k++)
				{
					sum += data[k * sweep_length + index];
				}

				fprintf(file, "%u\t", (unsigned int)(sum / sweep_count));
			}

			break;
		}
		case 'c':
		{
			// Average absolute difference between consecutive sweeps
			for (uint16_t index = 0; index < sweep_length; index++)
			{
				int32_t sum = 0;
				for (uint16_t k = 1; k < sweep_count; k++)
				{
					uint16_t i = k * sweep_length + index;
					sum += abs(data[i] - data[i - sweep_length]);
				}

				unsigned int result = sweep_count > 1 ? sum / (sweep_count - 1) : 0;
				fprintf(file, "%u\t", result);
			}

			break;
		}
		case 'd':
		{
			// Average absolute deviation from the average overs sweeps
			for (uint16_t index = 0; index < sweep_length; index++)
			{
				int32_t sum = 0;
				for (uint16_t k = 0; k < sweep_count; k++)
				{
					sum += data[k * sweep_length + index];
				}

				int32_t average = sum / sweep_count;
				sum = 0;
				for (uint16_t k = 0; k < sweep_count; k++)
				{
					sum += abs(data[k * sweep_length + index] - average);
				}

				fprintf(file, "%u\t", (unsigned int)(sum / sweep_count));
			}

			break;
		}
		case 'f':
		{
			// Full data frame
			for (uint16_t index = 0; index < data_length; index++)
			{
				fprintf(file, "%u\t", (unsigned int)data[index]);
			}

			break;
		}
	}
}
