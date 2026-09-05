#if defined(CLAY_HAS_STB_VORBIS)
#define STB_VORBIS_HEADER_ONLY
#include "stb_vorbis.c"
#endif
#define MINIAUDIO_IMPLEMENTATION
#define MA_NO_ENCODING
#define MA_NO_DEVICE_IO
#include "miniaudio.h"
#if defined(CLAY_HAS_STB_VORBIS)
#undef STB_VORBIS_HEADER_ONLY
#include "stb_vorbis.c"
#endif
