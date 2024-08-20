
#include "test_cpp.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

class TestCPP
{
  public:
    TestCPP()
    {
        printf("\n\tTestCPP::TestCPP() - constructor\n");
    }
    ~TestCPP()
    {
        printf("\n\tTestCPP::~TestCPP() - destructor\n");
    }
    uint8_t _meg_buffer[1024 * 1024];
};

void *test_cpp_new(void)
{
    TestCPP *tmp = new TestCPP();
    return (void *)tmp;
}

void test_cpp_delete(void *ptr)
{
    delete (TestCPP *)ptr;
}