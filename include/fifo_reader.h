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

	struct		bdring_reader bdrr;

	void		(*do_wakeup_writer)(void *arg);
	void		*do_wakeup_writer_arg;
};

/**
 * @brief Initialize the fifo_reader struct
 */
static inline void fifo_reader_init(struct fifo_reader *preader, struct fifo *pfifo)
{
	bdring_reader_init(&preader->bdrr, &pfifo->bdr);

	preader->pfifo = pfifo;
	preader->pdata = pfifo->pdata;
	preader->datasize = pfifo->pheader->datasize;

	preader->do_wakeup_writer = NULL;
	preader->do_wakeup_writer_arg = NULL;
}

/**
 * @brief Try to get the most blocks, fitting into "batch_size_max"
 */
static inline void * fifo_reader_get_batch(struct fifo_reader *preader, unsigned int *batch_count, unsigned int *batch_size, unsigned int batch_size_max)
{
	unsigned int offset_first;
	unsigned int temp_size;
	unsigned int index;
	struct bdring_reader *pbdrr;
	struct fifo_bd bd;

	pbdrr = &preader->bdrr;
	index = pbdrr->index;

	/* Get first BD */
	if (bdring_bd_get(pbdrr->pbdr, index, &bd.data) == 0)
		return NULL;

	if (bd.size > batch_size_max)
		return NULL; // too big

	offset_first = bd.offset;

	*batch_count = 1;
	*batch_size = bd.size;

	/* Find all continuous data */
	index = bdring_reader_next(pbdrr, index);
	while (bdring_bd_get(pbdrr->pbdr, index, &bd.data)) {

		if (bd.offset <= offset_first)
			break; // not continuous

		temp_size = (bd.offset - offset_first) + bd.size;
		if (temp_size > batch_size_max)
			break; // too big

		(*batch_count)++;
		*batch_size = temp_size;

		index = bdring_reader_next(pbdrr, index);

	}

	return (preader->pdata + offset_first);
}

/**
 * @brief Clear the entire fifo
 */
static inline void fifo_reader_clear(struct fifo_reader *preader)
{
	bdring_clear(preader->bdrr.pbdr);
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
	return bdring_reader_is_empty(&preader->bdrr);
}

/**
 * @brief Get the first packet if there is one
 */
static inline unsigned int fifo_reader_get(struct fifo_reader *preader, void ** pdata)
{
	struct fifo_bd bd;

	bdring_reader_get(&preader->bdrr, &bd.data);

	if (pdata != NULL)
		*pdata = (bd.size == 0) ? NULL : (preader->pdata + bd.offset);

	return bd.size;
}

/**
 * @brief Pop the first packet, this frees the data for the writer
 */
static inline void fifo_reader_pop(struct fifo_reader *preader)
{
	bdring_reader_pop(&preader->bdrr);
}

#ifdef __cplusplus
};
#endif

#endif
