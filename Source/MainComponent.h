#pragma once

#include <JuceHeader.h>
#include "WorkWatchEngine.h"

namespace
{
    float levelToDbNorm(float linearLevel)
    {
        const float clamped = juce::jmax(linearLevel, 1.0e-5f);
        const float db = juce::Decibels::gainToDecibels(clamped, -60.0f); // -60dB ~ 0dB
        return juce::jlimit(0.0f, 1.0f, (db + 60.0f) / 60.0f);
    }
}

#include <vector>

enum class CrueltyLevel
{
    Gentle = 0, // 60s
    Normal = 1, // 25s
    Brutal = 2  // 10s
};

class MainComponent : public juce::Component,
                      private juce::Timer
{
public:
    MainComponent();
    ~MainComponent() override;

    void paint(juce::Graphics& g) override;
    void resized() override;

private:
    // private: 멤버 영역에 추가
    juce::TextButton gentleButton { "Gentle" };
    juce::TextButton normalButton { "Normal" };
    juce::TextButton brutalButton { "Brutal" };
    CrueltyLevel currentCruelty = CrueltyLevel::Brutal;

    void setCrueltyLevel(CrueltyLevel level);
    void updateCrueltyButtonStyles();


    void timerCallback() override;
    void drawOscilloscope(juce::Graphics& g, juce::Rectangle<int> bounds);
    void showSilenceAlert();

    WorkWatchEngine workWatchEngine;

    std::vector<float> waveformSamples;
    float latestLevel = 0.0f;

    // MainComponent.h private 멤버에 추가
    float displayLevel = 0.0f;       // 화면 표시용 스무딩 레벨 (0.0 ~ 1.0)
    float peakHoldLevel = 0.0f;      // 피크 홀드 레벨
    int holdCounterTicks = 0;      // 홀드 유지 틱 카운터

    // MainComponent.h private 멤버에 추가/정리
    std::vector<float> historyPeaks;          // 과거 시간대별 피크 기록 (0.0 ~ 1.0)
    float currentBucketPeak = 0.0f;           // 현재 시간 칸에서 잡힌 최대 dB 레벨
    double bucketElapsedSec = 0.0;            // 현재 시간 칸 누적 시간(초)
    double bucketDurationSec = 3.33;          // 10분(600초)을 180칸으로 나눈 값 (~3.33초/칸)
    int lastTickMs = 0;

    double latestIdleSec = 0.0;
    WorkWatchEngine::WatchState latestState = WorkWatchEngine::WatchState::Monitoring;

    bool isWarningBannerVisible = false;
    juce::String statusSummaryText = "Monitoring...";
    std::unique_ptr<juce::AlertWindow> silencePopup;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MainComponent)
};

