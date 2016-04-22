#include <iostream>

#include "fifo.h"
#include "fifo_reader.h"
#include "fifo_writer.h"
#include "fifo_pipe.h"

#include "testcommon.h"
#include "cdmasim.h"
#include "cpipe.h"


/*
 * Test 03: Simulate a looptest on the Playstation 2
 *
 * Datapath in this test:
 * EE:
 *   1 - prod			(testproducer)	thread producing data
 *   2 - fifo1_writer		(fifo_writer)
 *   3 - fifo1			(fifo)
 *   4 - fifo1_reader		(fifo_reader)
 *   5 - fifo_pipe12		(fifo_pipe)	thread kicking DMA controller
 *   6 - fifo2_writer		(fifo_writer)
 * IOP:
 *   7 - fifo2			(fifo)
 *   8 - fifo2_reader		(fifo_reader)
 *   9 - fifo_pipe23		(fifo_pipe)	thread kicking DMA controller
 *  10 - fifo3_writer		(fifo_writer)
 * EE:
 *  11 - fifo3			(fifo)
 *  12 - fifo3_reader		(fifo_reader)
 *  13 - cons			(testconsumer)	thread consuming data
 */


//---------------------------------------------------------------------------
static void dma_transfer_complete(void * arg)
{
	struct fifo_pipe_transfer *ptransfer = (struct fifo_pipe_transfer *)arg;
	fifo_pipe_transfer_commit(ptransfer->ppipe, ptransfer);
}

//---------------------------------------------------------------------------
static void dma_transfer_ee(struct fifo_pipe_transfer *ptransfer)
{
	dma_ee.put(ptransfer->dst, ptransfer->src, ptransfer->size, dma_transfer_complete, ptransfer);
}

//---------------------------------------------------------------------------
static void dma_transfer_iop(struct fifo_pipe_transfer *ptransfer)
{
	dma_iop.put(ptransfer->dst, ptransfer->src, ptransfer->size, dma_transfer_complete, ptransfer);
}

//---------------------------------------------------------------------------
void
test03()
{
	uint8_t			*databuffer1;		// fifo data
	struct fifo		fifo1;			// fifo object
	struct fifo_writer	fifo1_writer;		// fifo writer object
	struct fifo_reader	fifo1_reader;		// fifo reader object

	uint8_t			*databuffer2;		// fifo data
	struct fifo		fifo2;			// fifo object
	struct fifo_writer	fifo2_writer;		// fifo writer object
	struct fifo_reader	fifo2_reader;		// fifo reader object

	struct fifo_pipe	fifo_pipe12;		// fifo pipe object from fifo1 -> fifo2

	uint8_t			*databuffer3;		// fifo data
	struct fifo		fifo3;			// fifo object
	struct fifo_writer	fifo3_writer;		// fifo writer object
	struct fifo_reader	fifo3_reader;		// fifo reader object

	struct fifo_pipe	fifo_pipe23;		// fifo pipe object from fifo2 -> fifo3

	struct testconsumer	cons;
	struct testproducer	prod;

	// Init fifo 1
	databuffer1 = new uint8_t[FIFO_SIZE];
	fifo_init_create(&fifo1, databuffer1, FIFO_SIZE, FIFO_BD_COUNT, 16);
	fifo_writer_init(&fifo1_writer, &fifo1);
	fifo_reader_init(&fifo1_reader, &fifo1);

	// Init fifo 2
	databuffer2 = new uint8_t[FIFO_SIZE];
	fifo_init_create(&fifo2, databuffer2, FIFO_SIZE, FIFO_BD_COUNT, 16);
	fifo_writer_init(&fifo2_writer, &fifo2);
	fifo_reader_init(&fifo2_reader, &fifo2);

	// Init fifo pipe 12
	fifo_pipe_init(&fifo_pipe12, &fifo1_reader, &fifo2_writer);
	fifo_pipe12.fp_transfer = dma_transfer_ee;

	// Create and hookup thread for pipe 12
	CPipe cpipe12("Pipe12", &fifo_pipe12);
	fifo_writer_set_wakeup_handler(&fifo1_writer, CPipe::wakeup, &cpipe12);
	fifo_reader_set_wakeup_handler(&fifo2_reader, CPipe::wakeup, &cpipe12);

	// Init fifo 3
	databuffer3 = new uint8_t[FIFO_SIZE];
	fifo_init_create(&fifo3, databuffer3, FIFO_SIZE, FIFO_BD_COUNT, 16);
	fifo_writer_init(&fifo3_writer, &fifo3);
	fifo_reader_init(&fifo3_reader, &fifo3);

	// Init fifo pipe 23
	fifo_pipe_init(&fifo_pipe23, &fifo2_reader, &fifo3_writer);
	fifo_pipe23.fp_transfer = dma_transfer_iop;

	// Create and hookup thread for pipe 23
	CPipe cpipe23("Pipe23", &fifo_pipe23);
	fifo_writer_set_wakeup_handler(&fifo2_writer, CPipe::wakeup, &cpipe23);
	fifo_reader_set_wakeup_handler(&fifo3_reader, CPipe::wakeup, &cpipe23);

	// Init test
	testproducer_init(&prod, &fifo1_writer, TEST_COUNT);
	testconsumer_init(&cons, &fifo3_reader, TEST_COUNT);

	// Run the test
	run_test(&prod, &cons);

	// Cleanup
	delete[] databuffer1;
	delete[] databuffer2;
	delete[] databuffer3;
}
