#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

struct slrdata_t
{
	int fd;
	void *ptr;
	uint_fast64_t size;
	bool readonly;
};

typedef struct slrdata_t slrdata_t;

struct slrdata_element_t
{
	unsigned char *label[100];
};


typedef struct slrdata_element_t slrdata_element_t;
typedef struct slrdata_elementlist_t slrdata_elementlist_t;

struct slrdata_elementlist_t {
	slrdata_element_t element_entry;
	slrdata_elementlist_t *element_next;
};

struct slrdata_tuple_t
{
	slrdata_elementlist_t *elements;
};

typedef struct slrdata_tuple_t slrdata_tuple_t;

struct slrdata_relation_t
{

	uint_fast64_t arity;
	unsigned char *label[100];
	slrdata_tuple_t *tuples[];
};

typedef struct slrdata_relation_t slrdata_relation_t;

struct slrdata_create_t
{
	uint_fast64_t degree;
	uint_fast64_t element_count;
	uint_fast64_t relation_count;
	slrdata_elementlist_t *elements;
	slrdata_relation_t *relations[100];
};

typedef struct slrdata_create_t slrdata_create_t;

int slrdata_open_or_create(slrdata_t *d, const char *restrict filename, bool readonly);

void slrdata_close(slrdata_t *d);

int slrdata_create(slrdata_t *d, slrdata_create_t *c, const char *restrict filename);

uint_fast64_t slrdata_number_of_elements(slrdata_t *d);

uint_fast64_t slrdata_number_of_relations(slrdata_t *d);

uint_fast64_t slrdata_element_label(slrdata_t *d, uint_fast64_t n);

uint_fast64_t slrdata_relation_label(slrdata_t *d, uint_fast64_t n);

uint_fast64_t slrdata_relation_arity(slrdata_t *d, uint_fast64_t n);


