#ifndef CDMASIM_H
#define CDMASIM_H


#include <thread>
#include <mutex>
#include <condition_variable>
#include <list>
#include <string>

typedef void (*fp_dma_completion_callback)(void * arg);

struct SDMAOperation
{
	void *dst;
	const void *src;
	size_t size;

	fp_dma_completion_callback fp_compl;
	void * fp_compl_arg;
};

class CDMASim
{
public:
	CDMASim(const char * sName = "DMA engine");
	~CDMASim();

	void put(void *dst, const void *src, size_t size, fp_dma_completion_callback fp_compl = NULL, void * compl_arg = NULL);

private:
	void mainloop();
	void put(SDMAOperation * pdmaop);
	SDMAOperation * get();

private:
	std::string sName;

	std::thread thr;
	volatile bool bExit;

	std::list<SDMAOperation *> queue;
	std::mutex mutex;
	std::condition_variable	cv;
};


extern CDMASim dma1;


#endif // CDMASIM_H
