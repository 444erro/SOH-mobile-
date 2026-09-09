#pragma once
#include "AudioPlayer.h"
#include <SDL2/SDL.h>
#include <vector>

namespace Ship {
class SDLAudioPlayer : public AudioPlayer {
  public:
    SDLAudioPlayer(AudioSettings settings) : AudioPlayer(settings) {
    }
    ~SDLAudioPlayer();

    int Buffered();
    void Play(const uint8_t* buf, size_t len);

  protected:
    bool DoInit();

  private:
    SDL_AudioDeviceID mDevice;
    int32_t mNumChannels = 2;
#if defined(__ANDROID__)
    bool IsHeadsetAudioConnected();
    bool mHeadsetAudioConnected = false;
    uint32_t mLastAudioRouteCheck = 0;
    std::vector<int16_t> mMonoBuffer;
#endif
};
} // namespace Ship
