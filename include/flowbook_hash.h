#ifndef _FLOWBOOK_HASH_H_
#define _FLOWBOOK_HASH_H_

#include <cstdio>
#include <random>
#include <vector>
#include <unordered_set>

#define MAX_PRIME32 1229
#define MAX_BIG_PRIME32 50

class flow_hasher
{
public:
	flow_hasher();
	~flow_hasher();
	flow_hasher(uint32_t prime32Num);
	uint32_t run(const char * str, uint32_t len);
private:
	uint32_t prime32Num;
};

#endif //_FLOWBOOK_HASH_H_