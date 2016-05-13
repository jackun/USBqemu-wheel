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

FUNDEFDECL(pa_usec_to_bytes);
FUNDEFDECL(pa_bytes_per_second);
FUNDEFDECL(pa_threaded_mainloop_start);
FUNDEFDECL(pa_threaded_mainloop_free);
FUNDEFDECL(pa_threaded_mainloop_stop);
FUNDEFDECL(pa_stream_unref);
FUNDEFDECL(pa_stream_disconnect);
FUNDEFDECL(pa_threaded_mainloop_new);
FUNDEFDECL(pa_threaded_mainloop_get_api);
FUNDEFDECL(pa_stream_set_read_callback);
FUNDEFDECL(pa_stream_connect_record);
FUNDEFDECL(pa_stream_new);
FUNDEFDECL(pa_stream_peek);
FUNDEFDECL(pa_strerror);
FUNDEFDECL(pa_stream_drop);
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

	if (pulse_handle && pf_pa_mainloop_free)
		return true;

	//dlopen itself is refcounted too
	pulse_handle = dlopen ("libpulse.so.0", RTLD_LAZY);
	if (!pulse_handle) {
		std::cerr << dlerror() << std::endl;
		return false;
	}

	refCntPulse++;
	FUN_LOAD(pulse_handle, pa_usec_to_bytes);
	FUN_LOAD(pulse_handle, pa_bytes_per_second);
	FUN_LOAD(pulse_handle, pa_threaded_mainloop_start);
	FUN_LOAD(pulse_handle, pa_threaded_mainloop_free);
	FUN_LOAD(pulse_handle, pa_threaded_mainloop_stop);
	FUN_LOAD(pulse_handle, pa_stream_unref);
	FUN_LOAD(pulse_handle, pa_stream_disconnect);
	FUN_LOAD(pulse_handle, pa_threaded_mainloop_new);
	FUN_LOAD(pulse_handle, pa_threaded_mainloop_get_api);
	FUN_LOAD(pulse_handle, pa_stream_set_read_callback);
	FUN_LOAD(pulse_handle, pa_stream_connect_record);
	FUN_LOAD(pulse_handle, pa_stream_new);
	FUN_LOAD(pulse_handle, pa_stream_peek);
	FUN_LOAD(pulse_handle, pa_strerror);
	FUN_LOAD(pulse_handle, pa_stream_drop);
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
	if (!pulse_handle && !pf_pa_mainloop_free)
		return;

	if(!refCntPulse || --refCntPulse > 0)
		return;

	FUN_UNLOAD(pa_usec_to_bytes);
	FUN_UNLOAD(pa_bytes_per_second);
	FUN_UNLOAD(pa_threaded_mainloop_start);
	FUN_UNLOAD(pa_threaded_mainloop_free);
	FUN_UNLOAD(pa_threaded_mainloop_stop);
	FUN_UNLOAD(pa_stream_unref);
	FUN_UNLOAD(pa_stream_disconnect);
	FUN_UNLOAD(pa_threaded_mainloop_new);
	FUN_UNLOAD(pa_threaded_mainloop_get_api);
	FUN_UNLOAD(pa_stream_set_read_callback);
	FUN_UNLOAD(pa_stream_connect_record);
	FUN_UNLOAD(pa_stream_new);
	FUN_UNLOAD(pa_stream_peek);
	FUN_UNLOAD(pa_strerror);
	FUN_UNLOAD(pa_stream_drop);
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

