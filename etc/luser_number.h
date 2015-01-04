/* luser_number.h -- number type configuration for Lua core
*
* #define LUA_USER_H to this file and #define one of USE_* below
*/

#ifdef USE_DOUBLE
#define LUA_NUMBER		double
#define LUA_NUMBER_SCAN		"%lf"
#define LUA_NUMBER_FMT		"%.14g"
#endif

#ifdef USE_FLOAT
#define LUA_NUMBER		float
#define LUA_NUMBER_SCAN		"%f"
#define LUA_NUMBER_FMT		"%.5g"
#endif

#ifdef USE_LONG
#define LUA_NUMBER		long
#define LUA_NUMBER_SCAN		"%ld"
#define LUA_NUMBER_FMT		"%ld"
#define lua_str2number(s,p)     strtol((s), (p), 10)
#endif

#ifdef USE_INT
#define LUA_NUMBER		int
#define LUA_NUMBER_SCAN		"%d"
#define LUA_NUMBER_FMT		"%d"
#define lua_str2number(s,p)     ((int) strtol((s), (p), 10))
#endif

#ifdef USE_FASTROUND
#define lua_number2int(i,d)	__asm__("fldl %1\nfistpl %0":"=m"(i):"m"(d))
#endif
