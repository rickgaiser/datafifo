#include <iostream>

#include "fifo.h"
#include "fifo_reader.h"
#include "fifo_writer.h"
#include "fifo_pipe.h"

#include "testcommon.h"
#include "cdmasim.h"


#define TEST_COUNT (1024*1024*1024)


/*
 * Datapath in this test:
 *   1 - prod			(testproducer)
 *   2 - fifo1_writer		(fifo_writer)
 *   3 - fifo1			(fifo)
 *   4 - fifo1_reader		(fifo_reader)
 *   5 - fifo_pipe12		(fifo_pipe)
 *   6 - fifo2_writer		(fifo_writer)
 *   7 - fifo2			(fifo)
 *   8 - fifo2_reader		(fifo_reader)
 *   9 - cons			(testconsumer)
 */


//---------------------------------------------------------------------------
void
fifo_pipe_fill(struct fifo_pipe * ppipe)
{
	while(fifo_pipe_transfer(ppipe) > 1)
		;
}

//---------------------------------------------------------------------------
static void fifo_pipe_transfer_async_complete(void * arg)
{
	struct fifo_pipe_transfer *ptransfer = (struct fifo_pipe_transfer *)arg;
	fifo_pipe_transfer_commit(ptransfer->ppipe, ptransfer);
}

//---------------------------------------------------------------------------
static void fifo_pipe_transfer_async(struct fifo_pipe_transfer *ptransfer)
{
	dma1.put(ptransfer->dst, ptransfer->src, ptransfer->size, fifo_pipe_transfer_async_complete, ptransfer);
}

//---------------------------------------------------------------------------
std::mutex pipe_mutex;
std::condition_variable	pipe_cv;
volatile bool bPipeExit;
//---------------------------------------------------------------------------
static void thr_fifo_pipe_wait()
{
	std::unique_lock<std::mutex> locker(pipe_mutex);
	pipe_cv.wait(locker);
}

//---------------------------------------------------------------------------
static void thr_fifo_pipe_wakeup(void * arg)
{
	std::unique_lock<std::mutex> locker(pipe_mutex);
	pipe_cv.notify_one();
}

//---------------------------------------------------------------------------
static void thr_fifo_pipe(struct fifo_pipe * ppipe)
{
	std::cout<<"Pipe running"<<std::endl;

	while(bPipeExit == false) {
		fifo_pipe_fill(ppipe);
		thr_fifo_pipe_wait();
	}

	std::cout<<"Pipe stopping"<<std::endl;
}

//---------------------------------------------------------------------------
void
test02()
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

	// Init fifo pipe
	fifo_pipe_init(&fifo_pipe12, &fifo1_reader, &fifo2_writer);
	fifo_pipe12.fp_transfer = fifo_pipe_transfer_async;

	// Setup callbacks to activate the pipe
	fifo1_writer.do_wakeup_reader = thr_fifo_pipe_wakeup;
	fifo1_writer.do_wakeup_reader_arg = &fifo_pipe12;
	fifo2_reader.do_wakeup_writer = thr_fifo_pipe_wakeup;
	fifo2_reader.do_wakeup_writer_arg = &fifo_pipe12;

	// Init test
	testproducer_init(&prod, &fifo1_writer, TEST_COUNT);
	testconsumer_init(&cons, &fifo2_reader, TEST_COUNT);

	// Start pipe thread
	bPipeExit = false;
	std::thread thr_pipe(thr_fifo_pipe, &fifo_pipe12);

	// Run the test
	run_test(&prod, &cons);

	// Stop the pipe thread
	bPipeExit = true;
	thr_fifo_pipe_wakeup(&fifo_pipe12);
	thr_pipe.join();

	// Cleanup
	delete[] databuffer1;
	delete[] databuffer2;
}
