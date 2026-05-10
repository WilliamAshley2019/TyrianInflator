//==============================================================================
// JS Inflator  v1.5  -  PluginProcessor.cpp  (JUCE 8.0.12)
// Optimized for CPU efficiency with block-based metering & SIMD hints
//==============================================================================
#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <cmath>
#include <algorithm>

//==============================================================================
// K‑Weighting Filter Implementation
//==============================================================================
void KWeightingFilter::prepare(double fs)
{
    preFilter.coefficients = juce::dsp::IIR::Coefficients<float>::makeHighPass(fs, 38.0f, 0.707f);
    rlbFilter.coefficients = juce::dsp::IIR::Coefficients<float>::makePeakFilter(fs, 2000.0f, 0.5f, 4.0f);
    preFilter.reset();
    rlbFilter.reset();
}

void KWeightingFilter::processBlock(const float* input, float* output, int numSamples) noexcept
{
    for (int i = 0; i < numSamples; ++i)
    {
        float s = input[i];
        s = preFilter.processSample(s);
        output[i] = rlbFilter.processSample(s);
    }
}

void KWeightingFilter::reset() noexcept
{
    preFilter.reset();
    rlbFilter.reset();
}

//==============================================================================
// LUFS Meter Implementation
//==============================================================================
void LUFSMeter::prepare(double sampleRate, int blockSize)
{
    fs = sampleRate;
    filteredBuf.setSize(2, blockSize);
    kWeight[0].prepare(sampleRate);
    kWeight[1].prepare(sampleRate);
    momentaryWindowSamples = static_cast<int>(sampleRate * 0.4);
    shortTermWindowSamples = static_cast<int>(sampleRate * 3.0);
    reset();
}

bool LUFSMeter::processBlock(const float* const* channels, int numChannels, int numSamples) noexcept
{
    if (!processingActive) return false;
    if (++processCounter < JS_INFLATOR_METER_DECIMATION) return false;
    processCounter = 0;

    for (int ch = 0; ch < numChannels && ch < 2; ++ch)
        kWeight[ch].processBlock(channels[ch], filteredBuf.getWritePointer(ch), numSamples);

    double blockSum = 0.0;
    int chCount = juce::jmin(numChannels, 2);
    for (int ch = 0; ch < chCount; ++ch)
    {
        const float* src = filteredBuf.getReadPointer(ch);
        double chSum = 0.0;
        for (int i = 0; i < numSamples; ++i)
            chSum += static_cast<double>(src[i] * src[i]);
        blockSum += chSum;
    }
    double avgPower = blockSum / (numSamples * chCount);

    momentarySum += avgPower;
    shortTermSum += avgPower;
    integratedSum += avgPower;
    integratedWeight += 1.0;
    momentaryCount += numSamples;
    shortTermCount += numSamples;

    bool updated = false;
    if (momentaryCount >= momentaryWindowSamples)
    {
        double avg = momentarySum / momentaryCount;
        momentaryLUFS = static_cast<float>(-0.691 + juce::Decibels::gainToDecibels(avg + 1e-12));
        momentarySum = 0.0; momentaryCount = 0;
        updated = true;
    }
    if (shortTermCount >= shortTermWindowSamples)
    {
        double avg = shortTermSum / shortTermCount;
        shortTermLUFS = static_cast<float>(-0.691 + juce::Decibels::gainToDecibels(avg + 1e-12));
        shortTermSum = 0.0; shortTermCount = 0;
        updated = true;
    }
    if (integratedWeight > 0.0)
    {
        double avg = integratedSum / integratedWeight;
        integratedLUFS = static_cast<float>(-0.691 + juce::Decibels::gainToDecibels(avg + 1e-12));
    }
    return updated;
}

void LUFSMeter::reset() noexcept
{
    momentarySum = shortTermSum = integratedSum = integratedWeight = 0.0;
    momentaryCount = shortTermCount = 0;
    momentaryLUFS = shortTermLUFS = integratedLUFS = -100.0f;
    processCounter = 0;
    kWeight[0].reset(); kWeight[1].reset();
}

//==============================================================================
// AdvancedLimiter Implementation
//==============================================================================
void AdvancedLimiter::prepare(double fs)
{
    lookahead = juce::jmin(MAX_BUF, juce::jmax(1, static_cast<int>(fs * 0.001)));
    updateReleaseCoeff(fs, 1.0);
    reset();
}

void AdvancedLimiter::updateReleaseCoeff(double fs, double adaptiveFactor) noexcept
{
    double releaseTime = releaseBase * (1.0 - adaptiveFactor * 0.99) + 0.002 * adaptiveFactor;
    releaseCoeff = std::exp(-1.0 / (fs * releaseTime));
}

void AdvancedLimiter::processBlock(float* L, float* R, int numSamples, double ceiling, bool useTruePeak, double adaptiveFactor) noexcept
{
    updateReleaseCoeff(44100.0, adaptiveFactor);
    for (int i = 0; i < numSamples; ++i)
    {
        bufL[writePos] = L[i];
        bufR[writePos] = R[i];
        double peak = juce::jmax(std::fabs(L[i]), std::fabs(R[i]));
        if (useTruePeak && writePos > 0)
        {
            double prevL = bufL[(writePos - 1 + MAX_BUF) % MAX_BUF];
            double prevR = bufR[(writePos - 1 + MAX_BUF) % MAX_BUF];
            double midL = (prevL + L[i]) * 0.5;
            double midR = (prevR + R[i]) * 0.5;
            peak = juce::jmax(peak, juce::jmax(std::fabs(midL), std::fabs(midR)));
        }
        if (peak > ceiling && peak > 1e-12)
        {
            double targetGain = ceiling / peak;
            gainReduction = juce::jmin(gainReduction, targetGain);
        }
        gainReduction += (1.0 - gainReduction) * (1.0 - releaseCoeff);
        gainReduction = juce::jmin(gainReduction, 1.0);
        int rp = (writePos - lookahead + MAX_BUF) % MAX_BUF;
        L[i] = static_cast<float>(bufL[rp] * gainReduction);
        R[i] = static_cast<float>(bufR[rp] * gainReduction);
        writePos = (writePos + 1) % MAX_BUF;
    }
    gainReductionDb = juce::Decibels::gainToDecibels(gainReduction);
}

void AdvancedLimiter::reset() noexcept
{
    juce::zeromem(bufL, sizeof(bufL));
    juce::zeromem(bufR, sizeof(bufR));
    writePos = 0;
    gainReduction = 1.0;
    gainReductionDb = 0.0;
}

//==============================================================================
// SubHarmonicGenerator Implementation
//==============================================================================
void SubHarmonicGenerator::prepare(double fs)
{
    coeff = 1.0 - std::exp(-2.0 * juce::MathConstants<double>::pi * 80.0 / fs);
    reset();
}

//==============================================================================
// MultiStageSaturation Implementation
//==============================================================================
void MultiStageSaturation::setCharacter(CharacterMode mode)
{
    switch (mode)
    {
    case CharacterMode::Warm:
        driveScale = 1.3f; satCoeff = 0.3f; clipCoeff = 0.5f; asymmetry = 0.2f; clipThreshold = 1.2f; dryMix = 0.3f; wetMix = 0.7f; break;
    case CharacterMode::Neutral:
        driveScale = 1.0f; satCoeff = 0.5f; clipCoeff = 0.5f; asymmetry = 0.0f; clipThreshold = 1.0f; dryMix = 0.0f; wetMix = 1.0f; break;
    case CharacterMode::Aggressive:
        driveScale = 1.5f; satCoeff = 0.8f; clipCoeff = 0.3f; asymmetry = 0.4f; clipThreshold = 0.8f; dryMix = 0.0f; wetMix = 1.0f; break;
    case CharacterMode::Transparent:
        driveScale = 1.1f; satCoeff = 0.1f; clipCoeff = 0.7f; asymmetry = 0.0f; clipThreshold = 1.5f; dryMix = 0.5f; wetMix = 0.5f; break;
    }
}

//==============================================================================
// Parameter Layout (v1.5)
//==============================================================================
juce::AudioProcessorValueTreeState::ParameterLayout
JSInflatorProcessor::createParameterLayout()
{
    using Range = juce::NormalisableRange<float>;
    juce::AudioProcessorValueTreeState::ParameterLayout layout;
    auto dBStr = [](float v, int) -> juce::String { return juce::String(v, 1) + " dB"; };
    auto pctStr = [](float v, int) -> juce::String { return juce::String(v, 1) + "%"; };
    auto freqStr = [](float v, int) -> juce::String { return juce::String(v, 0) + " Hz"; };
    auto targetStr = [](float v, int) -> juce::String { return juce::String(v, 1) + " LUFS"; };

    layout.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID(ParamID::INPUT, 1), "Input Gain", Range(-12.0f, 12.0f, 0.01f), 0.0f, juce::AudioParameterFloatAttributes().withStringFromValueFunction(dBStr)));
    layout.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID(ParamID::EFFECT, 1), "Effect", Range(0.0f, 100.0f, 0.1f), 0.0f, juce::AudioParameterFloatAttributes().withStringFromValueFunction(pctStr)));
    layout.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID(ParamID::CURVE, 1), "Curve", Range(0.0f, 100.0f, 0.1f), 50.0f));
    layout.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID(ParamID::OUTPUT, 1), "Output Gain", Range(-12.0f, 0.0f, 0.01f), 0.0f, juce::AudioParameterFloatAttributes().withStringFromValueFunction(dBStr)));
    layout.add(std::make_unique<juce::AudioParameterChoice>(juce::ParameterID(ParamID::OS, 1), "Oversampling", juce::StringArray{ "1x", "2x", "4x", "8x" }, 0));
    layout.add(std::make_unique<juce::AudioParameterChoice>(juce::ParameterID(ParamID::OS_QUAL, 1), "OS Phase", juce::StringArray{ "Min Phase", "Linear Phase" }, 0));
    layout.add(std::make_unique<juce::AudioParameterChoice>(juce::ParameterID(ParamID::CLIP_MODE, 1), "Clip Mode", juce::StringArray{ "Off", "Hard", "Soft", "Hard+Soft" }, 0));
    layout.add(std::make_unique<juce::AudioParameterChoice>(juce::ParameterID(ParamID::SPLIT_TYPE, 1), "Split Type", juce::StringArray{ "Simple", "Original SVF" }, 0));
    layout.add(std::make_unique<juce::AudioParameterBool>(juce::ParameterID(ParamID::SPLIT, 1), "Band Split", false));
    layout.add(std::make_unique<juce::AudioParameterBool>(juce::ParameterID(ParamID::MS_MODE, 1), "Mid/Side", false));
    layout.add(std::make_unique<juce::AudioParameterBool>(juce::ParameterID(ParamID::IN, 1), "In", true));
    layout.add(std::make_unique<juce::AudioParameterBool>(juce::ParameterID(ParamID::BYPASS, 1), "Bypass", false));
    layout.add(std::make_unique<juce::AudioParameterChoice>(juce::ParameterID(ParamID::AGC_MODE, 1), "Auto Gain", juce::StringArray{ "Off", "Static", "Dynamic" }, 0));
    layout.add(std::make_unique<juce::AudioParameterBool>(juce::ParameterID(ParamID::DELTA, 1), "Delta Monitor", false));
    layout.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID(ParamID::TONE, 1), "Tone", Range(-50.0f, 50.0f, 0.1f), 0.0f));
    layout.add(std::make_unique<juce::AudioParameterChoice>(juce::ParameterID(ParamID::FOCUS, 1), "Frequency Focus", juce::StringArray{ "Full", "Low", "Mid", "High" }, 0));
    layout.add(std::make_unique<juce::AudioParameterChoice>(juce::ParameterID(ParamID::DYN_MODE, 1), "Dynamics Sens", juce::StringArray{ "Smooth", "Neutral", "Punch", "Dense" }, 1));
    layout.add(std::make_unique<juce::AudioParameterBool>(juce::ParameterID(ParamID::LIMITER, 1), "Safety Limiter", false));
    layout.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID(ParamID::LIM_CEIL, 1), "Limiter Ceiling", Range(-6.0f, -0.1f, 0.05f), -0.3f, juce::AudioParameterFloatAttributes().withStringFromValueFunction(dBStr)));
    layout.add(std::make_unique<juce::AudioParameterBool>(juce::ParameterID(ParamID::DC_BLOCK, 1), "DC Blocker", true));
    layout.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID(ParamID::TILT_FREQ, 1), "Tilt Freq", Range(200.0f, 2000.0f, 1.0f), 800.0f, juce::AudioParameterFloatAttributes().withStringFromValueFunction(freqStr)));
    layout.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID(ParamID::SUB_HARMONIC, 1), "Sub Harmonic", Range(0.0f, 100.0f, 0.5f), 0.0f, juce::AudioParameterFloatAttributes().withStringFromValueFunction(pctStr)));
    layout.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID(ParamID::STEREO_WIDTH, 1), "Stereo Width", Range(0.0f, 200.0f, 0.5f), 100.0f));
    layout.add(std::make_unique<juce::AudioParameterChoice>(juce::ParameterID(ParamID::CHARACTER, 1), "Character", juce::StringArray{ "Warm", "Neutral", "Aggressive", "Transparent" }, 1));
    layout.add(std::make_unique<juce::AudioParameterBool>(juce::ParameterID(ParamID::ADAPT_RELEASE, 1), "Adaptive Release", true));
    layout.add(std::make_unique<juce::AudioParameterBool>(juce::ParameterID(ParamID::TRUE_PEAK, 1), "True Peak Limiting", true));
    layout.add(std::make_unique<juce::AudioParameterChoice>(juce::ParameterID(ParamID::DITHER, 1), "Dither", juce::StringArray{ "Off", "16-bit", "24-bit" }, 0));
    layout.add(std::make_unique<juce::AudioParameterBool>(juce::ParameterID(ParamID::LEVEL_MATCH, 1), "Level Match Bypass", false));
    layout.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID(ParamID::LUFS_TARGET, 1), "LUFS Target", Range(-30.0f, -10.0f, 0.5f), -14.0f, juce::AudioParameterFloatAttributes().withStringFromValueFunction(targetStr)));
    layout.add(std::make_unique<juce::AudioParameterBool>(juce::ParameterID(ParamID::CORR_METER_ON, 1), "Correlation Meter", true));
    return layout;
}

//==============================================================================
// Constructor / Destructor
//==============================================================================
JSInflatorProcessor::JSInflatorProcessor()
    : AudioProcessor(BusesProperties().withInput("Input", juce::AudioChannelSet::stereo(), true)
        .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
    apvts(*this, nullptr, "JSInflator", createParameterLayout())
{
    apvts.addParameterListener(ParamID::OS, &osListener);
    apvts.addParameterListener(ParamID::OS_QUAL, &osListener);
    updateCurveCoefficients(0.5);
    multiSat.setCharacter(CharacterMode::Neutral);
}

JSInflatorProcessor::~JSInflatorProcessor()
{
    apvts.removeParameterListener(ParamID::OS, &osListener);
    apvts.removeParameterListener(ParamID::OS_QUAL, &osListener);
}

//==============================================================================
// Bus Layout Support
//==============================================================================
bool JSInflatorProcessor::isBusesLayoutSupported(const BusesLayout& l) const
{
    auto& in = l.getMainInputChannelSet();
    return in == l.getMainOutputChannelSet() && (in == juce::AudioChannelSet::mono() || in == juce::AudioChannelSet::stereo());
}

//==============================================================================
// Prepare to Play
//==============================================================================
void JSInflatorProcessor::prepareToPlay(double sr, int blks)
{
    currentSampleRate = sr;
    currentBlockSize = blks;
    int numCh = getTotalNumInputChannels();

    for (int qi = 0; qi < 2; ++qi)
    {
        auto ft = (qi == 0) ? juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR
            : juce::dsp::Oversampling<float>::filterHalfBandFIREquiripple;
        for (int st = 1; st <= 3; ++st)
        {
            int idx = (st - 1) + qi * 3;
            oversamplers[idx] = std::make_unique<juce::dsp::Oversampling<float>>(numCh, st, ft, true, false);
            oversamplers[idx]->initProcessing(static_cast<size_t>(blks));
        }
    }

    int osIdx = static_cast<int>(apvts.getRawParameterValue(ParamID::OS)->load() + 0.5f);
    int osQual = static_cast<int>(apvts.getRawParameterValue(ParamID::OS_QUAL)->load() + 0.5f);
    requestedOSIndex.store(osIdx);
    requestedOSQual.store(osQual);
    int osLat = 0;
    if (osIdx > 0)
    {
        int ai = getOversamplerArrayIndex(osIdx, osQual);
        if (ai >= 0 && ai < 6 && oversamplers[ai]) osLat = static_cast<int>(oversamplers[ai]->getLatencyInSamples());
    }
    advancedLimiter.prepare(sr);
    setLatencySamples(osLat + advancedLimiter.getLatency());

    dryDelayLen = juce::jmin(osLat + advancedLimiter.getLatency(), DRY_DELAY_MAX - 1);
    dryDelayWrite = 0;
    std::fill(dryDelayL, dryDelayL + DRY_DELAY_MAX, 0.0f);
    std::fill(dryDelayR, dryDelayR + DRY_DELAY_MAX, 0.0f);

    for (int ch = 0; ch < 2; ++ch)
    {
        svfSplit[ch].setFrequencies(240, 2400, sr);
        simpleSplit[ch].setFrequencies(240, 2400, sr);
        svfSplitOS[ch].reset();
        simpleSplitOS[ch].reset();
        dcBlocker[ch].reset();
        tiltEQ[ch].prepare(sr);
        transientDet[ch].prepare(sr);
        inputFollower[ch].prepare(sr);
        outputFollower[ch].prepare(sr);
    }
    subHarmonic.prepare(sr);
    lufsMeter.prepare(sr, blks);
    corrMeter.prepare(sr);
    dcOffsetMeter.prepare(sr);
    for (int ch = 0; ch < 2; ++ch) inputMeter[ch].prepare(sr), outputMeter[ch].prepare(sr);

    inputGain.reset(sr, 0.020);
    outputGain.reset(sr, 0.020);
    effectWet.reset(sr, 0.020);
    curveSmoother.reset(sr, 0.020);
    toneSmoother.reset(sr, 0.020);
    agcGainSmooth.reset(sr, 0.500);
    auto dB2g = [](float dB) { return juce::Decibels::decibelsToGain<double>(dB); };
    inputGain.setCurrentAndTargetValue(dB2g(apvts.getRawParameterValue(ParamID::INPUT)->load()));
    outputGain.setCurrentAndTargetValue(dB2g(apvts.getRawParameterValue(ParamID::OUTPUT)->load()));
    effectWet.setCurrentAndTargetValue(apvts.getRawParameterValue(ParamID::EFFECT)->load() / 100.0);
    curveSmoother.setCurrentAndTargetValue(apvts.getRawParameterValue(ParamID::CURVE)->load() / 100.0);
    toneSmoother.setCurrentAndTargetValue(apvts.getRawParameterValue(ParamID::TONE)->load() / 50.0);
    agcGainSmooth.setCurrentAndTargetValue(1.0);
    agcCoeffSlow = std::exp(-1.0 / (sr * 5.0));
    agcCoeffFast = std::exp(-1.0 / (sr * 0.5));
    rmsCoeff = std::exp(-1.0 / (sr * 0.300));
    peakHoldSamples = static_cast<int>(sr * 2.0);
    peakHoldCounter = 0;
    dryBuffer.setSize(numCh, blks);
}

void JSInflatorProcessor::releaseResources()
{
    for (auto& os : oversamplers) os.reset();
    lufsMeter.reset();
    corrMeter.reset();
    dcOffsetMeter.reset();
}

//==============================================================================
// Oversampling Parameter Change Handler
//==============================================================================
void JSInflatorProcessor::onOSParamChanged(int nI, int nQ)
{
    requestedOSIndex.store(nI);
    requestedOSQual.store(nQ);
    int osLat = 0;
    if (nI > 0)
    {
        int ai = getOversamplerArrayIndex(nI, nQ);
        if (ai >= 0 && ai < 6 && oversamplers[ai]) osLat = static_cast<int>(oversamplers[ai]->getLatencyInSamples());
    }
    setLatencySamples(osLat + advancedLimiter.getLatency());
    updateHostDisplay(juce::AudioProcessorListener::ChangeDetails().withLatencyChanged(true));
}

//==============================================================================
// Dry Delay Buffer Helpers
//==============================================================================
void JSInflatorProcessor::pushDry(float l, float r) noexcept
{
    dryDelayL[dryDelayWrite] = l;
    dryDelayR[dryDelayWrite] = r;
    dryDelayWrite = (dryDelayWrite + 1) % DRY_DELAY_MAX;
}

void JSInflatorProcessor::peekDry(float& l, float& r) const noexcept
{
    int rp = (dryDelayWrite - dryDelayLen + DRY_DELAY_MAX) % DRY_DELAY_MAX;
    l = dryDelayL[rp];
    r = dryDelayR[rp];
}

//==============================================================================
// Core Inflator Processing
//==============================================================================
double JSInflatorProcessor::processInflatorSample(double x) const noexcept
{
    double sign = (x >= 0.0) ? 1.0 : -1.0;
    double s1 = std::fabs(x), s2 = s1 * s1, s3 = s2 * s1, s4 = s2 * s2;
    double out;
    if (s1 >= 2.0) out = 0.0;
    else if (s1 > 1.0) out = 2.0 * s1 - s2;
    else out = curveA * s1 + curveB * s2 + curveC * s3 - curveD * (s2 - 2.0 * s3 + s4);
    return out * sign;
}

void JSInflatorProcessor::updateCurveCoefficients(double cp_norm) noexcept
{
    double cp = cp_norm - 0.5;
    curveA = 1.5 + cp;
    curveB = -(cp + cp);
    curveC = cp - 0.5;
    curveD = 0.0625 - cp * 0.25 + cp * cp * 0.25;
}

double JSInflatorProcessor::softClip(double x) noexcept
{
    if (x > 4.0) return 1.0;
    if (x < -4.0) return -1.0;
    double x2 = x * x;
    return x * (27.0 + x2) / (27.0 + 9.0 * x2);
}

double JSInflatorProcessor::hardClip(double x) noexcept
{
    return juce::jlimit(-1.0, 1.0, x);
}

void JSInflatorProcessor::processBandSplit(int ch, bool useOS, bool useSVF, double x,
    double& l, double& m, double& h, double& G, double& GR) noexcept
{
    if (useSVF)
    {
        auto& bs = useOS ? svfSplitOS[ch] : svfSplit[ch];
        bs.process(x, l, m, h);
        G = bs.G; GR = bs.GR;
    }
    else
    {
        auto& bs = useOS ? simpleSplitOS[ch] : simpleSplit[ch];
        bs.process(x, l, m, h);
        G = bs.G; GR = bs.GR;
    }
}

//==============================================================================
// MAIN PROCESS BLOCK (float version, corrected)
//==============================================================================
void JSInflatorProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    auto raw = [this](juce::StringRef id) { return apvts.getRawParameterValue(id)->load(); };

    bool bypass = raw(ParamID::BYPASS) > 0.5f;
    bool isIn = raw(ParamID::IN) > 0.5f;
    bool doSplit = raw(ParamID::SPLIT) > 0.5f;
    bool doMS = raw(ParamID::MS_MODE) > 0.5f;
    bool doDelta = raw(ParamID::DELTA) > 0.5f;
    bool doLim = raw(ParamID::LIMITER) > 0.5f;
    bool doDC = raw(ParamID::DC_BLOCK) > 0.5f;
    bool doAdaptRelease = raw(ParamID::ADAPT_RELEASE) > 0.5f;
    bool doTruePeak = raw(ParamID::TRUE_PEAK) > 0.5f;
    bool levelMatch = raw(ParamID::LEVEL_MATCH) > 0.5f;
    bool corrMeterOn = raw(ParamID::CORR_METER_ON) > 0.5f;

    float subAmount = raw(ParamID::SUB_HARMONIC) / 100.0f;
    float stereoWidth = raw(ParamID::STEREO_WIDTH) / 100.0f;
    double limCeil = juce::Decibels::decibelsToGain(raw(ParamID::LIM_CEIL));
    int ditherMode = static_cast<int>(raw(ParamID::DITHER) + 0.5f);
    int character = static_cast<int>(raw(ParamID::CHARACTER) + 0.5f);
    int clipMode = static_cast<int>(raw(ParamID::CLIP_MODE) + 0.5f);
    int splitType = static_cast<int>(raw(ParamID::SPLIT_TYPE) + 0.5f);
    bool useSVF = (splitType == 1);
    int agcMode = static_cast<int>(raw(ParamID::AGC_MODE) + 0.5f);
    int focus = static_cast<int>(raw(ParamID::FOCUS) + 0.5f);
    int dynMode = static_cast<int>(raw(ParamID::DYN_MODE) + 0.5f);

    CharacterMode charMode = CharacterMode::Neutral;
    switch (character) {
    case 0: charMode = CharacterMode::Warm; break;
    case 2: charMode = CharacterMode::Aggressive; break;
    case 3: charMode = CharacterMode::Transparent; break;
    default: charMode = CharacterMode::Neutral;
    }
    multiSat.setCharacter(charMode);

    int osIdx = requestedOSIndex.load();
    int osQual = requestedOSQual.load();
    int numCh = buffer.getNumChannels();
    int numS = buffer.getNumSamples();

    inputGain.setTargetValue(juce::Decibels::decibelsToGain<double>(raw(ParamID::INPUT)));
    outputGain.setTargetValue(juce::Decibels::decibelsToGain<double>(raw(ParamID::OUTPUT)));
    effectWet.setTargetValue(raw(ParamID::EFFECT) / 100.0);
    curveSmoother.setTargetValue(raw(ParamID::CURVE) / 100.0);
    toneSmoother.setTargetValue(raw(ParamID::TONE) / 50.0);

    double adaptiveFactor = 0.5;
    if (doAdaptRelease && numCh >= 2) adaptiveFactor = 0.7; // simplified

    lufsMeter.processBlock(buffer.getArrayOfReadPointers(), numCh, numS);
    momentaryLUFS.store(lufsMeter.getMomentary());
    shortTermLUFS.store(lufsMeter.getShortTerm());
    integratedLUFS.store(lufsMeter.getIntegrated());
    for (int ch = 0; ch < numCh; ++ch) dcOffsetMeter.processBlock(buffer.getReadPointer(ch), numS);
    dcOffset.store(dcOffsetMeter.getOffset());

    if (bypass)
    {
        for (int ch = 0; ch < numCh; ++ch) outputFollower[ch].update(0.0f);
        return;
    }

    // M/S encode
    if (doMS && numCh == 2)
    {
        float* L = buffer.getWritePointer(0);
        float* R = buffer.getWritePointer(1);
        for (int i = 0; i < numS; ++i) encodeMS(L[i], R[i]);
    }

    juce::dsp::AudioBlock<float> block(buffer);
    juce::dsp::AudioBlock<float> osBlock;
    juce::dsp::Oversampling<float>* activeOS = nullptr;
    if (osIdx > 0)
    {
        int ai = getOversamplerArrayIndex(osIdx, osQual);
        if (ai >= 0 && ai < 6 && oversamplers[ai])
        {
            activeOS = oversamplers[ai].get();
            osBlock = activeOS->processSamplesUp(block);
        }
    }
    if (!activeOS) osBlock = block;
    int osN = static_cast<int>(osBlock.getNumSamples());
    double osFS = currentSampleRate * (osIdx == 0 ? 1 : (1 << osIdx));

    for (int ch = 0; ch < juce::jmin(numCh, 2); ++ch)
    {
        svfSplitOS[ch].setFrequencies(240, 2400, osFS);
        svfSplitOS[ch].reset();
        simpleSplitOS[ch].setFrequencies(240, 2400, osFS);
        simpleSplitOS[ch].reset();
    }
    updateCurveCoefficients(curveSmoother.getCurrentValue());
    double toneNorm = toneSmoother.getCurrentValue();
    double tiltLow = juce::Decibels::decibelsToGain(-toneNorm * 6.0);
    double tiltHigh = juce::Decibels::decibelsToGain(toneNorm * 6.0);

    for (int ch = 0; ch < juce::jmin(numCh, 2); ++ch)
    {
        float* samples = osBlock.getChannelPointer(ch);
        for (int i = 0; i < osN; ++i)
        {
            double inG = inputGain.getNextValue();
            double outG = outputGain.getNextValue();
            double wet = effectWet.getNextValue();
            double x = static_cast<double>(samples[i]) * inG;
            if (clipMode == 1 || clipMode == 3) x = hardClip(x);
            x = juce::jlimit(-2.0, 2.0, x);
            double dry = x;
            double processed = x;
            if (isIn)
            {
                if (doSplit)
                {
                    double l, m, h, G, GR;
                    processBandSplit(ch, true, useSVF, x, l, m, h, G, GR);
                    double midNorm = m * G;
                    switch (focus)
                    {
                    case 0: l = multiSat.process(static_cast<float>(l), static_cast<float>(wet));
                        m = multiSat.process(static_cast<float>(midNorm), static_cast<float>(wet)) * GR;
                        h = multiSat.process(static_cast<float>(h), static_cast<float>(wet));
                        break;
                    case 1: l = multiSat.process(static_cast<float>(l), static_cast<float>(wet));
                        m = midNorm * GR; break;
                    case 2: m = multiSat.process(static_cast<float>(midNorm), static_cast<float>(wet)) * GR; break;
                    case 3: h = multiSat.process(static_cast<float>(h), static_cast<float>(wet)); break;
                    }
                    processed = l + m + h;
                }
                else
                {
                    processed = multiSat.process(static_cast<float>(x), static_cast<float>(wet));
                }
            }
            if (subAmount > 0.001f) processed = subHarmonic.process(static_cast<float>(processed), subAmount);
            switch (clipMode)
            {
            case 1: processed = hardClip(processed); break;
            case 2: processed = softClip(processed); break;
            case 3: processed = hardClip(softClip(processed)); break;
            }
            processed = dry * (1.0 - wet) + processed * wet;
            if (doDC) processed = dcBlocker[ch].process(static_cast<float>(processed));
            processed = tiltEQ[ch].process(static_cast<float>(processed), tiltLow, tiltHigh);
            processed *= outG;
            samples[i] = static_cast<float>(processed);
        }
    }
    if (activeOS) activeOS->processSamplesDown(block);
    if (stereoWidth != 1.0f && numCh == 2)
    {
        float* L = buffer.getWritePointer(0);
        float* R = buffer.getWritePointer(1);
        for (int i = 0; i < numS; ++i)
        {
            float m = (L[i] + R[i]) * 0.5f;
            float s = (L[i] - R[i]) * 0.5f * stereoWidth;
            L[i] = m + s;
            R[i] = m - s;
        }
    }
    if (doDelta && numCh >= 2)
    {
        float* outL = buffer.getWritePointer(0);
        float* outR = buffer.getWritePointer(1);
        int bs = (dryDelayWrite - numS + DRY_DELAY_MAX) % DRY_DELAY_MAX;
        for (int i = 0; i < numS; ++i)
        {
            int rp = (bs + i) % DRY_DELAY_MAX;
            outL[i] -= dryDelayL[rp];
            outR[i] -= dryDelayR[rp];
        }
    }
    if (doLim && numCh >= 2)
    {
        float* L = buffer.getWritePointer(0);
        float* R = buffer.getWritePointer(1);
        advancedLimiter.processBlock(L, R, numS, limCeil, doTruePeak, adaptiveFactor);
        limiterGR.store(static_cast<float>(advancedLimiter.getGR()));
    }
    if (doMS && numCh == 2)
    {
        float* L = buffer.getWritePointer(0);
        float* R = buffer.getWritePointer(1);
        for (int i = 0; i < numS; ++i) decodeMS(L[i], R[i]);
    }
    if (agcMode > 0)
    {
        double coeff = (agcMode == 2) ? agcCoeffFast : agcCoeffSlow;
        double inPow = (inRmsAcc[0] + (numCh > 1 ? inRmsAcc[1] : inRmsAcc[0])) * 0.5;
        double outPow = 0.0;
        for (int ch = 0; ch < juce::jmin(numCh, 2); ++ch)
        {
            const float* src = buffer.getReadPointer(ch);
            for (int i = 0; i < numS; ++i) outPow += src[i] * src[i];
        }
        outPow /= (numS * juce::jmin(numCh, 2));
        agcRmsIn = coeff * agcRmsIn + (1.0 - coeff) * inPow;
        agcRmsOut = coeff * agcRmsOut + (1.0 - coeff) * outPow;
        if (agcRmsOut > 1e-12 && agcRmsIn > 1e-12)
            agcGainSmooth.setTargetValue(juce::jlimit(0.1, 4.0, std::sqrt(agcRmsIn / agcRmsOut)));
        buffer.applyGain(static_cast<float>(agcGainSmooth.getNextValue()));
    }
    if (ditherMode > 0)
    {
        float bits = (ditherMode == 1) ? 16.0f : 24.0f;
        float scale = std::pow(2.0f, bits - 1);
        float ditherAmp = 1.0f / scale;
        for (int ch = 0; ch < numCh; ++ch)
        {
            float* samples = buffer.getWritePointer(ch);
            for (int i = 0; i < numS; ++i)
            {
                float noise = (juce::Random::getSystemRandom().nextFloat() * 2.0f - 1.0f) +
                    (juce::Random::getSystemRandom().nextFloat() * 2.0f - 1.0f);
                samples[i] += noise * ditherAmp;
            }
        }
    }
    if (corrMeterOn && numCh >= 2)
    {
        const float* L = buffer.getReadPointer(0);
        const float* R = buffer.getReadPointer(1);
        corrMeter.processBlock(L, R, numS);
        correlation.store(corrMeter.getCorrelation());
    }
}

//==============================================================================
// DOUBLE-PRECISION PROCESS BLOCK – completely rewritten to avoid copyFrom issues
//==============================================================================
void JSInflatorProcessor::processBlock(juce::AudioBuffer<double>& buffer, juce::MidiBuffer& midi)
{
    const int numChannels = buffer.getNumChannels();
    const int numSamples = buffer.getNumSamples();
    juce::AudioBuffer<float> fb(numChannels, numSamples);

    // Convert double to float
    for (int ch = 0; ch < numChannels; ++ch)
    {
        const double* src = buffer.getReadPointer(ch);
        float* dst = fb.getWritePointer(ch);
        for (int i = 0; i < numSamples; ++i)
            dst[i] = static_cast<float>(src[i]);
    }

    processBlock(fb, midi);

    // Convert float back to double
    for (int ch = 0; ch < numChannels; ++ch)
    {
        const float* src = fb.getReadPointer(ch);
        double* dst = buffer.getWritePointer(ch);
        for (int i = 0; i < numSamples; ++i)
            dst[i] = static_cast<double>(src[i]);
    }
}

//==============================================================================
// Editor & State Management
//==============================================================================
juce::AudioProcessorEditor* JSInflatorProcessor::createEditor()
{
    return new JSInflatorEditor(*this);
}

void JSInflatorProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    auto xml = apvts.copyState().createXml();
    copyXmlToBinary(*xml, destData);
}

void JSInflatorProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    auto xml = getXmlFromBinary(data, sizeInBytes);
    if (xml && xml->hasTagName(apvts.state.getType()))
    {
        apvts.replaceState(juce::ValueTree::fromXml(*xml));
        int idx = static_cast<int>(apvts.getRawParameterValue(ParamID::OS)->load() + 0.5f);
        int qual = static_cast<int>(apvts.getRawParameterValue(ParamID::OS_QUAL)->load() + 0.5f);
        onOSParamChanged(idx, qual);
    }
}

//==============================================================================
// Plugin Factory
//==============================================================================
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new JSInflatorProcessor();
}