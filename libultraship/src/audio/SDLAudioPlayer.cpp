#include "SDLAudioPlayer.h"
#include <spdlog/spdlog.h>

#if defined(__ANDROID__)
#include <jni.h>
#include <SDL2/SDL_system.h>
#endif

namespace Ship {

SDLAudioPlayer::~SDLAudioPlayer() {
    SPDLOG_TRACE("destruct SDL audio player");
    SDL_QuitSubSystem(SDL_INIT_AUDIO);
}

bool SDLAudioPlayer::DoInit() {
    if (SDL_Init(SDL_INIT_AUDIO) != 0) {
        SPDLOG_ERROR("SDL init error: %s\n", SDL_GetError());
        return false;
    }
    mNumChannels = this->GetAudioChannels() == AudioChannelsSetting::audioSurround51 ? 6 : 2;
    SDL_AudioSpec want, have;
    SDL_zero(want);
    want.freq = this->GetSampleRate();
    want.format = AUDIO_S16SYS;
    want.channels = mNumChannels;
    want.samples = this->GetSampleLength();
    want.callback = NULL;
    mDevice = SDL_OpenAudioDevice(NULL, 0, &want, &have, 0);
    if (mDevice == 0) {
        SPDLOG_ERROR("SDL_OpenAudio error: {}", SDL_GetError());
        return false;
    }
    SDL_PauseAudioDevice(mDevice, 0);
    return true;
}

int SDLAudioPlayer::Buffered() {
    return SDL_GetQueuedAudioSize(mDevice) / (sizeof(int16_t) * mNumChannels);
}

#if defined(__ANDROID__)
bool SDLAudioPlayer::IsHeadsetAudioConnected() {
    const uint32_t now = SDL_GetTicks();
    if (mLastAudioRouteCheck != 0 && now - mLastAudioRouteCheck < 1000) {
        return mHeadsetAudioConnected;
    }
    mLastAudioRouteCheck = now;

    JNIEnv* env = static_cast<JNIEnv*>(SDL_AndroidGetJNIEnv());
    jobject activity = static_cast<jobject>(SDL_AndroidGetActivity());
    if (env == nullptr || activity == nullptr) {
        mHeadsetAudioConnected = false;
        return false;
    }

    jclass activityClass = env->GetObjectClass(activity);
    jmethodID method = activityClass != nullptr
                           ? env->GetMethodID(activityClass, "isHeadsetAudioConnected", "()Z")
                           : nullptr;
    if (method != nullptr) {
        mHeadsetAudioConnected = env->CallBooleanMethod(activity, method) == JNI_TRUE;
    } else {
        mHeadsetAudioConnected = false;
    }
    if (env->ExceptionCheck()) {
        env->ExceptionClear();
        mHeadsetAudioConnected = false;
    }
    if (activityClass != nullptr) {
        env->DeleteLocalRef(activityClass);
    }
    env->DeleteLocalRef(activity);
    return mHeadsetAudioConnected;
}
#endif

void SDLAudioPlayer::Play(const uint8_t* buf, size_t len) {
    if (Buffered() < 6000) {
        // Don't fill the audio buffer too much in case this happens
#if defined(__ANDROID__)
        if (mNumChannels == 2 && !IsHeadsetAudioConnected()) {
            const size_t sampleCount = len / sizeof(int16_t);
            mMonoBuffer.resize(sampleCount);
            const int16_t* source = reinterpret_cast<const int16_t*>(buf);
            for (size_t i = 0; i + 1 < sampleCount; i += 2) {
                const int16_t mono = static_cast<int16_t>(
                    (static_cast<int32_t>(source[i]) + static_cast<int32_t>(source[i + 1])) / 2);
                mMonoBuffer[i] = mono;
                mMonoBuffer[i + 1] = mono;
            }
            SDL_QueueAudio(mDevice, mMonoBuffer.data(), len);
            return;
        }
#endif
        SDL_QueueAudio(mDevice, buf, len);
    }
}
} // namespace Ship
