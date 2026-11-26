#include "WorkWatchEngine.h"

WorkWatchEngine::WorkWatchEngine()
{
    lastActiveTimeMs.store(juce::Time::currentTimeMillis());
    latestSamples.assign(512, 0.0f);
    startTimerHz(20); // 50ms마다 무음 시간 및 상태 체크
}

WorkWatchEngine::~WorkWatchEngine()
{
    stopTimer();
    stopAudio();
}

void WorkWatchEngine::startAudio(int numInputChannels, int numOutputChannels)
{
    auto err = deviceManager.initialise(numInputChannels, numOutputChannels, nullptr, true);
    if (err.isNotEmpty())
    {
        DBG("AudioDeviceManager init error: " + err);
    }
    deviceManager.addAudioCallback(this);
}

void WorkWatchEngine::stopAudio()
{
    deviceManager.removeAudioCallback(this);
    deviceManager.closeAudioDevice();
}

void WorkWatchEngine::audioDeviceAboutToStart(juce::AudioIODevice* device)
{
    juce::ignoreUnused(device);
    lastActiveTimeMs.store(juce::Time::currentTimeMillis());
}

void WorkWatchEngine::audioDeviceStopped()
{
}

void WorkWatchEngine::audioDeviceIOCallbackWithContext(const float* const* inputChannelData,
                                                       int numInputChannels,
                                                       float* const* outputChannelData,
                                                       int numOutputChannels,
                                                       int numSamples,
                                                       const juce::AudioIODeviceCallbackContext& context)
{
    juce::ignoreUnused(outputChannelData, numOutputChannels, context);

    if (numInputChannels <= 0 || inputChannelData == nullptr || inputChannelData[0] == nullptr || numSamples <= 0)
    {
        currentLevel.store(0.0f);
        return;
    }

    const float* samples = inputChannelData[0];

    // 1. 구간 피크 & RMS 혼합 레벨 계산 (자판 타건 같은 짧은 스파이크를 놓치지 않기 위함)
    float peak = 0.0f;
    double sumSquares = 0.0;

    for (int i = 0; i < numSamples; ++i)
    {
        const float s = samples[i];
        const float absVal = std::abs(s);
        if (absVal > peak)
            peak = absVal;
        sumSquares += static_cast<double>(s) * static_cast<double>(s);
    }

    const float rms = static_cast<float>(std::sqrt(sumSquares / numSamples));
    // 타건음은 순간 피크가 튀므로 peak와 rms를 섞어서 감지 지표로 사용
    const float activityMetric = juce::jmax(peak * 0.85f, rms * 1.4f);
    currentLevel.store(activityMetric);

    // 2. 임계치 초과 시 작업 활동 시각 갱신
    if (activityMetric >= threshold.load())
    {
        lastActiveTimeMs.store(juce::Time::currentTimeMillis());
    }

    // 3. 오실로스코프 시각화용 샘플 버퍼 저장
    {
        const juce::SpinLock::ScopedTryLockType lock(sampleLock);
        if (lock.isLocked())
        {
            latestSamples.assign(samples, samples + numSamples);
        }
    }
}

void WorkWatchEngine::copyLatestSamples(std::vector<float>& dest)
{
    const juce::SpinLock::ScopedTryLockType lock(sampleLock);
    if (lock.isLocked())
    {
        dest = latestSamples;
    }
}

void WorkWatchEngine::timerCallback()
{
    const int64_t nowMs = juce::Time::currentTimeMillis();
    const int64_t diffMs = juce::jmax<int64_t>(0, nowMs - lastActiveTimeMs.load());
    const double idleSec = static_cast<double>(diffMs) / 1000.0;
    elapsedIdleSec.store(idleSec);

    const WatchState nextState = (idleSec >= static_cast<double>(idleTimeoutSec.load()))
                                 ? WatchState::Warning
                                 : WatchState::Monitoring;

    state.store(nextState);

    if (nextState != lastNotifiedState)
    {
        lastNotifiedState = nextState;
        if (onStateChanged)
            onStateChanged(nextState);
    }
}