#include <iostream>
#include <chrono>

#include "testcommon.h"


using std::chrono::time_point;
using std::chrono::system_clock;
using std::chrono::milliseconds;


void test01();
void test02();


//---------------------------------------------------------------------------
int
main()
{
	time_point<system_clock> tstart, tend;

	std::cout<<"datafifo test01"<<std::endl;
	tstart = system_clock::now();
	test01();
	tend = system_clock::now();
	std::cout<<"  - Time: "<<std::chrono::duration_cast<milliseconds>(tend - tstart).count()<<"ms"<<std::endl<<std::endl;

	std::cout<<"datafifo test02"<<std::endl;
	tstart = system_clock::now();
	test02();
	tend = system_clock::now();
	std::cout<<"  - Time: "<<std::chrono::duration_cast<milliseconds>(tend - tstart).count()<<"ms"<<std::endl<<std::endl;

	return 0;
}
