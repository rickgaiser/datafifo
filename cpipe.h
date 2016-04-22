#ifndef CPIPE_H
#define CPIPE_H


#include <thread>
#include <mutex>
#include <condition_variable>
#include <string>

class CPipe
{
public:
	CPipe(const char * sName, struct fifo_pipe * ppipe);
	~CPipe();

	static void wakeup(void * arg);

private:
	void mainloop();

private:
	std::string sName;

	std::thread thr;
	volatile bool bExit;

	std::mutex mutex;
	std::condition_variable	cv;
	volatile int wake_count;

	struct fifo_pipe * ppipe;
};


#endif // CPIPE_H
