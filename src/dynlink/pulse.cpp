#include "pulse.h"
#include <dlfcn.h>
#include <iostream>
#include <atomic>

#define FUNDEFDECL(x) decltype(&x) pf_##x = nullptr
#define FUN_UNLOAD(fun) pf_##fun = nullptr
#define FUN_LOAD(h,fun) \
	pf_##fun = (decltype(&fun))(dlsym(h, #fun)); \
	if((error = dlerror()) != NULL) \
	{ \
		std::cerr << error << std::endl; \
		DynUnloadPulse(); \
		return false; \
	}

FUNDEFDECL(pa_context_connect);
FUNDEFDECL(pa_operation_unref);
FUNDEFDECL(pa_context_set_state_callback);
FUNDEFDECL(pa_context_get_state);
FUNDEFDECL(pa_mainloop_get_api);
FUNDEFDECL(pa_context_unref);
FUNDEFDECL(pa_context_disconnect);
FUNDEFDECL(pa_operation_get_state);
FUNDEFDECL(pa_context_get_source_info_list);
FUNDEFDECL(pa_mainloop_new);
FUNDEFDECL(pa_context_new);
FUNDEFDECL(pa_mainloop_iterate);
FUNDEFDECL(pa_mainloop_free);

static void* pulse_handle = nullptr;
static std::atomic<int> refCntPulse (0);

//TODO Probably needs mutex somewhere, but PCSX2 usually inits pretty serially
bool DynLoadPulse()
{
	const char* error = nullptr;

	refCntPulse++;
	if (pulse_handle && pf_pa_mainloop_free)
		return true;

	pulse_handle = dlopen ("libpulse.so", RTLD_LAZY);
	if (!pulse_handle) {
		std::cerr << dlerror() << std::endl;
		return false;
	}
	FUN_LOAD(pulse_handle, pa_context_connect);
	FUN_LOAD(pulse_handle, pa_operation_unref);
	FUN_LOAD(pulse_handle, pa_context_set_state_callback);
	FUN_LOAD(pulse_handle, pa_context_get_state);
	FUN_LOAD(pulse_handle, pa_mainloop_get_api);
	FUN_LOAD(pulse_handle, pa_context_unref);
	FUN_LOAD(pulse_handle, pa_context_disconnect);
	FUN_LOAD(pulse_handle, pa_operation_get_state);
	FUN_LOAD(pulse_handle, pa_context_get_source_info_list);
	FUN_LOAD(pulse_handle, pa_mainloop_new);
	FUN_LOAD(pulse_handle, pa_context_new);
	FUN_LOAD(pulse_handle, pa_mainloop_iterate);
	FUN_LOAD(pulse_handle, pa_mainloop_free);
	return true;
}

void DynUnloadPulse()
{
	if(!refCntPulse || --refCntPulse > 0)
		return;
	if (!pulse_handle && !pf_pa_mainloop_free)
		return;

	FUN_UNLOAD(pa_context_connect);
	FUN_UNLOAD(pa_operation_unref);
	FUN_UNLOAD(pa_context_set_state_callback);
	FUN_UNLOAD(pa_context_get_state);
	FUN_UNLOAD(pa_mainloop_get_api);
	FUN_UNLOAD(pa_context_unref);
	FUN_UNLOAD(pa_context_disconnect);
	FUN_UNLOAD(pa_operation_get_state);
	FUN_UNLOAD(pa_context_get_source_info_list);
	FUN_UNLOAD(pa_mainloop_new);
	FUN_UNLOAD(pa_context_new);
	FUN_UNLOAD(pa_mainloop_iterate);
	FUN_UNLOAD(pa_mainloop_free);

	dlclose(pulse_handle);
	pulse_handle = nullptr;
}
#undef FUNDEFDECL
#undef FUN_LOAD
#undef FUN_UNLOAD

