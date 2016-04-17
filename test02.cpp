#include <iostream>

#include "fifo.h"
#include "fifo_reader.h"
#include "fifo_writer.h"
#include "fifo_pipe.h"

#include "testproducer.h"
#include "testconsumer.h"


#define TEST_COUNT (1024*1024*1024)


void run_test(struct testproducer * pprod, struct testconsumer * pcons);


/*
 * Datapath in this test:
 *   1 - prod			(testproducer)
 *   2 - fifo_writer1		(fifo_writer)
 *   3 - fifo1			(fifo)
 *   4 - fifo_reader1		(fifo_reader)
 *   5 - fifo_pipe12		(fifo_pipe)
 *   6 - fifo_writer2		(fifo_writer)
 *   7 - fifo2			(fifo)
 *   8 - fifo_reader2		(fifo_reader)
 *   9 - cons			(testconsumer)
 */


//---------------------------------------------------------------------------
void
do_wakeup_pipe(void *arg)
{
	struct fifo_pipe * ppipe = (struct fifo_pipe *)arg;

	while(fifo_pipe_transfer(ppipe) > 1)
		;
}

//---------------------------------------------------------------------------
void
test02()
{
	uint8_t			*databuffer1;		// fifo data
	struct fifo		fifo1;			// fifo object
	struct fifo_writer	fifo_writer1;		// fifo writer object
	struct fifo_reader	fifo_reader1;		// fifo reader object

	uint8_t			*databuffer2;		// fifo data
	struct fifo		fifo2;			// fifo object
	struct fifo_writer	fifo_writer2;		// fifo writer object
	struct fifo_reader	fifo_reader2;		// fifo reader object

	struct fifo_pipe	fifo_pipe12;		// fifo pipe object from fifo1 -> fifo2

	struct testconsumer	cons;
	struct testproducer	prod;

	// Init fifo 1
	databuffer1 = new uint8_t[FIFO_SIZE];
	fifo_init_create(&fifo1, databuffer1, FIFO_SIZE, FIFO_BD_COUNT, 16);
	fifo_writer_init(&fifo_writer1, &fifo1);
	fifo_reader_init(&fifo_reader1, &fifo1);

	// Init fifo 2
	databuffer2 = new uint8_t[FIFO_SIZE];
	fifo_init_create(&fifo2, databuffer2, FIFO_SIZE, FIFO_BD_COUNT, 16);
	fifo_writer_init(&fifo_writer2, &fifo2);
	fifo_reader_init(&fifo_reader2, &fifo2);

	// Init fifo pipe
	fifo_pipe_init(&fifo_pipe12, &fifo_reader1, &fifo_writer2);
	fifo_writer1.do_wakeup_reader = do_wakeup_pipe;
	fifo_writer1.do_wakeup_reader_arg = &fifo_pipe12;
	//fifo_reader2.do_wakeup_writer = do_wakeup_pipe;
	//fifo_reader2.do_wakeup_writer_arg = &fifo_pipe12;

	// Init test
	testproducer_init(&prod, &fifo_writer1, TEST_COUNT);
	testconsumer_init(&cons, &fifo_reader2, TEST_COUNT);

	run_test(&prod, &cons);

	// Cleanup
	delete[] databuffer1;
	delete[] databuffer2;
}
