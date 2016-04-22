#include <iostream>

#include "cpipe.h"

#include "fifo_pipe.h"


//---------------------------------------------------------------------------
CPipe::CPipe(const char * sName, struct fifo_pipe * ppipe)
 : sName(sName)
 , thr(&CPipe::mainloop, this)
 , bExit(false)
 , wake_count(0)
 , ppipe(ppipe)
{
}

//---------------------------------------------------------------------------
CPipe::~CPipe()
{
	bExit = true;
	CPipe::wakeup(this);
	thr.join();
}

//---------------------------------------------------------------------------
void
CPipe::mainloop()
{
	std::cout<<sName<<" running"<<std::endl;

	while(bExit == false)
	{
		// Fill the pipe
		while (fifo_pipe_transfer(ppipe) > 1);

		// Wait for wake-ups
		{
			std::unique_lock<std::mutex> locker(mutex);
			cv.wait(locker, [this]{ return wake_count > 0; });
			wake_count--;
		}
	}

	std::cout<<sName<<" stopping"<<std::endl;
}

//---------------------------------------------------------------------------
void
CPipe::wakeup(void * arg)
{
	CPipe * pcpipe = (CPipe *)arg;

	std::unique_lock<std::mutex> locker(pcpipe->mutex);
	pcpipe->wake_count++;
	pcpipe->cv.notify_one();
}
