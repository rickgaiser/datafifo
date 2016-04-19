#ifndef __FIFO_READER_H
#define __FIFO_READER_H

/**
 * @file fifo_reader.h
 * @brief Read data from a fifo.
 */

#include "linux_port.h"
#include "bdring.h"
#include "fifo.h"

#ifdef __cplusplus
extern "C" {
#endif

struct fifo_reader
{
	struct fifo	*pfifo;
	uint8_t		*pdata;
	unsigned int	datasize;

	struct bdring	*pbdr;
	unsigned int	first_index;
	unsigned int	last_index;

	void		(*do_wakeup_writer)(void *arg);
	void		*do_wakeup_writer_arg;
};

/**
 * @brief Initialize the fifo_reader struct
 */
static inline void fifo_reader_init(struct fifo_reader *preader, struct fifo *pfifo)
{
	preader->pfifo = pfifo;
	preader->pdata = pfifo->pdata;
	preader->datasize = pfifo->pheader->datasize;

	preader->pbdr = &pfifo->bdr;
	preader->first_index = 0;
	preader->last_index = 0;

	preader->do_wakeup_writer = NULL;
	preader->do_wakeup_writer_arg = NULL;
}

/*
 * Private function
 */
static inline void * _fifo_reader_get_batch(struct fifo_reader *preader, unsigned int *batch_count, unsigned int *batch_size, unsigned int batch_size_max, unsigned int index)
{
	unsigned int offset_first;
	unsigned int temp_size;
	struct bdring *pbdr = preader->pbdr;
	struct fifo_bd bd;

	/* Get first BD */
	if (bdring_bd_get(pbdr, index, &bd.data) == 0)
		return NULL;

	if (bd.size > batch_size_max)
		return NULL; // too big

	offset_first = bd.offset;

	*batch_count = 1;
	*batch_size = bd.size;

	/* Find all continuous data */
	index = bdring_next(pbdr, index);
	while (bdring_bd_get(pbdr, index, &bd.data)) {

		if (bd.offset <= offset_first)
			break; // not continuous

		temp_size = (bd.offset - offset_first) + bd.size;
		if (temp_size > batch_size_max)
			break; // too big

		(*batch_count)++;
		*batch_size = temp_size;

		index = bdring_next(pbdr, index);

	}

	return (preader->pdata + offset_first);
}

/**
 * @brief Try to get the most blocks, fitting into "batch_size_max"
 *
 * NOTE: Staring at the first index
 */
static inline void * fifo_reader_get_batch_first(struct fifo_reader *preader, unsigned int *batch_count, unsigned int *batch_size, unsigned int batch_size_max)
{
	return _fifo_reader_get_batch(preader, batch_count, batch_size, batch_size_max, preader->first_index);
}

/**
 * @brief Try to get the most blocks, fitting into "batch_size_max"
 *
 * NOTE: Staring at the last index
 */
static inline void * fifo_reader_get_batch_last(struct fifo_reader *preader, unsigned int *batch_count, unsigned int *batch_size, unsigned int batch_size_max)
{
	return _fifo_reader_get_batch(preader, batch_count, batch_size, batch_size_max, preader->last_index);
}

/**
 * @brief Clear the entire fifo
 */
static inline void fifo_reader_clear(struct fifo_reader *preader)
{
	bdring_clear(preader->pbdr);
}

/**
 * @brief Wakeup a writer that is waiting for free space
 *
 * Call this function after reading data from the fifo to wake the writer.
 * NOTE: The wakeup only works when a wakeup handler is set.
 */
static inline void fifo_reader_wakeup_writer(struct fifo_reader *preader, unsigned int force)
{
	if (preader->do_wakeup_writer == NULL)
		return;

	if ((force) || (preader->pfifo->pheader->writer_status & WR_STS_WAITING))
		preader->do_wakeup_writer(preader->do_wakeup_writer_arg);
}

/**
 * @brief Check if the fifo is empty
 *
 * It is not possible to check if the fifo is full.
 */
static inline int fifo_reader_is_empty(struct fifo_reader *preader)
{
	return bdring_bd_is_used(preader->pbdr, preader->first_index) == 0;
}

/*
 * Private function
 */
static inline unsigned int _fifo_reader_get(struct fifo_reader *preader, void ** pdata, unsigned int index)
{
	struct fifo_bd bd;

	bdring_bd_get(preader->pbdr, index, &bd.data);

	if (pdata != NULL)
		*pdata = (bd.size == 0) ? NULL : (preader->pdata + bd.offset);

	return bd.size;
}

/**
 * @brief Get the first packet if there is one
 */
static inline unsigned int fifo_reader_get_first(struct fifo_reader *preader, void ** pdata)
{
	return _fifo_reader_get(preader, pdata, preader->first_index);
}

/**
 * @brief Get the last packet if there is one
 */
static inline unsigned int fifo_reader_get_last(struct fifo_reader *preader, void ** pdata)
{
	return _fifo_reader_get(preader, pdata, preader->last_index);
}

/**
 * @brief Free packets, so the writer can use them again
 */
static inline void fifo_reader_free(struct fifo_reader *preader)
{
	bdring_bd_clear(preader->pbdr, preader->first_index);
	preader->first_index = bdring_next(preader->pbdr, preader->first_index);
}

/**
 * @brief Advance the last index, but do not free the data to the writer
 */
static inline void fifo_reader_advance_last(struct fifo_reader *preader, unsigned int count)
{
	while(count--)
		preader->last_index = bdring_next(preader->pbdr, preader->last_index);
}

#ifdef __cplusplus
};
#endif

#endif
