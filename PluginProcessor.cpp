#include "PluginProcessor.h"
#include "PluginEditor.h"

using namespace sv;

namespace
{
    inline void updatePeak (std::atomic<float>& a, float p)
    {
        float cur = a.load();
        while (p > cur && ! a.compare_exchange_weak (cur, p)) {}
    }
    inline float dbToGain (float db) { return std::pow (10.0f, db * 0.05f); }
}

juce::File SpaceVocalProcessor::defaultUserPresetDir()
{
    return juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
               .getChildFile ("SpaceVocal").getChildFile ("Presets");
}

SpaceVocalProcessor::SpaceVocalProcessor()
    : AudioProcessor (BusesProperties().withInput ("Input", juce::AudioChannelSet::stereo(), true)
                                       .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "STATE", createLayout()),
      presets (apvts, defaultUserPresetDir())
{
}

bool SpaceVocalProcessor::isBusesLayoutSupported (const BusesLayout& l) const
{
    const auto o = l.getMainOutputChannelSet();
    return (o == juce::AudioChannelSet::mono() || o == juce::AudioChannelSet::stereo()) && o == l.getMainInputChannelSet();
}

void SpaceVocalProcessor::prepareToPlay (double sr, int block)
{
    sampleRate = sr;
    scratch.setSize (14, juce::jmax (block, 64), false, true, false);
    comp.prepare (sr); magic.prepare (sr); reverb.prepare (sr); delay.prepare (sr); coral.prepare (sr);

    auto init = [&] (Smooth& s, float val, double sec) { s.reset (sr, sec); s.setCurrentAndTargetValue (val); };
    init (inG, dbToGain (v (id::inGain)), 0.02);
    init (outG, dbToGain (v (id::outGain)), 0.02);
    init (compMix, on (id::compOn) ? 1.f : 0.f, 0.03);
    init (magicMix, on (id::magicOn) ? 1.f : 0.f, 0.03);
    init (bypassMix, on (id::bypass) ? 1.f : 0.f, 0.03);
    init (revIn, on (id::revOn) ? 1.f : 0.f, 0.05);
    init (revOut, 0.f, 0.05);
    init (dlyIn, on (id::dlyOn) ? 1.f : 0.f, 0.05);
    init (dlyOut, 0.f, 0.05);
    init (coralIn, on (id::coralOn) ? 1.f : 0.f, 0.05);
    init (coralOut, 0.f, 0.05);
    revOut.setCurrentAndTargetValue (0.8f * std::pow (v (id::revAmt), 1.5f) * v (id::revMix));
    dlyOut.setCurrentAndTargetValue (v (id::dlyAmt) * v (id::dlyMix));
}

double SpaceVocalProcessor::currentBpm()
{
    if (auto* ph = getPlayHead())
        if (auto pos = ph->getPosition())
            if (auto bpm = pos->getBpm())
                if (*bpm > 20.0 && *bpm < 400.0) { meters.bpm.store ((float) *bpm); return *bpm; }
    return 120.0;   // fallback documentado quando o anfitrião não fornece o tempo
}

void SpaceVocalProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    const int total = buffer.getNumSamples();
    const int nIn = getTotalNumInputChannels(), nOut = getTotalNumOutputChannels();
    for (int c = nIn; c < nOut; ++c) buffer.clear (c, 0, total);
    if (total == 0 || nOut == 0) return;

    float* l = buffer.getWritePointer (0);
    float* r = nOut > 1 ? buffer.getWritePointer (1) : nullptr;
    const int chunk = scratch.getNumSamples();
    for (int start = 0; start < total; start += chunk)
        processChunk (l + start, r ? r + start : nullptr, juce::jmin (chunk, total - start));
}

void SpaceVocalProcessor::processChunk (float* outL, float* outR, int n)
{
    float* wl = scratch.getWritePointer (0);  float* wr = scratch.getWritePointer (1);
    float* dl = scratch.getWritePointer (2);  float* dr = scratch.getWritePointer (3);
    float* tl = scratch.getWritePointer (4);  float* tr = scratch.getWritePointer (5);
    float* sl = scratch.getWritePointer (6);  float* sr = scratch.getWritePointer (7);
    float* rl = scratch.getWritePointer (8);  float* rr = scratch.getWritePointer (9);
    float* el = scratch.getWritePointer (10); float* er = scratch.getWritePointer (11);
    float* cl = scratch.getWritePointer (12); float* cr = scratch.getWritePointer (13);

    const float* inR = outR ? outR : outL;
    for (int i = 0; i < n; ++i) { wl[i] = dl[i] = outL[i]; wr[i] = dr[i] = inR[i]; }

    // 1) INPUT GAIN + medidor de entrada (pós-ganho)
    inG.setTargetValue (dbToGain (v (id::inGain)));
    float pl = 0, pr = 0;
    for (int i = 0; i < n; ++i)
    {
        const float g = inG.getNextValue();
        wl[i] *= g; wr[i] *= g;
        pl = std::max (pl, std::abs (wl[i])); pr = std::max (pr, std::abs (wr[i]));
    }
    updatePeak (meters.inL, pl); updatePeak (meters.inR, pr);
    if (std::max (pl, pr) >= 1.0f) meters.inClip.store (true);

    // 2) COMP (amount escala o ratio; makeup automático moderado + makeup manual)
    compMix.setTargetValue (on (id::compOn) ? 1.f : 0.f);
    if (compMix.isSmoothing() || compMix.getTargetValue() > 0.f)
    {
        const float amt = v (id::compAmt), thr = v (id::compThresh);
        const float ratio = 1.0f + (v (id::compRatio) - 1.0f) * juce::jlimit (0.0f, 1.5f, amt * 2.0f);
        const float autoMakeup = 0.5f * std::max (0.0f, -6.0f - thr) * (1.0f - 1.0f / ratio);
        comp.setParams (thr, ratio, v (id::compAttack), v (id::compRelease), v (id::compKnee), v (id::compMakeup) + autoMakeup);
        for (int i = 0; i < n; ++i) { tl[i] = wl[i]; tr[i] = wr[i]; }
        comp.process (tl, tr, n);
        for (int i = 0; i < n; ++i) { const float m = compMix.getNextValue(); wl[i] += (tl[i] - wl[i]) * m; wr[i] += (tr[i] - wr[i]) * m; }
        meters.gr.store (comp.getGainReductionDb() * compMix.getTargetValue());
    }
    else { comp.reset(); meters.gr.store (0.f); }

    // 3) MAGIC
    magicMix.setTargetValue (on (id::magicOn) ? 1.f : 0.f);
    if (magicMix.isSmoothing() || magicMix.getTargetValue() > 0.f)
    {
        magic.setParams (v (id::magicAmt), v (id::magicDrive), v (id::magicPres), v (id::magicTone), v (id::magicHarm), v (id::magicMix));
        for (int i = 0; i < n; ++i) { tl[i] = wl[i]; tr[i] = wr[i]; }
        magic.process (tl, tr, n);
        for (int i = 0; i < n; ++i) { const float m = magicMix.getNextValue(); wl[i] += (tl[i] - wl[i]) * m; wr[i] += (tr[i] - wr[i]) * m; }
    }
    else magic.reset();

    // 4) Envios paralelos: REVERB, DELAY, CORAL (a entrada é fechada com rampa; a cauda continua)
    revIn.setTargetValue (on (id::revOn) ? 1.f : 0.f);
    revOut.setTargetValue (0.8f * std::pow (v (id::revAmt), 1.5f) * v (id::revMix));
    reverb.setParams ((int) v (id::revType), v (id::revDecay), v (id::revPre), v (id::revDamp), v (id::revTone), v (id::revWidth));
    for (int i = 0; i < n; ++i) { const float g = revIn.getNextValue(); sl[i] = wl[i] * g; sr[i] = wr[i] * g; }
    reverb.process (sl, sr, rl, rr, n);

    const float beats[3] = { 0.25f, 0.5f, 1.0f };
    const double bpm = currentBpm();
    float timeMs = on (id::dlyHostSync) ? (float) (60000.0 / bpm * beats[juce::jlimit (0, 2, (int) v (id::dlySync))]) : v (id::dlyTimeMs);
    timeMs = juce::jlimit (10.0f, 1500.0f, timeMs);
    dlyIn.setTargetValue (on (id::dlyOn) ? 1.f : 0.f);
    dlyOut.setTargetValue (v (id::dlyAmt) * v (id::dlyMix));
    delay.setParams (timeMs, v (id::dlyFeedback), v (id::dlyLow), v (id::dlyHigh), on (id::dlyPing));
    for (int i = 0; i < n; ++i) { const float g = dlyIn.getNextValue(); sl[i] = wl[i] * g; sr[i] = wr[i] * g; }
    delay.process (sl, sr, el, er, n);

    coralIn.setTargetValue (on (id::coralOn) ? 1.f : 0.f);
    coralOut.setTargetValue (0.7f * v (id::coralAmt));
    for (int i = 0; i < n; ++i) { const float g = coralIn.getNextValue(); sl[i] = wl[i] * g; sr[i] = wr[i] * g; }
    coral.process (sl, sr, cl, cr, n);

    // 5) soma + OUTPUT GAIN + bypass geral (crossfade)
    outG.setTargetValue (dbToGain (v (id::outGain)));
    bypassMix.setTargetValue (on (id::bypass) ? 1.f : 0.f);
    float ql = 0, qr = 0;
    for (int i = 0; i < n; ++i)
    {
        const float ro = revOut.getNextValue(), dO = dlyOut.getNextValue(), co = coralOut.getNextValue();
        const float g = outG.getNextValue(), b = bypassMix.getNextValue();
        float yl = (wl[i] + rl[i] * ro + el[i] * dO + cl[i] * co) * g;
        float yr = (wr[i] + rr[i] * ro + er[i] * dO + cr[i] * co) * g;
        yl = yl * (1.f - b) + dl[i] * b; yr = yr * (1.f - b) + dr[i] * b;
        if (! std::isfinite (yl)) yl = 0.f;
        if (! std::isfinite (yr)) yr = 0.f;
        ql = std::max (ql, std::abs (yl)); qr = std::max (qr, std::abs (yr));
        if (outR) { outL[i] = yl; outR[i] = yr; } else outL[i] = 0.5f * (yl + yr);
    }
    updatePeak (meters.outL, ql); updatePeak (meters.outR, qr);
    if (std::max (ql, qr) >= 1.0f) meters.outClip.store (true);
}

juce::AudioProcessorEditor* SpaceVocalProcessor::createEditor()
{
#if SV_NO_EDITOR
    return nullptr;
#else
    return new SpaceVocalEditor (*this);
#endif
}

void SpaceVocalProcessor::getStateInformation (juce::MemoryBlock& dest)
{
    if (auto xml = apvts.copyState().createXml()) copyXmlToBinary (*xml, dest);
}

void SpaceVocalProcessor::setStateInformation (const void* data, int size)
{
    if (auto xml = getXmlFromBinary (data, size))
        if (xml->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new SpaceVocalProcessor(); }
