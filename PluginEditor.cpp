#include "PluginEditor.h"

using namespace juce;
namespace
{
    namespace C
    {
        const Colour bg0 (0xff050608), bg1 (0xff0a0d12), module (0xff10141a), raised (0xff151a22), line (0xff252d38),
                     text (0xfff4f6fa), text2 (0xff98a2b2), red (0xffff243a), red2 (0xffff102a), redDark (0xff8e0d20);
    }
    constexpr float W = 1200.f, H = 760.f;

    Font font (float size, bool bold = false) { return Font (FontOptions (size, bold ? Font::bold : Font::plain)); }

    //==========================================================================
    struct SvLookAndFeel : LookAndFeel_V4
    {
        SvLookAndFeel()
        {
            setColour (Slider::textBoxTextColourId, C::text);
            setColour (Slider::textBoxOutlineColourId, Colours::transparentBlack);
            setColour (Slider::textBoxBackgroundColourId, Colours::transparentBlack);
            setColour (TextButton::textColourOffId, C::text2);
            setColour (TextButton::textColourOnId, C::text);
            setColour (ComboBox::backgroundColourId, C::raised);
            setColour (ComboBox::outlineColourId, C::line);
            setColour (ComboBox::textColourId, C::text);
            setColour (ComboBox::arrowColourId, C::red);
            setColour (PopupMenu::backgroundColourId, C::raised);
            setColour (PopupMenu::textColourId, C::text);
            setColour (PopupMenu::highlightedBackgroundColourId, C::redDark);
            setColour (TooltipWindow::backgroundColourId, C::raised);
            setColour (TooltipWindow::textColourId, C::text);
            setColour (TooltipWindow::outlineColourId, C::line);
            setColour (Label::textColourId, C::text2);
        }

        void drawRotarySlider (Graphics& g, int x, int y, int w, int h, float pos, float a0, float a1, Slider& s) override
        {
            auto b = Rectangle<float> ((float) x, (float) y, (float) w, (float) h).reduced (4.f);
            const float d = jmin (b.getWidth(), b.getHeight());
            auto r = Rectangle<float> (d, d).withCentre (b.getCentre());
            const bool en = s.isEnabled();
            const float th = jmax (2.5f, d * 0.055f);
            const auto arcR = r.reduced (th);
            Path track; track.addCentredArc (arcR.getCentreX(), arcR.getCentreY(), arcR.getWidth() / 2, arcR.getHeight() / 2, 0, a0, a1, true);
            g.setColour (C::line); g.strokePath (track, PathStrokeType (th, PathStrokeType::curved, PathStrokeType::rounded));
            const float ang = a0 + pos * (a1 - a0);
            Path val; val.addCentredArc (arcR.getCentreX(), arcR.getCentreY(), arcR.getWidth() / 2, arcR.getHeight() / 2, 0, a0, ang, true);
            if (en)
            {
                g.setColour (C::red.withAlpha (0.22f)); g.strokePath (val, PathStrokeType (th * 2.4f, PathStrokeType::curved, PathStrokeType::rounded));
                g.setColour (C::red); g.strokePath (val, PathStrokeType (th, PathStrokeType::curved, PathStrokeType::rounded));
            }
            auto body = r.reduced (th * 2.6f);
            g.setGradientFill (ColourGradient (Colour (0xff3a424e), body.getX(), body.getY(), Colour (0xff12161c), body.getRight(), body.getBottom(), false));
            g.fillEllipse (body);
            g.setColour (Colour (0xff0a0c10)); g.drawEllipse (body, 1.5f);
            g.setColour (Colours::white.withAlpha (0.08f)); g.drawEllipse (body.reduced (1.5f), 1.f);
            const float pl = body.getWidth() * 0.5f;
            Point<float> c = body.getCentre();
            Line<float> ptr (c.getPointOnCircumference (pl * 0.45f, ang), c.getPointOnCircumference (pl * 0.92f, ang));
            g.setColour (en ? C::red : C::text2); g.drawLine (ptr, jmax (2.f, th * 0.8f));
            if (s.hasKeyboardFocus (false)) { g.setColour (C::red.withAlpha (0.5f)); g.drawEllipse (r, 1.f); }
        }

        void drawButtonBackground (Graphics& g, Button& b, const Colour&, bool hover, bool down) override
        {
            auto r = b.getLocalBounds().toFloat().reduced (0.5f);
            const bool on = b.getToggleState(), en = b.isEnabled();
            Colour fill = on ? C::redDark.withAlpha (0.55f) : C::raised;
            if (hover && en) fill = fill.brighter (0.12f);
            if (down) fill = fill.darker (0.2f);
            g.setColour (en ? fill : fill.withAlpha (0.4f)); g.fillRoundedRectangle (r, 5.f);
            g.setColour (on ? C::red : C::line); g.drawRoundedRectangle (r, 5.f, on ? 1.6f : 1.f);
            if (on && en) { g.setColour (C::red.withAlpha (0.18f)); g.drawRoundedRectangle (r.expanded (1.5f), 6.f, 2.f); }
        }

        Font getTextButtonFont (TextButton&, int h) override { return font (jmin (13.f, h * 0.42f), true); }
    };

    //==========================================================================
    struct Knob : Component
    {
        Knob (AudioProcessorValueTreeState& s, const char* pid, const String& name, const String& tip)
        {
            slider.setSliderStyle (Slider::RotaryHorizontalVerticalDrag);
            slider.setTextBoxStyle (Slider::TextBoxBelow, false, 64, 16);
            slider.setTooltip (tip);
            slider.setTitle (name);
            addAndMakeVisible (slider);
            label.setText (name, dontSendNotification);
            label.setJustificationType (Justification::centred);
            label.setFont (font (11.f, true));
            label.setInterceptsMouseClicks (false, false);
            addAndMakeVisible (label);
            att = std::make_unique<AudioProcessorValueTreeState::SliderAttachment> (s, pid, slider);
        }
        void resized() override { auto b = getLocalBounds(); label.setBounds (b.removeFromBottom (14)); slider.setBounds (b); }
        Slider slider; Label label;
        std::unique_ptr<AudioProcessorValueTreeState::SliderAttachment> att;
    };

    struct ToggleBtn : TextButton
    {
        ToggleBtn (AudioProcessorValueTreeState& s, const char* pid, const String& txt, const String& tip)
        {
            setButtonText (txt); setClickingTogglesState (true); setTooltip (tip); setTitle (txt);
            att = std::make_unique<AudioProcessorValueTreeState::ButtonAttachment> (s, pid, *this);
        }
        std::unique_ptr<AudioProcessorValueTreeState::ButtonAttachment> att;
    };

    // Botões exclusivos ligados a um parâmetro de escolha (ROOM/HALL/PLATE, 1/16 1/8 1/4)
    struct ChoiceButtons : Component
    {
        ChoiceButtons (AudioProcessorValueTreeState& s, const char* pid, StringArray names, const String& tip)
            : att (*s.getParameter (pid), [this] (float v) { setSel ((int) std::round (v)); })
        {
            for (int i = 0; i < names.size(); ++i)
            {
                auto* b = btns.add (new TextButton (names[i]));
                b->setClickingTogglesState (false); b->setTooltip (tip + ": " + names[i]); b->setTitle (names[i]);
                b->onClick = [this, i] { att.setValueAsCompleteGesture ((float) i); };
                addAndMakeVisible (b);
            }
            att.sendInitialUpdate();
        }
        void setSel (int i) { for (int k = 0; k < btns.size(); ++k) btns[k]->setToggleState (k == i, dontSendNotification); }
        void resized() override
        {
            auto b = getLocalBounds(); const int w = b.getWidth() / jmax (1, btns.size());
            for (auto* x : btns) x->setBounds (b.removeFromLeft (w).reduced (2, 0));
        }
        OwnedArray<TextButton> btns; ParameterAttachment att;
    };

    //==========================================================================
    struct LevelMeter : Component, SettableTooltipClient, Timer
    {
        LevelMeter (std::atomic<float>& l, std::atomic<float>& r, std::atomic<bool>& clipFlag, const String& tip)
            : a { &l, &r }, clip (clipFlag)
        { setTooltip (tip + " (clique para limpar o indicador de clipping)"); startTimerHz (30); }

        void timerCallback() override
        {
            for (int c = 0; c < 2; ++c)
            {
                const float p = a[c]->exchange (0.f);
                const float db = p > 1.0e-5f ? Decibels::gainToDecibels (p) : -100.f;
                disp[c] = db > disp[c] ? db : disp[c] - 1.5f;
                if (db >= hold[c]) { hold[c] = db; holdT[c] = 45; } else if (--holdT[c] <= 0) hold[c] = jmax (-100.f, hold[c] - 1.f);
            }
            repaint();
        }
        void mouseDown (const MouseEvent&) override { clip.store (false); hold[0] = hold[1] = -100.f; }

        void paint (Graphics& g) override
        {
            auto b = getLocalBounds();
            auto top = b.removeFromTop (34); auto bot = b.removeFromBottom (16);
            const float mx = jmax (hold[0], hold[1]);
            g.setColour (clip.load() ? C::red2 : C::line); g.fillRoundedRectangle (top.removeFromTop (8).reduced (8, 1).toFloat(), 2.f);
            g.setColour (clip.load() ? C::red : C::text); g.setFont (font (12.f, true));
            g.drawText (mx > -99.f ? String (mx, 1) : String ("-inf"), top, Justification::centred);
            g.setColour (C::text2); g.setFont (font (11.f, true));
            const int bw = 18;
            Rectangle<int> barsArea = b.withWidth (bw * 2 + 6).withX (b.getX() + 2);
            g.drawText ("L", barsArea.withHeight (16).withY (bot.getY()).withWidth (bw).withX (barsArea.getX()), Justification::centred);
            g.drawText ("R", barsArea.withHeight (16).withY (bot.getY()).withWidth (bw).withX (barsArea.getX() + bw + 6), Justification::centred);
            const int nseg = 32;
            for (int c = 0; c < 2; ++c)
            {
                auto bar = Rectangle<int> (barsArea.getX() + c * (bw + 6), b.getY(), bw, b.getHeight());
                const float segH = bar.getHeight() / (float) nseg;
                for (int s = 0; s < nseg; ++s)
                {
                    const float segDb = -60.f + 60.f * (s + 1) / nseg;
                    const bool lit = disp[c] >= segDb - 60.f / nseg * 0.5f;
                    Colour col = segDb > -3.f ? C::red : segDb > -12.f ? Colour (0xffe8c33a) : Colour (0xff2fd27a);
                    auto r = Rectangle<float> ((float) bar.getX(), bar.getBottom() - (s + 1) * segH + 1.f, (float) bar.getWidth(), segH - 1.5f);
                    g.setColour (lit ? col : col.withAlpha (0.12f)); g.fillRect (r);
                }
            }
            g.setColour (C::text2); g.setFont (font (9.5f));
            const float marks[] = { 0.f, -6.f, -12.f, -24.f, -48.f };
            for (float m : marks)
            {
                const float y = b.getBottom() - b.getHeight() * (m + 60.f) / 60.f;
                g.drawText (String ((int) m), barsArea.getRight() + 2, (int) y - 6, 28, 12, Justification::left);
            }
        }
        std::atomic<float>* a[2]; std::atomic<bool>& clip;
        float disp[2] = { -100.f, -100.f }, hold[2] = { -100.f, -100.f }; int holdT[2] = { 0, 0 };
    };

    //==========================================================================
    struct ModuleInfo
    {
        String title, desc, hint; Rectangle<int> area; ToggleBtn* power = nullptr; Array<Component*> dim, advanced;
    };
}

//==============================================================================
struct SpaceVocalEditor::Content : Component, Timer
{
    SpaceVocalProcessor& p;
    SvLookAndFeel laf;
    AudioProcessorValueTreeState& s;

    // cabeçalho
    TextButton prev { "<" }, next { ">" }, save { "SAVE" }, load { "LOAD" }, del { "DEL" }, simple { "SIMPLE" }, adv { "ADVANCED" };
    ComboBox presetBox;
    ToggleBtn bypass;
    ParameterAttachment advAtt;
    // knobs
    Knob inG, compA, magicA, revA, dlyA, outG, coralK;
    Knob cThr, cRat, cAtk, cRel, cKnee, cMk, mDrv, mPres, mTone, mHarm, mMix, rDec, rPre, rDmp, rTone, rWid, rMix, dTime, dFb, dLow, dHigh, dMix;
    ToggleBtn compOn, magicOn, revOn, dlyOn, coralOn, dHost, dPing;
    ChoiceButtons revType, dlySync;
    LevelMeter inM, outM;
    Label status, grLabel, bpmLabel;
    std::unique_ptr<FileChooser> chooser;
    OwnedArray<TextButton> chips;
    Array<ModuleInfo> mods;
    String shownPreset; bool advMode = false;

    static constexpr int modY = 104, modH = 440;

    explicit Content (SpaceVocalProcessor& proc)
        : p (proc), s (proc.apvts),
          bypass (s, sv::id::bypass, "BYPASS", "Bypass geral: ouve o sinal original"),
          advAtt (*s.getParameter (sv::id::advanced), [this] (float v) { setAdvanced (v > 0.5f); }),
          inG (s, sv::id::inGain, "INPUT GAIN", "Ganho de entrada (-24 a +24 dB), aplicado antes dos efeitos"),
          compA (s, sv::id::compAmt, "AMOUNT", "Intensidade da compressão"),
          magicA (s, sv::id::magicAmt, "AMOUNT", "Intensidade do realce vocal (saturação suave, presença, harmónicos)"),
          revA (s, sv::id::revAmt, "AMOUNT", "Quantidade de reverb"),
          dlyA (s, sv::id::dlyAmt, "AMOUNT", "Quantidade de eco"),
          outG (s, sv::id::outGain, "OUTPUT GAIN", "Ganho de saída (-24 a +12 dB)"),
          coralK (s, sv::id::coralAmt, "CORAL", "Espessura estéreo a partir de uma única voz (não cria vozes reais)"),
          cThr (s, sv::id::compThresh, "THRESH", "Threshold"), cRat (s, sv::id::compRatio, "RATIO", "Ratio"),
          cAtk (s, sv::id::compAttack, "ATTACK", "Attack"), cRel (s, sv::id::compRelease, "RELEASE", "Release"),
          cKnee (s, sv::id::compKnee, "KNEE", "Joelho"), cMk (s, sv::id::compMakeup, "MAKEUP", "Makeup manual (somado ao automático)"),
          mDrv (s, sv::id::magicDrive, "DRIVE", "Saturation drive"), mPres (s, sv::id::magicPres, "PRESENCE", "Presença (~3.5 kHz)"),
          mTone (s, sv::id::magicTone, "TONE", "Tom: escuro <-> brilhante"), mHarm (s, sv::id::magicHarm, "HARMONICS", "Intensidade harmónica"),
          mMix (s, sv::id::magicMix, "MIX", "Mistura dry/wet"),
          rDec (s, sv::id::revDecay, "DECAY", "Tempo de decay"), rPre (s, sv::id::revPre, "PRE-DLY", "Pre-delay"),
          rDmp (s, sv::id::revDamp, "DAMP", "Amortecimento de agudos"), rTone (s, sv::id::revTone, "TONE", "Tom da reverb"),
          rWid (s, sv::id::revWidth, "WIDTH", "Largura estéreo"), rMix (s, sv::id::revMix, "MIX", "Nível wet"),
          dTime (s, sv::id::dlyTimeMs, "TIME", "Tempo manual (quando HOST SYNC está desligado)"),
          dFb (s, sv::id::dlyFeedback, "FEEDBACK", "Feedback (limitado a 92%)"),
          dLow (s, sv::id::dlyLow, "LOW CUT", "Filtro de graves nas repetições"), dHigh (s, sv::id::dlyHigh, "HIGH CUT", "Filtro de agudos nas repetições"),
          dMix (s, sv::id::dlyMix, "MIX", "Nível wet"),
          compOn (s, sv::id::compOn, "", "Ativar/desativar COMP"), magicOn (s, sv::id::magicOn, "", "Ativar/desativar MAGIC"),
          revOn (s, sv::id::revOn, "", "Ativar/desativar REVERB"), dlyOn (s, sv::id::dlyOn, "", "Ativar/desativar DELAY"),
          coralOn (s, sv::id::coralOn, "ON", "Ativar/desativar CORAL"),
          dHost (s, sv::id::dlyHostSync, "HOST SYNC", "Sincronizar com o BPM da DAW"),
          dPing (s, sv::id::dlyPing, "PING-PONG", "Repetições alternadas esquerda/direita"),
          revType (s, sv::id::revType, { "ROOM", "HALL", "PLATE" }, "Tipo de reverb"),
          dlySync (s, sv::id::dlySync, { "1/16", "1/8", "1/4" }, "Divisão rítmica"),
          inM (p.meters.inL, p.meters.inR, p.meters.inClip, "Nível de entrada (pós-ganho) em dBFS"),
          outM (p.meters.outL, p.meters.outR, p.meters.outClip, "Nível de saída em dBFS")
    {
        setLookAndFeel (&laf);
        for (auto* c : std::initializer_list<Component*> { &prev, &next, &save, &load, &del, &simple, &adv, &presetBox, &bypass,
                 &inG, &compA, &magicA, &revA, &dlyA, &outG, &coralK, &cThr, &cRat, &cAtk, &cRel, &cKnee, &cMk, &mDrv, &mPres, &mTone, &mHarm, &mMix,
                 &rDec, &rPre, &rDmp, &rTone, &rWid, &rMix, &dTime, &dFb, &dLow, &dHigh, &dMix, &compOn, &magicOn, &revOn, &dlyOn, &coralOn,
                 &dHost, &dPing, &revType, &dlySync, &inM, &outM, &status, &grLabel, &bpmLabel })
            addAndMakeVisible (c);

        prev.setTooltip ("Preset anterior"); next.setTooltip ("Preset seguinte");
        save.setTooltip ("Guardar a configuração atual como preset de utilizador"); load.setTooltip ("Carregar um preset (.svpreset) de um ficheiro");
        del.setTooltip ("Eliminar o preset de utilizador atual");
        simple.setTooltip ("Modo SIMPLE: controlos essenciais"); adv.setTooltip ("Modo ADVANCED: parâmetros técnicos");
        presetBox.setTooltip ("Presets de fábrica e de utilizador"); presetBox.setTitle ("Preset");
        prev.onClick = [this] { p.presets.previous(); refreshPresetUi(); };
        next.onClick = [this] { p.presets.next(); refreshPresetUi(); };
        simple.onClick = [this] { advAtt.setValueAsCompleteGesture (0.f); };
        adv.onClick = [this] { advAtt.setValueAsCompleteGesture (1.f); };
        presetBox.onChange = [this] { auto names = p.presets.getNames(); const int i = presetBox.getSelectedId() - 1; if (i >= 0 && i < names.size() && names[i] != p.presets.getCurrent()) { report (p.presets.load (names[i]), "Preset carregado: " + names[i]); refreshPresetUi(); } };
        save.onClick = [this] { askSave(); };
        load.onClick = [this] { askLoad(); };
        del.onClick = [this] { askDelete(); };

        for (auto* l : { &status, &grLabel, &bpmLabel }) { l->setFont (font (12.f)); l->setColour (Label::textColourId, C::text2); }
        grLabel.setJustificationType (Justification::centredRight); bpmLabel.setJustificationType (Justification::centredRight);
        grLabel.setTooltip ("Redução de ganho do compressor");

        for (int i = 0; i < p.presets.getNumFactory(); ++i)
        {
            auto* c = chips.add (new TextButton (p.presets.getNames()[i]));
            c->setTooltip ("Carregar preset " + c->getButtonText());
            c->onClick = [this, c] { report (p.presets.load (c->getButtonText()), "Preset carregado: " + c->getButtonText()); refreshPresetUi(); };
            addAndMakeVisible (c);
        }

        // módulos (áreas fixas em 1200x760)
        auto mk = [&] (const char* t, const char* d, const char* h, int x, int w, ToggleBtn* pw)
        { ModuleInfo m; m.title = t; m.desc = d; m.hint = h; m.area = { x, modY, w, modH }; m.power = pw; mods.add (m); };
        mk ("INPUT", "Nível de entrada", "", 24, 160, nullptr);
        mk ("COMP", "Compressor", "Controla a dinâmica da voz.\n\nAMOUNT define a intensidade.\nAtive ADVANCED para threshold, ratio, attack, release, joelho e makeup.", 198, 190, &compOn);
        mk ("MAGIC", "Vocal Enhancer", "Saturação suave, presença e harmónicos (DSP convencional, não é IA).\n\nAMOUNT 0% = sem efeito.", 402, 190, &magicOn);
        mk ("REVERB", "Espaço e Profundidade", "ROOM compacto, HALL amplo, PLATE denso e brilhante.", 606, 190, &revOn);
        mk ("DELAY", "Eco Sincronizado", "Sincroniza com o BPM da DAW. Sem BPM do anfitrião usa 120 BPM.", 810, 190, &dlyOn);
        mk ("OUTPUT", "Nível de saída", "", 1014, 160, nullptr);

        mods.getReference (1).advanced = { &cThr, &cRat, &cAtk, &cRel, &cKnee, &cMk };
        mods.getReference (2).advanced = { &mDrv, &mPres, &mTone, &mHarm, &mMix };
        mods.getReference (3).advanced = { &rDec, &rPre, &rDmp, &rTone, &rWid, &rMix };
        mods.getReference (4).advanced = { &dTime, &dFb, &dLow, &dHigh, &dMix, &dHost, &dPing };
        mods.getReference (1).dim = { &compA, &cThr, &cRat, &cAtk, &cRel, &cKnee, &cMk };
        mods.getReference (2).dim = { &magicA, &mDrv, &mPres, &mTone, &mHarm, &mMix };
        mods.getReference (3).dim = { &revA, &revType, &rDec, &rPre, &rDmp, &rTone, &rWid, &rMix };
        mods.getReference (4).dim = { &dlyA, &dlySync, &dTime, &dFb, &dLow, &dHigh, &dMix, &dHost, &dPing };

        setSize ((int) W, (int) H);
        advAtt.sendInitialUpdate();
        refreshPresetUi();
        startTimerHz (10);
    }

    ~Content() override { setLookAndFeel (nullptr); }

    //------------------------------------------------------------------ lógica
    void report (const Result& r, const String& okMsg)
    {
        status.setText (r.wasOk() ? okMsg : "Erro: " + r.getErrorMessage(), dontSendNotification);
        status.setColour (Label::textColourId, r.wasOk() ? C::text2 : C::red);
    }

    void setAdvanced (bool a)
    {
        advMode = a;
        simple.setToggleState (! a, dontSendNotification); adv.setToggleState (a, dontSendNotification);
        for (auto& m : mods) for (auto* c : m.advanced) c->setVisible (a);
        repaint();
    }

    void refreshPresetUi()
    {
        const auto cur = p.presets.getCurrent();
        shownPreset = cur;
        auto names = p.presets.getNames();
        presetBox.clear (dontSendNotification);
        for (int i = 0; i < names.size(); ++i)
        {
            if (i == p.presets.getNumFactory()) presetBox.addSeparator();
            presetBox.addItem ((p.presets.isFactory (names[i]) ? "" : "* ") + names[i], i + 1);
        }
        // IDs: nomes de utilizador mostram "* ", por isso seleciona por índice
        presetBox.setSelectedId (names.indexOf (cur) + 1, dontSendNotification);
        if (names.indexOf (cur) < 0) presetBox.setText (cur, dontSendNotification);
        for (auto* c : chips) c->setToggleState (c->getButtonText() == cur, dontSendNotification);
        del.setEnabled (! p.presets.isFactory (cur));
    }

    void askSave()
    {
        auto* w = new AlertWindow ("Guardar preset", "Nome do preset:", MessageBoxIconType::NoIcon, this);
        w->addTextEditor ("name", p.presets.isFactory (p.presets.getCurrent()) ? String() : p.presets.getCurrent());
        w->addButton ("Guardar", 1, KeyPress (KeyPress::returnKey)); w->addButton ("Cancelar", 0, KeyPress (KeyPress::escapeKey));
        SafePointer<Content> sp (this);
        w->enterModalState (true, ModalCallbackFunction::create ([sp, w] (int r)
        {
            std::unique_ptr<AlertWindow> own (w);
            if (sp == nullptr || r != 1) return;
            const auto name = own->getTextEditorContents ("name").trim();
            auto res = sp->p.presets.saveUser (name, false);
            if (! res.wasOk() && res.getErrorMessage().startsWith ("Já existe"))
            {
                AlertWindow::showAsync (MessageBoxOptions().withIconType (MessageBoxIconType::QuestionIcon).withTitle ("Substituir preset?")
                                            .withMessage ("Já existe \"" + name + "\". Substituir?").withButton ("Substituir").withButton ("Cancelar"),
                                        [sp, name] (int b) { if (sp != nullptr && b == 1) { sp->report (sp->p.presets.saveUser (name, true), "Preset guardado: " + name); sp->refreshPresetUi(); } });
                return;
            }
            sp->report (res, "Preset guardado: " + name); sp->refreshPresetUi();
        }), false);
    }

    void askLoad()
    {
        chooser = std::make_unique<FileChooser> ("Carregar preset", p.presets.getUserDirectory(), "*.svpreset");
        SafePointer<Content> sp (this);
        chooser->launchAsync (FileBrowserComponent::openMode | FileBrowserComponent::canSelectFiles, [sp] (const FileChooser& fc)
        {
            if (sp == nullptr || fc.getResult() == File()) return;
            sp->report (sp->p.presets.loadFromFile (fc.getResult()), "Preset carregado: " + fc.getResult().getFileNameWithoutExtension());
            sp->refreshPresetUi();
        });
    }

    void askDelete()
    {
        const auto cur = p.presets.getCurrent();
        if (p.presets.isFactory (cur)) { report (Result::fail ("Presets de fábrica não podem ser eliminados"), {}); return; }
        SafePointer<Content> sp (this);
        AlertWindow::showAsync (MessageBoxOptions().withIconType (MessageBoxIconType::WarningIcon).withTitle ("Eliminar preset")
                                    .withMessage ("Eliminar \"" + cur + "\"?").withButton ("Eliminar").withButton ("Cancelar"),
                                [sp, cur] (int b) { if (sp != nullptr && b == 1) { sp->report (sp->p.presets.deleteUser (cur), "Preset eliminado: " + cur); sp->p.presets.load ("Default"); sp->refreshPresetUi(); } });
    }

    void timerCallback() override
    {
        if (p.presets.getCurrent() != shownPreset) refreshPresetUi();
        for (auto& m : mods)
        {
            const float a = (m.power == nullptr || m.power->getToggleState()) ? 1.f : 0.4f;
            for (auto* c : m.dim) c->setAlpha (a);
        }
        grLabel.setText ("GR  -" + String (p.meters.gr.load(), 1) + " dB", dontSendNotification);
        bpmLabel.setText ("BPM " + String (p.meters.bpm.load(), 1), dontSendNotification);
    }

    //------------------------------------------------------------------ layout
    void resized() override
    {
        auto at = [] (Component& c, int x, int y, int w, int h) { c.setBounds (x, y, w, h); };
        at (prev, 340, 25, 32, 34); at (presetBox, 376, 25, 220, 34); at (next, 600, 25, 32, 34);
        at (save, 648, 25, 64, 34); at (load, 718, 25, 64, 34); at (del, 788, 25, 52, 34);
        at (simple, 880, 25, 90, 34); at (adv, 976, 25, 100, 34); at (bypass, 1090, 25, 84, 34);

        for (auto& m : mods)
        {
            const auto a = m.area;
            if (m.power) m.power->setBounds (a.getX() + a.getWidth() - 40, a.getY() + 12, 28, 24);
        }
        // INPUT / OUTPUT
        auto io = [&] (const ModuleInfo& m, LevelMeter& meter, Knob& k)
        {
            at (meter, m.area.getX() + 30, m.area.getY() + 60, 100, 270);
            at (k, m.area.getX() + 20, m.area.getY() + 340, 120, 92);
        };
        io (mods[0], inM, inG); io (mods[5], outM, outG);

        auto main = [&] (const ModuleInfo& m, Knob& k) { at (k, m.area.getX() + 25, m.area.getY() + 72, 140, 130); };
        main (mods[1], compA); main (mods[2], magicA); main (mods[3], revA); main (mods[4], dlyA);
        at (revType, mods[3].area.getX() + 10, mods[3].area.getY() + 214, 170, 30);
        at (dlySync, mods[4].area.getX() + 10, mods[4].area.getY() + 214, 170, 30);

        auto grid = [&] (const ModuleInfo& m, std::initializer_list<Component*> cs)
        {
            int i = 0;
            for (auto* c : cs) { at (*c, m.area.getX() + 6 + (i % 3) * 61, m.area.getY() + 262 + (i / 3) * 86, 61, 84); ++i; }
        };
        grid (mods[1], { &cThr, &cRat, &cAtk, &cRel, &cKnee, &cMk });
        grid (mods[2], { &mDrv, &mPres, &mTone, &mHarm, &mMix });
        grid (mods[3], { &rDec, &rPre, &rDmp, &rTone, &rWid, &rMix });
        grid (mods[4], { &dTime, &dFb, &dLow, &dHigh, &dMix });
        at (dHost, mods[4].area.getX() + 6 + 2 * 61, mods[4].area.getY() + 262 + 86 + 20, 61, 24);
        at (dPing, mods[4].area.getX() + 6 + 2 * 61 - 0, mods[4].area.getY() + 262 + 86 + 48, 61, 24);
        dHost.setBounds (dHost.getBounds().withX (mods[4].area.getX() + 6 + 61 * 2 - 0).withWidth (61));

        // presets / rodapé
        for (int i = 0; i < chips.size(); ++i) at (*chips[i], 24 + (i % 6) * 128, 590 + (i / 6) * 42, 120, 34);
        at (status, 24, 682, 760, 20);
        at (coralOn, 840, 575, 56, 28); at (coralK, 920, 560, 120, 108);
        at (grLabel, 1040, 590, 134, 20); at (bpmLabel, 1040, 616, 134, 20);
    }

    //------------------------------------------------------------------ desenho
    void paint (Graphics& g) override
    {
        g.setGradientFill (ColourGradient (C::bg1, 0, 0, C::bg0, 0, H, false)); g.fillAll();
        // estrelas discretas (determinísticas)
        Random rnd (7);
        for (int i = 0; i < 90; ++i) { g.setColour (Colours::white.withAlpha (rnd.nextFloat() * 0.12f)); g.fillRect (rnd.nextFloat() * W, rnd.nextFloat() * H, 1.4f, 1.4f); }

        drawLogo (g, 24, 14);

        for (auto& m : mods)
        {
            auto r = m.area.toFloat();
            g.setColour (C::module); g.fillRoundedRectangle (r, 8.f);
            g.setColour (C::line); g.drawRoundedRectangle (r.reduced (0.5f), 8.f, 1.f);
            g.setColour (C::red.withAlpha (0.55f)); g.fillRect (r.getX() + 14, r.getY() + 1, 40.f, 2.f);
            const bool on = m.power == nullptr || m.power->getToggleState();
            if (m.power) { g.setColour (on ? C::red : C::line); g.fillEllipse (m.power->getBounds().toFloat().getCentre().x - 5.f, m.power->getBounds().toFloat().getCentre().y - 5.f, 10.f, 10.f); }
            g.setColour (C::text); g.setFont (font (16.f, true)); g.drawText (m.title, m.area.getX() + 14, m.area.getY() + 12, 120, 22, Justification::left);
            g.setColour (C::text2); g.setFont (font (11.5f)); g.drawText (m.desc, m.area.getX() + 14, m.area.getY() + 36, m.area.getWidth() - 20, 16, Justification::left);
            if (! advMode && m.hint.isNotEmpty())
            {
                g.setColour (C::text2.withAlpha (on ? 0.8f : 0.4f)); g.setFont (font (11.f));
                g.drawFittedText (m.hint, m.area.getX() + 14, m.area.getY() + 290, m.area.getWidth() - 28, 120, Justification::topLeft, 8);
            }
        }
        // painel inferior
        g.setColour (C::module); g.fillRoundedRectangle (Rectangle<float> (14.f, 556.f, 780.f, 156.f), 8.f);
        g.fillRoundedRectangle (Rectangle<float> (820.f, 556.f, 366.f, 156.f), 8.f);
        g.setColour (C::line); g.drawRoundedRectangle (Rectangle<float> (14.5f, 556.5f, 779.f, 155.f), 8.f, 1.f);
        g.drawRoundedRectangle (Rectangle<float> (820.5f, 556.5f, 365.f, 155.f), 8.f, 1.f);
        g.setColour (C::text2); g.setFont (font (11.f, true));
        g.drawText ("PRESETS", 24, 560, 100, 16, Justification::left);
        g.drawText ("CORAL · MONITOR", 836, 560, 160, 16, Justification::left);
        g.setColour (C::text2.withAlpha (0.7f)); g.setFont (font (11.f));
        g.drawText ("Single voice thickener", 836, 612, 80, 40, Justification::topLeft);

        g.setColour (C::text2); g.setFont (FontOptions (11.f, Font::bold).withKerningFactor (0.18f));
        g.drawText ("SPACE VOCAL   ·   YOUR VOICE. ANOTHER DIMENSION.", 0, 728, (int) W, 22, Justification::centred);
    }

    static void drawLogo (Graphics& g, float x, float y)
    {
        // planeta + órbita (vetorial, original)
        const Rectangle<float> pr (x + 10, y + 12, 36, 36);
        g.setGradientFill (ColourGradient (Colour (0xff2a303a), pr.getX(), pr.getY(), Colour (0xff07090c), pr.getRight(), pr.getBottom(), false));
        g.fillEllipse (pr);
        g.setColour (C::red.withAlpha (0.35f)); g.drawEllipse (pr, 3.f); g.setColour (C::red); g.drawEllipse (pr, 1.2f);
        Path orbit; orbit.addEllipse (x - 2, y + 24, 60, 14);
        orbit.applyTransform (AffineTransform::rotation (-0.45f, x + 28, y + 30));
        g.setColour (C::red.withAlpha (0.25f)); g.strokePath (orbit, PathStrokeType (4.f));
        g.setColour (C::red); g.strokePath (orbit, PathStrokeType (1.4f));
        g.setColour (C::text); g.fillEllipse (x + 50, y + 12, 5.f, 5.f);
        g.setFont (FontOptions (32.f, Font::bold).withKerningFactor (0.08f));
        g.setColour (C::text); g.drawText ("SPACE", (int) x + 68, (int) y + 4, 130, 36, Justification::left);
        g.setColour (C::red); g.drawText ("VOCAL", (int) x + 68 + 124, (int) y + 4, 130, 36, Justification::left);
        g.setColour (C::text2); g.setFont (FontOptions (10.f, Font::plain).withKerningFactor (0.2f));
        g.drawText ("VOCAL PROCESSOR · SIMPLE EDITION", (int) x + 70, (int) y + 40, 260, 14, Justification::left);
    }
};

//==============================================================================
SpaceVocalEditor::SpaceVocalEditor (SpaceVocalProcessor& p) : AudioProcessorEditor (&p)
{
    content = std::make_unique<Content> (p);
    addAndMakeVisible (*content);
    setResizable (true, true);
    setResizeLimits (900, 570, 1800, 1140);
    getConstrainer()->setFixedAspectRatio ((double) W / H);
    setSize (1200, 760);
}

SpaceVocalEditor::~SpaceVocalEditor() = default;

void SpaceVocalEditor::paint (Graphics& g) { g.fillAll (C::bg0); }

void SpaceVocalEditor::resized()
{
    content->setBounds (0, 0, (int) W, (int) H);
    content->setTransform (AffineTransform::scale ((float) getWidth() / W));
}
