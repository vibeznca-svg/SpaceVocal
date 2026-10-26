#pragma once
#include <juce_audio_processors/juce_audio_processors.h>

namespace sv
{
namespace id
{
    // Globais
    inline constexpr auto bypass      = "bypass";
    inline constexpr auto advanced    = "advanced";
    inline constexpr auto inGain      = "inGain";
    inline constexpr auto outGain     = "outGain";
    // Comp
    inline constexpr auto compOn      = "compOn";
    inline constexpr auto compAmt     = "compAmt";
    inline constexpr auto compThresh  = "compThresh";
    inline constexpr auto compRatio   = "compRatio";
    inline constexpr auto compAttack  = "compAttack";
    inline constexpr auto compRelease = "compRelease";
    inline constexpr auto compKnee    = "compKnee";
    inline constexpr auto compMakeup  = "compMakeup";
    // Magic
    inline constexpr auto magicOn     = "magicOn";
    inline constexpr auto magicAmt    = "magicAmt";
    inline constexpr auto magicDrive  = "magicDrive";
    inline constexpr auto magicPres   = "magicPres";
    inline constexpr auto magicTone   = "magicTone";
    inline constexpr auto magicHarm   = "magicHarm";
    inline constexpr auto magicMix    = "magicMix";
    // Reverb
    inline constexpr auto revOn       = "revOn";
    inline constexpr auto revAmt      = "revAmt";
    inline constexpr auto revType     = "revType";
    inline constexpr auto revDecay    = "revDecay";
    inline constexpr auto revPre      = "revPre";
    inline constexpr auto revDamp     = "revDamp";
    inline constexpr auto revTone     = "revTone";
    inline constexpr auto revWidth    = "revWidth";
    inline constexpr auto revMix      = "revMix";
    // Delay
    inline constexpr auto dlyOn       = "dlyOn";
    inline constexpr auto dlyAmt      = "dlyAmt";
    inline constexpr auto dlySync     = "dlySync";
    inline constexpr auto dlyHostSync = "dlyHostSync";
    inline constexpr auto dlyTimeMs   = "dlyTimeMs";
    inline constexpr auto dlyFeedback = "dlyFeedback";
    inline constexpr auto dlyLow      = "dlyLow";
    inline constexpr auto dlyHigh     = "dlyHigh";
    inline constexpr auto dlyPing     = "dlyPing";
    inline constexpr auto dlyMix      = "dlyMix";
    // Coral (espessura/amplitude a partir de uma única voz)
    inline constexpr auto coralOn     = "coralOn";
    inline constexpr auto coralAmt    = "coralAmt";
}

inline juce::AudioProcessorValueTreeState::ParameterLayout createLayout()
{
    using namespace juce;
    using F = AudioParameterFloat;
    using B = AudioParameterBool;
    using C = AudioParameterChoice;
    std::vector<std::unique_ptr<RangedAudioParameter>> p;

    auto pct = [](float v, int) { return String(roundToInt(v * 100.0f)) + " %"; };
    auto db  = [](float v, int) { return String(v, 1) + " dB"; };
    auto ms  = [](float v, int) { return String(v, v < 10.0f ? 1 : 0) + " ms"; };
    auto amt = [&](const char* i, const char* n, float def)
    {
        p.push_back(std::make_unique<F>(ParameterID{ i, 1 }, n, NormalisableRange<float>(0.f, 1.f, 0.001f), def,
                                        AudioParameterFloatAttributes().withStringFromValueFunction(pct)));
    };
    auto flt = [&](const char* i, const char* n, NormalisableRange<float> r, float def, auto fmt)
    {
        p.push_back(std::make_unique<F>(ParameterID{ i, 1 }, n, r, def,
                                        AudioParameterFloatAttributes().withStringFromValueFunction(fmt)));
    };
    auto onoff = [&](const char* i, const char* n, bool def) { p.push_back(std::make_unique<B>(ParameterID{ i, 1 }, n, def)); };

    onoff(id::bypass, "Bypass", false);
    onoff(id::advanced, "Advanced", false);
    flt(id::inGain,  "Input Gain",  { -24.f, 24.f, 0.1f }, 0.f, db);
    flt(id::outGain, "Output Gain", { -24.f, 12.f, 0.1f }, 0.f, db);

    onoff(id::compOn, "Comp On", true);
    amt(id::compAmt, "Comp Amount", 0.4f);
    flt(id::compThresh, "Comp Threshold", { -48.f, 0.f, 0.1f }, -18.f, db);
    flt(id::compRatio, "Comp Ratio", { 1.f, 20.f, 0.1f, 0.5f }, 3.f,
        [](float v, int) { return String(v, 1) + ":1"; });
    flt(id::compAttack, "Comp Attack", { 0.1f, 100.f, 0.01f, 0.4f }, 8.f, ms);
    flt(id::compRelease, "Comp Release", { 10.f, 1000.f, 0.1f, 0.4f }, 120.f, ms);
    flt(id::compKnee, "Comp Knee", { 0.f, 24.f, 0.1f }, 6.f, db);
    flt(id::compMakeup, "Comp Makeup", { 0.f, 18.f, 0.1f }, 0.f, db);

    onoff(id::magicOn, "Magic On", true);
    amt(id::magicAmt, "Magic Amount", 0.3f);
    amt(id::magicDrive, "Magic Drive", 0.4f);
    amt(id::magicPres, "Magic Presence", 0.5f);
    amt(id::magicTone, "Magic Tone", 0.5f);
    amt(id::magicHarm, "Magic Harmonics", 0.5f);
    amt(id::magicMix, "Magic Mix", 1.0f);

    onoff(id::revOn, "Reverb On", true);
    amt(id::revAmt, "Reverb Amount", 0.2f);
    p.push_back(std::make_unique<C>(ParameterID{ id::revType, 1 }, "Reverb Type", StringArray{ "ROOM", "HALL", "PLATE" }, 2));
    flt(id::revDecay, "Reverb Decay", { 0.2f, 8.f, 0.01f, 0.5f }, 1.8f,
        [](float v, int) { return String(v, 2) + " s"; });
    flt(id::revPre, "Reverb Pre-delay", { 0.f, 150.f, 0.1f }, 20.f, ms);
    amt(id::revDamp, "Reverb Damping", 0.5f);
    amt(id::revTone, "Reverb Tone", 0.5f);
    amt(id::revWidth, "Reverb Width", 1.0f);
    amt(id::revMix, "Reverb Mix", 1.0f);

    onoff(id::dlyOn, "Delay On", true);
    amt(id::dlyAmt, "Delay Amount", 0.2f);
    p.push_back(std::make_unique<C>(ParameterID{ id::dlySync, 1 }, "Delay Sync", StringArray{ "1/16", "1/8", "1/4" }, 1));
    onoff(id::dlyHostSync, "Delay Host Sync", true);
    flt(id::dlyTimeMs, "Delay Time", { 10.f, 1500.f, 0.1f, 0.5f }, 350.f, ms);
    amt(id::dlyFeedback, "Delay Feedback", 0.35f);
    flt(id::dlyLow, "Delay Low Cut", { 20.f, 1000.f, 1.f, 0.4f }, 120.f,
        [](float v, int) { return String(roundToInt(v)) + " Hz"; });
    flt(id::dlyHigh, "Delay High Cut", { 1000.f, 20000.f, 1.f, 0.4f }, 6000.f,
        [](float v, int) { return String(roundToInt(v)) + " Hz"; });
    onoff(id::dlyPing, "Delay Ping-Pong", true);
    amt(id::dlyMix, "Delay Mix", 1.0f);

    onoff(id::coralOn, "Coral On", false);
    amt(id::coralAmt, "Coral Amount", 0.0f);

    return { p.begin(), p.end() };
}
} // namespace sv
