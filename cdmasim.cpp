#include <string.h> // memcpy
#include <iostream>

#include "cdmasim.h"


CDMASim dma1("DMA1");


//---------------------------------------------------------------------------
CDMASim::CDMASim(const char * sName)
 : sName(sName)
 , thr(&CDMASim::mainloop, this)
 , bExit(false)
{
}

//---------------------------------------------------------------------------
CDMASim::~CDMASim()
{
	std::cout<<sName<<" stopping"<<std::endl;
	{
		std::unique_lock<std::mutex> locker(mutex);
		bExit = true;
		cv.notify_all();
	}
	std::cout<<sName<<" waiting"<<std::endl;
	thr.join();
	std::cout<<sName<<" stopped"<<std::endl;
}

//---------------------------------------------------------------------------
void
CDMASim::put(void *dst, const void *src, size_t size, fp_dma_completion_callback fp_compl, void * fp_compl_arg)
{
	SDMAOperation * pdmaop = new SDMAOperation;

	pdmaop->dst = dst;
	pdmaop->src = src;
	pdmaop->size = size;
	pdmaop->fp_compl = fp_compl;
	pdmaop->fp_compl_arg = fp_compl_arg;

	put(pdmaop);
}

//---------------------------------------------------------------------------
void
CDMASim::mainloop()
{
	std::cout<<sName<<" running"<<std::endl;

	while(true)
	{
		SDMAOperation * pdmaop = get();
		if (pdmaop == NULL)
			break;

		memcpy(pdmaop->dst, pdmaop->src, pdmaop->size);
		if (pdmaop->fp_compl != NULL)
			pdmaop->fp_compl(pdmaop->fp_compl_arg);
	}
}

//---------------------------------------------------------------------------
SDMAOperation *
CDMASim::get()
{
	SDMAOperation * pdmaop;

	std::unique_lock<std::mutex> locker(mutex);
	cv.wait(locker, [this]{return (!queue.empty() || bExit);});

	if (bExit)
		return NULL;

	pdmaop = queue.front();
	queue.pop_front();

	return pdmaop;
}

//---------------------------------------------------------------------------
void
CDMASim::put(SDMAOperation * pdmaop)
{
	std::unique_lock<std::mutex> locker(mutex);

	queue.push_back(pdmaop);

	cv.notify_one();
}
