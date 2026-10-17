#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <functional>

// A draggable vertical marker drawn over the waveform display.
// Represents either the sample start or end point.
class WaveformMarker : public juce::Component
{
public:
    enum class Type { Start, End };

    explicit WaveformMarker (Type markerType);

    void paint (juce::Graphics& g) override;
    void mouseDown (const juce::MouseEvent& e) override;
    void mouseDrag (const juce::MouseEvent& e) override;
    void mouseEnter (const juce::MouseEvent& e) override;
    void mouseExit  (const juce::MouseEvent& e) override;

    // Set the normalised position (0–1); repositions the component within its parent.
    void setNormalisedPosition (float pos);
    float getNormalisedPosition() const { return normalisedPos; }

    // Called when the user drags the marker; argument is the new normalised position.
    std::function<void (float)> onPositionChanged;

    // Clamp limits set by WaveformDisplay to enforce start < end.
    float minPos { 0.0f };
    float maxPos { 1.0f };

    // View window set by WaveformDisplay so positioning and drag are zoom-aware.
    void setViewWindow (float start, float width);

    static constexpr int kWidth { 58 };

private:

    Type  type;
    float normalisedPos { 0.0f };
    bool  hovered       { false };
    int   dragOffsetX   { 0 };

    float viewStart { 0.0f };
    float viewWidth { 1.0f };

};
