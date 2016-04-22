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

	struct bdring	*pbdr;
	unsigned int	index_claimed; // points to the first index that is claimed
	unsigned int	index_write;   // points to the first index to be written or claimed

	fifo_wakeup_handler wakeup_handler;
	void		*wakeup_handler_arg;

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
	pwriter->pfifo = pfifo;
	pwriter->pdata = pfifo->pdata;
	pwriter->datasize = pfifo->pheader->datasize;

	pwriter->pbdr = &pfifo->bdr;
	pwriter->index_claimed = 0;
	pwriter->index_write = 0;

	pwriter->wakeup_handler = NULL;
	pwriter->wakeup_handler_arg = NULL;

	pwriter->freesize = pwriter->datasize;
	pwriter->freesize_next = 0;
	pwriter->pwrite = pwriter->pdata;

	pwriter->bdring_last_reader_idx = 0;
	pwriter->plast_read = pwriter->pdata - 1;

	pwriter->align_bits = pfifo->pheader->align-1;
}

/**
 * @brief Set a wakeup handler to be called when the writer wants to wakeup the reader
 */
static inline void fifo_writer_set_wakeup_handler(struct fifo_writer *pwriter, fifo_wakeup_handler wakeup_handler, void *wakeup_handler_arg)
{
	pwriter->wakeup_handler = wakeup_handler;
	pwriter->wakeup_handler_arg = wakeup_handler_arg;
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
	struct fifo_bd bd;
	unsigned int *plast_reader_idx = &pwriter->bdring_last_reader_idx;
	unsigned int idx;

	/* Try to find where the reader is */
	for (idx = *plast_reader_idx; idx != pwriter->index_claimed; idx = bdring_next(pwriter->pbdr, idx)) {
		if (bdring_bd_get(pwriter->pbdr, idx, &bd.data)) {
			*plast_reader_idx = idx;
			*poffset = bd.offset;
			*psize   = bd.size;
			return;
		}
	}

	/* Reader is at the same location as the writer, so we are full, or empty */
	*plast_reader_idx = pwriter->index_claimed;
	if (bdring_bd_get(pwriter->pbdr, pwriter->index_claimed, &bd.data)) {
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
	else if (pwriter->index_claimed == pwriter->index_write) {
		/* Empty, reset */
		pwriter->freesize = pwriter->datasize;
		pwriter->freesize_next = 0;
		pwriter->pwrite = pwriter->pdata;
		pwriter->plast_read = pwriter->pdata - 1;
	}
	else {
		/*
		 * FIXME: Claimed packets are not yet committed.
		 *        What is the reader position?
		 */
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
	if (pwriter->wakeup_handler == NULL)
		return;

	if ((force) || (pwriter->pfifo->pheader->reader_status & RD_STS_WAITING))
		pwriter->wakeup_handler(pwriter->wakeup_handler_arg);
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

	// Commit the data to the reader
	bdring_bd_put(pwriter->pbdr, pwriter->index_claimed, bd.data);

	if (pwriter->index_claimed == pwriter->index_write) {
		// Advance both indices
		pwriter->index_claimed = bdring_next(pwriter->pbdr, pwriter->index_claimed);
		pwriter->index_write = pwriter->index_claimed;
	}
	else {
		// Advance the read index only
		pwriter->index_claimed = bdring_next(pwriter->pbdr, pwriter->index_claimed);
	}

	return size;
}

/**
 * @brief Advance the write pointer, but do not commit the data to the reader
 */
static inline void fifo_writer_claim(struct fifo_writer *pwriter, unsigned int count, unsigned int size)
{
	unsigned int aligned_size = _fifo_writer_align_up(pwriter, size);

	pwriter->freesize -= aligned_size;
	pwriter->pwrite   += aligned_size;

	while(count--)
		pwriter->index_write = bdring_next(pwriter->pbdr, pwriter->index_write);
}

#ifdef __cplusplus
};
#endif

#endif
