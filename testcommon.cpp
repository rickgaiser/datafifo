#include <iostream>
#include <thread>

#include "testcommon.h"


static bool bError;


//---------------------------------------------------------------------------
void
print_data_size(unsigned int size)
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
thr_produce(struct testproducer * pprod)
{
	std::cout<<"producer running"<<std::endl;

	while ((testproducer_done(pprod) == 0) && (bError == false)) {
		testproducer_produce(pprod);
	}

	std::cout<<"producer stopping"<<std::endl;
}

//---------------------------------------------------------------------------
void
thr_consume(struct testconsumer * pcons)
{
	std::cout<<"consumer running"<<std::endl;

	while ((testconsumer_done(pcons) == 0) && (bError == false)) {
		testconsumer_consume(pcons);
		bError = testconsumer_error(pcons) != 0;
	}

	std::cout<<"consumer stopping"<<std::endl;
}

//---------------------------------------------------------------------------
void
run_test(struct testproducer * pprod, struct testconsumer * pcons)
{
	bError = false;

	std::thread tProd(thr_produce, pprod);
	std::thread tCons(thr_consume, pcons);

	tProd.join();
	tCons.join();

	std::cout<<"Done, produced: ";
	print_data_size(pprod->actual32*4);
	std::cout<<    ", consumed: ";
	print_data_size(pcons->actual32*4);
	if (bError == true)
		std::cout<<", ERROR@nr"<<pcons->actual32;
	std::cout<<std::endl;
}
