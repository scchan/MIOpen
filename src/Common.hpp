#ifndef _MLOPEN_COMMON_HPP_
#define _MLOPEN_COMMON_HPP_

#include "MLOpen.h"

#if MLOpen_BACKEND_OPENCL

typedef cl_mem Data_t;

inline Data_t DataCast(void *p) {
	return (Data_t)p;
}

inline const Data_t DataCast(const void *p) {
	return (Data_t)p;
}

#elif MLOpen_BACKEND_HIP

typedef void * Data_t;

inline Data_t DataCast(void *p) {
	return p;
}

inline const Data_t DataCast(const void *p) {
	return p;
}
#endif // OpenCL vs HIP
#endif // _MLOPEN_COMMON_HPP_
