#ifndef TESTCOMMON_H
#define TESTCOMMON_H


#include "testproducer.h"
#include "testconsumer.h"


#define TEST_COUNT (1024*1024*1024)


void run_test(struct testproducer * pprod, struct testconsumer * pcons);


#endif // TESTCOMMON_H
