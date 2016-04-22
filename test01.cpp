#include "fifo.h"
#include "fifo_reader.h"
#include "fifo_writer.h"

#include "testcommon.h"


#define TEST_COUNT (1024*1024*1024)


/*
 * Datapath in this test:
 *   1 - prod			(testproducer)	thread
 *   2 - fifo_writer		(fifo_writer)
 *   3 - fifo			(fifo)
 *   4 - fifo_reader		(fifo_reader)
 *   5 - cons			(testconsumer)	thread
 */


//---------------------------------------------------------------------------
void
test01()
{
	uint8_t			*databuffer;		// fifo data
	struct fifo		fifo;			// fifo object
	struct fifo_writer	fifo_writer;		// fifo writer object
	struct fifo_reader	fifo_reader;		// fifo reader object
	struct testconsumer	cons;
	struct testproducer	prod;

	// Init fifo
	databuffer = new uint8_t[FIFO_SIZE];
	fifo_init_create(&fifo, databuffer, FIFO_SIZE, FIFO_BD_COUNT, 16);
	fifo_writer_init(&fifo_writer, &fifo);
	fifo_reader_init(&fifo_reader, &fifo);

	// Init test
	testproducer_init(&prod, &fifo_writer, TEST_COUNT);
	testconsumer_init(&cons, &fifo_reader, TEST_COUNT);

	run_test(&prod, &cons);

	// Cleanup
	delete[] databuffer;
}
