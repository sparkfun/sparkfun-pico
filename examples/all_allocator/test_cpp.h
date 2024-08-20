

#ifndef __TEST_CPP_H__
#define __TEST_CPP_H__

#ifdef __cplusplus
extern "C"
{
#endif
    void *test_cpp_new(void);
    void test_cpp_delete(void *ptr);

#ifdef __cplusplus
}
#endif
#endif // __TEST_CPP_H__