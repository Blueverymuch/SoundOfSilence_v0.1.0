#include "MainComponent.h"
//initial

MainComponent::MainComponent()
{
    historyPeaks.assign(180, 0.0f);
    lastTickMs = juce::Time::getMillisecondCounter();
    startTimerHz(10); // 화면 갱신은 100ms(10Hz)로 차분하게

    waveformSamples.assign(512, 0.0f);

    // 테스트 편의를 위해 기본 무음 제한을 짧게(예: 10초) 두고 필요시 setIdleTimeoutSeconds로 조절
    workWatchEngine.setIdleTimeoutSeconds(10);
    workWatchEngine.setThreshold(0.03f);

    // 상태 변경 시 경고 팝업/배너 트리거
    workWatchEngine.onStateChanged = [this](WorkWatchEngine::WatchState newState)
    {
        juce::MessageManager::callAsync([this, newState]()
                                        {
                                            if (newState == WorkWatchEngine::WatchState::Warning)
                                            {
                                                isWarningBannerVisible = true;
                                                showSilenceAlert();
                                            }
                                            else
                                            {
                                                isWarningBannerVisible = false;
                                                silencePopup.reset();
                                            }
                                            repaint();
                                        });
    };

    // MainComponent 생성자 안에서 startAudio 직접 호출 대신 권한 요청 후 시작
    juce::RuntimePermissions::request(
            juce::RuntimePermissions::recordAudio,
            [this](bool granted)
            {
                juce::MessageManager::callAsync([this, granted]()
                                                {
                                                    if (granted)
                                                    {
                                                        workWatchEngine.startAudio(1, 0);
                                                    }
                                                    else
                                                    {
                                                        statusSummaryText = "Microphone permission denied";
                                                        repaint();
                                                    }
                                                });
            });

    // 마이크 입력 1채널, 출력 0채널로 시작
    workWatchEngine.startAudio(1, 0);

    // UI 갱신 타이머 시작 (약 30fps)
    startTimerHz(30);

    setSize(800, 480);

    // 젠틀한 버튼 추가 ㅋㅋ
    addAndMakeVisible(gentleButton);
    addAndMakeVisible(normalButton);
    addAndMakeVisible(brutalButton);

    gentleButton.onClick = [this]() { setCrueltyLevel(CrueltyLevel::Gentle); };
    normalButton.onClick = [this]() { setCrueltyLevel(CrueltyLevel::Normal); };
    brutalButton.onClick = [this]() { setCrueltyLevel(CrueltyLevel::Brutal); };

    setCrueltyLevel(CrueltyLevel::Brutal);

}

MainComponent::~MainComponent()
{
    stopTimer();
    silencePopup.reset();
    workWatchEngine.stopAudio();
}

// MainComponent.cpp 안에 들어가야 할 함수
void MainComponent::timerCallback()
{
    const int nowMs = juce::Time::getMillisecondCounter();
    const double dtSec = juce::jmax(0.0, static_cast<double>(nowMs - lastTickMs) / 1000.0);
    lastTickMs = nowMs;

    latestLevel = workWatchEngine.getCurrentLevel();
    latestIdleSec = workWatchEngine.getElapsedIdleSeconds();
    latestState = workWatchEngine.getState();

    const float dbNorm = levelToDbNorm(latestLevel);

    // 현재 시간 칸 동안 들어온 소리 중 가장 높은 피크를 기록 (소리가 뿅 하고 올라오게)
    if (dbNorm > currentBucketPeak)
        currentBucketPeak = dbNorm;
    else
        currentBucketPeak = juce::jmax(0.0f, currentBucketPeak - static_cast<float>(dtSec * 0.08));

    bucketElapsedSec += dtSec;
    if (bucketElapsedSec >= bucketDurationSec)
    {
        bucketElapsedSec = std::fmod(bucketElapsedSec, bucketDurationSec);

        // 왼쪽으로 오래된 값 밀어내고 오른쪽 끝에 현재 칸 피크 추가
        if (!historyPeaks.empty())
        {
            std::rotate(historyPeaks.begin(), historyPeaks.begin() + 1, historyPeaks.end());
            historyPeaks.back() = currentBucketPeak;
        }

        // 다음 시간 칸을 위해 살짝만 낮춰서 연속된 소리 흐름 유지
        currentBucketPeak *= 0.35f;
    }

    displayLevel = juce::jmax(dbNorm, currentBucketPeak);

    const char* stateLabel = (latestState == WorkWatchEngine::WatchState::Warning)
                             ? "SILENCE WARNING"
                             : "ACTIVE / MONITORING";

    statusSummaryText = juce::String(stateLabel)
                        + "  |  Idle: " + juce::String(latestIdleSec, 1) + "s"
                        + "  |  10m Timeline Peak: " + juce::String(displayLevel, 2);

    repaint();
}

void MainComponent::drawOscilloscope(juce::Graphics& g, juce::Rectangle<int> bounds)
{
    g.setColour(juce::Colour(0xff141820));
    g.fillRoundedRectangle(bounds.toFloat(), 8.0f);

    g.setColour(juce::Colour(0xff2a3242));
    g.drawRoundedRectangle(bounds.toFloat(), 8.0f, 1.5f);

    if (historyPeaks.empty() || bounds.isEmpty())
        return;

    // drawOscilloscope 안에서 historyPeaks를 막대/스파이크로 그리기
    const int count = static_cast<int>(historyPeaks.size());
    const float slotW = static_cast<float>(bounds.getWidth()) / static_cast<float>(juce::jmax(1, count));
    const float bottomY = static_cast<float>(bounds.getBottom()) - 6.0f;
    const float maxBarH = static_cast<float>(bounds.getHeight()) - 14.0f;

    for (int i = 0; i < count; ++i)
    {
        const float v = juce::jlimit(0.0f, 1.0f, historyPeaks[static_cast<size_t>(i)]);
        const float barH = v * maxBarH;
        if (barH <= 1.0f)
            continue;

        const float x = static_cast<float>(bounds.getX()) + static_cast<float>(i) * slotW;
        const float y = bottomY - barH;

        g.setColour(latestState == WorkWatchEngine::WatchState::Warning
                    ? juce::Colour(0xffff5c5c).withAlpha(0.85f)
                    : juce::Colour(0xff38d9a9).withAlpha(0.9f));
        g.fillRect(x, y, juce::jmax(1.5f, slotW - 1.0f), barH);
    }
}

// showSilenceAlert() 영문 통일 예시
void MainComponent::showSilenceAlert()
{
    if (silencePopup != nullptr)
        return;

    silencePopup = std::make_unique<juce::AlertWindow>(
            "SoundOfSilence",
            "Silence detected! Time to get back to work!",
            juce::MessageBoxIconType::NoIcon);

    silencePopup->addButton("OK", 1, juce::KeyPress(juce::KeyPress::returnKey));
    silencePopup->setSize(250, 200);

    silencePopup->enterModalState(true, juce::ModalCallbackFunction::create([this](int)
                                                                            {
                                                                                silencePopup.reset();
                                                                            }), false);
}

void MainComponent::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour(0xff0d1017));

    auto area = getLocalBounds().reduced(20);

    // 1층: 상단 상태 텍스트 (가로 폭 전체 사용 가능해서 글씨 안 잘림)
    auto statusTextArea = area.removeFromTop(32);
    area.removeFromTop(8);

    // 아래쪽에서 24px를 먼저 떼어 내고(바닥 여백이나 푸터용), 남은 위쪽 영역을 오실로스코프로 사용
    auto bottomSpace = area.removeFromBottom(24);
    //juce::ignoreUnused(bottomSpace);


    // 2층: 버튼 영역 자리 비우기 (실제 버튼 컴포넌트가 위에 그려짐)
    area.removeFromTop(34);
    area.removeFromTop(12);

    g.setColour(juce::Colours::white);
    g.setFont(15.5f);
    g.drawFittedText(statusSummaryText, statusTextArea, juce::Justification::centredLeft, 1, 0.85f);

    // 무음 경고 배너
    if (isWarningBannerVisible)
    {
        auto bannerArea = area.removeFromTop(42);
        area.removeFromTop(10);
        g.setColour(juce::Colour(0xff7f1d1d));
        g.fillRoundedRectangle(bannerArea.toFloat(), 6.0f);
        g.setColour(juce::Colour(0xfffecaca));
        g.setFont(15.0f);
        g.drawText("SILENCE DETECTED! Check your workspace audio.", bannerArea, juce::Justification::centred, true);
    }

    // 중앙 오실로스코프 영역
    drawOscilloscope(g, area);
}

namespace
{
    // 작은 소리/타건도 높이가 확 차이나게 dB 구간(-75dB ~ -15dB)을 넓게 매핑
    float levelToWideDbNorm(float linearLevel)
    {
        const float clamped = juce::jmax(linearLevel, 1.0e-6f);
        const float db = juce::Decibels::gainToDecibels(clamped, -75.0f);

        const float minDb = -75.0f;
        const float maxDb = -15.0f;
        const float norm = juce::jlimit(0.0f, 1.0f, (db - minDb) / (maxDb - minDb));

        // 저레벨 구간 간격을 더 벌리기 위한 완만한 곡선
        return std::pow(norm, 0.65f);
    }



}


void MainComponent::setCrueltyLevel(CrueltyLevel level)
{
    currentCruelty = level;

    switch (currentCruelty)
    {
        case CrueltyLevel::Gentle:
            workWatchEngine.setIdleTimeoutSeconds(60);
            break;
        case CrueltyLevel::Normal:
            workWatchEngine.setIdleTimeoutSeconds(25);
            break;
        case CrueltyLevel::Brutal:
        default:
            workWatchEngine.setIdleTimeoutSeconds(10);
            break;
    }

    updateCrueltyButtonStyles();
    repaint();
}

void MainComponent::updateCrueltyButtonStyles()
{
    auto styleButton = [](juce::TextButton& btn, bool active, juce::Colour activeBg)
    {
        btn.setColour(juce::TextButton::buttonColourId,
                      active ? activeBg : juce::Colour(0xff1e2430));
        btn.setColour(juce::TextButton::buttonOnColourId, activeBg);
        btn.setColour(juce::TextButton::textColourOffId,
                      active ? juce::Colours::white : juce::Colour(0xff9aa4b2));
    };

    styleButton(gentleButton, currentCruelty == CrueltyLevel::Gentle, juce::Colour(0xff2b8a78));
    styleButton(normalButton, currentCruelty == CrueltyLevel::Normal, juce::Colour(0xff3b6ea5));
    styleButton(brutalButton, currentCruelty == CrueltyLevel::Brutal, juce::Colour(0xffb83232));
}


void MainComponent::resized()
{
    auto area = getLocalBounds().reduced(20);

    // 1층: 상태 텍스트 영역 (높이 32px 확보)
    area.removeFromTop(32);
    area.removeFromTop(8);

    // 2층: 잔인함 조절 버튼 3개 (높이 34px 확보)
    auto buttonRow = area.removeFromTop(34);

    const int btnWidth = 84;
    const int gap = 8;

    gentleButton.setBounds(buttonRow.removeFromLeft(btnWidth));
    buttonRow.removeFromLeft(gap);
    normalButton.setBounds(buttonRow.removeFromLeft(btnWidth));
    buttonRow.removeFromLeft(gap);
    brutalButton.setBounds(buttonRow.removeFromLeft(btnWidth));
}