#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

struct slrdata_t
{
	int fd;
	unsigned char *ptr;
	uint_fast64_t size;
	bool readonly;
	const char *restrict foldername;
	const char *restrict filename;
};

typedef struct slrdata_t slrdata_t;

int slrdata_open(slrdata_t *d, const char *restrict foldername, const char *restrict filename, bool readonly, bool is_relation);

void slrdata_close(slrdata_t *d);

uint_fast64_t slrdata_arity(slrdata_t *relation);

uint_fast64_t slrdata_degree(slrdata_t *relation, uint_fast64_t e);

uint_fast64_t * slrdata_tuple(slrdata_t *relation, uint_fast64_t e, uint_fast64_t i);

uint_fast64_t slrdata_element_count(slrdata_t *d);

uint_fast64_t slrdata_relation_count(slrdata_t *d);

uint_fast64_t slrdata_tuple_count(slrdata_t *relation);

uint_fast64_t * slrdata_ith_tuple(slrdata_t *relation, uint_fast64_t i);

int slrdata_create_directory(const char *restrict foldername);

int slrdata_create_element_file(slrdata_t *d, const char *restrict foldername);

int slrdata_create_relation_file(slrdata_t *d, const char *restrict foldername, const char *restrict relationname);

int slrdata_add_element(slrdata_t *d, const char *restrict label);

int slrdata_add_tuple(slrdata_t *d, uint_fast64_t * tuple, uint_fast64_t arity);

int slrdata_add_incidence_lists(slrdata_t *relation, slrdata_t *elements, uint_fast64_t norm_degree, uint_fast64_t max_degree);

int slrdata_reduce_size(slrdata_t *rel);

