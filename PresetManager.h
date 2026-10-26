#pragma once
#include <juce_audio_processors/juce_audio_processors.h>

namespace sv
{
// Gestão de presets: de fábrica (compilados no binário) e de utilizador (XML local, offline).
class PresetManager
{
public:
    PresetManager (juce::AudioProcessorValueTreeState& state, juce::File userDir);

    void setUserDirectory (juce::File dir) { userDir = std::move (dir); rescan(); }
    juce::File getUserDirectory() const { return userDir; }

    void rescan();
    juce::StringArray getNames() const;        // fábrica primeiro, depois utilizador
    int  getNumFactory() const;
    bool isFactory (const juce::String& name) const;
    juce::String getCurrent() const;
    int  getCurrentIndex() const;

    juce::Result load (const juce::String& name);
    juce::Result saveUser (const juce::String& name, bool overwrite);
    juce::Result deleteUser (const juce::String& name);
    juce::Result loadFromFile (const juce::File& f);
    juce::Result saveToFile (const juce::File& f, const juce::String& name) const;
    void resetToDefault();
    void next();
    void previous();

private:
    struct Value { const char* id; float value; };
    void applyDefaults();
    void setParam (const juce::String& id, float realValue);
    juce::Result applyXml (const juce::XmlElement& xml, bool setName);
    juce::File fileFor (const juce::String& name) const;

    juce::AudioProcessorValueTreeState& apvts;
    juce::File userDir;
    juce::StringArray userNames;
};
} // namespace sv
