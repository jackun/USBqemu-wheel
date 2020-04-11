#include "mididev-noop.h"
namespace mididev_noop {
REGISTER_MIDIDEV(mididev_noop::APINAME, NoopMidiDevice);
}