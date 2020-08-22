#include "thd3d8mitigation.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#define CINTERFACE
#include <d3d8.h>

#include <stdexcept>
#include <unordered_map>

using d3d8pair = std::pair<IDirect3D8*, struct IDirect3D8ExtraData>;
using d3ddevice8pair = std::pair<IDirect3DDevice8*, struct IDirect3DDevice8ExtraData>;

struct IDirect3D8ExtraDataTable {
	std::unordered_map<IDirect3D8*, struct IDirect3D8ExtraData> ht;
};

struct IDirect3DDevice8ExtraDataTable {
	std::unordered_map<IDirect3DDevice8*, struct IDirect3DDevice8ExtraData> ht;
};

extern "C" struct IDirect3D8ExtraDataTable* IDirect3D8ExtraDataTableNew(void)
{
	try
	{
		return new struct IDirect3D8ExtraDataTable;
	}
	catch (std::exception& e)
	{
		Fatal(1, "%s: fatal: unexpected exception (%s).", __FUNCTION__, e.what());
	}
	catch (...)
	{
		Fatal(1, "%s: fatal: unexpected exception (unknown).", __FUNCTION__);
	}
}

extern "C" BOOL IDirect3D8ExtraDataTableInsert(struct IDirect3D8ExtraDataTable* h, IDirect3D8* k, struct IDirect3D8ExtraData v)
{
	try
	{
		return h->ht.insert(d3d8pair{ k, v }).second;
	}
	catch (std::exception& e)
	{
		Fatal(1, "%s: fatal: unexpected exception (%s).", __FUNCTION__, e.what());
	}
	catch (...)
	{
		Fatal(1, "%s: fatal: unexpected exception (unknown).", __FUNCTION__);
	}
}

extern "C" void IDirect3D8ExtraDataTableErase(struct IDirect3D8ExtraDataTable* h, IDirect3D8* k)
{
	try
	{
		h->ht.erase(k);
	}
	catch (std::exception& e)
	{
		Fatal(1, "%s: fatal: unexpected exception (%s).", __FUNCTION__, e.what());
	}
	catch (...)
	{
		Fatal(1, "%s: fatal: unexpected exception (unknown).", __FUNCTION__);
	}
}

extern "C" void IDirect3D8ExtraDataTableShrinkToFit(struct IDirect3D8ExtraDataTable* h)
{
	try
	{
		// https://ja.wikibooks.org/wiki/More_C%2B%2B_Idioms/%E7%B8%AE%E3%82%81%E3%81%A6%E5%90%88%E3%82%8F%E3%81%9B%E3%82%8B(Shrink-to-fit)
		std::unordered_map<IDirect3D8*, struct IDirect3D8ExtraData>(h->ht).swap(h->ht);
	}
	catch (std::exception& e)
	{
		Fatal(1, "%s: fatal: unexpected exception (%s).", __FUNCTION__, e.what());
	}
	catch (...)
	{
		Fatal(1, "%s: fatal: unexpected exception (unknown).", __FUNCTION__);
	}
}

extern "C" struct IDirect3D8ExtraData* IDirect3D8ExtraDataTableGet(struct IDirect3D8ExtraDataTable* h, IDirect3D8* k)
{
	try
	{
		try
		{
			return &h->ht.at(k);
		}
		catch (std::out_of_range&)
		{
			return NULL;
		}
	}
	catch (std::exception& e)
	{
		Fatal(1, "%s: fatal: unexpected exception (%s).", __FUNCTION__, e.what());
	}
	catch (...)
	{
		Fatal(1, "%s: fatal: unexpected exception (unknown).", __FUNCTION__);
	}
}

extern "C" struct IDirect3DDevice8ExtraDataTable* IDirect3DDevice8ExtraDataTableNew(void)
{
	try
	{
		return new struct IDirect3DDevice8ExtraDataTable;
	}
	catch (std::exception& e)
	{
		Fatal(1, "%s: fatal: unexpected exception (%s).", __FUNCTION__, e.what());
	}
	catch (...)
	{
		Fatal(1, "%s: fatal: unexpected exception (unknown).", __FUNCTION__);
	}
}

extern "C" BOOL IDirect3DDevice8ExtraDataTableInsert(struct IDirect3DDevice8ExtraDataTable* h, IDirect3DDevice8* k, struct IDirect3DDevice8ExtraData v)
{
	try
	{
		return h->ht.insert(d3ddevice8pair{ k, v }).second;
	}
	catch (std::exception& e)
	{
		Fatal(1, "%s: fatal: unexpected exception (%s).", __FUNCTION__, e.what());
	}
	catch (...)
	{
		Fatal(1, "%s: fatal: unexpected exception (unknown).", __FUNCTION__);
	}
}

extern "C" void IDirect3DDevice8ExtraDataTableErase(struct IDirect3DDevice8ExtraDataTable* h, IDirect3DDevice8 * k)
{
	try
	{
		h->ht.erase(k);
	}
	catch (std::exception& e)
	{
		Fatal(1, "%s: fatal: unexpected exception (%s).", __FUNCTION__, e.what());
	}
	catch (...)
	{
		Fatal(1, "%s: fatal: unexpected exception (unknown).", __FUNCTION__);
	}
}

extern "C" void IDirect3DDevice8ExtraDataTableShrinkToFit(struct IDirect3DDevice8ExtraDataTable* h)
{
	try
	{
		// https://ja.wikibooks.org/wiki/More_C%2B%2B_Idioms/%E7%B8%AE%E3%82%81%E3%81%A6%E5%90%88%E3%82%8F%E3%81%9B%E3%82%8B(Shrink-to-fit)
		std::unordered_map<IDirect3DDevice8*, struct IDirect3DDevice8ExtraData>(h->ht).swap(h->ht);
	}
	catch (std::exception& e)
	{
		Fatal(1, "%s: fatal: unexpected exception (%s).", __FUNCTION__, e.what());
	}
	catch (...)
	{
		Fatal(1, "%s: fatal: unexpected exception (unknown).", __FUNCTION__);
	}
}

extern "C" struct IDirect3DDevice8ExtraData* IDirect3DDevice8ExtraDataTableGet(struct IDirect3DDevice8ExtraDataTable* h, IDirect3DDevice8* k)
{
	try
	{
		try
		{
			return &h->ht.at(k);
		}
		catch (std::out_of_range&)
		{
			return NULL;
		}
	}
	catch (std::exception& e)
	{
		Fatal(1, "%s: fatal: unexpected exception (%s).", __FUNCTION__, e.what());
	}
	catch (...)
	{
		Fatal(1, "%s: fatal: unexpected exception (unknown).", __FUNCTION__);
	}
}
