#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor.h"

class SpaceVocalEditor : public juce::AudioProcessorEditor
{
public:
    explicit SpaceVocalEditor (SpaceVocalProcessor&);
    ~SpaceVocalEditor() override;
    void resized() override;
    void paint (juce::Graphics&) override;

private:
    struct Content;
    std::unique_ptr<Content> content;
    juce::TooltipWindow tooltips { this, 600 };
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SpaceVocalEditor)
};
