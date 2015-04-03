/**
 * Check predefined macro values
 */
//#include <math.h>

#define __CONCAT(x, y) x ## y

#define __MATHCALL(function,suffix, args)	\
  __MATHDECL (_Mdouble_,function,suffix, args)

/*
#define __MATHDECL(type, function,suffix, args) \
  __MATHDECL_1(type, function,suffix, args); \
  __MATHDECL_1(type, __CONCAT(__,function),suffix, args)
*/
#define __MATHDECL(type, function,suffix, args) \
  __MATHDECL_1(type, __CONCAT(__,function),suffix, args)

#define __MATHDECL_1(type, function,suffix, args) \
  extern type __MATH_PRECNAME(function,suffix) args __THROW

#define _Mdouble_		double
#define __MATH_PRECNAME(name,r)	__CONCAT(name,r)

__MATHCALL (acos,, (_Mdouble_ __x));

/*
#define FOO(a) a
#define BAR(a) int FOO(__CONCAT(__, a))
BAR(yolo);
*/

int main() {
    __x86_64__;
    return 0;
}
