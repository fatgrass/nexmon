#define _XOPEN_SOURCE 700

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <argp.h>
#include <string.h>
#include "darm/darm.h"

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <unistd.h>

// C preprocessor stringizing:
// https://gcc.gnu.org/onlinedocs/cpp/Stringizing.html
#define AS_STR2(TEXT) #TEXT
#define AS_STR(TEXT) AS_STR2(TEXT)

char *patch_array = NULL;
long patch_len = 0;

char *patch_file_name = AS_STR(RAM_FILE_NAME);
char *outdir_name = 0;
static const char *slots_file_name = "used_slots.txt";
char slots[255];
int last_slot = 0;

const char *argp_program_version = "bthcd_rompatch_extractor";
const char *argp_program_bug_address = "https://github.com/seemoo-lab/nexmon";

static char doc[] = "bthcd_rompatch_extractor -- Tool to extract rompatches from a given write_ram bin file.";

static struct argp_option options[] = {
	{"patchblob", 'p', "FILE", 0, "Read write_ram patch file from FILE"},
	{"outdir", 'o', "FILE", 0, "Output directory for the extracted patches"},
	{ 0 }
};

static error_t
parse_opt(int key, char *arg, struct argp_state *state)
{
	switch (key) {

		case 'p':
			patch_file_name = arg;
			break;

		case 'o':
			outdir_name = arg;
			break;
		
		default:
			return ARGP_ERR_UNKNOWN;
	}

	return 0;
}

static struct argp argp = { options, parse_opt, 0, doc };

int
read_file_to_array(char *filename, char **buffer, long *filelen)
{
	FILE *fileptr;

	if((fileptr = fopen(filename, "rb"))) {
		fseek(fileptr, 0, SEEK_END);
		*filelen = ftell(fileptr);
		rewind(fileptr);

		*buffer = (char *) malloc(*filelen + 1);
		fread(*buffer, *filelen, 1, fileptr);
		fclose(fileptr);

		return *filelen;
	}

	return 0;
}

void
analyze_patch_file(void)
{
	int i = 0;
	int counter = 0;
	bool addrcnt_new = true;
	FILE *patch_out_file = NULL;
	char patch_out_file_name[256];

	while (i < patch_len) {
		uint8_t type = *(uint8_t *) (void *) &patch_array[i];
		uint16_t len = *(uint16_t *) (void *) &patch_array[i+1];

		// 0xFE marks the end of the TLV-list
		if ( type == 0xFE )
			counter = 999;
		uint8_t slot = -1;
		if(type == 0x08) {
			slot = *(uint8_t *) (void *) &patch_array[i+3];
			slots[last_slot++] = slot;	
		}
		snprintf(patch_out_file_name, sizeof(patch_out_file_name), "%s/rompatch_nr%04d_0x%02X.bin", outdir_name, counter*10, type);
		patch_out_file = fopen(patch_out_file_name,"wb");

		if (patch_out_file) {
			fwrite(&patch_array[i], len + sizeof(len) + sizeof(type), 1, patch_out_file);
			fclose(patch_out_file);
		} else {
			fprintf(stderr, "Could not open file: %s\n\t", patch_out_file_name);
			perror(NULL);
		}

		printf("type %04x len %d slot %d\n", type, len, slot);
		i += sizeof(type) + sizeof(len) + len;
		counter++;
	}
}

void 
write_used_slots_file(void)
{
	int path_length = strlen(outdir_name) + strlen(slots_file_name) + 1 + 1;
	char* path = malloc(path_length);
	FILE *slot_out_file = NULL;
	snprintf(path, path_length, "%s/%s", outdir_name, slots_file_name);
	printf("Writing used rompatch slots to %s", path);
	slot_out_file = fopen(path, "w");
	for(int i = 0; i < last_slot; i++) {
		fprintf(slot_out_file, "%d\n", slots[i]); 
	}
	fclose(slot_out_file);
}

int
main(int argc, char **argv)
{
	argp_parse(&argp, argc, argv, 0, 0, 0);

	if(!read_file_to_array(patch_file_name, &patch_array, &patch_len)) {
		fprintf(stderr, "ERR: patch file empty or unavailable.\n");
		exit(EXIT_FAILURE);
	}

	if(outdir_name == NULL) {
		outdir_name = ".";
	} else {
		if(strlen(outdir_name) > 200) {
			outdir_name[200] = '\0';
		}

		struct stat st = {0};
		if (stat(outdir_name, &st) == -1) {
			mkdir(outdir_name, 0700);
		}
	}

	analyze_patch_file();
	write_used_slots_file();

	if (patch_array) free(patch_array);

	exit(EXIT_SUCCESS);
}