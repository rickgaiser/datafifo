#include <string.h> // memcpy
#include <iostream>

#include "cdmasim.h"


CDMASim dma_ee ("DMA_EE");
CDMASim dma_iop("DMA_IOP");


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
	{
		std::unique_lock<std::mutex> locker(mutex);
		bExit = true;
		cv.notify_all();
	}
	thr.join();
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

	while(bExit == false)
	{
		SDMAOperation * pdmaop = get();
		if (pdmaop == NULL)
			break;

		memcpy(pdmaop->dst, pdmaop->src, pdmaop->size);
		if (pdmaop->fp_compl != NULL)
			pdmaop->fp_compl(pdmaop->fp_compl_arg);

		delete pdmaop;
	}

	std::cout<<sName<<" stopping"<<std::endl;
}

//---------------------------------------------------------------------------
SDMAOperation *
CDMASim::get()
{
	SDMAOperation * pdmaop;

	std::unique_lock<std::mutex> locker(mutex);
	cv.wait(locker, [this]{return (!queue.empty() || (queue.empty() && bExit));});

	if (queue.empty() && bExit)
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

	if (bExit)
		return;

	queue.push_back(pdmaop);

	cv.notify_one();
}
