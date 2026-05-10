//==============================================================================
// JS Inflator  v1.5  -  PluginEditor.cpp  (JUCE 8.0.12)
// Hardware-style faceplate: Tyrian purple metallic rack unit aesthetic
// –– Full layout with no overlapping ––
//==============================================================================
#include "PluginEditor.h"
#include "PluginProcessor.h"

using juce::Colour;
using juce::Rectangle;
static inline Colour mc(uint32_t c) { return Colour(c); }

//==============================================================================
// Correlation Meter (unchanged)
//==============================================================================
void CorrelationMeter::setCorrelation(float value)
{
    corr = juce::jlimit(-1.0f, 1.0f, value);
    repaint();
}

void CorrelationMeter::paint(juce::Graphics& g)
{
    const auto b = getLocalBounds().toFloat().reduced(2.0f);
    const float cx = b.getCentreX(), cy = b.getCentreY();

    g.setColour(mc(Murex::ENGRAVED));
    g.fillRoundedRectangle(b, 3.0f);

    const float range = b.getWidth() * 0.45f;
    const float barW = corr * range;
    const float barX = cx + barW * 0.5f - std::abs(barW) * 0.5f;
    const float barY = b.getY() + 2.0f;
    const float barH = b.getHeight() - 4.0f;

    Colour barCol;
    if (corr < -0.5f) barCol = mc(Murex::LED_RED);
    else if (corr < 0.0f) barCol = mc(Murex::LED_YELLOW).interpolatedWith(mc(Murex::LED_RED), 1.0f + corr * 2.0f);
    else if (corr < 0.5f) barCol = mc(Murex::LED_GREEN).interpolatedWith(mc(Murex::LED_YELLOW), 1.0f - corr * 2.0f);
    else barCol = mc(Murex::LED_GREEN);

    g.setColour(barCol);
    g.fillRoundedRectangle(barX, barY, std::abs(barW), barH, 2.0f);

    g.setColour(mc(Murex::TYRIAN_BRIGHT).withAlpha(0.6f));
    g.fillRect(cx - 1.0f, b.getY() + 1.0f, 2.0f, barH - 2.0f);
    g.setColour(mc(Murex::TYRIAN_GLOW).withAlpha(0.4f));
    g.fillEllipse(cx - 8.0f, cy - 8.0f, 16.0f, 16.0f);

    g.setColour(mc(Murex::TEXT_DIM));
    g.setFont(juce::Font(juce::FontOptions{}.withHeight(9.0f).withStyle("Bold")));
    g.drawText("-1", b.getX(), cy - 5.0f, b.getWidth() * 0.2f, 10.0f, juce::Justification::centred);
    g.drawText("+1", b.getRight() - b.getWidth() * 0.2f, cy - 5.0f, b.getWidth() * 0.2f, 10.0f, juce::Justification::centred);
    g.drawText("0", cx - 10.0f, cy - 5.0f, 20.0f, 10.0f, juce::Justification::centred);

    g.setColour(mc(Murex::RIM_OUTER));
    g.drawRoundedRectangle(getLocalBounds().toFloat().reduced(0.5f), 3.0f, 1.0f);
}

void CorrelationMeter::resized() {}

//==============================================================================
// LUFS Meter Component (unchanged)
//==============================================================================
LufsMeterComponent::LufsMeterComponent() { barColour = mc(Murex::LCD_ACTIVE); }

void LufsMeterComponent::setValues(float momentary, float shortTerm, float integrated)
{
    mom = juce::jlimit(-100.0f, 0.0f, momentary);
    st = juce::jlimit(-100.0f, 0.0f, shortTerm);
    intg = juce::jlimit(-100.0f, 0.0f, integrated);
    momText = juce::String(mom, 1);
    stText = juce::String(st, 1);
    intgText = juce::String(intg, 1);
    barValue = juce::jlimit(0.0f, 1.0f, (intg + 24.0f) / 24.0f);
    repaint();
}

void LufsMeterComponent::paint(juce::Graphics& g)
{
    const auto b = getLocalBounds().toFloat();
    const float rowH = b.getHeight() / 3.0f;

    g.setColour(mc(Murex::LCD_BG));
    g.fillRoundedRectangle(b, 2.0f);
    g.setColour(mc(Murex::LCD_BORDER));
    g.drawRoundedRectangle(b.reduced(0.5f), 2.0f, 1.0f);

    auto drawRow = [&](float y, const juce::String& label, const juce::String& val)
        {
            const juce::Rectangle<float> row(b.getX() + 3.0f, y + 1.0f, b.getWidth() - 6.0f, rowH - 2.0f);
            g.setColour(mc(Murex::LCD_TEXT));
            g.setFont(juce::Font(juce::FontOptions{}.withHeight(8.0f).withStyle("Bold")));
            g.drawText(label, row.getX(), row.getY(), 32.0f, row.getHeight(), juce::Justification::centredLeft);
            g.drawText(val, row.getRight() - 40.0f, row.getY(), 38.0f, row.getHeight(), juce::Justification::centredRight);
            const float barW = row.getWidth() - 44.0f;
            const float fill = std::max(0.0f, (barW * barValue));
            g.setColour(mc(Murex::LCD_INACTIVE));
            g.fillRoundedRectangle(row.getX() + 34.0f, row.getY() + 2.0f, barW, 4.0f, 2.0f);
            g.setColour(mc(Murex::LCD_ACTIVE));
            g.fillRoundedRectangle(row.getX() + 34.0f, row.getY() + 2.0f, fill, 4.0f, 2.0f);
        };
    drawRow(0.0f, "MOM:", momText);
    drawRow(rowH, "ST :", stText);
    drawRow(rowH * 2.0f, "INT:", intgText);
}

void LufsMeterComponent::resized() {}

//==============================================================================
// ScrewDecal (unchanged)
//==============================================================================
void ScrewDecal::paint(juce::Graphics& g)
{
    const auto b = getLocalBounds().toFloat();
    const float r = std::min(b.getWidth(), b.getHeight()) * 0.4f;
    const float cx = b.getCentreX(), cy = b.getCentreY();

    g.setColour(mc(Murex::ENGRAVED));
    g.fillEllipse(cx - r, cy - r, r * 2.0f, r * 2.0f);
    juce::ColourGradient gHead(mc(Murex::FACE_LIGHT), cx, cy - r, mc(Murex::KNOB_DARK), cx, cy + r, false);
    g.setGradientFill(gHead);
    g.fillEllipse(cx - r, cy - r, r * 2.0f, r * 2.0f);
    g.setColour(mc(Murex::SCREW).withAlpha(0.6f));
    g.drawLine(cx - r * 0.6f, cy, cx + r * 0.6f, cy, 1.5f);
    g.drawLine(cx, cy - r * 0.6f, cx, cy + r * 0.6f, 1.5f);
    g.setColour(juce::Colours::white.withAlpha(0.15f));
    g.fillEllipse(cx - r * 0.3f, cy - r * 0.4f, r * 0.5f, r * 0.5f);
}

//==============================================================================
// MetalBezel (unchanged)
//==============================================================================
void MetalBezel::paint(juce::Graphics& g)
{
    const auto b = getLocalBounds().toFloat();
    switch (style)
    {
    case Inner: g.setColour(mc(Murex::RIM_INNER)); g.drawRect(b, 1.0f); break;
    case Outer: g.setColour(mc(Murex::RIM_OUTER)); g.drawRect(b, 1.0f); break;
    case Accent: g.setColour(mc(Murex::TYRIAN_DIM)); g.fillRect(b);
        g.setColour(mc(Murex::TYRIAN_BRIGHT).withAlpha(0.5f)); g.drawRect(b, 1.0f); break;
    }
}

//==============================================================================
// StereoLedMeter (unchanged)
//==============================================================================
StereoLedMeter::StereoLedMeter(const juce::String& lbl, MurexLedMeter::Mode mode) : label(lbl)
{
    left.setMode(mode); right.setMode(mode);
    addAndMakeVisible(left); addAndMakeVisible(right);
}

void StereoLedMeter::setLevels(float pL, float rL, float pkL, float pR, float rR, float pkR)
{
    left.setLevels(pL, rL, pkL); right.setLevels(pR, rR, pkR);
}

void StereoLedMeter::setOvers(bool L, bool R) { left.setOvers(L); right.setOvers(R); }

void StereoLedMeter::paint(juce::Graphics& g)
{
    g.setColour(mc(Murex::TEXT_MID));
    g.setFont(juce::Font(juce::FontOptions{}.withHeight(9.0f).withStyle("Bold")));
    g.drawText(label, getLocalBounds().removeFromBottom(12), juce::Justification::centred);
}

void StereoLedMeter::resized()
{
    auto b = getLocalBounds().withTrimmedBottom(13);
    const int gap = 2, hw = (b.getWidth() - gap) / 2;
    left.setBounds(b.removeFromLeft(hw)); b.removeFromLeft(gap); right.setBounds(b);
}

//==============================================================================
// HorizLedBar (unchanged)
//==============================================================================
void HorizLedBar::setValue(float v) { value = juce::jlimit(0.0f, 1.0f, v); repaint(); }

void HorizLedBar::paint(juce::Graphics& g)
{
    const auto b = getLocalBounds().toFloat().reduced(1.0f);
    constexpr int N = 24;
    const float segW = b.getWidth() / N;
    const int lit = (int)(value * N);

    g.setColour(mc(Murex::ENGRAVED));
    g.fillRoundedRectangle(b, 2.0f);
    for (int i = 0; i < N; ++i)
    {
        const float sx = b.getX() + i * segW + 0.5f;
        const bool on = i < lit;
        g.setColour(on ? mc(Murex::LED_PURPLE) : mc(Murex::LED_PUR_DIM));
        g.fillRoundedRectangle(sx, b.getY() + 0.5f, segW - 1.0f, b.getHeight() - 1.0f, 1.0f);
    }
    g.setColour(mc(Murex::RIM_OUTER));
    g.drawRoundedRectangle(getLocalBounds().toFloat().reduced(0.5f), 2.0f, 1.0f);
}

//==============================================================================
// JSInflatorEditor Constructor (unchanged except layout)
//==============================================================================
JSInflatorEditor::JSInflatorEditor(JSInflatorProcessor& p)
    : AudioProcessorEditor(p), proc(p)
{
    setLookAndFeel(&laf);
    setResizable(true, false);
    setResizeLimits(BASE_W, BASE_H, BASE_W * 2, BASE_H * 2);
    setSize(BASE_W, BASE_H);

    // Scale combo
    scaleCombo.addItem("100%", 1); scaleCombo.addItem("125%", 2);
    scaleCombo.addItem("150%", 3); scaleCombo.addItem("200%", 4);
    scaleCombo.setSelectedId(1, juce::dontSendNotification);
    scaleCombo.onChange = [this] {
        const float s[]{ 1.0f, 1.25f, 1.5f, 2.0f };
        applyScale(s[juce::jlimit(0, 3, scaleCombo.getSelectedId() - 1)]);
        };
    addAndMakeVisible(scaleCombo);

    // Knobs
    setupKnob(inputKnob, "INPUT", " dB");
    setupKnob(effectKnob, "EFFECT", "%");
    setupKnob(curveKnob, "CURVE", "");
    setupKnob(outputKnob, "OUTPUT", " dB");
    setupKnob(toneKnob, "TONE", "");
    setupKnob(tiltFreqKnob, "TILT FREQ", " Hz");
    setupKnob(subHarmKnob, "SUB HARM", "%");
    setupKnob(stereoWidthKnob, "WIDTH", "%");

    // Combos
    setupCombo(characterCombo, charLbl, "CHAR", { "Warm","Neutral","Aggr","Trans" });
    setupCombo(ditherCombo, ditherLbl, "DITHER", { "Off","16-bit","24-bit" });
    setupCombo(osCombo, osLbl, "OVERSAMP", { "1x","2x","4x","8x" });
    setupCombo(osQualCombo, osQualLbl, "PHASE", { "Min","Lin" });
    setupCombo(clipCombo, clipLbl, "CLIP", { "Off","Hard","Soft","H+S" });
    setupCombo(splitTypeCombo, splitTypeLbl, "SPLIT", { "Simple","Orig SVF" });
    setupCombo(agcCombo, agcLbl, "AGC", { "Off","Static","Dynamic" });
    setupCombo(focusCombo, focusLbl, "FOCUS", { "Full","Low","Mid","High" });
    setupCombo(dynCombo, dynLbl, "DYN", { "Smooth","Neutral","Punch","Dense" });

    // Toggles
    setupToggle(adaptReleaseBtn); setupToggle(truePeakBtn); setupToggle(levelMatchBtn);
    setupToggle(inBtn); setupToggle(splitBtn); setupToggle(msBtn); setupToggle(dcBtn);
    setupToggle(limBtn); setupToggle(deltaBtn); setupToggle(bypassBtn);

    // Sliders & LCDs
    limCeilSlider.setSliderStyle(juce::Slider::LinearHorizontal);
    limCeilSlider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    addAndMakeVisible(limCeilSlider);
    limCeilLbl.setText("CEIL", juce::dontSendNotification);
    limCeilLbl.setFont(juce::Font(juce::FontOptions{}.withHeight(9.0f).withStyle("Bold")));
    limCeilLbl.setColour(juce::Label::textColourId, mc(Murex::TEXT_MID));
    addAndMakeVisible(limCeilLbl); addAndMakeVisible(limCeilLcd);
    limCeilSlider.onValueChange = [this] {
        float v = limCeilSlider.getValue();
        float n = (-v - 0.1f) / 5.9f;
        limCeilLcd.setValue(1.0f - n);
        limCeilLcd.setValueText(juce::String(v, 2) + " dB");
        };

    lufsTargetSlider.setSliderStyle(juce::Slider::LinearHorizontal);
    lufsTargetSlider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    addAndMakeVisible(lufsTargetSlider);
    lufsTargetLbl.setText("LUFS TG", juce::dontSendNotification);
    lufsTargetLbl.setFont(juce::Font(juce::FontOptions{}.withHeight(9.0f).withStyle("Bold")));
    lufsTargetLbl.setColour(juce::Label::textColourId, mc(Murex::TEXT_MID));
    addAndMakeVisible(lufsTargetLbl); addAndMakeVisible(lufsTargetLcd);
    lufsTargetSlider.onValueChange = [this] {
        float v = lufsTargetSlider.getValue();
        float n = (v + 30.0f) / 20.0f;
        lufsTargetLcd.setValue(n);
        lufsTargetLcd.setValueText(juce::String(v, 1) + " LUFS");
        };

    // Meters
    addAndMakeVisible(inMeter); addAndMakeVisible(outMeter);
    addAndMakeVisible(effectBar); addAndMakeVisible(grBar);
    addAndMakeVisible(corrMeter); addAndMakeVisible(lufsMeter);

    // Labels
    auto sl = [&](juce::Label& l, const juce::String& t) {
        l.setText(t, juce::dontSendNotification);
        l.setFont(juce::Font(juce::FontOptions{}.withHeight(8.5f).withStyle("Bold")));
        l.setColour(juce::Label::textColourId, mc(Murex::TEXT_MID));
        l.setJustificationType(juce::Justification::centred);
        addAndMakeVisible(l);
        };
    sl(effectBarLbl, "EFFECT"); sl(grBarLbl, "LIM GR");
    sl(corrLbl, "PHASE CORR"); sl(lufsLbl, "LOUDNESS");
    sl(dcOffsetLbl, "DC OFF:"); addAndMakeVisible(dcOffsetVal);
    dcOffsetVal.setFont(juce::Font(juce::FontOptions{}.withHeight(9.0f).withStyle("Bold")));
    dcOffsetVal.setColour(juce::Label::textColourId, mc(Murex::LCD_TEXT));

    // A/B buttons
    for (auto* b : { &abSaveA, &abSaveB, &abLoadA, &abLoadB }) setupTextBtn(*b);
    abSaveA.onClick = [this] { saveSnap(snapA); };
    abSaveB.onClick = [this] { saveSnap(snapB); };
    abLoadA.onClick = [this] { onAB(true); };
    abLoadB.onClick = [this] { onAB(false); };

    // Screws
    for (auto& s : screw) addAndMakeVisible(s);

    // APVTS attachments (unchanged)
    auto& a = proc.apvts;
    attInput = std::make_unique<SlAtt>(a, ParamID::INPUT, inputKnob.getSlider());
    attEffect = std::make_unique<SlAtt>(a, ParamID::EFFECT, effectKnob.getSlider());
    attCurve = std::make_unique<SlAtt>(a, ParamID::CURVE, curveKnob.getSlider());
    attOutput = std::make_unique<SlAtt>(a, ParamID::OUTPUT, outputKnob.getSlider());
    attTone = std::make_unique<SlAtt>(a, ParamID::TONE, toneKnob.getSlider());
    attLimCeil = std::make_unique<SlAtt>(a, ParamID::LIM_CEIL, limCeilSlider);
    attIn = std::make_unique<BtnAtt>(a, ParamID::IN, inBtn);
    attSplit = std::make_unique<BtnAtt>(a, ParamID::SPLIT, splitBtn);
    attMS = std::make_unique<BtnAtt>(a, ParamID::MS_MODE, msBtn);
    attDCBlock = std::make_unique<BtnAtt>(a, ParamID::DC_BLOCK, dcBtn);
    attLimiter = std::make_unique<BtnAtt>(a, ParamID::LIMITER, limBtn);
    attDelta = std::make_unique<BtnAtt>(a, ParamID::DELTA, deltaBtn);
    attBypass = std::make_unique<BtnAtt>(a, ParamID::BYPASS, bypassBtn);
    attOS = std::make_unique<CbAtt>(a, ParamID::OS, osCombo);
    attOSQual = std::make_unique<CbAtt>(a, ParamID::OS_QUAL, osQualCombo);
    attClip = std::make_unique<CbAtt>(a, ParamID::CLIP_MODE, clipCombo);
    attSplitType = std::make_unique<CbAtt>(a, ParamID::SPLIT_TYPE, splitTypeCombo);
    attAGC = std::make_unique<CbAtt>(a, ParamID::AGC_MODE, agcCombo);
    attFocus = std::make_unique<CbAtt>(a, ParamID::FOCUS, focusCombo);
    attDyn = std::make_unique<CbAtt>(a, ParamID::DYN_MODE, dynCombo);
    attTiltFreq = std::make_unique<SlAtt>(a, ParamID::TILT_FREQ, tiltFreqKnob.getSlider());
    attSubHarm = std::make_unique<SlAtt>(a, ParamID::SUB_HARMONIC, subHarmKnob.getSlider());
    attStereoWidth = std::make_unique<SlAtt>(a, ParamID::STEREO_WIDTH, stereoWidthKnob.getSlider());
    attLufsTarget = std::make_unique<SlAtt>(a, ParamID::LUFS_TARGET, lufsTargetSlider);
    attCharacter = std::make_unique<CbAtt>(a, ParamID::CHARACTER, characterCombo);
    attDither = std::make_unique<CbAtt>(a, ParamID::DITHER, ditherCombo);
    attAdaptRelease = std::make_unique<BtnAtt>(a, ParamID::ADAPT_RELEASE, adaptReleaseBtn);
    attTruePeak = std::make_unique<BtnAtt>(a, ParamID::TRUE_PEAK, truePeakBtn);
    attLevelMatch = std::make_unique<BtnAtt>(a, ParamID::LEVEL_MATCH, levelMatchBtn);

    // Init values
    for (auto* w : { &inputKnob, &effectKnob, &curveKnob, &outputKnob, &toneKnob,
                     &tiltFreqKnob, &subHarmKnob, &stereoWidthKnob }) w->updateLcd();
    limCeilSlider.setValue(-0.3);
    lufsTargetSlider.setValue(-14.0);

    startTimerHz(30);
}

JSInflatorEditor::~JSInflatorEditor()
{
    stopTimer();
    setLookAndFeel(nullptr);
}

//==============================================================================
// Helper methods (unchanged)
//==============================================================================
void JSInflatorEditor::setupKnob(MurexKnobWidget& w, const juce::String& n, const juce::String& u)
{
    w.setup(n, u);
    addAndMakeVisible(w);
}

void JSInflatorEditor::setupToggle(juce::ToggleButton& b)
{
    b.setClickingTogglesState(true);
    addAndMakeVisible(b);
}

void JSInflatorEditor::setupCombo(juce::ComboBox& c, juce::Label& l, const juce::String& t, const juce::StringArray& i)
{
    c.addItemList(i, 1);
    c.setScrollWheelEnabled(true);
    addAndMakeVisible(c);
    l.setText(t, juce::dontSendNotification);
    l.setFont(juce::Font(juce::FontOptions{}.withHeight(8.5f).withStyle("Bold")));
    l.setColour(juce::Label::textColourId, mc(Murex::TEXT_MID));
    l.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(l);
}

void JSInflatorEditor::setupTextBtn(juce::TextButton& b)
{
    addAndMakeVisible(b);
}

void JSInflatorEditor::applyScale(float s)
{
    uiScale = s;
    setSize(BASE_W * s, BASE_H * s);
}

void JSInflatorEditor::saveSnap(ABSnapshot& s)
{
    s.values.clear();
    for (auto* p : proc.apvts.processor.getParameters())
        if (auto* r = dynamic_cast<juce::RangedAudioParameter*>(p))
            s.values.set(r->getParameterID(), r->getValue());
    s.valid = true;
}

void JSInflatorEditor::loadSnap(const ABSnapshot& s)
{
    if (!s.valid) return;
    for (auto* p : proc.apvts.processor.getParameters())
        if (auto* r = dynamic_cast<juce::RangedAudioParameter*>(p))
            if (s.values.contains(r->getParameterID()))
                r->setValueNotifyingHost(s.values[r->getParameterID()]);
}

void JSInflatorEditor::onAB(bool loadA)
{
    ABSnapshot& snap = loadA ? snapA : snapB;
    if (!snap.valid) { saveSnap(snap); return; }
    loadSnap(snap);
    abLoadA.setColour(juce::TextButton::buttonColourId, loadA ? mc(Murex::TYRIAN) : mc(Murex::FACE_LIGHT));
    abLoadB.setColour(juce::TextButton::buttonColourId, !loadA ? mc(Murex::TYRIAN) : mc(Murex::FACE_LIGHT));
    repaint();
}

void JSInflatorEditor::timerCallback()
{
    const float pIL = proc.getInLevelL(), pIR = proc.getInLevelR(), pOL = proc.getOutLevelL(), pOR = proc.getOutLevelR();
    const float rIL = proc.getInRmsL(), rIR = proc.getInRmsR(), rOL = proc.getOutRmsL(), rOR = proc.getOutRmsR();
    const float pkIL = proc.getInPeakL(), pkIR = proc.getInPeakR(), pkOL = proc.getOutPeakL(), pkOR = proc.getOutPeakR();
    const int oIL = proc.getAndClearInOversL(), oIR = proc.getAndClearInOversR(), oOL = proc.getAndClearOutOversL(), oOR = proc.getAndClearOutOversR();

    auto toB = [](float l) {
        if (l <= 0.0f) return 0.0f;
        return juce::jlimit(0.0f, 1.0f, (juce::Decibels::gainToDecibels(l) + 60.0f) / 66.0f);
        };
    inMeter.setLevels(toB(pIL), toB(rIL), toB(pkIL), toB(pIR), toB(rIR), toB(pkIR));
    outMeter.setLevels(toB(pOL), toB(rOL), toB(pkOL), toB(pOR), toB(rOR), toB(pkOR));
    inMeter.setOvers(oIL > 0, oIR > 0);
    outMeter.setOvers(oOL > 0, oOR > 0);
    effectBar.setValue(proc.getEffectMeter());
    grBar.setValue(juce::jlimit(0.0f, 1.0f, -proc.getLimiterGR() / 20.0f));

    corrMeter.setCorrelation(proc.getCorrelation());
    lufsMeter.setValues(proc.getMomentaryLUFS(), proc.getShortTermLUFS(), proc.getIntegratedLUFS());
    dcOffsetVal.setText(juce::String(proc.getDCOffset(), 4), juce::dontSendNotification);

    for (auto* w : { &inputKnob, &effectKnob, &curveKnob, &outputKnob, &toneKnob,
                     &tiltFreqKnob, &subHarmKnob, &stereoWidthKnob }) w->updateLcd();
}

//==============================================================================
// Painting (unchanged)
//==============================================================================
void JSInflatorEditor::paint(juce::Graphics& g)
{
    paintBackground(g);
    paintFaceplate(g);
    paintSections(g);
    paintGlossyOverlay(g);
}

void JSInflatorEditor::paintBackground(juce::Graphics& g)
{
    juce::ColourGradient bg(mc(0xFF1E1E24u), juce::Point<float>(0.0f, 0.0f),
        mc(0xFF0A0A0Eu), juce::Point<float>(0.0f, (float)getHeight()), false);
    g.setGradientFill(bg);
    g.fillAll();
}

void JSInflatorEditor::paintFaceplate(juce::Graphics& g)
{
    const float W = (float)getWidth(), H = (float)getHeight(), s = uiScale;

    g.setColour(mc(Murex::RIM_INNER));
    g.drawLine(0, 0, W, 0, 1.5f);
    g.drawLine(0, 0, 0, H, 1.5f);
    g.setColour(mc(Murex::RIM_OUTER));
    g.drawLine(0, H - 1, W, H - 1, 1.5f);
    g.drawLine(W - 1, 0, W - 1, H, 1.5f);

    juce::ColourGradient tb(mc(Murex::TYRIAN_BRIGHT).withAlpha(0.9f), juce::Point<float>(0.0f, 0.0f),
        mc(Murex::TYRIAN_DIM).withAlpha(0.6f), juce::Point<float>(W, 0.0f), false);
    g.setGradientFill(tb);
    g.fillRect(juce::Rectangle<float>(0.0f, 0.0f, W, 4.0f * s));

    juce::ColourGradient bb(mc(Murex::TYRIAN_DIM), juce::Point<float>(0.0f, H - 4.0f * s),
        mc(Murex::TYRIAN_BRIGHT).withAlpha(0.5f), juce::Point<float>(W, H - 4.0f * s), false);
    g.setGradientFill(bb);
    g.fillRect(juce::Rectangle<float>(0.0f, H - 4.0f * s, W, 4.0f * s));

    g.setColour(mc(Murex::TEXT_BRIGHT));
    g.setFont(juce::Font(juce::FontOptions{}.withHeight(16.0f * s).withStyle("Bold")));
    g.drawText("JS INFLATOR v1.5", juce::Rectangle<float>(12.0f * s, 6.0f * s, 300.0f * s, 22.0f * s), juce::Justification::centredLeft);
    g.setColour(mc(Murex::TYRIAN_PALE));
    g.setFont(juce::Font(juce::FontOptions{}.withHeight(8.5f * s)));
    g.drawText("Harmonic Exciter / Loudness Engine | Tyrian Edition",
        juce::Rectangle<float>(12.0f * s, 24.0f * s, 400.0f * s, 12.0f * s), juce::Justification::centredLeft);
}

void JSInflatorEditor::paintSections(juce::Graphics& g)
{
    const float s = uiScale;
    auto drawLabel = [&](const juce::String& text, float x, float y) {
        g.setColour(mc(Murex::TYRIAN_PALE).withAlpha(0.4f));
        g.setFont(juce::Font(juce::FontOptions{}.withHeight(8.0f * s).withStyle("Bold")));
        g.drawText(text, juce::Rectangle<float>(x * s, y * s, 100.0f * s, 11.0f * s), juce::Justification::centredLeft);
        };
    drawLabel("CORE", 10, 43);
    drawLabel("TONE & SHAPE", 280, 43);
    drawLabel("SPATIAL / SUB", 420, 43);
    drawLabel("SIGNAL PATH", 10, 185);
    drawLabel("LIMIT / TARGET", 280, 185);
    drawLabel("ANALYSIS", 420, 185);
    drawLabel("ENGINE", 10, 280);
    drawLabel("A/B COMP", 280, 280);
    drawLabel("UTILITY", 420, 280);

    auto drawDivider = [&](float x, float y, float w, float h) {
        g.setColour(mc(Murex::ENGRAVED));
        g.fillRect(juce::Rectangle<float>(x * s, y * s, w * s, h * s));
        g.setColour(mc(Murex::RIM_INNER).withAlpha(0.25f));
        g.fillRect(juce::Rectangle<float>(x * s, (y + h) * s, w * s, h * s));
        };
    drawDivider(8, 180, getWidth() / s - 16, 1);
    drawDivider(8, 276, getWidth() / s - 16, 1);
    g.setColour(mc(Murex::ENGRAVED));
    g.fillRect(274 * s, 43 * s, 1 * s, 132 * s);
    g.fillRect(414 * s, 43 * s, 1 * s, 132 * s);
    g.fillRect(274 * s, 182 * s, 1 * s, 88 * s);
    g.fillRect(414 * s, 182 * s, 1 * s, 88 * s);
}

void JSInflatorEditor::paintGlossyOverlay(juce::Graphics& g)
{
    const auto b = getLocalBounds().toFloat();
    juce::ColourGradient gloss(juce::Colours::white.withAlpha(0.08f), 0.0f, 0.0f, juce::Colours::transparentWhite, 0.0f, 40.0f * uiScale, false);
    g.setGradientFill(gloss);
    g.fillRect(b.withHeight(40.0f * uiScale));
    juce::ColourGradient fade(juce::Colours::transparentBlack, 0.0f, b.getHeight() - 20.0f * uiScale, juce::Colour(0xFF000000).withAlpha(0.4f), 0.0f, b.getHeight(), false);
    g.setGradientFill(fade);
    g.fillRect(b.withTrimmedTop(b.getHeight() - 20.0f * uiScale));
}

//==============================================================================
// resized()  –  Strict grid layout, no overlapping
//
//  Base canvas: 720 × 420 px
//
//  ┌──────────────────────────────────────────────────────────────────────┐
//  │  HEADER  (Y 0-42)  title · subtitle · screws · scale combo          │
//  ├──────────────┬─────────────┬────────────────────────┬───────────────┤
//  │ CORE         │ TONE&SHAPE  │ SPATIAL / SUB          │ I/O METERS    │
//  │ (Y 43-179)   │ (Y 43-179)  │ (Y 43-179)             │ (Y 43-179)    │
//  │ X 10-272     │ X 276-412   │ X 416-626              │ X 630-708     │
//  │ 4 knobs      │ 2 knobs     │ 2 knobs + char combo   │ in/out meters │
//  ├──────────────┼─────────────┼────────────────────────┼───────────────┤
//  │ SIGNAL PATH  │ LIMIT/TGT   │ ANALYSIS               │ FX/GR bars    │
//  │ (Y 182-274)  │ (Y 182-274) │ (Y 182-274)            │ (Y 182-274)   │
//  │ toggles ×7   │ 3 toggles   │ corr + LUFS + DC       │               │
//  │              │ 2 sliders   │                        │               │
//  ├──────────────┼─────────────┼────────────────────────┼───────────────┤
//  │ ENGINE       │ A/B COMP    │ UTILITY                │               │
//  │ (Y 278-410)  │ (Y 278-410) │ (Y 278-410)            │               │
//  │ 7 combos     │ A/B buttons │ dither combo           │               │
//  └──────────────┴─────────────┴────────────────────────┴───────────────┘
//==============================================================================
void JSInflatorEditor::resized()
{
    uiScale = getWidth() / (float)BASE_W;
    const float s = uiScale;

    // sc: scale one float value → int pixels (round-to-nearest)
    auto sc = [s](float v) { return (int)(v * s + 0.5f); };

    // sr: build a scaled Rectangle<int> from unscaled float coords
    auto sr = [sc](float x, float y, float w, float h) -> Rectangle<int> {
        return { sc(x), sc(y), sc(w), sc(h) };
        };

    //------------------------------------------------------------------
    // HEADER  –  screws + scale combo
    //------------------------------------------------------------------
    scaleCombo.setBounds(sr((float)(BASE_W - 62), 8.0f, 58.0f, 20.0f));
    for (int i = 0; i < 4; ++i)
    {
        const float ix = (i % 2) ? (float)(BASE_W - 16) : 4.0f;
        const float iy = (i < 2) ? 4.0f : (float)(BASE_H - 16);
        screw[i].setBounds(sr(ix, iy, 12.0f, 12.0f));
    }

    //==================================================================
    // ROW 1  (Y 43-179)  –  KNOBS
    //
    //   Column boundaries (matching paintSections dividers):
    //     Col1  x 10-272   (262 px)
    //     Col2  x 276-412  (136 px)
    //     Col3  x 416-626  (210 px)
    //     Mtrs  x 630-708  ( 78 px)
    //
    //   Knob Y band: 55-171  (116 px tall; 55-43=12 px for section label)
    //==================================================================

    // ── COL1  CORE  –  4 knobs, w=60, gap=4, startX=12 ──────────────
    //   x positions: 12, 76, 140, 204  →  max right edge 264 < 274  ✓
    {
        const float X0 = 12.f, Y = 55.f, W = 60.f, H = 116.f, G = 4.f;
        inputKnob.setBounds(sr(X0, Y, W, H));
        effectKnob.setBounds(sr(X0 + (W + G), Y, W, H));
        curveKnob.setBounds(sr(X0 + (W + G) * 2, Y, W, H));
        outputKnob.setBounds(sr(X0 + (W + G) * 3, Y, W, H));
        // OUTPUT right edge: 12 + 3*64 + 60 = 264  ✓
    }

    // ── COL2  TONE & SHAPE  –  2 knobs, w=60, gap=4, startX=278 ─────
    //   x positions: 278, 342  →  max right edge 402 < 414  ✓
    {
        const float X0 = 278.f, Y = 55.f, W = 60.f, H = 116.f, G = 4.f;
        toneKnob.setBounds(sr(X0, Y, W, H));
        tiltFreqKnob.setBounds(sr(X0 + W + G, Y, W, H));
        // TILT right edge: 278 + 60 + 4 + 60 = 402  ✓
    }

    // ── COL3  SPATIAL / SUB  –  2 knobs + character combo ───────────
    //   Knobs: w=66, gap=6, startX=418
    //     x positions: 418, 490  →  max right edge 556 < 626  ✓
    //   Character combo in right remainder: x=562, w=58
    //     right edge: 562 + 58 = 620 < 626  ✓
    {
        const float X0 = 418.f, Y = 55.f, W = 66.f, H = 116.f, G = 6.f;
        subHarmKnob.setBounds(sr(X0, Y, W, H));
        stereoWidthKnob.setBounds(sr(X0 + W + G, Y, W, H));

        // Character selector – stacked vertically in the right remainder
        charLbl.setBounds(sr(562.f, 55.f, 58.f, 10.f));
        characterCombo.setBounds(sr(562.f, 67.f, 58.f, 20.f));
        // right edge 620 < 626, bottom 87 < 179  ✓
    }

    // ── METERS COL  –  vertical LED meters (rows 1 spans full height) ──
    //   inMeter x=630, outMeter x=670, both w=38, h=134
    //   StereoLedMeter reserves bottom 13 px for its own text label
    //   right edge: 670 + 38 = 708 < 720  ✓
    inMeter.setBounds(sr(630.f, 44.f, 38.f, 134.f));
    outMeter.setBounds(sr(670.f, 44.f, 38.f, 134.f));

    //==================================================================
    // ROW 2  (Y 182-274)  –  SIGNAL PATH | LIMIT-TARGET | ANALYSIS
    //   92 px tall;  components kept ≥ 2 px inside row boundary
    //==================================================================

    // ── COL1  SIGNAL PATH  ──────────────────────────────────────────
    //   Row-a: 5 toggles  w=50, gap=2, startX=12
    //     x: 12, 64, 116, 168, 220  →  max right 270 < 274  ✓
    {
        const float X0 = 12.f, Y = 196.f, W = 50.f, H = 22.f, G = 2.f;
        inBtn.setBounds(sr(X0, Y, W, H));
        splitBtn.setBounds(sr(X0 + (W + G), Y, W, H));
        msBtn.setBounds(sr(X0 + (W + G) * 2, Y, W, H));
        dcBtn.setBounds(sr(X0 + (W + G) * 3, Y, W, H));
        limBtn.setBounds(sr(X0 + (W + G) * 4, Y, W, H));
        // LIMIT right edge: 12 + 4*52 + 50 = 270  ✓
    }
    //   Row-b: DELTA + BYPASS  (below row-a, y=222)
    //     x: 12, 74  →  BYPASS right edge 74+58=132 < 274  ✓
    deltaBtn.setBounds(sr(12.f, 222.f, 58.f, 22.f));
    bypassBtn.setBounds(sr(74.f, 222.f, 58.f, 22.f));

    // ── COL2  LIMIT / TARGET  ────────────────────────────────────────
    //   Three mode toggles on one row, y=194
    //     x: 278, 322, 366  →  LEVELMATCH right edge 366+44=410 < 414  ✓
    adaptReleaseBtn.setBounds(sr(278.f, 194.f, 42.f, 20.f));
    truePeakBtn.setBounds(sr(322.f, 194.f, 42.f, 20.f));
    levelMatchBtn.setBounds(sr(366.f, 194.f, 44.f, 20.f));

    //   LIM CEIL: label (y=218) + slider (y=229, w=88) + LCD (y=225, right-aligned)
    //     slider right edge: 278+88=366, LCD right edge: 370+40=410 < 414  ✓
    //     bottom: 229+12=241 (slider), 225+16=241 (lcd)  ✓
    limCeilLbl.setBounds(sr(278.f, 218.f, 42.f, 10.f));
    limCeilSlider.setBounds(sr(278.f, 229.f, 88.f, 12.f));
    limCeilLcd.setBounds(sr(370.f, 225.f, 40.f, 16.f));

    //   LUFS TARGET: label (y=245) + slider (y=256, w=88) + LCD (y=252)
    //     bottom: 256+12=268 (slider), 252+16=268 (lcd) < 274  ✓
    lufsTargetLbl.setBounds(sr(278.f, 245.f, 60.f, 10.f));
    lufsTargetSlider.setBounds(sr(278.f, 256.f, 88.f, 12.f));
    lufsTargetLcd.setBounds(sr(370.f, 252.f, 40.f, 16.f));

    // ── COL3  ANALYSIS  ─────────────────────────────────────────────
    //   All content x=418, w=204  →  right edge 622 < 626  ✓
    //   Bottom: DC offset label ends at y=270 < 274  ✓
    corrLbl.setBounds(sr(418.f, 183.f, 100.f, 10.f));
    corrMeter.setBounds(sr(418.f, 195.f, 204.f, 20.f));   // bottom 215

    lufsLbl.setBounds(sr(418.f, 217.f, 100.f, 10.f));
    lufsMeter.setBounds(sr(418.f, 228.f, 204.f, 30.f));   // bottom 258

    dcOffsetLbl.setBounds(sr(418.f, 261.f, 52.f, 10.f));
    dcOffsetVal.setBounds(sr(474.f, 261.f, 76.f, 10.f)); // right 550, bottom 271 ✓

    // ── METERS COL  Row-2  –  EFFECT + GR horizontal bars ────────────
    //   x=630, w=78  →  right edge 708 < 720  ✓
    //   bottom: 227+8=235 < 274  ✓
    effectBarLbl.setBounds(sr(630.f, 193.f, 78.f, 10.f));
    effectBar.setBounds(sr(630.f, 204.f, 78.f, 8.f));
    grBarLbl.setBounds(sr(630.f, 216.f, 78.f, 10.f));
    grBar.setBounds(sr(630.f, 227.f, 78.f, 8.f));

    //==================================================================
    // ROW 3  (Y 278-410)  –  ENGINE | A/B COMP | UTILITY
    //   132 px tall;  painted section labels occupy y≈280-291
    //   so component rows start at y=293 / 302
    //==================================================================

    // ── COL1  ENGINE  –  7 combos in two sub-rows ───────────────────
    //   w=62, gap=3.  4 combos + 3 combos.
    //   Row-1 x positions: 12, 77, 142, 207  →  max right 207+62=269 < 274  ✓
    //   Row-2 x positions: 12, 77, 142        →  max right 142+62=204 < 274  ✓
    {
        const float X0 = 12.f, CW = 62.f, CH = 20.f, CG = 3.f;
        const float LY1 = 292.f, CY1 = 303.f;   // sub-row 1: label / combo Y
        osLbl.setBounds(sr(X0, LY1, CW, 10.f));
        osCombo.setBounds(sr(X0, CY1, CW, CH));
        osQualLbl.setBounds(sr(X0 + (CW + CG), LY1, CW, 10.f));
        osQualCombo.setBounds(sr(X0 + (CW + CG), CY1, CW, CH));
        clipLbl.setBounds(sr(X0 + (CW + CG) * 2, LY1, CW, 10.f));
        clipCombo.setBounds(sr(X0 + (CW + CG) * 2, CY1, CW, CH));
        splitTypeLbl.setBounds(sr(X0 + (CW + CG) * 3, LY1, CW, 10.f));
        splitTypeCombo.setBounds(sr(X0 + (CW + CG) * 3, CY1, CW, CH));
        // splitTypeCombo bottom: 303+20=323 < 410  ✓

        const float LY2 = 328.f, CY2 = 339.f;   // sub-row 2: label / combo Y
        agcLbl.setBounds(sr(X0, LY2, CW, 10.f));
        agcCombo.setBounds(sr(X0, CY2, CW, CH));
        focusLbl.setBounds(sr(X0 + (CW + CG), LY2, CW, 10.f));
        focusCombo.setBounds(sr(X0 + (CW + CG), CY2, CW, CH));
        dynLbl.setBounds(sr(X0 + (CW + CG) * 2, LY2, CW, 10.f));
        dynCombo.setBounds(sr(X0 + (CW + CG) * 2, CY2, CW, CH));
        // dynCombo bottom: 339+20=359 < 410  ✓
    }

    // ── COL2  A/B COMPARISON  ────────────────────────────────────────
    //   Save row y=292: SAVE A(278) SAVE B(344)  →  right edge 408 < 414  ✓
    //   Load row y=318: LOAD A(278) LOAD B(344)  →  bottom 340 < 410  ✓
    abSaveA.setBounds(sr(278.f, 292.f, 64.f, 22.f));
    abSaveB.setBounds(sr(344.f, 292.f, 64.f, 22.f));
    abLoadA.setBounds(sr(278.f, 318.f, 64.f, 22.f));
    abLoadB.setBounds(sr(344.f, 318.f, 64.f, 22.f));

    // ── COL3  UTILITY  –  dither combo ───────────────────────────────
    //   x=418, w=100  →  right edge 518 < 626  ✓
    //   bottom: 303+20=323 < 410  ✓
    ditherLbl.setBounds(sr(418.f, 292.f, 60.f, 10.f));
    ditherCombo.setBounds(sr(418.f, 303.f, 100.f, 20.f));
}