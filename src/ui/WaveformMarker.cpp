#include "WaveformMarker.h"

WaveformMarker::WaveformMarker (Type markerType) : type (markerType)
{
    setSize (kWidth, 1);   // height is set by parent in resized()
    setRepaintsOnMouseActivity (true);
}

void WaveformMarker::paint (juce::Graphics& g)
{
    const auto colour = (type == Type::Start)
        ? juce::Colours::limegreen
        : juce::Colours::orangered;

    g.setColour (hovered ? colour.brighter (0.4f) : colour);

    // Vertical line.
    g.fillRect (getWidth() / 2 - 1, 0, 2, getHeight());

    // Text label at top — start hangs right of line, end hangs left, so neither
    // clips against the waveform edge.
    const juce::String label = (type == Type::Start) ? "Start" : "End";
    const int lineX = getWidth() / 2;
    g.setFont (juce::Font (11.0f, juce::Font::bold));
    if (type == Type::Start)
        g.drawText (label, lineX + 2, 0, getWidth() - lineX - 2, 14, juce::Justification::centredLeft,  false);
    else
        g.drawText (label, 0,         0, lineX - 2,               14, juce::Justification::centredRight, false);
}

void WaveformMarker::mouseDown (const juce::MouseEvent& e)
{
    // Record how far the click landed from the line centre so drag doesn't snap.
    const int lineCentreInParent = getX() + getWidth() / 2;
    dragOffsetX = e.getEventRelativeTo (getParentComponent()).x - lineCentreInParent;
}

void WaveformMarker::mouseDrag (const juce::MouseEvent& e)
{
    if (getParentComponent() == nullptr)
        return;

    const int parentWidth = getParentWidth();
    if (parentWidth <= 0)
        return;

    // Derive position directly from the mouse X in parent coordinates.
    // Map pixel → normalised through the current view window so drag is
    // accurate at any zoom level.
    const float pixelFraction = static_cast<float> (e.getEventRelativeTo (getParentComponent()).x - dragOffsetX)
                                / static_cast<float> (parentWidth);
    const float rawPos = viewStart + pixelFraction * viewWidth;

    setNormalisedPosition (rawPos);

    if (onPositionChanged)
        onPositionChanged (normalisedPos);
}

void WaveformMarker::setViewWindow (float start, float width)
{
    viewStart = start;
    viewWidth = (width > 0.0f) ? width : 1.0f;
}

void WaveformMarker::mouseEnter (const juce::MouseEvent&) { hovered = true;  repaint(); }
void WaveformMarker::mouseExit  (const juce::MouseEvent&) { hovered = false; repaint(); }

void WaveformMarker::setNormalisedPosition (float pos)
{
    normalisedPos = juce::jlimit (minPos, maxPos, pos);

    if (getParentComponent() != nullptr)
    {
        // Map normalised position through the view window so the marker sits
        // at the correct pixel coordinate when zoomed in.
        const float pixelFraction = (normalisedPos - viewStart) / viewWidth;
        const int x = static_cast<int> (pixelFraction * static_cast<float> (getParentWidth()))
                      - kWidth / 2;
        setTopLeftPosition (x, 0);
    }
}
