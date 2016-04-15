#include <iostream>

#include "fifo.h"
#include "fifo_reader.h"
#include "fifo_writer.h"

#include "testproducer.h"
#include "testconsumer.h"


#define TEST_COUNT (1024*1024*1024)


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
	testconsumer_init(&cons, &fifo_reader, TEST_COUNT);
	testproducer_init(&prod, &fifo_writer, TEST_COUNT);

	while ( (testproducer_done (&prod) == 0) &&
		(testconsumer_done (&cons) == 0) &&
		(testconsumer_error(&cons) == 0))
	{
		// Fill the fifo to the IOP
		testproducer_produce(&prod);
		// Empty the fifo from the IOP
		testconsumer_consume(&cons);
	}

	std::cout<<"Done, produced: "<<((prod.actual32*4)/(1024*1024))<<"MiB"
	         <<    ", consumed: "<<((cons.actual32*4)/(1024*1024))<<"MiB"
	         <<std::endl;

	// Cleanup
	delete[] databuffer;
}
