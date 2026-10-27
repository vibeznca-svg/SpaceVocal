#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "Parameters.h"
#include "PresetManager.h"
#include "Compressor.h"
#include "Enhancer.h"
#include "SpaceReverb.h"
#include "TempoDelay.h"
#include "Coral.h"

class SpaceVocalProcessor : public juce::AudioProcessor
{
public:
    SpaceVocalProcessor();
    ~SpaceVocalProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout&) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "SPACE VOCAL"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    double getTailLengthSeconds() const override { return 10.0; }
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    juce::AudioProcessorValueTreeState apvts;
    sv::PresetManager presets;

    // Dados para a interface (escritos pela thread de áudio, lidos/consumidos pela GUI)
    struct Meters
    {
        std::atomic<float> inL { 0.f }, inR { 0.f }, outL { 0.f }, outR { 0.f }, gr { 0.f }, bpm { 120.f };
        std::atomic<bool> inClip { false }, outClip { false };
    } meters;

    static juce::File defaultUserPresetDir();

private:
    void processChunk (float* l, float* r, int n);
    float v (const char* id) const { return apvts.getRawParameterValue (id)->load(); }
    bool  on (const char* id) const { return v (id) > 0.5f; }
    double currentBpm();

    using Smooth = juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear>;
    Smooth inG, outG, compMix, magicMix, bypassMix, revIn, revOut, dlyIn, dlyOut, coralIn, coralOut;

    sv::Compressor comp;
    sv::Enhancer magic;
    sv::SpaceReverb reverb;
    sv::TempoDelay delay;
    sv::Coral coral;

    juce::AudioBuffer<float> scratch;   // 14 canais: wl wr dl dr tl tr sl sr rl rr el er cl cr
    double sampleRate = 44100.0;
};

