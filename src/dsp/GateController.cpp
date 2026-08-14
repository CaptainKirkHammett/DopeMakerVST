#include "GateController.h"

void GateController::prepare (const juce::dsp::ProcessSpec& spec)
{
    sampleRate = spec.sampleRate;
    stepAmplitudes.fill (1.0f);
    smoothedOutput.reset (sampleRate, kGlideSeconds);
    smoothedOutput.setCurrentAndTargetValue (1.0f);
    reset();
}

void GateController::setStepCount (int count)
{
    stepCount = juce::jlimit (1, kMaxSteps, count);
    // Keep currentStep in range if stepCount was reduced.
    if (currentStep >= stepCount)
        currentStep = 0;
}

void GateController::setDivision (SyncDivision div)
{
    division = div;
}

void GateController::setStepAmplitude (int stepIndex, float amplitude)
{
    if (stepIndex >= 0 && stepIndex < kMaxSteps)
        stepAmplitudes[stepIndex] = juce::jlimit (0.0f, 1.0f, amplitude);
}

float GateController::advance (int numSamples, double bpm)
{
    const double samplesPerStep = computeSamplesPerStep (bpm);

    samplePosition += static_cast<double> (numSamples);

    while (samplePosition >= samplesPerStep)
    {
        samplePosition -= samplesPerStep;
        currentStep = (currentStep + 1) % stepCount;
    }

    smoothedOutput.setTargetValue (stepAmplitudes[currentStep]);
    smoothedOutput.skip (numSamples);

    const float smoothed = smoothedOutput.getCurrentValue();

    currentStepForUI.store      (currentStep,  std::memory_order_relaxed);
    currentStepLevelForUI.store (smoothed,     std::memory_order_relaxed);
    return smoothed;
}

void GateController::reset()
{
    currentStep    = 0;
    samplePosition = 0.0;
    smoothedOutput.setCurrentAndTargetValue (stepAmplitudes[0]);
    currentStepForUI.store (0, std::memory_order_relaxed);
}

double GateController::computeSamplesPerStep (double bpm) const
{
    const float hz = LfoProcessor::syncDivisionToHz (bpm, division);
    if (hz <= 0.0f)
        return sampleRate; // fallback: 1 second per step
    return sampleRate / static_cast<double> (hz);
}
