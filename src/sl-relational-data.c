#include "../include/sl-relational-data.h"
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/mman.h>

// Size of 'slrdata', version number
#define SLRDATA_HEADERSIZE_BASIC 16
// SLRDATA_HEADERSIZE_BASIC plus size of file size, elements and relations offsets
#define SLRDATA_HEADERSIZE (SLRDATA_HEADERSIZE_BASIC + 3 * 8)
#define SLRDATA_HEADER_ELEMENTSOFFSET (SLRDATA_HEADERSIZE_BASIC + 8)
#define SLRDATA_HEADER_RELATIONSOFFSET (SLRDATA_HEADERSIZE_BASIC + 2 * 8)

#define SLRDATA_ELEMENTLIST_HEADERSIZE 12
#define SLRDATA_ELEMENTSIZE 6

#define SLRDATA_RELATIONLIST_HEADERSIZE 12
#define SLRDATA_RELATIONSIZE 20

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

// Write a new header (and empty element and relations lists)
static void slrdata_headerinit(unsigned char *header, uint_fast64_t filesize)
{
	memcpy(header, u8"slrdata", 8);

	// Version
	slrdata_write(8, header + 8, 1);

	// Size
	slrdata_write(8, header + SLRDATA_HEADERSIZE_BASIC, filesize);

	// elements list and relations list offsets - 0 as there isn't any yet
	slrdata_write(8, header + SLRDATA_HEADERSIZE_BASIC + 8, 0);
	slrdata_write(8, header + SLRDATA_HEADERSIZE_BASIC + 16, 0);
}

static void slrdata_add_element_list(unsigned char *data, slrdata_t *d, slrdata_create_t *c, uint_fast64_t offset)
{
	// update element list offset
	slrdata_write(8, data + SLRDATA_HEADER_ELEMENTSOFFSET, offset);

	// write element list header
	slrdata_write(6, data + offset, SLRDATA_ELEMENTSIZE * c->element_count);
	slrdata_write(6, data + offset + 6, c->element_count);

	// add elements TODO write actual label instead of number
	for(uint_fast64_t i = 0; i < c->element_count; i++)
	{
		slrdata_write(6, data + offset + SLRDATA_ELEMENTLIST_HEADERSIZE + i * SLRDATA_ELEMENTSIZE, i + 1);
	}
}

static void slrdata_add_relations_list(unsigned char *data, slrdata_t *d, slrdata_create_t *c, uint_fast64_t offset)
{
	// update relation list offset
	slrdata_write(8, data + SLRDATA_HEADER_RELATIONSOFFSET, offset);

	// write relation list header
	slrdata_write(6, data + offset, SLRDATA_RELATIONSIZE * c->relation_count);
	slrdata_write(6, data + offset + 6, c->relation_count);

	// add relations
	uint_fast64_t relations_offset = offset + SLRDATA_RELATIONLIST_HEADERSIZE;
	for(uint_fast64_t i = 0; i < c->relation_count; i++)
	{
		// label TODO
		slrdata_write(6, data + relations_offset + i * SLRDATA_RELATIONSIZE, i + 1);

		// arity
		slrdata_write(6, data + relations_offset + (i * SLRDATA_RELATIONSIZE) + 6, c->relations[i]->arity);

		// element list offset
		uint_fast64_t elements_offset = relations_offset + SLRDATA_RELATIONSIZE * c->relation_count + i * (SLRDATA_ELEMENTLIST_HEADERSIZE + 8 * c->element_count);
		slrdata_write(8, data + relations_offset + i * SLRDATA_RELATIONSIZE + 12, elements_offset);

		// element list
		// header
		slrdata_write(6, data + elements_offset, SLRDATA_ELEMENTSIZE * c->element_count);
		slrdata_write(6, data + elements_offset + 6, c->element_count);
		for(uint_fast64_t j = 0; j < c->element_count; j++)
		{
			// offset of list of tuples element j is in in relation i- initialise as 0
			slrdata_write(8, data + elements_offset + SLRDATA_ELEMENTLIST_HEADERSIZE + j * 8, 0);
		}
	}
}

int slrdata_create(slrdata_t *d, slrdata_create_t *c, const char *restrict filename)
{
	// checks if the file already exists
	if(access(filename, F_OK) != -1)
		return(-1);

	// create file
	if((d->fd = open(filename, (O_RDWR | O_CREAT), S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH)) == -1)
			return(-1);


	uint_fast64_t element_list_size = SLRDATA_ELEMENTLIST_HEADERSIZE + SLRDATA_ELEMENTSIZE * c->element_count;
	uint_fast64_t relations_list_size = SLRDATA_RELATIONLIST_HEADERSIZE + SLRDATA_RELATIONSIZE * c->relation_count;
	uint_fast64_t relations_elements_list_size = SLRDATA_ELEMENTLIST_HEADERSIZE + 8 * c->element_count;

	uint_fast64_t size = SLRDATA_HEADERSIZE + element_list_size + relations_list_size + c->relation_count * relations_elements_list_size;
	unsigned char data[size];

	slrdata_headerinit(data, size);

	slrdata_add_element_list(data, d, c, SLRDATA_HEADERSIZE);

	uint_fast64_t relations_offset = SLRDATA_HEADERSIZE + SLRDATA_ELEMENTLIST_HEADERSIZE + SLRDATA_ELEMENTSIZE * c->element_count;
	slrdata_add_relations_list(data, d, c, relations_offset);

	// write data
	if(write(d->fd, data, size) == -1)
	{
		close(d->fd);
		return(-2);
	}

	d->size = size;


	if((d->ptr = mmap(0, d->size, (PROT_READ | PROT_WRITE), MAP_SHARED, d->fd, 0)) == MAP_FAILED)
	{
		close(d->fd);
		return(-6);
	}

	return(0);
}

int slrdata_open_or_create(slrdata_t *d, const char *restrict filename, bool readonly)
{
	struct stat stat;

	if((d->fd = open(filename, readonly ? O_RDONLY : (O_RDWR | O_CREAT), S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH)) == -1)
		return(-1);

	fstat(d->fd, &stat);

	// If file is empty and not read only then create header
	if(!stat.st_size && !readonly)
	{
		unsigned char header[SLRDATA_HEADERSIZE];

		slrdata_headerinit(header, SLRDATA_HEADERSIZE);

		// write header
		if(write(d->fd, header, SLRDATA_HEADERSIZE) == -1)
		{
			close(d->fd);
			return(-2);
		}

		d->size = SLRDATA_HEADERSIZE;
	}
	else if(stat.st_size < SLRDATA_HEADERSIZE_BASIC) // File is too small to be valid
	{
		close(d->fd);
		return(-3);
	}
	else if(stat.st_size) // Check if file is valid
	{
		// Checks we can map
		if((d->ptr = mmap(0, SLRDATA_HEADERSIZE_BASIC, PROT_READ, MAP_SHARED, d->fd, 0)) == MAP_FAILED)
		{
			close(d->fd);
			return(-4);
		}

		// We check:
		// it starts with 'slrdata' AND
		// it is version 1
		if(strncmp(d->ptr, "slrdata", 7) || slrdata_read(8, d->ptr + 8) != 1)
		{
			close(d->fd);
			return(-5);
		}

		d->size = slrdata_read(8, d->ptr + SLRDATA_HEADERSIZE_BASIC);

		munmap(d->ptr, SLRDATA_HEADERSIZE_BASIC);
	}


	if((d->ptr = mmap(0, d->size, readonly ? PROT_READ : (PROT_READ | PROT_WRITE), MAP_SHARED, d->fd, 0)) == MAP_FAILED)
	{
		close(d->fd);
		return(-6);
	}

	return(0);
}

void slrdata_close(slrdata_t *d)
{
	munmap(d->ptr, d->size);
	close(d->fd);
}

// The following are for testing purposes

uint_fast64_t slrdata_number_of_elements(slrdata_t *d)
{
	uint_fast64_t offset = slrdata_read(8, d->ptr + SLRDATA_HEADER_ELEMENTSOFFSET);
	return slrdata_read(6, d->ptr + offset + 6);
}

uint_fast64_t slrdata_number_of_relations(slrdata_t *d)
{
	uint_fast64_t offset = slrdata_read(8, d->ptr + SLRDATA_HEADER_RELATIONSOFFSET);
	return slrdata_read(6, d->ptr + offset + 6);
}

uint_fast64_t slrdata_element_label(slrdata_t *d, uint_fast64_t n)
{
	uint_fast64_t offset = slrdata_read(8, d->ptr + SLRDATA_HEADER_ELEMENTSOFFSET);
	return slrdata_read(6, d->ptr + offset + SLRDATA_ELEMENTLIST_HEADERSIZE + n * SLRDATA_ELEMENTSIZE);
}

uint_fast64_t slrdata_relation_label(slrdata_t *d, uint_fast64_t n)
{
	uint_fast64_t offset = slrdata_read(8, d->ptr + SLRDATA_HEADER_RELATIONSOFFSET);
	return slrdata_read(6, d->ptr + offset + SLRDATA_RELATIONLIST_HEADERSIZE + n * SLRDATA_RELATIONSIZE);
}

uint_fast64_t slrdata_relation_arity(slrdata_t *d, uint_fast64_t n)
{
	uint_fast64_t offset = slrdata_read(8, d->ptr + SLRDATA_HEADER_RELATIONSOFFSET);
	return slrdata_read(6, d->ptr + offset + SLRDATA_RELATIONLIST_HEADERSIZE + n * SLRDATA_RELATIONSIZE + 6);
}
