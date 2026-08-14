#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include "../StereoFieldSource.h"

// Stereo vectorscope drawn inside the Stereo knob face.
//
// Coordinate mapping (standard goniometer, M/S rotated):
//   X  =  L − R   (right = more R)
//   Y  =  L + R   (up = louder / more mono)
//
// Keeps a ring buffer of recent L/R pairs. paint() draws them
// newest-brightest, oldest-transparent. No image compositing involved.
class GoniometerComponent : public juce::Component,
                             private juce::Timer
{
public:
    GoniometerComponent();
    void setSource (StereoFieldSource* source);
    void paint     (juce::Graphics&) override;

private:
    void timerCallback() override;

    static constexpr int kMaxPoints = 600;   // display history
    static constexpr int kPullSize  = 4096;  // drain a full block per tick
    static constexpr int kDecimate  = 6;     // plot every N-th sample

    StereoFieldSource* source { nullptr };

    struct Point { float l, r; };
    std::array<Point, kMaxPoints> ring {};
    int writeIdx  { 0 };
    int totalPts  { 0 };

    float pullL[kPullSize] {};
    float pullR[kPullSize] {};

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (GoniometerComponent)
};
