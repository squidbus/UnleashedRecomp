#include <os/media.h>
#include <os/media_detail.h>

bool os::media::IsExternalMediaPlaying()
{
    return detail::IsExternalMediaPlaying();
}
