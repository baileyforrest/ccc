#ifndef _RUNTIME_H_
#define _RUNTIME_H_

#include <stdbool.h>
#include <stdlib.h>
#include <assert.h>

#define alloc(type) calloc(1, sizeof(type))
#define alloc_array(type, num) calloc(num, sizeof(type))
typedef char string[];

#endif /* _RUNTIME_H_ */
