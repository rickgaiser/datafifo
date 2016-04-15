#ifndef __BDRING_H
#define __BDRING_H

/*!
 * @file bdring.h
 * @brief Buffer descriptor ring (bdring)
 *
 * Designed as a simple ring buffer between two devices:
 * - no locks
 * - no atomics
 * - fixed size
 * - one writer, one reader
 */

#include "linux_port.h"

#ifdef __cplusplus
extern "C" {
#endif

/*!
 * @brief Buffer descriptor (bd)
 *
 * Simple 32bit value, so it can be written and read atomically.
 *
 * Writing
 * Setting the used bit will signal the reader of new data. The other 31 bits
 * contain user defined data. The entire BD must be written in one atomic
 * operation. Use the "data" to do this.
 *
 * Reading
 * Once the data has been read, the reader will clear the used bit to signal
 * the BD is free. After the bit has been cleared, the writer can write to the
 * BD again.
 */
struct bd
{
	union {
		uint32_t data;
		struct {
			uint32_t userdata:31;
			uint32_t used:1;
		} __attribute__ ((packed));
	};
} __attribute__ ((packed));
#define BD_USED		((uint32_t)(1<<31))
#define BD_USERDATA	(~BD_USED)

/**
 * @brief Buffer descriptor ring (bdring)
 */
struct bdring {
	volatile struct bd *pbd;
	unsigned int count;
};

/**
 * @brief Initialize the bdring struct
 */
static inline void bdring_init(struct bdring *pbdr, volatile void *pdata, unsigned int count)
{
	pbdr->pbd = (volatile struct bd *)pdata;
	pbdr->count = count;
}

static inline void bdring_clear(struct bdring *pbdr)
{
	unsigned int idx;
	for (idx = 0; idx < pbdr->count; idx++)
		pbdr->pbd[idx].data = 0;
}

static inline int bdring_bd_is_used(struct bdring *pbdr, unsigned int idx)
{
	uint32_t data = pbdr->pbd[idx].data;

	return data >= BD_USED;
}

static inline int bdring_bd_get(struct bdring *pbdr, unsigned int idx, uint32_t *pdata)
{
	uint32_t data = pbdr->pbd[idx].data;

	*pdata = data & BD_USERDATA;

	return data >= BD_USED;
}

static inline void bdring_bd_put(struct bdring *pbdr, unsigned int idx, uint32_t data)
{
	pbdr->pbd[idx].data = data | BD_USED;
}

/**
 * @brief Read from a bdring
 */
struct bdring_reader {
	struct bdring *pbdr;
	unsigned int index;
	unsigned int mask;
};

/**
 * @brief Initialize the bdring_reader struct
 */
static inline void bdring_reader_init(struct bdring_reader *pbdrr, struct bdring *pbdr)
{
	pbdrr->pbdr	= pbdr;
	pbdrr->index	= 0;
	pbdrr->mask	= pbdr->count - 1;
}

static inline int bdring_reader_is_empty(struct bdring_reader *pbdrr)
{
	return bdring_bd_is_used(pbdrr->pbdr, pbdrr->index) == 0;
}

static inline unsigned int bdring_reader_next(struct bdring_reader *pbdrr, unsigned int index)
{
	return ((index + 1) & pbdrr->mask);
}

static inline int bdring_reader_get(struct bdring_reader *pbdrr, uint32_t *pdata)
{
	return bdring_bd_get(pbdrr->pbdr, pbdrr->index, pdata);
}

static inline void bdring_reader_pop(struct bdring_reader *pbdrr)
{
	pbdrr->pbdr->pbd[pbdrr->index].data = 0;
	pbdrr->index = bdring_reader_next(pbdrr, pbdrr->index);
}

/**
 * @brief Write to a bdring
 */
struct bdring_writer {
	struct bdring *pbdr;
	unsigned int index;
	unsigned int mask;
};

/**
 * @brief Initialize the bdring_writer struct
 */
static inline void bdring_writer_init(struct bdring_writer *pbdrw, struct bdring *pbdr)
{
	pbdrw->pbdr	= pbdr;
	pbdrw->index	= 0;
	pbdrw->mask	= pbdr->count - 1;
}

static inline int bdring_writer_is_full(struct bdring_writer *pbdrw)
{
	return bdring_bd_is_used(pbdrw->pbdr, pbdrw->index);
}

static inline unsigned int bdring_writer_next(struct bdring_writer *pbdrw, unsigned int index)
{
	return ((index + 1) & pbdrw->mask);
}

static inline void bdring_writer_put(struct bdring_writer *pbdrw, uint32_t data)
{
	bdring_bd_put(pbdrw->pbdr, pbdrw->index, data);
	pbdrw->index = bdring_writer_next(pbdrw, pbdrw->index);
}

#ifdef __cplusplus
};
#endif

#endif
