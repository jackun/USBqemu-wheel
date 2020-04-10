#include "audiodev-noop.h"
namespace audiodev_noop {
REGISTER_AUDIODEV(audiodev_noop::APINAME, NoopAudioDevice);
}