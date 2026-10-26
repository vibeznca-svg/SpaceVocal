#include "PresetManager.h"
#include "Parameters.h"

namespace sv
{
namespace
{
    struct PV { const char* id; float v; };
    struct Factory { const char* name; std::vector<PV> values; };

    // Valores de partida (reais). Não listados => valor por defeito do parâmetro.
    // NOTA: são pontos de partida a afinar por audição; ver docs/LIMITACOES.md.
    const std::vector<Factory>& factory()
    {
        using namespace id;
        static const std::vector<Factory> f = {
            { "Default", { { compAmt, .40f }, { compThresh, -18 }, { compRatio, 3 }, { compAttack, 8 }, { compRelease, 120 },
                           { magicAmt, .30f }, { revAmt, .20f }, { revType, 2 }, { dlyAmt, .15f }, { dlySync, 1 } } },
            { "Adlibs",  { { compAmt, .55f }, { compThresh, -20 }, { compRatio, 4 }, { compAttack, 5 }, { compRelease, 90 },
                           { magicAmt, .45f }, { magicDrive, .5f }, { magicPres, .6f },
                           { revAmt, .30f }, { revType, 2 }, { revDecay, 1.6f }, { dlyAmt, .30f }, { dlySync, 1 }, { dlyFeedback, .4f } } },
            { "Coral",   { { compAmt, .35f }, { compThresh, -16 }, { compRatio, 2.5f }, { compAttack, 15 }, { compRelease, 200 },
                           { magicAmt, .20f }, { magicTone, .55f },
                           { revAmt, .50f }, { revType, 1 }, { revDecay, 3.5f }, { revPre, 30 }, { revDamp, .45f },
                           { dlyAmt, .10f }, { dlySync, 2 }, { coralOn, 1 }, { coralAmt, .7f } } },
            { "Back Vocal", { { compAmt, .50f }, { magicAmt, .15f }, { magicPres, .3f }, { magicTone, .4f },
                           { revAmt, .35f }, { revType, 2 }, { revDecay, 1.6f }, { revPre, 10 }, { revTone, .4f },
                           { dlyAmt, .20f }, { dlySync, 2 }, { dlyFeedback, .3f }, { dlyHigh, 4000 } } },
            { "Rap / Trap", { { compAmt, .60f }, { compThresh, -22 }, { compRatio, 4 }, { compAttack, 4 }, { compRelease, 80 },
                           { magicAmt, .40f }, { magicDrive, .5f }, { magicPres, .65f },
                           { revAmt, .12f }, { revType, 0 }, { revDecay, .7f }, { dlyAmt, .15f }, { dlySync, 1 }, { dlyFeedback, .3f } } },
            { "Drill",   { { compAmt, .65f }, { compThresh, -22 }, { compRatio, 5 }, { compAttack, 3 }, { compRelease, 70 },
                           { magicAmt, .35f }, { magicPres, .7f }, { magicTone, .55f },
                           { revAmt, .10f }, { revType, 0 }, { revDecay, .6f }, { revPre, 10 },
                           { dlyAmt, .10f }, { dlySync, 0 }, { dlyFeedback, .25f }, { dlyPing, 0 } } },
            { "R&B",     { { compAmt, .40f }, { compRatio, 2.5f }, { compAttack, 12 }, { compRelease, 160 },
                           { magicAmt, .30f }, { magicTone, .45f }, { magicDrive, .3f },
                           { revAmt, .35f }, { revType, 2 }, { revDecay, 2.2f }, { revPre, 25 },
                           { dlyAmt, .25f }, { dlySync, 1 }, { dlyFeedback, .4f }, { dlyHigh, 5000 } } },
            { "Mel\xc3\xb3" "dico", { { compAmt, .40f }, { magicAmt, .30f },
                           { revAmt, .35f }, { revType, 1 }, { revDecay, 2.8f }, { dlyAmt, .20f }, { dlySync, 2 } } },
            { "Clean",   { { compAmt, .25f }, { compThresh, -14 }, { compRatio, 2 }, { compAttack, 15 }, { compRelease, 150 },
                           { magicAmt, .10f }, { revAmt, .10f }, { revType, 2 }, { dlyOn, 0 }, { dlyAmt, 0 } } },
            { "Warm",    { { compAmt, .40f }, { magicAmt, .45f }, { magicDrive, .65f }, { magicTone, .35f }, { magicPres, .35f },
                           { magicHarm, .4f }, { revAmt, .15f }, { revType, 0 }, { dlyAmt, .10f }, { dlySync, 1 } } },
            { "Space",   { { compAmt, .35f }, { magicAmt, .30f }, { magicPres, .55f },
                           { revAmt, .50f }, { revType, 1 }, { revDecay, 4.0f }, { revPre, 35 },
                           { dlyAmt, .35f }, { dlySync, 2 }, { dlyFeedback, .5f }, { dlyPing, 1 } } },
        };
        return f;
    }
} // namespace

PresetManager::PresetManager (juce::AudioProcessorValueTreeState& s, juce::File dir) : apvts (s), userDir (std::move (dir))
{
    rescan();
    if (! apvts.state.hasProperty ("preset")) apvts.state.setProperty ("preset", "Default", nullptr);
}

void PresetManager::rescan()
{
    userNames.clear();
    if (userDir.isDirectory())
        for (auto& f : userDir.findChildFiles (juce::File::findFiles, false, "*.svpreset"))
            userNames.add (f.getFileNameWithoutExtension());
    userNames.sort (true);
}

juce::StringArray PresetManager::getNames() const
{
    juce::StringArray n;
    for (auto& f : factory()) n.add (juce::String::fromUTF8 (f.name));
    for (auto& u : userNames) if (! n.contains (u)) n.add (u);
    return n;
}

int PresetManager::getNumFactory() const { return (int) factory().size(); }

bool PresetManager::isFactory (const juce::String& name) const
{
    for (auto& f : factory()) if (juce::String::fromUTF8 (f.name) == name) return true;
    return false;
}

juce::String PresetManager::getCurrent() const { return apvts.state.getProperty ("preset", "Default").toString(); }
int PresetManager::getCurrentIndex() const { return getNames().indexOf (getCurrent()); }

juce::File PresetManager::fileFor (const juce::String& name) const
{
    return userDir.getChildFile (juce::File::createLegalFileName (name)).withFileExtension ("svpreset");
}

void PresetManager::setParam (const juce::String& pid, float v)
{
    if (auto* p = apvts.getParameter (pid))
    {
        const auto r = p->getNormalisableRange();
        v = juce::jlimit (r.start, r.end, v);
        p->setValueNotifyingHost (p->convertTo0to1 (v));
    }
}

void PresetManager::applyDefaults()
{
    for (auto* p : apvts.processor.getParameters())
        if (auto* r = dynamic_cast<juce::RangedAudioParameter*> (p))
        {
            const auto pid = r->paramID;
            if (pid == id::advanced || pid == id::bypass) continue;   // estado da interface/global, não do preset
            r->setValueNotifyingHost (r->getDefaultValue());
        }
}

juce::Result PresetManager::load (const juce::String& name)
{
    for (auto& f : factory())
        if (juce::String::fromUTF8 (f.name) == name)
        {
            applyDefaults();
            for (auto& v : f.values) setParam (v.id, v.v);
            apvts.state.setProperty ("preset", name, nullptr);
            return juce::Result::ok();
        }
    auto file = fileFor (name);
    if (! file.existsAsFile()) return juce::Result::fail ("Preset não encontrado: " + name);
    auto r = loadFromFile (file);
    if (r.wasOk()) apvts.state.setProperty ("preset", name, nullptr);
    return r;
}

juce::Result PresetManager::applyXml (const juce::XmlElement& xml, bool)
{
    if (! xml.hasTagName ("SpaceVocalPreset")) return juce::Result::fail ("Ficheiro inválido: não é um preset SPACE VOCAL");
    if (xml.getIntAttribute ("version", 0) < 1)  return juce::Result::fail ("Versão de preset incompatível");
    applyDefaults();
    for (auto* e : xml.getChildWithTagNameIterator ("P"))
    {
        const auto pid = e->getStringAttribute ("id");
        const double v = e->getDoubleAttribute ("v", std::numeric_limits<double>::quiet_NaN());
        if (apvts.getParameter (pid) == nullptr || ! std::isfinite (v)) continue;   // ignora desconhecidos / inválidos
        if (pid == id::advanced || pid == id::bypass) continue;
        setParam (pid, (float) v);
    }
    return juce::Result::ok();
}

juce::Result PresetManager::loadFromFile (const juce::File& f)
{
    auto xml = juce::parseXML (f);
    if (xml == nullptr) return juce::Result::fail ("Não foi possível ler o ficheiro");
    auto r = applyXml (*xml, true);
    if (r.wasOk()) apvts.state.setProperty ("preset", xml->getStringAttribute ("name", f.getFileNameWithoutExtension()), nullptr);
    return r;
}

juce::Result PresetManager::saveToFile (const juce::File& f, const juce::String& name) const
{
    juce::XmlElement root ("SpaceVocalPreset");
    root.setAttribute ("version", 1);
    root.setAttribute ("name", name);
    for (auto* p : apvts.processor.getParameters())
        if (auto* r = dynamic_cast<juce::RangedAudioParameter*> (p))
        {
            if (r->paramID == id::advanced || r->paramID == id::bypass) continue;
            auto* e = root.createNewChildElement ("P");
            e->setAttribute ("id", r->paramID);
            e->setAttribute ("v", (double) r->convertFrom0to1 (r->getValue()));
        }
    f.getParentDirectory().createDirectory();
    if (! root.writeTo (f)) return juce::Result::fail ("Não foi possível escrever em " + f.getFullPathName());
    return juce::Result::ok();
}

juce::Result PresetManager::saveUser (const juce::String& nameIn, bool overwrite)
{
    const auto name = nameIn.trim();
    if (name.isEmpty()) return juce::Result::fail ("Indique um nome para o preset");
    if (isFactory (name)) return juce::Result::fail ("Não é possível substituir um preset de fábrica");
    auto f = fileFor (name);
    if (f.existsAsFile() && ! overwrite) return juce::Result::fail ("Já existe um preset com esse nome");
    auto r = saveToFile (f, name);
    if (r.wasOk()) { rescan(); apvts.state.setProperty ("preset", name, nullptr); }
    return r;
}

juce::Result PresetManager::deleteUser (const juce::String& name)
{
    if (isFactory (name)) return juce::Result::fail ("Presets de fábrica não podem ser eliminados");
    auto f = fileFor (name);
    if (! f.existsAsFile()) return juce::Result::fail ("Preset não encontrado");
    if (! f.deleteFile()) return juce::Result::fail ("Não foi possível eliminar o ficheiro");
    rescan();
    return juce::Result::ok();
}

void PresetManager::resetToDefault() { load ("Default"); }

void PresetManager::next()
{
    auto n = getNames(); if (n.isEmpty()) return;
    load (n[(juce::jmax (0, getCurrentIndex()) + 1) % n.size()]);
}

void PresetManager::previous()
{
    auto n = getNames(); if (n.isEmpty()) return;
    load (n[(juce::jmax (0, getCurrentIndex()) - 1 + n.size()) % n.size()]);
}
} // namespace sv
