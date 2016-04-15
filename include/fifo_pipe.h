#ifndef __FIFO_PIPE_H
#define __FIFO_PIPE_H

/**
 * @file fifo_pipe.h
 * @brief Pipe data from a fifo_reader to a fifo_writer.
 */

#include <string.h> // memcpy
#include <stdlib.h> // malloc / free

#include "linux_port.h"
#include "fifo_reader.h"
#include "fifo_writer.h"

#define USE_BATCHES

#if 1
/* Use small batches if the receiver is needing data urgently */
#define MAX_BATCH_SIZE		(FIFO_SIZE/2)
#define MAX_BATCH_SIZE_URGENT	(2*1024)
#else
/* No limits */
#define MAX_BATCH_SIZE		(FIFO_SIZE)
#define MAX_BATCH_SIZE_URGENT	(FIFO_SIZE)
#endif
#define MAX_FREE_SIZE_URGENT	(FIFO_SIZE-MAX_BATCH_SIZE_URGENT)

#ifdef __cplusplus
extern "C" {
#endif

struct fifo_pipe;
struct fifo_pipe_transfer;

//---------------------------------------------------------------------------
struct fifo_pipe
{
	struct fifo_reader *preader;
	struct fifo_writer *pwriter;

	void (*fp_transfer)(void *dst, const void *src, size_t size, struct fifo_pipe_transfer *ptransfer);
};

//---------------------------------------------------------------------------
struct fifo_pipe_transfer
{
	struct fifo_pipe *ppipe;
	unsigned int batch_size;
#ifdef USE_BATCHES
	unsigned int batch_count;
#endif
};

#ifdef USE_BATCHES
/** @brief Commit a batch data transfer into the fifo
 *
 *  The output fifo will be notified of the new data
 *  The input fifo data will be freed
 *
 *  Note: This function should be called after the data has been successfully
 *  transferred into the output fifo.
 *
 *  Note: Because of alignment restriction the batch_size is not always equal
 *  to the sum of all data transfers in the batch.
 *
 *  @param ppipe the fifo_pipe object
 *  @param batch_count the number of data transfers in this batch
 *  @param batch_size the total batch size
 */
static inline void fifo_pipe_commit_batch(struct fifo_pipe *ppipe, unsigned int batch_count, unsigned int batch_size)
{
	uint32_t *blockout;
	struct fifo_bd bd;
	unsigned int offset_first;
	struct fifo_reader *preader = ppipe->preader;
	struct fifo_writer *pwriter = ppipe->pwriter;
	struct bdring_reader *pbdrr = &preader->bdrr;
	unsigned int i;

	blockout = (uint32_t *)fifo_writer_get_pointer(pwriter);

	/* Commit the first packet */
	bdring_reader_get(pbdrr, &bd.data);
	offset_first = bd.offset;
	fifo_writer_commit_raw(pwriter, (uint8_t *)blockout, bd.size);
	fifo_reader_pop(preader);

	/* Commit the rest of the packets */
	for (i = 1; i < batch_count; i++) {
		bdring_reader_get(pbdrr, &bd.data);
		fifo_writer_commit_raw(pwriter, (uint8_t *)blockout + (bd.offset - offset_first), bd.size);
		fifo_reader_pop(preader);
	}

	/* Advance the write pointer for the whole batch */
	fifo_writer_advance(pwriter, batch_size);
}
#endif // USE_BATCHES

#ifndef USE_BATCHES
/** @brief Commit a single data transfer into the fifo
 *
 *  The output fifo will be notified of the new data
 *  The input fifo data will be freed
 *
 *  Note: This function should be called after the data has been successfully
 *  transferred into the output fifo.
 *
 *  @param ppipe the fifo_pipe object
 */
static inline void fifo_pipe_commit_single(struct fifo_pipe *ppipe)
{
	struct fifo_reader *preader = ppipe->preader;
	struct fifo_writer *pwriter = ppipe->pwriter;
	uint32_t *blockout = (uint32_t *)fifo_writer_get_pointer(pwriter);
	unsigned int size = fifo_reader_get_size(preader);
	fifo_writer_commit(pwriter, blockout, size);
	fifo_reader_pop(preader);
}
#endif // USE_BATCHES

//---------------------------------------------------------------------------
static inline void fifo_pipe_transfer_complete(struct fifo_pipe *ppipe, struct fifo_pipe_transfer *ptransfer)
{
#ifdef USE_BATCHES
	fifo_pipe_commit_batch(ppipe, ptransfer->batch_count, ptransfer->batch_size);
#else
	fifo_pipe_commit_single(ppipe);
#endif

	fifo_reader_wakeup_writer(ppipe->preader, 0);
	fifo_writer_wakeup_reader(ppipe->pwriter, 0);

	free(ptransfer);
}

//---------------------------------------------------------------------------
static inline void fifo_pipe_transfer_default(void *dst, const void *src, size_t size, struct fifo_pipe_transfer *ptransfer)
{
	memcpy(dst, src, size);
	fifo_pipe_transfer_complete(ptransfer->ppipe, ptransfer);
}

/** @brief Transfer as much data as possible from the reader to the writer
 *
 *  The pipe will stop when either the reader is empty, or the writer is full
 *
 *  @param ppipe the fifo_pipe object
 */
static inline uint32_t fifo_pipe_transfer(struct fifo_pipe *ppipe)
{
	struct fifo_reader *preader = ppipe->preader;
	struct fifo_writer *pwriter = ppipe->pwriter;
	uint32_t *blockin, *blockout;
	void *pdummy;
	unsigned int batch_size = 0, batch_size_min, batch_size_max;
#ifdef USE_BATCHES
	unsigned int batch_count = 0;
#endif

	/*
	 * 1 get minimal size needed by first block in reader
	 * 2 get maximum size free in writer, using minimum space required
	 * 3 get maximum number of continuous blocks from reader, using maximum space
	 */

	/* 1 get minimal size needed by first block */
	batch_size_min = fifo_reader_get(preader, &pdummy);
	if (batch_size_min == 0)
		return 0;

	/* Update so we know how much free space there is */
	fifo_writer_update_reader(pwriter);

	/* 2 get maximum size free in writer */
	batch_size_max = fifo_writer_get_free_contiguous(pwriter, batch_size_min);
	if (batch_size_max < batch_size_min)
		return 1;

#ifdef USE_BATCHES
	/* Limit the batch size when urgent */
	if (fifo_writer_get_free_total(pwriter) >= MAX_FREE_SIZE_URGENT) {
		if (batch_size_max > MAX_BATCH_SIZE_URGENT)
			batch_size_max = MAX_BATCH_SIZE_URGENT;
	}

	/* Limit the batch size to prevent batches from taking too long */
	if (batch_size_max > MAX_BATCH_SIZE)
		batch_size_max = MAX_BATCH_SIZE;

	/* 3 get maximum number of continuous blocks */
	blockin  = (uint32_t *)fifo_reader_get_batch(preader, &batch_count, &batch_size, batch_size_max);
#else
	blockin = (uint32_t *)fifo_reader_get_pointer(&pin->fifo_reader);
	batch_size = batch_size_min;
#endif
	/* Get write pointer */
	blockout = (uint32_t *)fifo_writer_get_pointer(pwriter);

	struct fifo_pipe_transfer *ptransfer = (struct fifo_pipe_transfer *)malloc(sizeof(struct fifo_pipe_transfer));
	ptransfer->ppipe = ppipe;
	ptransfer->batch_size = batch_size;
#ifdef USE_BATCHES
	ptransfer->batch_count = batch_count;
#endif
	ppipe->fp_transfer(blockout, blockin, batch_size, ptransfer);

	return batch_size;
}

/**
 * @brief Initialize the fifo_pipe struct
 */
static inline void fifo_pipe_init(struct fifo_pipe *ppipe, struct fifo_reader *preader, struct fifo_writer *pwriter)
{
	ppipe->preader = preader;
	ppipe->pwriter = pwriter;

	ppipe->fp_transfer = fifo_pipe_transfer_default;
}

#ifdef __cplusplus
};
#endif

#endif
