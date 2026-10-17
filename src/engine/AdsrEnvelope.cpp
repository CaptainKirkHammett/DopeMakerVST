#include "AdsrEnvelope.h"

void AdsrEnvelope::prepare (double sampleRate)
{
    currentSampleRate = sampleRate;
    adsr.setSampleRate (sampleRate);
    adsr.reset();
}

void AdsrEnvelope::setParameters (float attackMs, float decayMs,
                                   float sustainLevel, float releaseMs)
{
    juce::ADSR::Parameters p;
    p.attack  = attackMs  * 0.001f;
    p.decay   = decayMs   * 0.001f;
    p.sustain = sustainLevel;
    p.release = releaseMs * 0.001f;
    adsr.setParameters (p);
}

void AdsrEnvelope::noteOn()  { adsr.noteOn(); }
void AdsrEnvelope::noteOff() { adsr.noteOff(); }
void AdsrEnvelope::reset()   { adsr.reset(); }

float AdsrEnvelope::getNextSample() { return adsr.getNextSample(); }

bool AdsrEnvelope::isActive() const { return adsr.isActive(); }
