#pragma once

#include <JuceHeader.h>
#include <atomic>
#include <cmath>
#include <cstdint>
#include <functional>
#include <vector>

class WorkWatchEngine : public juce::AudioIODeviceCallback,
                        private juce::Timer
{
public:
    enum class WatchState
    {
        Monitoring,
        Warning
    };

    WorkWatchEngine();
    ~WorkWatchEngine() override;

    void startAudio(int numInputChannels = 1, int numOutputChannels = 0);
    void stopAudio();

    void setThreshold(float newThreshold) { threshold.store(newThreshold); }
    float getThreshold() const { return threshold.load(); }

    void setIdleTimeoutSeconds(int seconds) { idleTimeoutSec.store(seconds); }
    int getIdleTimeoutSeconds() const { return idleTimeoutSec.load(); }

    float getCurrentLevel() const { return currentLevel.load(); }
    double getElapsedIdleSeconds() const { return elapsedIdleSec.load(); }
    WatchState getState() const { return state.load(); }

    void copyLatestSamples(std::vector<float>& dest);

    std::function<void(WatchState)> onStateChanged;

    // juce::AudioIODeviceCallback overrides
    void audioDeviceAboutToStart(juce::AudioIODevice* device) override;
    void audioDeviceStopped() override;
    void audioDeviceIOCallbackWithContext(const float* const* inputChannelData,
                                          int numInputChannels,
                                          float* const* outputChannelData,
                                          int numOutputChannels,
                                          int numSamples,
                                          const juce::AudioIODeviceCallbackContext& context) override;

private:
    void timerCallback() override;

    juce::AudioDeviceManager deviceManager;

    std::atomic<float> threshold { 0.03f };
    std::atomic<int> idleTimeoutSec { 10 };
    std::atomic<float> currentLevel { 0.0f };
    std::atomic<double> elapsedIdleSec { 0.0 };
    std::atomic<WatchState> state { WatchState::Monitoring };
    std::atomic<int64_t> lastActiveTimeMs { 0 };

    WatchState lastNotifiedState { WatchState::Monitoring };

    juce::SpinLock sampleLock;
    std::vector<float> latestSamples;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(WorkWatchEngine)
};