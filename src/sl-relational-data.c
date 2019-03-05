#include "../include/sl-relational-data.h"
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <stdlib.h>
#include <stdio.h>

#define SLRDATA_HEADERSIZE_BASIC (3 * 8)
#define SLRDATA_MAIN_HEADERSIZE (SLRDATA_HEADERSIZE_BASIC + 8)
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

static void slrdata_write(uint_fast8_t bytes, unsigned char *ptr, uint_fast64_t v)
{
	for(uint_fast8_t i = 0; i < bytes; i++)
		ptr[i] = (v >> i * 8) & 0xff;
}

static uint_fast64_t slrdata_read(uint_fast8_t bytes, const unsigned char *ptr)
{
	uint_fast32_t ret = 0;

	for(uint_fast8_t i = 0; i < bytes; i++)
		ret |= ((uint_fast64_t)(ptr[i]) << i * 8);

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

static int slrdata_create_and_write_file(slrdata_t *d, char* filepath, unsigned char* header, uint_fast64_t size)
{
	// open and create file
	if((d->fd = open(filepath, (O_RDWR | O_CREAT), S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH)) == -1)
			return(-1);


	if(write(d->fd, header, size) == -1)
	{
		close(d->fd);
		return(-1);
	}

	if((d->ptr = mmap(0, size, (PROT_READ | PROT_WRITE), MAP_SHARED, d->fd, 0)) == MAP_FAILED)
		{
			close(d->fd);
			return(-1);
		}

	d->readonly = false;
	d->size = size;

	return(0);
}

char* slrdata_filepath(const char *restrict foldername, const char *restrict filename)
{
	char *filepath = malloc(strlen(foldername) + strlen(filename) + 2 + 5);
	filepath = strcpy(filepath, foldername);
	filepath = strncat(filepath, "/", 1);
	filepath = strncat(filepath, filename, strlen(filename));
	filepath = strncat(filepath, ".sld", 4);
	return filepath;
}

static int slrdata_resize(slrdata_t *d, size_t s, bool is_relation)
{
	munmap(d->ptr, d->size);

	if(ftruncate(d->fd, s))
	{
		close(d->fd);
		return(-1);
	}

	d->size = s;

	if((d->ptr = mmap(0, d->size, d->readonly ? PROT_READ : (PROT_READ | PROT_WRITE), MAP_SHARED, d->fd, 0)) == MAP_FAILED)
	{
		close(d->fd);
		return(-1);
	}

	slrdata_write(8, d->ptr + (is_relation ? SLRDATA_RELATION_HEADERSIZE_BASIC : SLRDATA_HEADERSIZE_BASIC) - 8, d->size);

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

	d->foldername = foldername;
	d->filename = filename;

	return(0);
}

void slrdata_close(slrdata_t *d)
{
	munmap(d->ptr, d->size);
	close(d->fd);
}

int slrdata_create_directory(const char *restrict foldername)
{
	// makes directory - errors if it already exists TODO check modes
	if(mkdir(foldername, S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH))
		return(-1);

	return(0);
}

int slrdata_create_element_file(slrdata_t *d, const char *restrict foldername)
{
	uint_fast64_t size = SLRDATA_HEADERSIZE_BASIC;
	unsigned char data[size];

	slrdata_headerinit(data, size, false);

	if(slrdata_create_and_write_file(d, slrdata_filepath(foldername, "elements"), data, size))
	{
		return(-1);
	}

	slrdata_resize(d, size + 8, false);
	slrdata_write(8, d->ptr + SLRDATA_HEADERSIZE_BASIC, 0);

	d->foldername = foldername;
	d->filename = "elements";

	return(0);
}

int slrdata_create_relation_file(slrdata_t *d, const char *restrict foldername, const char *restrict relationname)
{
	uint_fast64_t size = SLRDATA_RELATION_HEADERSIZE_BASIC;
	unsigned char data[size];

	slrdata_headerinit(data, size, true);

	if(slrdata_create_and_write_file(d, slrdata_filepath(foldername, relationname), data, size))
	{
		return(-1);
	}

	slrdata_resize(d, size + 8 + 8 + 6, true);

	slrdata_write(8, d->ptr + SLRDATA_RELATION_HEADERSIZE_BASIC, 0);
	slrdata_write(8, d->ptr + SLRDATA_RELATION_HEADERSIZE_BASIC + 8, 0);
	// TODO arity
	slrdata_write(6, d->ptr + SLRDATA_RELATION_HEADERSIZE_BASIC + 16, 0);

	d->foldername = foldername;
	d->filename = relationname;

	return(0);
}

// add element to element file
int slrdata_add_element(slrdata_t *d, const char *restrict label)
{
	// TODO reduce code and check it is the element file
	uint_fast64_t element_list_offset = slrdata_read(8, d->ptr + SLRDATA_HEADERSIZE_BASIC);
	if (element_list_offset == 0)
	{
		// update element list offset
		slrdata_write(8, d->ptr + SLRDATA_HEADERSIZE_BASIC, SLRDATA_MAIN_HEADERSIZE);

		// resize
		slrdata_resize(d, d->size + SLRDATA_MAIN_LISTHEADERSIZE + SLRDATA_MAIN_ELEMENTSIZE, false);

		// add element list header
		slrdata_write(6, d->ptr + SLRDATA_MAIN_HEADERSIZE, SLRDATA_MAIN_ELEMENTSIZE);
		slrdata_write(6, d->ptr + SLRDATA_MAIN_HEADERSIZE + 6, 1);

		// add element
		// TODO label
		slrdata_write(6, d->ptr + SLRDATA_MAIN_HEADERSIZE + SLRDATA_MAIN_LISTHEADERSIZE, 1);
	}
	else
	{
		uint_fast64_t list_size = slrdata_read(6, d->ptr + element_list_offset);
		uint_fast64_t element_count = slrdata_read(6, d->ptr + element_list_offset + 6);

		// update element list header
		slrdata_write(6, d->ptr + element_list_offset, list_size + SLRDATA_MAIN_ELEMENTSIZE);
		slrdata_write(6, d->ptr + element_list_offset + 6, element_count + 1);

		// TODO label
		// resize
		slrdata_resize(d, d->size + SLRDATA_MAIN_ELEMENTSIZE, false);

		// add element
		slrdata_write(6, d->ptr + SLRDATA_MAIN_HEADERSIZE + SLRDATA_MAIN_LISTHEADERSIZE + list_size, element_count + 1);
	}

	return(0);
}

int slrdata_add_tuple(slrdata_t *d, uint_fast64_t * tuple, uint_fast64_t arity)
{
	// TODO reduce code
	uint_fast64_t tuple_list_offset = slrdata_read(8, d->ptr + SLRDATA_RELATION_HEADERSIZE_BASIC);
	if (tuple_list_offset == 0)
	{
		tuple_list_offset = SLRDATA_RELATION_HEADERSIZE;
		// TODO assumes there is no list of elements
		// update tuple list offset and arity
		slrdata_write(8, d->ptr + SLRDATA_RELATION_HEADERSIZE_BASIC, tuple_list_offset);
		slrdata_write(8, d->ptr + SLRDATA_RELATION_HEADERSIZE_BASIC + 16, arity);

		// resize
		slrdata_resize(d, d->size + SLRDATA_RELATION_LISTHEADERSIZE + (SLRDATA_RELATION_TUPLEELEMENTSIZE * arity), true);

		// add tuple list header
		slrdata_write(6, d->ptr + tuple_list_offset, (SLRDATA_RELATION_TUPLEELEMENTSIZE * arity));
		slrdata_write(6, d->ptr + tuple_list_offset + 6, 1);

		// add tuple
		for(int i = 0; i < arity; i++)
			slrdata_write(6, d->ptr + tuple_list_offset + SLRDATA_RELATION_LISTHEADERSIZE + (i * SLRDATA_RELATION_TUPLEELEMENTSIZE), tuple[i]);
	}
	else
	{
		uint_fast64_t list_size = slrdata_read(6, d->ptr + tuple_list_offset);
		uint_fast64_t tuple_count = slrdata_read(6, d->ptr + tuple_list_offset + 6);

		// update tuple list header
		slrdata_write(6, d->ptr + tuple_list_offset, list_size + (arity * SLRDATA_RELATION_TUPLEELEMENTSIZE));
		slrdata_write(6, d->ptr + tuple_list_offset + 6, tuple_count + 1);

		// resize
		slrdata_resize(d, d->size + (arity * SLRDATA_RELATION_TUPLEELEMENTSIZE), true);

		// add tuple
		for(int i = 0; i < arity; i++)
					slrdata_write(6, d->ptr + tuple_list_offset + SLRDATA_RELATION_LISTHEADERSIZE + list_size + (i * SLRDATA_RELATION_TUPLEELEMENTSIZE), tuple[i]);
	}

	return(0);
}

int slrdata_add_incidence_lists(slrdata_t *relation, slrdata_t *elements, uint_fast64_t norm_degree, uint_fast64_t max_degree)
{
	uint_fast64_t element_count = slrdata_element_count(elements);
	uint_fast64_t arity = slrdata_arity(relation);
	uint_fast64_t element_list_offset = relation->size;
	uint_fast64_t tuple_count = slrdata_tuple_count(relation);

	// update element list offset
	slrdata_write(8, relation->ptr + SLRDATA_RELATION_HEADERSIZE_BASIC + 8, element_list_offset);

	// resize
	uint_fast64_t new_size = relation->size + SLRDATA_RELATION_LISTHEADERSIZE + element_count * (SLRDATA_RELATION_ELEMENTSIZE);
	slrdata_resize(relation, new_size, true);

	// element list header
	uint_fast64_t list_size = element_count * SLRDATA_RELATION_ELEMENTSIZE;
	slrdata_write(6, relation->ptr + element_list_offset, list_size);
	slrdata_write(6, relation->ptr + element_list_offset + 6, element_count);

	// element and incidence lists
	for(uint_fast64_t i = 0; i < element_count; i++)
	{
		uint_fast64_t element_offset = element_list_offset + SLRDATA_RELATION_LISTHEADERSIZE + (i * SLRDATA_RELATION_ELEMENTSIZE);

		slrdata_write(8, relation->ptr + element_offset, relation->size);
		slrdata_resize(relation, relation->size + SLRDATA_RELATION_LISTHEADERSIZE + (norm_degree * SLRDATA_RELATION_INCIDENCESIZE), true);

		// incidence list offset
		uint_fast64_t incidence_list_offset = slrdata_read(8, relation->ptr + element_offset);
		slrdata_write(8, relation->ptr + element_offset, incidence_list_offset);

		// incidence list header
		slrdata_write(6, relation->ptr + incidence_list_offset, norm_degree * SLRDATA_RELATION_INCIDENCESIZE);
		slrdata_write(6, relation->ptr + incidence_list_offset + 6, 0);
	}

	for(uint_fast64_t i = 0; i < tuple_count; i++)
	{
		uint_fast64_t *tuple;
		tuple = slrdata_ith_tuple(relation, i);

		for(int a = 0; a < arity; a++)
		{
			uint_fast64_t element_offset = element_list_offset + SLRDATA_RELATION_LISTHEADERSIZE + (tuple[a] * SLRDATA_RELATION_ELEMENTSIZE);
			uint_fast64_t element_incidence_list_offset = slrdata_read(8, relation->ptr + element_offset);
			uint_fast64_t old_degree = slrdata_read(6, relation->ptr + element_incidence_list_offset + 6);
			uint_fast64_t current_list_size = slrdata_read(6, relation->ptr + element_incidence_list_offset);

			if(old_degree + 1 > max_degree)
			{
				return(-1);
			}

			if((old_degree + 1) * SLRDATA_RELATION_INCIDENCESIZE  > current_list_size)
			{
				// incidence list offset
				slrdata_write(8, relation->ptr + element_offset, relation->size);
				slrdata_resize(relation, relation->size + SLRDATA_RELATION_LISTHEADERSIZE + (norm_degree * SLRDATA_RELATION_INCIDENCESIZE), true);

				// header
				uint_fast64_t new_incidence_list_offset = slrdata_read(8, relation->ptr + element_offset);
				memcpy(relation->ptr + new_incidence_list_offset, relation->ptr + element_incidence_list_offset, SLRDATA_RELATION_LISTHEADERSIZE + (old_degree * SLRDATA_RELATION_INCIDENCESIZE));
				slrdata_write(6, relation->ptr + new_incidence_list_offset, current_list_size + (norm_degree * SLRDATA_RELATION_INCIDENCESIZE));
				slrdata_write(6, relation->ptr + new_incidence_list_offset + 6, old_degree + 1);

				slrdata_write(6, relation->ptr + new_incidence_list_offset + SLRDATA_RELATION_LISTHEADERSIZE + (old_degree * SLRDATA_RELATION_INCIDENCESIZE), i);
			}
			else{
				// update degree
				slrdata_write(6, relation->ptr + element_incidence_list_offset + 6, old_degree + 1);
				// add tuple index
				slrdata_write(6, relation->ptr + element_incidence_list_offset + SLRDATA_RELATION_LISTHEADERSIZE + (old_degree * SLRDATA_RELATION_INCIDENCESIZE), i);
			}
		}
	}

	return(0);
}

// reduce size by removing unneeded space between incidence lists
int slrdata_reduce_size(slrdata_t *rel)
{
	slrdata_t rel_new;
	char * filenamedup = strdup(rel->filename);
	if(slrdata_create_relation_file(&rel_new, (const char *restrict)rel->foldername, strcat(filenamedup, "copy")))
		return -1;

	uint_fast64_t tuple_count = slrdata_tuple_count(rel);
	uint_fast64_t arity = slrdata_arity(rel);
	uint_fast64_t tuple_list_size = SLRDATA_RELATION_LISTHEADERSIZE + tuple_count * (arity * SLRDATA_RELATION_TUPLEELEMENTSIZE);
	uint_fast64_t element_count = slrdata_element_count(rel);
	uint_fast64_t element_list_size = SLRDATA_RELATION_LISTHEADERSIZE + (element_count * SLRDATA_RELATION_ELEMENTSIZE);

	// resize
	if(slrdata_resize(&rel_new, rel_new.size + tuple_list_size + element_list_size, true))
		return -1;

	// update arity
	slrdata_write(6, rel_new.ptr + SLRDATA_RELATION_HEADERSIZE_BASIC + 16, arity);

	// copy tuple list
	uint_fast64_t old_tuple_list_offset = slrdata_read(8, rel->ptr + SLRDATA_RELATION_HEADERSIZE_BASIC);
	slrdata_write(8, rel_new.ptr + SLRDATA_RELATION_HEADERSIZE_BASIC, SLRDATA_RELATION_HEADERSIZE);
	memcpy(rel_new.ptr + SLRDATA_RELATION_HEADERSIZE, rel->ptr + old_tuple_list_offset, tuple_list_size);

	// add element list offset
	uint_fast64_t new_element_list_offset = SLRDATA_RELATION_HEADERSIZE + tuple_list_size;
	slrdata_write(8, rel_new.ptr + SLRDATA_RELATION_HEADERSIZE_BASIC + 8, new_element_list_offset);

	// copy element list header
	slrdata_write(6, rel_new.ptr + new_element_list_offset, element_count * SLRDATA_RELATION_ELEMENTSIZE);
	slrdata_write(6, rel_new.ptr + new_element_list_offset + 6, element_count);

	// copy incidence lists
	uint_fast64_t old_element_list_offset = slrdata_read(8, rel->ptr + SLRDATA_RELATION_HEADERSIZE_BASIC + 8);
	uint_fast64_t current_offset = new_element_list_offset + element_list_size;
	for(uint_fast64_t i = 0; i < element_count; i++)
	{
		// update incidence list offset
		slrdata_write(8, rel_new.ptr + new_element_list_offset + SLRDATA_RELATION_LISTHEADERSIZE + (i * SLRDATA_RELATION_ELEMENTSIZE), current_offset);

		// add incidence list header
		uint_fast64_t degree = slrdata_degree(rel, i);

		if(slrdata_resize(&rel_new, rel_new.size + SLRDATA_RELATION_LISTHEADERSIZE + (degree * SLRDATA_RELATION_INCIDENCESIZE), true))
				return -1;

		uint_fast64_t old_incidence_list_offset = slrdata_read(8, rel->ptr + old_element_list_offset + SLRDATA_RELATION_LISTHEADERSIZE + (i * SLRDATA_RELATION_ELEMENTSIZE));
		slrdata_write(6, rel_new.ptr + current_offset, degree * SLRDATA_RELATION_INCIDENCESIZE);
		slrdata_write(6, rel_new.ptr + current_offset + 6, degree);

		// copy incidence list
		memcpy(rel_new.ptr + current_offset + 12, rel->ptr + old_incidence_list_offset + 12, degree * SLRDATA_RELATION_INCIDENCESIZE);

		current_offset += SLRDATA_RELATION_LISTHEADERSIZE + degree * SLRDATA_RELATION_INCIDENCESIZE;
	}

	if(slrdata_resize(&rel_new, current_offset, true))
		return -1;

	if(remove(slrdata_filepath(rel->foldername, rel->filename)))
		return -1;

	if(rename(slrdata_filepath(rel_new.foldername, rel_new.filename), slrdata_filepath(rel->foldername, rel->filename)))
			return -1;

	rel = &rel_new;

	return 0;
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

uint_fast64_t * slrdata_ith_tuple(slrdata_t *relation, uint_fast64_t i)
{
	uint_fast64_t arity = slrdata_arity(relation);
	static uint_fast64_t* tuple;
	tuple = (uint_fast64_t*)malloc(arity * sizeof(uint_fast64_t));

	uint_fast64_t tuple_count = slrdata_tuple_count(relation);
	if (tuple_count <= i)
	{
		return NULL;
	}

	uint_fast64_t tuple_list_offset = slrdata_read(8, relation->ptr + SLRDATA_RELATION_HEADERSIZE_BASIC);
	uint_fast64_t tuple_offset = tuple_list_offset + SLRDATA_RELATION_LISTHEADERSIZE + (i * SLRDATA_RELATION_TUPLEELEMENTSIZE * arity);

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
