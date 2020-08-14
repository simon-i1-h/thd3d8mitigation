#include <stdexcept>
#include <unordered_map>

// XXX TODO exception safety

using htpair = std::pair<uintptr_t, void*>;

struct hashtable {
	std::unordered_map<uintptr_t, void*> map;
};

extern "C" struct hashtable* hashtable_new(void)
{
	return new struct hashtable;
}

extern "C" void hashtable_del(struct hashtable* h)
{
	delete h;
}

extern "C" void hashtable_insert(struct hashtable* h, uintptr_t k, void* v)
{
	h->map.insert(htpair{ k, v });
}

extern "C" void hashtable_erase(struct hashtable* h, uintptr_t k)
{
	h->map.erase(k);
}

extern "C" void* hashtable_get(struct hashtable* h, uintptr_t k)
{
	void* ret;

	try
	{
		ret = h->map.at(k);
	}
	catch (std::out_of_range&)
	{
		ret = NULL;
	}

	return ret;
}
