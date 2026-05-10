//==============================================================================
// JS Inflator  v1.5  -  PluginEditor.h  (JUCE 8.0.12)
// Hardware-style faceplate: Tyrian purple metallic rack unit aesthetic
//==============================================================================
#pragma once

#include "PluginProcessor.h"
#include "MurexLAF.h"

//==============================================================================
// Snapshot structure for A/B comparison
//==============================================================================
struct ABSnapshot
{
    juce::HashMap<juce::String, float> values;
    bool valid = false;
};

//==============================================================================
// Correlation Meter: Horizontal bar with -1 to +1 range
//==============================================================================
class CorrelationMeter : public juce::Component
{
public:
    CorrelationMeter() { setWantsKeyboardFocus(false); }
    void setCorrelation(float value);
    void paint(juce::Graphics& g) override;
    void resized() override;
private:
    float corr = 1.0f;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(CorrelationMeter)
};

//==============================================================================
// LUFS Meter Component: Numeric display + bar graph
//==============================================================================
class LufsMeterComponent : public juce::Component
{
public:
    LufsMeterComponent();
    void setValues(float momentary, float shortTerm, float integrated);
    void paint(juce::Graphics& g) override;
    void resized() override;
private:
    float mom = -100.0f, st = -100.0f, intg = -100.0f;
    juce::String momText, stText, intgText;
    float barValue = 0.0f;
    juce::Colour barColour;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(LufsMeterComponent)
};

//==============================================================================
// Hardware-style Screw Decal
//==============================================================================
class ScrewDecal : public juce::Component
{
public:
    void paint(juce::Graphics& g) override;
};

//==============================================================================
// Metallic Bezel Component for section dividers
//==============================================================================
class MetalBezel : public juce::Component
{
public:
    enum Style { Inner, Outer, Accent };
    MetalBezel(Style s = Inner) : style(s) {}
    void paint(juce::Graphics& g) override;
private:
    Style style;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MetalBezel)
};

//==============================================================================
// Stereo LED Meter (two vertical MurexLedMeter side by side)
//==============================================================================
class StereoLedMeter : public juce::Component
{
public:
    StereoLedMeter(const juce::String& lbl, MurexLedMeter::Mode mode);
    void setLevels(float pL, float rL, float pkL, float pR, float rR, float pkR);
    void setOvers(bool L, bool R);
    void paint(juce::Graphics& g) override;
    void resized() override;
private:
    juce::String label;
    MurexLedMeter left, right;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(StereoLedMeter)
};

//==============================================================================
// Horizontal LED bar (for effect and GR meters)
//==============================================================================
class HorizLedBar : public juce::Component
{
public:
    HorizLedBar() = default;
    void setValue(float v);
    void paint(juce::Graphics& g) override;
private:
    float value = 0.0f;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(HorizLedBar)
};

//==============================================================================
// Main Editor Class
//==============================================================================
class JSInflatorEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit JSInflatorEditor(JSInflatorProcessor&);
    ~JSInflatorEditor() override;

    void paint(juce::Graphics&) override;
    void resized() override;

private:
    // Processor reference & Look-and-Feel
    JSInflatorProcessor& proc;
    MurexLAF laf;

    // Base dimensions & scaling
    static constexpr int BASE_W = 720, BASE_H = 420;
    float uiScale = 1.0f;

    // Global Controls
    juce::ComboBox scaleCombo;

    // Core Processing Knobs
    MurexKnobWidget inputKnob, effectKnob, curveKnob, outputKnob, toneKnob;
    MurexKnobWidget tiltFreqKnob, subHarmKnob, stereoWidthKnob;

    // Character mode selector
    juce::ComboBox characterCombo;
    juce::Label charLbl;

    // Advanced limiter toggles
    juce::ToggleButton adaptReleaseBtn{ "ADAPT" }, truePeakBtn{ "TRUE PK" };

    // Dither & Level Match
    juce::ComboBox ditherCombo;
    juce::Label ditherLbl;
    juce::ToggleButton levelMatchBtn{ "LVL MATCH" };

    // LUFS Targeting
    juce::Slider lufsTargetSlider;
    juce::Label lufsTargetLbl;
    MurexLcdDisplay lufsTargetLcd;

    // Limiter Section
    juce::Slider limCeilSlider;
    juce::Label limCeilLbl;
    MurexLcdDisplay limCeilLcd;

    // Toggle Buttons
    juce::ToggleButton inBtn{ "IN" }, splitBtn{ "SPLIT" }, msBtn{ "M/S" };
    juce::ToggleButton dcBtn{ "DC BLK" }, limBtn{ "LIMIT" }, deltaBtn{ "DELTA" };
    juce::ToggleButton bypassBtn{ "BYPASS" };

    // Combo Boxes
    juce::ComboBox osCombo, osQualCombo, clipCombo, splitTypeCombo;
    juce::ComboBox agcCombo, focusCombo, dynCombo;
    juce::Label osLbl, osQualLbl, clipLbl, splitTypeLbl;
    juce::Label agcLbl, focusLbl, dynLbl;

    // Meters
    StereoLedMeter inMeter{ "INPUT", MurexLedMeter::Mode::Level };
    StereoLedMeter outMeter{ "OUTPUT", MurexLedMeter::Mode::Level };
    HorizLedBar effectBar, grBar;
    juce::Label effectBarLbl, grBarLbl;
    CorrelationMeter corrMeter;
    LufsMeterComponent lufsMeter;
    juce::Label corrLbl, lufsLbl;
    juce::Label dcOffsetLbl, dcOffsetVal;

    // A/B Comparison
    juce::TextButton abSaveA{ "SAVE A" }, abSaveB{ "SAVE B" };
    juce::TextButton abLoadA{ "A" }, abLoadB{ "B" };
    ABSnapshot snapA, snapB;

    // Hardware Decorations
    ScrewDecal screw[4];

    // Attachments
    using SlAtt = juce::AudioProcessorValueTreeState::SliderAttachment;
    using BtnAtt = juce::AudioProcessorValueTreeState::ButtonAttachment;
    using CbAtt = juce::AudioProcessorValueTreeState::ComboBoxAttachment;

    std::unique_ptr<SlAtt> attInput, attEffect, attCurve, attOutput, attTone, attLimCeil;
    std::unique_ptr<BtnAtt> attIn, attSplit, attMS, attBypass, attDelta, attLimiter, attDCBlock;
    std::unique_ptr<CbAtt> attOS, attOSQual, attClip, attSplitType, attAGC, attFocus, attDyn;
    std::unique_ptr<SlAtt> attTiltFreq, attSubHarm, attStereoWidth, attLufsTarget;
    std::unique_ptr<CbAtt> attCharacter, attDither;
    std::unique_ptr<BtnAtt> attAdaptRelease, attTruePeak, attLevelMatch;

    // Helper Methods
    void setupKnob(MurexKnobWidget& w, const juce::String& name, const juce::String& unit);
    void setupToggle(juce::ToggleButton& b);
    void setupCombo(juce::ComboBox& c, juce::Label& l, const juce::String& title, const juce::StringArray& items);
    void setupTextBtn(juce::TextButton& b);
    void applyScale(float s);
    void saveSnap(ABSnapshot& s);
    void loadSnap(const ABSnapshot& s);
    void onAB(bool loadA);
    void timerCallback() override;

    // Painting helpers
    void paintBackground(juce::Graphics& g);
    void paintFaceplate(juce::Graphics& g);
    void paintSections(juce::Graphics& g);
    void paintGlossyOverlay(juce::Graphics& g);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(JSInflatorEditor)
};