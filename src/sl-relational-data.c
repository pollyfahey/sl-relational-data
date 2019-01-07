#include "../include/sl-relational-data.h"
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <stdlib.h>

#define SLRDATA_HEADERSIZE_BASIC (3 * 8)
#define SLRDATA_MAIN_HEADERSIZE (SLRDATA_HEADERSIZE_BASIC + 2 * 8)
#define SLRDATA_MAIN_LISTHEADERSIZE 12
#define SLRDATA_MAIN_ELEMENTSIZE 6
#define SLRDATA_MAIN_RELATIONSIZE 6

#define SLRDATA_RELATION_HEADERSIZE_BASIC (4 * 8)
#define SLRDATA_RELATION_HEADERSIZE (SLRDATA_RELATION_HEADERSIZE_BASIC + (2 * 8) + 6)
#define SLRDATA_RELATION_LISTHEADERSIZE 12
#define SLRDATA_RELATION_ELEMENTSIZE 8
// size of tuple is SLRDATA_RELATION_TUPLEELEMENTSIZE * arity
#define SLRDATA_RELATION_TUPLEELEMENTSIZE 6
#define SLRDATA_RELATION_INCIDENCESIZE 6

static void slrdata_write( uint_fast8_t bytes, void *ptr, uint_fast64_t v)
{
	unsigned char *cptr = ptr;
	for(uint_fast8_t i = 0; i < bytes; i++)
		cptr[i] = (v >> i * 8) & 0xff;
}

static uint_fast64_t slrdata_read(uint_fast8_t bytes, const void *ptr)
{
	const unsigned char *cptr = ptr;
	uint_fast32_t ret = 0;

	for(uint_fast8_t i = 0; i < bytes; i++)
		ret |= ((uint_fast64_t)(cptr[i]) << i * 8);

	return(ret);
}

static bool slrdata_is_relation_file(slrdata_t *d)
{
	if(strncmp(d->ptr + 7, "relation", 8))
	{
		return false;
	}

	return true;
}

// Write a new basic header
static void slrdata_headerinit(unsigned char *header, uint_fast64_t filesize, bool is_relation)
{
	if (is_relation)
	{
		memcpy(header, u8"slrdatarelation", 16);
	}
	else
	{
		memcpy(header, u8"slrdata", 8);
	}

	// Version
	slrdata_write(8, header + (is_relation ? 16 : 8), 1);

	// Size
	slrdata_write(8, header + (is_relation ? 16 : 8) + 8, filesize);
}

static int slrdata_create_and_write_file(char* filepath, unsigned char* data, uint_fast64_t size)
{
	struct slrdata_t d;

	// create file - TODO change this to just create
	if((d.fd = open(filepath, (O_RDWR | O_CREAT), S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH)) == -1)
			return(-1);

	if(write(d.fd, data, size) == -1)
	{
		close(d.fd);
		return(-2);
	}

	close(d.fd);
	return(0);
}

static char* slrdata_filepath(const char *restrict foldername, const char *restrict filename)
{
	char *filepath = malloc(strlen(foldername) + strlen(filename) + 2 + 5);
	filepath = strcpy(filepath, foldername);
	filepath = strcat(filepath, "/");
	filepath = strcat(filepath, filename);
	filepath = strcat(filepath, ".sld");
	return filepath;
}

static int slrdata_create_main_file(slrdata_create_t *c, const char *restrict foldername)
{
	uint_fast64_t element_list_size = SLRDATA_MAIN_LISTHEADERSIZE + (c->element_count * SLRDATA_MAIN_ELEMENTSIZE);
	uint_fast64_t relation_list_size = SLRDATA_MAIN_LISTHEADERSIZE + (c->relation_count * SLRDATA_MAIN_RELATIONSIZE);

	uint_fast64_t elements_offset = SLRDATA_MAIN_HEADERSIZE;
	uint_fast64_t releations_offset = elements_offset + element_list_size;

	uint_fast64_t size = SLRDATA_MAIN_HEADERSIZE + element_list_size + relation_list_size;
	unsigned char data[size];

	slrdata_headerinit(data, size, false);

	// elements list and relations list offsets
	slrdata_write(8, data + SLRDATA_HEADERSIZE_BASIC, elements_offset);
	slrdata_write(8, data + SLRDATA_HEADERSIZE_BASIC + 8, releations_offset);

	// write element list header
	slrdata_write(6, data + elements_offset, SLRDATA_MAIN_ELEMENTSIZE * c->element_count);
	slrdata_write(6, data + elements_offset + 6, c->element_count);

	// write elements TODO write actual labels
	for(uint_fast64_t i = 0; i < c->element_count; i++)
	{
		slrdata_write(6, data + elements_offset + SLRDATA_MAIN_LISTHEADERSIZE + (i * SLRDATA_MAIN_ELEMENTSIZE), i);
	}

	// write relations list header
	slrdata_write(6, data + releations_offset, SLRDATA_MAIN_RELATIONSIZE * c->relation_count);
	slrdata_write(6, data + releations_offset + 6, c->relation_count);

	// write relations TODO write actual labels
	for(uint_fast64_t i = 0; i < c->relation_count; i++)
	{
		slrdata_write(6, data + releations_offset + SLRDATA_MAIN_LISTHEADERSIZE + (i * SLRDATA_MAIN_RELATIONSIZE), i);
	}

	if(slrdata_create_and_write_file(slrdata_filepath(foldername, "main"), data, size))
	{
		return(-1);
	}

	return(0);
}

static int slrdata_create_relation_file(slrdata_create_t *c, slrdata_relation_t *r, const char *restrict foldername)
{
	uint_fast64_t tuple_list_size = SLRDATA_RELATION_LISTHEADERSIZE + (r->tuple_count * r->arity * SLRDATA_RELATION_TUPLEELEMENTSIZE);
	uint_fast64_t element_list_size = SLRDATA_RELATION_LISTHEADERSIZE + (c->element_count * SLRDATA_RELATION_ELEMENTSIZE);
	uint_fast64_t incidence_list_size = SLRDATA_MAIN_LISTHEADERSIZE + (c->degree * SLRDATA_RELATION_INCIDENCESIZE);

	uint_fast64_t tuple_list_offset = SLRDATA_RELATION_HEADERSIZE;
	uint_fast64_t element_list_offset = tuple_list_offset + tuple_list_size;

	uint_fast64_t size = SLRDATA_RELATION_HEADERSIZE + tuple_list_size + element_list_size + (c->element_count * incidence_list_size);
	unsigned char data[size];

	// basic header
	slrdata_headerinit(data, size, true);

	// additional header
	slrdata_write(8, data + SLRDATA_RELATION_HEADERSIZE_BASIC, tuple_list_offset);
	slrdata_write(8, data + SLRDATA_RELATION_HEADERSIZE_BASIC + 8, element_list_offset);
	slrdata_write(6, data + SLRDATA_RELATION_HEADERSIZE_BASIC + 16, r->arity);

	// add tuple list header
	slrdata_write(6, data + tuple_list_offset, r->tuple_count * r->arity * SLRDATA_RELATION_TUPLEELEMENTSIZE);
	slrdata_write(6, data + tuple_list_offset + 6, r->tuple_count);

	// add tuple list
	slrdata_tuple_t *current_tuple = r->tuples;
	for(uint_fast64_t i = 0; i < r->tuple_count; i++)
	{
		for(uint_fast64_t j = 0; j < r->arity; j++)
		{
			uint_fast64_t offset = tuple_list_offset
					+ SLRDATA_RELATION_LISTHEADERSIZE
					+ (i * r->arity * SLRDATA_RELATION_TUPLEELEMENTSIZE)
					+ (j * SLRDATA_RELATION_TUPLEELEMENTSIZE);
			slrdata_write(6, data + offset, (uint_fast64_t)current_tuple->elements[j]);
		}

		current_tuple = current_tuple->tuple_next;
	}

	// add element list header
	slrdata_write(6, data + element_list_offset, c->element_count * SLRDATA_RELATION_ELEMENTSIZE);
	slrdata_write(6, data + element_list_offset + 6, c->element_count);

	// add element list - contains offsets to incidence lists
	for(uint_fast64_t i = 0; i < c->element_count; i++)
	{
		uint_fast64_t incidence_list_offset = element_list_offset + element_list_size + (i * (SLRDATA_RELATION_LISTHEADERSIZE + (c->degree * SLRDATA_RELATION_INCIDENCESIZE)));
		slrdata_write(8, data + element_list_offset + SLRDATA_RELATION_LISTHEADERSIZE + (i * SLRDATA_RELATION_ELEMENTSIZE), incidence_list_offset);

		// incidence list header
		slrdata_write(6, data + incidence_list_offset, c->degree * SLRDATA_RELATION_INCIDENCESIZE);
		slrdata_write(6, data + incidence_list_offset + 6, 0);
	}

	// add incidence lists
	slrdata_tuple_t *current_tuple_list = r->tuples;
	for(uint_fast64_t i = 0; i < r->tuple_count; i++)
	{
		for(uint_fast64_t j = 0; j < r->arity; j++)
		{
			uint_fast64_t element = (uint_fast64_t)current_tuple_list->elements[j];
			uint_fast64_t incidence_list_offset = slrdata_read(8, data + element_list_offset + SLRDATA_RELATION_LISTHEADERSIZE + (element * SLRDATA_RELATION_ELEMENTSIZE));
			uint_fast64_t degree = slrdata_read(6, data + incidence_list_offset + 6);

			slrdata_write(6, data + incidence_list_offset + 6, degree + 1);
            slrdata_write(6, data + incidence_list_offset + SLRDATA_RELATION_LISTHEADERSIZE + (degree * SLRDATA_RELATION_INCIDENCESIZE), i);
		}

		current_tuple_list = current_tuple_list->tuple_next;
	}

	if(slrdata_create_and_write_file(slrdata_filepath(foldername, r->label), data, size))
	{
		return(-1);
	}

	return(0);
}

int slrdata_create(slrdata_create_t *c, const char *restrict foldername)
{
	// makes directory - errors if it already exists TODO check modes
	if(mkdir(foldername, S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH))
		return(-1);

	// create main files
	if(slrdata_create_main_file(c, foldername))
	{
		return(-1);
	}

	// create relation files
	slrdata_relation_t *current_relation = c->relations;
	for(uint_fast64_t i = 0; i < c->relation_count; i++)
	{
		if(slrdata_create_relation_file(c, current_relation, foldername))
			return(-1);

		current_relation = current_relation->relation_next;
	}

	return(0);
}

int slrdata_open(slrdata_t *d, const char *restrict foldername, const char *restrict filename, bool readonly, bool is_relation)
{
	struct stat stat;

	if((d->fd = open(slrdata_filepath(foldername, filename), readonly ? O_RDONLY : O_RDWR, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH)) == -1)
		return(-1);

	fstat(d->fd, &stat);

	uint_fast64_t headersize_basic = is_relation ? SLRDATA_RELATION_HEADERSIZE_BASIC : SLRDATA_HEADERSIZE_BASIC;
	if(stat.st_size < headersize_basic) // File is too small to be valid
	{
		close(d->fd);
		return(-1);
	}
	else if(stat.st_size) // Check if file is valid
	{
		// Checks we can map
		if((d->ptr = mmap(0, headersize_basic, PROT_READ, MAP_SHARED, d->fd, 0)) == MAP_FAILED)
		{
			close(d->fd);
			return(-1);
		}

		// We check:
		// it starts with 'slrdata' AND
		// it is version 1
		if(strncmp(d->ptr, "slrdata", 7) || slrdata_read(8, d->ptr + (is_relation ? 16 : 8)) != 1
				|| (is_relation && strncmp(d->ptr + 7, "relation", 8)))
		{
			close(d->fd);
			return(-1);
		}

		d->size = slrdata_read(8, d->ptr + headersize_basic - 8);

		munmap(d->ptr, headersize_basic);
	}


	if((d->ptr = mmap(0, d->size, readonly ? PROT_READ : (PROT_READ | PROT_WRITE), MAP_SHARED, d->fd, 0)) == MAP_FAILED)
	{
		close(d->fd);
		return(-1);
	}

	return(0);
}

void slrdata_close(slrdata_t *d)
{
	munmap(d->ptr, d->size);
	close(d->fd);
}

uint_fast64_t slrdata_arity(slrdata_t *relation)
{
	return slrdata_read(6, relation->ptr + SLRDATA_RELATION_HEADERSIZE_BASIC + (2 * 8));
}

uint_fast64_t slrdata_degree(slrdata_t *relation, uint_fast64_t e)
{
	uint_fast64_t element_list_offset = slrdata_read(8, relation->ptr + SLRDATA_RELATION_HEADERSIZE_BASIC + 8);
	uint_fast64_t incidence_list_offset = slrdata_read(8, relation->ptr + element_list_offset + SLRDATA_RELATION_LISTHEADERSIZE + (e * SLRDATA_RELATION_ELEMENTSIZE));

	return slrdata_read(6, relation->ptr + incidence_list_offset + 6);
}

// TODO rename. i and e start at 0
uint_fast64_t * slrdata_tuple(slrdata_t *relation, uint_fast64_t e, uint_fast64_t i)
{
	uint_fast64_t arity = slrdata_arity(relation);
	static uint_fast64_t* tuple;
	tuple = (uint_fast64_t*)malloc(arity * sizeof(uint_fast64_t));

	uint_fast64_t degree = slrdata_degree(relation, e);
	if (degree <= i)
	{
		return NULL;
	}

	uint_fast64_t element_list_offset = slrdata_read(8, relation->ptr + SLRDATA_RELATION_HEADERSIZE_BASIC + 8);
	uint_fast64_t incidence_list_offset = slrdata_read(8, relation->ptr + element_list_offset + SLRDATA_RELATION_LISTHEADERSIZE + (e * SLRDATA_RELATION_ELEMENTSIZE));
	uint_fast64_t tuple_index = slrdata_read(6, relation->ptr + incidence_list_offset + SLRDATA_RELATION_LISTHEADERSIZE + (i * SLRDATA_RELATION_INCIDENCESIZE));

	uint_fast64_t tuple_list_offset = slrdata_read(8, relation->ptr + SLRDATA_RELATION_HEADERSIZE_BASIC);
	uint_fast64_t tuple_offset = tuple_list_offset + SLRDATA_RELATION_LISTHEADERSIZE + (tuple_index * SLRDATA_RELATION_TUPLEELEMENTSIZE * arity);

	for (uint_fast64_t j = 0; j < arity; j++)
	{
		tuple[j] = slrdata_read(6, relation->ptr + tuple_offset + (j * SLRDATA_RELATION_TUPLEELEMENTSIZE));
	}

	return tuple;
}

uint_fast64_t slrdata_element_count(slrdata_t *d)
{
	uint_fast64_t element_list_offset_offset = slrdata_is_relation_file(d) ?
			SLRDATA_RELATION_HEADERSIZE_BASIC + 8 :
			SLRDATA_HEADERSIZE_BASIC;

	uint_fast64_t element_list_offset = slrdata_read(8, d->ptr + element_list_offset_offset);
	return slrdata_read(6, d->ptr + element_list_offset + 6);
}

uint_fast64_t slrdata_relation_count(slrdata_t *d)
{
	if (slrdata_is_relation_file(d))
			return -1;

	uint_fast64_t relation_list_offset = slrdata_read(8, d->ptr + SLRDATA_HEADERSIZE_BASIC + 8);
	return slrdata_read(6, d->ptr + relation_list_offset + 6);
}

uint_fast64_t slrdata_tuple_count(slrdata_t *relation)
{
	if (!slrdata_is_relation_file(relation))
		return -1;

	uint_fast64_t tuple_list_offset = slrdata_read(8, relation->ptr + SLRDATA_RELATION_HEADERSIZE_BASIC);

	return slrdata_read(6, relation->ptr + tuple_list_offset + 6);
}
