#ifndef __FIFO_WRITER_H
#define __FIFO_WRITER_H

/**
 * @file fifo_writer.h
 * @brief Write data into the fifo.
 */

#include "linux_port.h"
#include "bdring.h"
#include "fifo.h"

#ifdef __cplusplus
extern "C" {
#endif

struct fifo_writer
{
	struct fifo	*pfifo;
	uint8_t		*pdata;
	unsigned int	datasize;

	struct		bdring_writer bdrw;

	void		(*do_wakeup_reader)(void *arg);
	void		*do_wakeup_reader_arg;

	unsigned int	freesize;
	unsigned int	freesize_next;
	uint8_t		*pwrite;

	unsigned int	bdring_last_reader_idx;
	uint8_t		*plast_read;

	unsigned int	align_bits;
};

/**
 * @brief Initialize the fifo_writer struct
 */
static inline void fifo_writer_init(struct fifo_writer *pwriter, struct fifo *pfifo)
{
	bdring_writer_init(&pwriter->bdrw, &pfifo->bdr);

	pwriter->pfifo = pfifo;
	pwriter->pdata = pfifo->pdata;
	pwriter->datasize = pfifo->pheader->datasize;

	pwriter->do_wakeup_reader = NULL;
	pwriter->do_wakeup_reader_arg = NULL;

	pwriter->freesize = pwriter->datasize;
	pwriter->freesize_next = 0;
	pwriter->pwrite = pwriter->pdata;

	pwriter->bdring_last_reader_idx = 0;
	pwriter->plast_read = pwriter->pdata - 1;

	pwriter->align_bits = pfifo->pheader->align-1;
}

/*
 * Private function
 */
static inline unsigned int _fifo_writer_align_up(struct fifo_writer *pwriter, unsigned int size)
{
	return ((size + pwriter->align_bits) & ~pwriter->align_bits);
}

/*
 * Private function
 */
static inline void _fifo_writer_get_reader(struct fifo_writer *pwriter, unsigned int *poffset, unsigned int *psize)
{
	struct bdring_writer *pbdrw = &pwriter->bdrw;
	struct fifo_bd bd;
	unsigned int *plast_reader_idx = &pwriter->bdring_last_reader_idx;
	unsigned int idx;

	/* Try to find where the reader is */
	for (idx = *plast_reader_idx; idx != pbdrw->index; idx = bdring_writer_next(pbdrw, idx)) {
		if (bdring_bd_get(pbdrw->pbdr, idx, &bd.data)) {
			*plast_reader_idx = idx;
			*poffset = bd.offset;
			*psize   = bd.size;
			return;
		}
	}

	/* Reader is at the same location as the writer, so we are full, or empty */
	*plast_reader_idx = pbdrw->index;
	if (bdring_bd_get(pbdrw->pbdr, pbdrw->index, &bd.data)) {
		/* full */
		*poffset = bd.offset;
		*psize   = bd.size;
		return;
	}
	else {
		/* empty */
		*poffset = 0;
		*psize   = 0;
		return;
	}
}

/**
 * @brief Update where the reader is
 *
 * The writer needs to know where the reader is so we know how much free space
 * there is in the fifo.
 */
static inline void fifo_writer_update_reader(struct fifo_writer *pwriter)
{
	unsigned int offset, size;

	_fifo_writer_get_reader(pwriter, &offset, &size);
	if (size != 0) {
		/* Update position */
		pwriter->plast_read = pwriter->pdata + offset;

		/*
		 * NOTE: When we are here we are not empty
		 *       so when read equals write, we are full!
		 */
		if (pwriter->plast_read >= pwriter->pwrite) {
			pwriter->freesize = pwriter->plast_read - pwriter->pwrite;
			pwriter->freesize_next = 0;
		}
		else {
			//pwriter->freesize = pwriter->freesize; /* unchanged */
			pwriter->freesize_next = pwriter->plast_read - pwriter->pdata;
		}
	}
	else {
		/* Empty, reset */
		pwriter->freesize = pwriter->datasize;
		pwriter->freesize_next = 0;
		pwriter->pwrite = pwriter->pdata;
		pwriter->plast_read = pwriter->pdata - 1;
	}
}

/**
 * @brief Wakeup a reader that is waiting for data
 *
 * Call this function after new data is placed in the fifo to wake the reader.
 * NOTE: The wakeup only works when a wakeup handler is set.
 */
static inline void fifo_writer_wakeup_reader(struct fifo_writer *pwriter, unsigned int force)
{
	if (pwriter->do_wakeup_reader == NULL)
		return;

	if ((force) || (pwriter->pfifo->pheader->reader_status & RD_STS_WAITING))
		pwriter->do_wakeup_reader(pwriter->do_wakeup_reader_arg);
}

/**
 * @brief Get pointer to the current write position
 */
static inline void * fifo_writer_get_pointer(struct fifo_writer *pwriter)
{
	return pwriter->pwrite;
}

/**
 * @brief Get free space (single block) of at least min_size
 *
 * NOTE: Update the reader with fifo_writer_update_reader before calling
 */
static inline unsigned int fifo_writer_get_free_contiguous(struct fifo_writer *pwriter, unsigned int min_size)
{
	unsigned int aligned_size = _fifo_writer_align_up(pwriter, min_size);

	/* Try to get at least min_size of free space */
	if (pwriter->freesize < aligned_size) {
		/* Reset to the beginning of the ringbuffer if there is more free space */
		if (pwriter->freesize_next > pwriter->freesize) {
			pwriter->pwrite = pwriter->pdata;
			pwriter->freesize = (pwriter->plast_read - pwriter->pdata);
			pwriter->freesize_next = 0;
		}
	}

	/* Return what we have */
	return pwriter->freesize;
}

/**
 * @brief Get total free space
 *
 * NOTE: Update the reader with fifo_writer_update_reader before calling
 */
static inline unsigned int fifo_writer_get_free_total(struct fifo_writer *pwriter)
{
	/* Return what we have, plus what we would have after we flip */
	return (pwriter->freesize + pwriter->freesize_next);
}

/**
 * @brief Commit the data into the fifo for the reader to pickup
 */
static inline unsigned int fifo_writer_commit(struct fifo_writer *pwriter, void *pdata, unsigned int size)
{
	struct fifo_bd bd;

	/* Validate block size */
	if (size > FIFO_BLOCK_MAX_SIZE)
		return 0;

	/* Validate pointer */
	if ((uint8_t *)pdata < pwriter->pdata)
		return 0;

	bd.offset = (uint8_t *)pdata - pwriter->pdata;
	bd.size   = size;

	bdring_writer_put(&pwriter->bdrw, bd.data);

	return size;
}

/**
 * @brief Advance the write pointer
 *
 * @return The actual amount of bytes advanced (aligned)
 */
static inline unsigned int fifo_writer_advance(struct fifo_writer *pwriter, unsigned int size)
{
	unsigned int aligned_size = _fifo_writer_align_up(pwriter, size);

	pwriter->freesize -= aligned_size;
	pwriter->pwrite = pwriter->pwrite + aligned_size;

	return aligned_size;
}

#ifdef __cplusplus
};
#endif

#endif
