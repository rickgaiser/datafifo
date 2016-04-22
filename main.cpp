#include <iostream>
#include <chrono>

#include "testcommon.h"


using std::chrono::time_point;
using std::chrono::system_clock;
using std::chrono::milliseconds;


void test01();
void test02();
void test03();


//---------------------------------------------------------------------------
int
main()
{
	time_point<system_clock> tstart, tend;

#if 1
	std::cout<<"datafifo test01"<<std::endl;
	tstart = system_clock::now();
	test01();
	tend = system_clock::now();
	std::cout<<"  - Time: "<<std::chrono::duration_cast<milliseconds>(tend - tstart).count()<<"ms"<<std::endl<<std::endl;
#endif

#if 1
	std::cout<<"datafifo test02"<<std::endl;
	tstart = system_clock::now();
	test02();
	tend = system_clock::now();
	std::cout<<"  - Time: "<<std::chrono::duration_cast<milliseconds>(tend - tstart).count()<<"ms"<<std::endl<<std::endl;
#endif

#if 1
	std::cout<<"datafifo test03"<<std::endl;
	tstart = system_clock::now();
	test03();
	tend = system_clock::now();
	std::cout<<"  - Time: "<<std::chrono::duration_cast<milliseconds>(tend - tstart).count()<<"ms"<<std::endl<<std::endl;
#endif

	return 0;
}
