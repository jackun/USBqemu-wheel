#pragma once

#include <pulse/pulseaudio.h>
#define FUNDEFDECL(x) extern decltype(&x) pf_##x

extern bool DynLoadPulse();
extern void DynUnloadPulse();

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

#undef FUNDEFDECL
