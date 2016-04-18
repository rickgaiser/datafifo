#include <iostream>

#include "testcommon.h"


//---------------------------------------------------------------------------
void print_data_size(unsigned int size)
{
	/*if(size > (5*1024*1024*1024))
		std::cout<<(size/(1024*1024*1024))<<"GiB";
	else*/ if(size > (5*1024*1024))
		std::cout<<(size/(1024*1024))<<"MiB";
	else if(size > (5*1024))
		std::cout<<(size/(1024))<<"KiB";
	else
		std::cout<<size<<"B";
}

//---------------------------------------------------------------------------
void
run_test(struct testproducer * pprod, struct testconsumer * pcons)
{
	while (((testproducer_done (pprod) == 0) || (testconsumer_done (pcons) == 0)) &&
		(testconsumer_error(pcons) == 0))
	{
		// Fill the fifo to the IOP
		testproducer_produce(pprod);
		// Empty the fifo from the IOP
		testconsumer_consume(pcons);
	}

	std::cout<<"Done, produced: ";
	print_data_size(pprod->actual32*4);
	std::cout<<    ", consumed: ";
	print_data_size(pcons->actual32*4);
	if (testconsumer_error(pcons) != 0)
		std::cout<<", ERROR!";
	std::cout<<std::endl;
}
