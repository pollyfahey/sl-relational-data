#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct slrdata_element_t slrdata_element_t;

struct slrdata_element_t {
	uint_fast64_t id;
	unsigned char *label;
	slrdata_element_t *element_next;
};

typedef struct slrdata_tuple_t slrdata_tuple_t;

struct slrdata_tuple_t {
	// pointer to an array of element ids
	uint_fast64_t *elements;
	slrdata_tuple_t *tuple_next;
};

typedef struct slrdata_relation_t slrdata_relation_t;

struct slrdata_relation_t {
	uint_fast64_t arity;
	uint_fast64_t tuple_count;
	const char *restrict label;
	slrdata_tuple_t *tuples;
	slrdata_relation_t *relation_next;
};

struct slrdata_create_t
{
	uint_fast64_t degree;
	uint_fast64_t element_count;
	uint_fast64_t relation_count;
	slrdata_element_t *elements;
	slrdata_relation_t *relations;
};

struct slrdata_t
{
	int fd;
	void *ptr;
	uint_fast64_t size;
	bool readonly;
};

typedef struct slrdata_t slrdata_t;
typedef struct slrdata_create_t slrdata_create_t;

int slrdata_open(slrdata_t *d, const char *restrict filepath, bool readonly);

void slrdata_close(slrdata_t *d);

int slrdata_create(slrdata_create_t *c, const char *restrict foldername);


