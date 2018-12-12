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
// Size of lists header size e.g elements
#define SLRDATA_LIST_HEADERSIZE 12

static void slrdata_write(uint_fast64_t bytes, void *ptr, uint_fast64_t v)
{
	unsigned char *cptr = ptr;
	for(uint_fast8_t i = 0; i < bytes; i++)
		cptr[i] = (v >> i * 8) & 0xff;
}

static uint_fast64_t slrdata_read(uint_fast64_t bytes, const void *ptr)
{
	const unsigned char *cptr = ptr;
	uint_fast32_t ret = 0;

	for(uint_fast8_t i = 0; i < bytes; i++)
		ret |= ((uint_fast64_t)(cptr[i]) << i * 8);

	return(ret);
}

// Write a new header (and empty element and relations lists)
static void slrdata_headerinit(unsigned char *header)
{
	memcpy(header, u8"slrdata", 8);

	// Version
	slrdata_write(8, header + 8, 1);

	// Size
	slrdata_write(8, header + SLRDATA_HEADERSIZE_BASIC, SLRDATA_HEADERSIZE);

	// elements list and relations list offsets - 0 as there isn't any yet
	slrdata_write(8, header + SLRDATA_HEADERSIZE_BASIC + 8, 0);
	slrdata_write(8, header + SLRDATA_HEADERSIZE_BASIC + 16, 0);
}

int slrdata_open(slrdata_t *d, const char *restrict filename, bool readonly)
{
	struct stat stat;

	if((d->fd = open(filename, readonly ? O_RDONLY : (O_RDWR | O_CREAT), S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH)) == -1)
		return(-1);

	fstat(d->fd, &stat);

	// If file is empty and not read only then create header
	if(!stat.st_size && !readonly)
	{
		unsigned char header[SLRDATA_HEADERSIZE];

		slrdata_headerinit(header);

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
