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

	void (*fp_transfer)(struct fifo_pipe_transfer *ptransfer);
};

//---------------------------------------------------------------------------
struct fifo_pipe_transfer
{
	struct fifo_pipe *ppipe;

	void *dst;
	const void *src;
	size_t size;

	unsigned int batch_count;
};

/** @brief Commit a transfer into the fifo
 *
 *  The output fifo will be notified of the new data
 *  The input fifo data will be freed
 *
 *  Note: This function should be called after the data has been successfully
 *  transferred into the output fifo.
 *
 *  @param ppipe the fifo_pipe object
 *  @param ptransfer the fifo_pipe_transfer object
 */
static inline void fifo_pipe_transfer_commit(struct fifo_pipe *ppipe, struct fifo_pipe_transfer *ptransfer)
{
#ifndef USE_BATCHES
	fifo_writer_commit(ppipe->pwriter, ptransfer->dst, ptransfer->size);
	fifo_reader_pop(ppipe->preader);
#else
	uint8_t *blockout = (uint8_t *)ptransfer->dst;
	struct fifo_bd bd;
	unsigned int offset_first;
	struct fifo_reader *preader = ppipe->preader;
	struct fifo_writer *pwriter = ppipe->pwriter;
	struct bdring_reader *pbdrr = &preader->bdrr;
	unsigned int i;

	/* Commit all packets, but do not advance the write pointer because of alignment */
	for (i = 0; i < ptransfer->batch_count; i++) {
		bdring_reader_get(pbdrr, &bd.data);
		if (i == 0) offset_first = bd.offset;
		fifo_writer_commit_raw(pwriter, blockout + (bd.offset - offset_first), bd.size);
		fifo_reader_pop(preader);
	}

	/* Advance the write pointer for the whole batch */
	fifo_writer_advance(pwriter, ptransfer->size);
#endif // USE_BATCHES

	fifo_reader_wakeup_writer(ppipe->preader, 0);
	fifo_writer_wakeup_reader(ppipe->pwriter, 0);

	free(ptransfer);
}

//---------------------------------------------------------------------------
static inline void fifo_pipe_transfer_default(struct fifo_pipe_transfer *ptransfer)
{
	memcpy(ptransfer->dst, ptransfer->src, ptransfer->size);
	fifo_pipe_transfer_commit(ptransfer->ppipe, ptransfer);
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
	void *blockin, *blockout;
	unsigned int batch_size = 0, batch_size_min, batch_size_max;
	unsigned int batch_count = 0;

	/*
	 * 1 get minimal size needed by first block in reader
	 * 2 get maximum size free in writer, using minimum space required
	 * 3 get maximum number of continuous blocks from reader, using maximum space
	 */

	/* 1 get minimal size needed by first block */
	batch_size_min = fifo_reader_get(preader, &blockin);
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

	/* Not smaller than the minimum batch size */
	if (batch_size_max < batch_size_min)
		batch_size_max = batch_size_min;

	/* 3 get maximum number of continuous blocks */
	blockin  = fifo_reader_get_batch(preader, &batch_count, &batch_size, batch_size_max);
#else
	batch_size = batch_size_min;
	batch_count = 1;
#endif
	/* Get write pointer */
	blockout = fifo_writer_get_pointer(pwriter);

	struct fifo_pipe_transfer *ptransfer = (struct fifo_pipe_transfer *)malloc(sizeof(struct fifo_pipe_transfer));
	ptransfer->ppipe = ppipe;
	ptransfer->dst = blockout;
	ptransfer->src = blockin;
	ptransfer->size = batch_size;
	ptransfer->batch_count = batch_count;

	ppipe->fp_transfer(ptransfer);

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
