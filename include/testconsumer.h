#ifndef __TESTCONSUMER_H
#define __TESTCONSUMER_H

/**
 * @file testconsumer.h
 * @brief Reads 32bit int's from the fifo and checks for incrementing numbers 0,1,2,3,...
 */

#include "fifo_writer.h"
#include "fifo_reader.h"

#ifdef __cplusplus
extern "C" {
#endif

struct testconsumer
{
	struct fifo_reader *preader;
	unsigned int count32;
	unsigned int actual32;
	unsigned int error;
};

/**
 * @brief Initialize the testconsumer struct
 */
static inline void testconsumer_init(struct testconsumer *pcons, struct fifo_reader *preader, unsigned int count)
{
	pcons->preader = preader;
	pcons->count32 = count/4;
	pcons->actual32 = 0;
	pcons->error = 0;
}

static inline int testconsumer_done(struct testconsumer *pcons)
{
	return (pcons->actual32 >= pcons->count32) ? 1 : 0;
}

static inline int testconsumer_error(struct testconsumer *pcons)
{
	return pcons->error;
}

static inline unsigned int testconsumer_consume_one(struct testconsumer *pcons)
{
	uint32_t *block;
	unsigned int idx;
	unsigned int size;
	unsigned int size32;
	struct fifo_reader *preader = pcons->preader;

	size = fifo_reader_get_first(preader, (void **)&block);
	if (size < sizeof(uint32_t))
		return 0;

	// Get number of ints to produce
	size32 = size / sizeof(uint32_t);
	if (size32 > (pcons->count32 - pcons->actual32))
		size32 = pcons->count32 - pcons->actual32;

	// Read from data block
	for (idx = 0; idx < size32; idx++)
	{
		if (block[idx] != pcons->actual32++) {
			pcons->error = 1;
			return 0;
		}
	}

	// Notify writer of free block
	fifo_reader_free(preader);
	fifo_reader_wakeup_writer(preader, 0);

	return size;
}

static inline unsigned int testconsumer_consume(struct testconsumer *pcons)
{
	unsigned int size = 0;
	unsigned int total_size = 0;

	while((size = testconsumer_consume_one(pcons)) > 0)
		total_size += size;

	// Force wakeup signal to the writer
	fifo_reader_wakeup_writer(pcons->preader, 1);

	return total_size;
}

#ifdef __cplusplus
};
#endif

#endif
