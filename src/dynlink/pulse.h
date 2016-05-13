#pragma once

#include <pulse/pulseaudio.h>
#define FUNDEFDECL(x) extern decltype(&x) pf_##x

bool DynLoadPulse();
void DynUnloadPulse();

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
FUNDEFDECL(pa_strerror);
FUNDEFDECL(pa_stream_drop);
FUNDEFDECL(pa_stream_peek);
FUNDEFDECL(pa_stream_new);
FUNDEFDECL(pa_stream_connect_record);
FUNDEFDECL(pa_stream_set_read_callback);
FUNDEFDECL(pa_threaded_mainloop_get_api);
FUNDEFDECL(pa_threaded_mainloop_new);

#undef FUNDEFDECL
