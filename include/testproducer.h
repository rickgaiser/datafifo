#ifndef __TESTPRODUCER_H
#define __TESTPRODUCER_H

/**
 * @file testproducer.h
 * @brief Writes 32bit int's into the fifo with incrementing numbers 0,1,2,3,...
 */

#include "fifo_writer.h"

#ifdef __cplusplus
extern "C" {
#endif

struct testproducer
{
	struct fifo_writer *pwriter;
	unsigned int count32;
	unsigned int actual32;
};

/**
 * @brief Initialize the testproducer struct
 */
static inline void testproducer_init(struct testproducer *pprod, struct fifo_writer *pwriter, unsigned int count)
{
	pprod->pwriter = pwriter;
	pprod->count32 = count/4;
	pprod->actual32 = 0;
}

static inline int testproducer_done(struct testproducer *pprod)
{
	return (pprod->actual32 >= pprod->count32) ? 1 : 0;
}

static inline unsigned int testproducer_produce_one(struct testproducer *pprod)
{
	uint32_t *block;
	unsigned int idx;
	unsigned int size;
	unsigned int size32;
	struct fifo_writer *pwriter = pprod->pwriter;

	// Get free size
	fifo_writer_update_reader(pwriter);
	size = fifo_writer_get_free_contiguous(pwriter, sizeof(uint32_t));
	if (size > FIFO_BLOCK_MAX_SIZE)
		size = FIFO_BLOCK_MAX_SIZE;
	if (size < sizeof(uint32_t))
		return 0;

	// Get number of ints to produce
	size32 = size / sizeof(uint32_t);
	if (size32 > (pprod->count32 - pprod->actual32))
		size32 = pprod->count32 - pprod->actual32;

	// Get data pointer
	block = (uint32_t *)fifo_writer_get_pointer(pwriter);

	// Write to data block
	for (idx = 0; idx < size32; idx++)
		block[idx] = pprod->actual32++;

	// Notify reader of new data
	fifo_writer_commit(pwriter, block, size);
	fifo_writer_wakeup_reader(pwriter, 0);

	return size;
}

static inline unsigned int testproducer_produce(struct testproducer *pprod)
{
	unsigned int size = 0;
	unsigned int total_size = 0;

	while((size = testproducer_produce_one(pprod)) > 0)
		total_size += size;

	// Force wakeup signal to the reader
	fifo_writer_wakeup_reader(pprod->pwriter, 1);

	return total_size;
}

#ifdef __cplusplus
};
#endif

#endif
