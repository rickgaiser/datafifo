#ifndef __FIFO_H
#define __FIFO_H

/**
 * @file fifo.h
 * @brief Main fifo object.
 *
 * The fifo is a block of data, containing:
 * - a header (fifo_header)
 * - a buffer descriptor ring (bdring)
 * - a data ring
 *
 * The fifo_writer is needed to write data into the fifo
 * The fifo_reader is needed to read data out of the fifo
 * The fifo_pipe can connect a fifo_reader with a fifo_writer
 */

#include "linux_port.h"
#include "bdring.h"

#ifdef __cplusplus
extern "C" {
#endif

#define FIFO_SIZE		(64*1024)
#define FIFO_BD_COUNT		(128)

/**
 * @brief Describes the fifo
 *
 * The fifo is a simple block of data, this header should be at the beginning.
 */
struct fifo_header
{
	uint16_t bd_count;
	uint16_t datasize;
	uint32_t reader_status;
	uint32_t writer_status;
	uint32_t align;
} __attribute__ ((packed));
/* Reader status flags */
#define RD_STS_WAITING		(1<<0)
/* Writer status flags */
#define WR_STS_WAITING		(1<<0)

/**
 * @brief fifo struct used by the fifo_reader and fifo_writer
 */
struct fifo
{
	volatile struct fifo_header *pheader;
	struct bdring bdr;
	uint8_t *pdata;
};

/**
 * @brief Buffer Descriptor as it is used by the fifo
 *
 * The buffer descriptor had 31 bits of user defined data. This struct defines
 * how the fifo uses it. It has:
 * - An offset into the data ring buffer, in bytes/4
 * - A size in bytes
 */
#define FIFO_BD_OFFSET_BITS	16 /* Max 256KiB - 1  (NOTE: offset is right shifted >> 2)*/
#define FIFO_BD_SIZE_BITS	12 /* Max   4KiB - 1 */
#define FIFO_BD_SPARE_BITS	(31-FIFO_BD_OFFSET_BITS-FIFO_BD_SIZE_BITS)
#define FIFO_BLOCK_MAX_SIZE     ((1<<FIFO_BD_SIZE_BITS)-1)
struct fifo_bd
{
	union {
		uint32_t data;
		struct {
			uint32_t offset : FIFO_BD_OFFSET_BITS;
			uint32_t size   : FIFO_BD_SIZE_BITS;
			uint32_t spare  : FIFO_BD_SPARE_BITS;
			uint32_t used   : 1;
		} __attribute__ ((packed));
	};
} __attribute__ ((packed));

/**
 * @brief Initialize the fifo struct
 */
static inline void fifo_init_create(struct fifo *pfifo, void *pfifodata, unsigned int fifosize, unsigned int bd_count, unsigned int align)
{
	uint8_t *pbdring;
	unsigned int header_size = sizeof(struct fifo_header);
	unsigned int bdring_size = sizeof(struct bd) * bd_count;
	unsigned int datasize;
	size_t offset;

	/* Align needs to be at least 4 bytes, becouse the bdring right shifts the offset */
	align = (align < 4) ? 4 : align;

	/* align fifo */
	offset = (size_t)pfifodata & (align-1);
	pfifodata = (void *)(((size_t)pfifodata + (align-1)) & ~(align-1));
	fifosize -= offset;

	/* align bdring */
	header_size = (header_size + (align-1)) & ~(align-1);

	/* align data */
	bdring_size = (bdring_size + (align-1)) & ~(align-1);

	/* whatever is left is our fifo data, at an aligned starting position */
	datasize = fifosize - header_size - bdring_size;

	/* remove unused data at the end */
	datasize = datasize & ~(align-1);

	/* header */
	pfifo->pheader = (struct fifo_header *)pfifodata;
	pfifo->pheader->bd_count	= bd_count;
	pfifo->pheader->datasize	= datasize;
	pfifo->pheader->reader_status	= 0;
	pfifo->pheader->writer_status	= 0;
	pfifo->pheader->align		= align;

	/* bdring */
	pbdring = (uint8_t *)pfifodata + sizeof(struct fifo_header);
	bdring_init(&pfifo->bdr, (volatile void *)pbdring, bd_count);
	bdring_clear(&pfifo->bdr);

	/* data */
	pfifo->pdata = pbdring + bdring_size;
}

/**
 * @brief Initialize the fifo struct
 */
static inline void fifo_init_connect(struct fifo *pfifo, void *pfifodata)
{
	#if 0 // FIXME: where does the header start?
	uint8_t *pbdring;

	/* header */
	pfifo->pheader = (struct fifo_header *)pfifodata;

	/* bdring */
	pbdring = (uint8_t *)pfifodata + sizeof(struct fifo_header);
	bdring_init(&pfifo->bdr, (volatile void *)pbdring, pfifo->pheader->bd_count);

	/* data */
	pfifo->pdata = pbdring + bdring_size;
	#endif
}

#ifdef __cplusplus
};
#endif

#endif
