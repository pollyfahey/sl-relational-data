#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

struct slrdata_t
{
	int fd;
	void *ptr;
	uint_fast64_t size;
};

typedef struct slrdata_t slrdata_t;

int slrdata_open(slrdata_t *g, const char *restrict filename, bool readonly);

void slrdata_close(slrdata_t *g);
