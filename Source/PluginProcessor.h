//==============================================================================
// JS Inflator  v1.5  -  PluginProcessor.h  (JUCE 8.0.12)
// Optimized for CPU efficiency while maintaining audio quality
//==============================================================================
#pragma once
#include <JuceHeader.h>
#include <array>
#include <atomic>
#include <cmath>
#include <cstring>
#include <memory>
#include <vector>

//==============================================================================
// Performance tuning flags (adjust for your target platform)
//==============================================================================
#ifndef JS_INFLATOR_METER_DECIMATION
#define JS_INFLATOR_METER_DECIMATION 4      // Process meters every N samples
#endif
#ifndef JS_INFLATOR_LUFS_DOWNSAMPLE
#define JS_INFLATOR_LUFS_DOWNSAMPLE 2       // Process LUFS at fs/N
#endif
#ifndef JS_INFLATOR_ENABLE_SIMD
#define JS_INFLATOR_ENABLE_SIMD 1           // Use JUCE's vector ops where possible
#endif

//==============================================================================
// Parameter IDs
//==============================================================================
namespace ParamID
{
    inline const juce::String INPUT{ "input" };
    inline const juce::String EFFECT{ "effect" };
    inline const juce::String CURVE{ "curve" };
    inline const juce::String OUTPUT{ "output" };
    inline const juce::String SPLIT{ "split" };
    inline const juce::String MS_MODE{ "msmode" };
    inline const juce::String IN{ "in" };
    inline const juce::String BYPASS{ "bypass" };
    inline const juce::String CLIP_MODE{ "clipMode" };
    inline const juce::String OS{ "os" };
    inline const juce::String OS_QUAL{ "osQual" };
    inline const juce::String SPLIT_TYPE{ "splitType" };
    inline const juce::String AGC_MODE{ "agcMode" };
    inline const juce::String DELTA{ "delta" };
    inline const juce::String TONE{ "tone" };
    inline const juce::String FOCUS{ "focus" };
    inline const juce::String DYN_MODE{ "dynMode" };
    inline const juce::String LIMITER{ "limiter" };
    inline const juce::String LIM_CEIL{ "limCeil" };
    inline const juce::String DC_BLOCK{ "dcBlock" };

    // New parameters
    inline const juce::String TILT_FREQ{ "tiltFreq" };
    inline const juce::String SUB_HARMONIC{ "subHarmonic" };
    inline const juce::String STEREO_WIDTH{ "stereoWidth" };
    inline const juce::String CHARACTER{ "character" };
    inline const juce::String ADAPT_RELEASE{ "adaptRelease" };
    inline const juce::String TRUE_PEAK{ "truePeak" };
    inline const juce::String DITHER{ "dither" };
    inline const juce::String LEVEL_MATCH{ "levelMatch" };
    inline const juce::String LUFS_TARGET{ "lufsTarget" };
    inline const juce::String CORR_METER_ON{ "corrMeterOn" };
}

//==============================================================================
// M/S encode/decode helpers
//==============================================================================
inline void encodeMS(float& L, float& R) noexcept
{
    float m = (L + R) * 0.5f;
    float s = (L - R) * 0.5f;
    L = m;
    R = s;
}

inline void decodeMS(float& L, float& R) noexcept
{
    float m = L;
    float s = R;
    L = m + s;
    R = m - s;
}

//==============================================================================
// Simple DC Blocker (1‑pole high‑pass)
//==============================================================================
struct DCBlocker
{
    void reset() noexcept { x1 = y1 = 0.0f; }
    float process(float x) noexcept
    {
        float y = x - x1 + 0.9997f * y1;
        x1 = x;
        y1 = y;
        return y;
    }
private:
    float x1 = 0.0f, y1 = 0.0f;
};

//==============================================================================
// Fixed Tilt EQ – no lowState/highState errors
//==============================================================================
struct TiltEQ
{
    void prepare(double fs)
    {
        sampleRate = fs;
        reset();
    }
    void reset() noexcept
    {
        lowZ1 = lowZ2 = lowY1 = lowY2 = 0.0;
        highZ1 = highZ2 = highY1 = highY2 = 0.0;
    }

    float process(float x, double lowGain, double highGain) noexcept
    {
        // Low shelf filter
        if (std::abs(lowGain) > 0.001)
        {
            double w0 = 2.0 * juce::MathConstants<double>::pi * 800.0 / sampleRate;
            double cosW0 = std::cos(w0);
            double sinW0 = std::sin(w0);
            double A = std::pow(10.0, lowGain / 20.0);
            double b0 = A * ((A + 1.0) + (A - 1.0) * cosW0 + 2.0 * std::sqrt(A) * sinW0);
            double b1 = -2.0 * A * ((A - 1.0) + (A + 1.0) * cosW0);
            double b2 = A * ((A + 1.0) + (A - 1.0) * cosW0 - 2.0 * std::sqrt(A) * sinW0);
            double a0 = (A + 1.0) - (A - 1.0) * cosW0 + 2.0 * std::sqrt(A) * sinW0;
            double a1 = 2.0 * ((A - 1.0) - (A + 1.0) * cosW0);
            double a2 = (A + 1.0) - (A - 1.0) * cosW0 - 2.0 * std::sqrt(A) * sinW0;
            double y = (b0 / a0) * x + (b1 / a0) * lowZ1 + (b2 / a0) * lowZ2 - (a1 / a0) * lowY1 - (a2 / a0) * lowY2;
            lowZ2 = lowZ1; lowZ1 = x;
            lowY2 = lowY1; lowY1 = y;
            x = static_cast<float>(y);
        }
        // High shelf filter
        if (std::abs(highGain) > 0.001)
        {
            double w0 = 2.0 * juce::MathConstants<double>::pi * 800.0 / sampleRate;
            double cosW0 = std::cos(w0);
            double sinW0 = std::sin(w0);
            double A = std::pow(10.0, highGain / 20.0);
            double b0 = A * ((A + 1.0) - (A - 1.0) * cosW0 + 2.0 * std::sqrt(A) * sinW0);
            double b1 = 2.0 * A * ((A - 1.0) - (A + 1.0) * cosW0);
            double b2 = A * ((A + 1.0) - (A - 1.0) * cosW0 - 2.0 * std::sqrt(A) * sinW0);
            double a0 = (A + 1.0) + (A - 1.0) * cosW0 + 2.0 * std::sqrt(A) * sinW0;
            double a1 = -2.0 * ((A - 1.0) + (A + 1.0) * cosW0);
            double a2 = (A + 1.0) + (A - 1.0) * cosW0 - 2.0 * std::sqrt(A) * sinW0;
            double y = (b0 / a0) * x + (b1 / a0) * highZ1 + (b2 / a0) * highZ2 - (a1 / a0) * highY1 - (a2 / a0) * highY2;
            highZ2 = highZ1; highZ1 = x;
            highY2 = highY1; highY1 = y;
            x = static_cast<float>(y);
        }
        return x;
    }

private:
    double sampleRate = 44100.0;
    double lowZ1 = 0.0, lowZ2 = 0.0, lowY1 = 0.0, lowY2 = 0.0;
    double highZ1 = 0.0, highZ2 = 0.0, highY1 = 0.0, highY2 = 0.0;
};

//==============================================================================
// Simple Band Split (placeholder)
//==============================================================================
struct SimpleBandSplit
{
    void setFrequencies(float, float, double) noexcept {}
    void reset() noexcept {}
    void process(double x, double& low, double& mid, double& high) noexcept
    {
        low = mid = high = x;
    }
    double G = 1.0, GR = 1.0;
};

//==============================================================================
// Original SVF Band Split (placeholder)
//==============================================================================
struct OriginalSVFBandSplit
{
    void setFrequencies(float, float, double) noexcept {}
    void reset() noexcept {}
    void process(double x, double& low, double& mid, double& high) noexcept
    {
        low = mid = high = x;
    }
    double G = 1.0, GR = 1.0;
};

//==============================================================================
// Transient Detector
//==============================================================================
struct TransientDetector
{
    void prepare(double fs) noexcept { sampleRate = fs; reset(); }
    void reset() noexcept { prev = 0.0f; env = 0.0f; }
    double process(double x) noexcept
    {
        double diff = std::fabs(x - prev);
        prev = x;
        env = 0.99 * env + 0.01 * diff;
        return juce::jlimit(0.0, 1.0, env * 100.0);
    }
private:
    double sampleRate = 44100.0;
    double prev = 0.0, env = 0.0;
};

//==============================================================================
// Level Follower
//==============================================================================
struct LevelFollower
{
    void prepare(double fs) noexcept { sampleRate = fs; reset(); }
    void reset() noexcept { level = 0.0f; }
    void update(float s) noexcept
    {
        float absS = std::fabs(s);
        level = 0.999f * level + 0.001f * absS;
    }
    float getValue() const noexcept { return level; }
private:
    double sampleRate = 44100.0;
    float level = 0.0f;
};

//==============================================================================
// Meter Accumulator
//==============================================================================
template<typename FloatType = float>
struct MeterAccumulator
{
    void prepare(double, int decimationFactor = JS_INFLATOR_METER_DECIMATION)
    {
        decim = decimationFactor;
        counter = 0;
        reset();
    }
    void reset() noexcept { sum = 0.0; sumSq = 0.0; count = 0; peak = 0.0; }
    void accumulateBlock(const FloatType* samples, int numSamples) noexcept
    {
        for (int i = 0; i < numSamples; ++i)
        {
            FloatType s = samples[i];
            sum += s;
            sumSq += s * s;
            peak = juce::jmax(peak, std::fabs(s));
        }
        count += numSamples;
    }
    FloatType getRMS() const noexcept { return count > 0 ? static_cast<FloatType>(std::sqrt(sumSq / count)) : FloatType(0); }
    FloatType getPeak() const noexcept { return peak; }
    FloatType getMean() const noexcept { return count > 0 ? static_cast<FloatType>(sum / count) : FloatType(0); }
private:
    double sum = 0.0, sumSq = 0.0, peak = 0.0;
    int count = 0, decim = JS_INFLATOR_METER_DECIMATION, counter = 0;
};

//==============================================================================
// K‑Weighting Filter
//==============================================================================
struct KWeightingFilter 
{
    KWeightingFilter() = default;
    void prepare(double fs);
    void processBlock(const float* input, float* output, int numSamples) noexcept;
    void reset() noexcept;
private:
    juce::dsp::IIR::Filter<float> preFilter, rlbFilter;
    JUCE_DECLARE_NON_COPYABLE(KWeightingFilter)
};

//==============================================================================
// LUFS Meter
//==============================================================================
class LUFSMeter
{
public:
    LUFSMeter() = default;            // Fix default constructor
    void prepare(double fs, int blockSize);
    bool processBlock(const float* const* channels, int numChannels, int numSamples) noexcept;
    float getMomentary() const noexcept { return momentaryLUFS; }
    float getShortTerm() const noexcept { return shortTermLUFS; }
    float getIntegrated() const noexcept { return integratedLUFS; }
    void reset() noexcept;
    void setProcessingActive(bool active) noexcept { processingActive = active; }
private:
    KWeightingFilter kWeight[2];
    juce::AudioBuffer<float> filteredBuf;
    double fs = 44100.0;
    double momentarySum = 0.0, shortTermSum = 0.0, integratedSum = 0.0, integratedWeight = 0.0;
    int momentaryCount = 0, shortTermCount = 0;
    int momentaryWindowSamples = 0, shortTermWindowSamples = 0;
    float momentaryLUFS = -100.0f, shortTermLUFS = -100.0f, integratedLUFS = -100.0f;
    bool processingActive = true;
    int processCounter = 0;
    JUCE_DECLARE_NON_COPYABLE(LUFSMeter)
};

//==============================================================================
// Stereo Correlation Meter
//==============================================================================
class StereoCorrelationMeter
{
public:
    void prepare(double fs) noexcept { sampleRate = fs; reset(); }
    void processBlock(const float* L, const float* R, int numSamples) noexcept
    {
        for (int i = 0; i < numSamples; ++i)
        {
            double l = L[i], r = R[i];
            acc += l * r;
            sumSqL += l * l;
            sumSqR += r * r;
        }
        count += numSamples;
        if (count >= updateInterval) updateCorrelation();
    }
    float getCorrelation() const noexcept { return correlation; }
    void reset() noexcept { acc = sumSqL = sumSqR = 0.0; count = 0; correlation = 1.0f; }
private:
    void updateCorrelation() noexcept
    {
        double denom = std::sqrt(sumSqL * sumSqR);
        correlation = denom > 1e-12 ? static_cast<float>(juce::jlimit(-1.0, 1.0, acc / denom)) : 1.0f;
        acc = sumSqL = sumSqR = 0.0;
        count = 0;
    }
    double acc = 0.0, sumSqL = 0.0, sumSqR = 0.0;
    int count = 0;
    float correlation = 1.0f;
    double sampleRate = 44100.0;
    int updateInterval = 44100 / JS_INFLATOR_METER_DECIMATION;
};

//==============================================================================
// DC Offset Meter
//==============================================================================
class DCOffsetMeter
{
public:
    void prepare(double fs) noexcept
    {
        windowSize = static_cast<int>(fs * 0.1);
        buffer.resize(windowSize);
        reset();
    }
    void processBlock(const float* samples, int numSamples) noexcept
    {
        for (int i = 0; i < numSamples; ++i)
        {
            buffer[writePos] = samples[i];
            writePos = (writePos + 1) % windowSize;
            if (count < windowSize) ++count;
        }
        updateOffset();
    }
    float getOffset() const noexcept { return offset; }
    void reset() noexcept
    {
        std::fill(buffer.begin(), buffer.end(), 0.0f);
        writePos = count = 0;
        offset = 0.0f;
    }
private:
    void updateOffset() noexcept
    {
        if (count == 0) return;
        double sum = 0.0;
        for (int i = 0; i < count; ++i) sum += buffer[i];
        offset = static_cast<float>(sum / count);
    }
    std::vector<float> buffer;
    int writePos = 0, count = 0, windowSize = 4410;
    float offset = 0.0f;
};

//==============================================================================
// Advanced Limiter
//==============================================================================
class AdvancedLimiter
{
public:
    void prepare(double fs);
    void processBlock(float* L, float* R, int numSamples, double ceiling, bool useTruePeak, double adaptiveFactor) noexcept;
    void reset() noexcept;
    double getGR() const noexcept { return gainReductionDb; }
    int getLatency() const noexcept { return lookahead; }
private:
    static constexpr int MAX_BUF = 256;
    double bufL[MAX_BUF] = {}, bufR[MAX_BUF] = {};
    int writePos = 0, lookahead = 48;
    double gainReduction = 1.0, gainReductionDb = 0.0;
    double releaseCoeff = 0.999, releaseBase = 0.050;
    void updateReleaseCoeff(double fs, double adaptiveFactor) noexcept;
};

//==============================================================================
// Sub‑Harmonic Generator
//==============================================================================
class SubHarmonicGenerator
{
public:
    void prepare(double fs);
    inline float process(float x, float amount) noexcept
    {
        if (amount < 0.001f) return x;
        double rect = std::fabs(static_cast<double>(x));
        lpState += coeff * (rect - lpState);
        return x + static_cast<float>(lpState) * amount * 0.5f;
    }
    void reset() noexcept { lpState = 0.0; }
private:
    double lpState = 0.0;
    double coeff = 0.0;
};

//==============================================================================
// Character Modes
//==============================================================================
enum class CharacterMode { Warm, Neutral, Aggressive, Transparent };

//==============================================================================
// Multi‑Stage Saturation – process method made const
//==============================================================================
class MultiStageSaturation
{
public:
    void setCharacter(CharacterMode mode);
    inline float process(float x, float drive) const noexcept     // const added
    {
        if (drive < 0.001f) return x;
        float dry = x;
        float s = x * driveScale;
        float s2 = s * s;
        float tape = s * (1.0f + s2 * satCoeff) / (1.0f + s2 * clipCoeff);
        if (asymmetry != 0.0f)
            tape += asymmetry * tape * tape * tape * 0.1f;
        float clipped = juce::jlimit(-clipThreshold, clipThreshold, tape);
        return dry * dryMix + clipped * wetMix;
    }
    void reset() noexcept {}
private:
    CharacterMode currentMode = CharacterMode::Neutral;
    float driveScale = 1.0f, satCoeff = 0.5f, clipCoeff = 0.5f;
    float asymmetry = 0.0f, clipThreshold = 1.0f;
    float dryMix = 0.0f, wetMix = 1.0f;
};

//==============================================================================
// Main Processor Class
//==============================================================================
class JSInflatorProcessor final : public juce::AudioProcessor
{
public:
    JSInflatorProcessor();
    ~JSInflatorProcessor() override;

    void prepareToPlay(double sr, int blks) override;
    void releaseResources() override;
    bool isBusesLayoutSupported(const BusesLayout&) const override;

    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    void processBlock(juce::AudioBuffer<double>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }
    const juce::String getName() const override { return "JS Inflator"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return {}; }
    void changeProgramName(int, const juce::String&) override {}

    void getStateInformation(juce::MemoryBlock&) override;
    void setStateInformation(const void*, int) override;

    juce::AudioProcessorValueTreeState apvts;

    // Meter getters
    float getInLevelL() const noexcept { return inLevelL.load(); }
    float getInLevelR() const noexcept { return inLevelR.load(); }
    float getOutLevelL() const noexcept { return outLevelL.load(); }
    float getOutLevelR() const noexcept { return outLevelR.load(); }
    float getInPeakL() const noexcept { return inPeakL.load(); }
    float getInPeakR() const noexcept { return inPeakR.load(); }
    float getOutPeakL() const noexcept { return outPeakL.load(); }
    float getOutPeakR() const noexcept { return outPeakR.load(); }
    float getInRmsL() const noexcept { return inRmsL.load(); }
    float getInRmsR() const noexcept { return inRmsR.load(); }
    float getOutRmsL() const noexcept { return outRmsL.load(); }
    float getOutRmsR() const noexcept { return outRmsR.load(); }
    int getAndClearInOversL() noexcept { return inOversL.exchange(0); }
    int getAndClearInOversR() noexcept { return inOversR.exchange(0); }
    int getAndClearOutOversL() noexcept { return outOversL.exchange(0); }
    int getAndClearOutOversR() noexcept { return outOversR.exchange(0); }
    float getEffectMeter() const noexcept { return effectMeter.load(); }
    float getLimiterGR() const noexcept { return limiterGR.load(); }
    float getCorrelation() const noexcept { return correlation.load(); }
    float getMomentaryLUFS() const noexcept { return momentaryLUFS.load(); }
    float getShortTermLUFS() const noexcept { return shortTermLUFS.load(); }
    float getIntegratedLUFS() const noexcept { return integratedLUFS.load(); }
    float getDCOffset() const noexcept { return dcOffset.load(); }

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    double processInflatorSample(double x) const noexcept;
    void updateCurveCoefficients(double c) noexcept;
    double curveA = 1.5, curveB = 0.0, curveC = -0.5, curveD = 0.0625;

    static double softClip(double x) noexcept;
    static double hardClip(double x) noexcept;

    std::array<std::unique_ptr<juce::dsp::Oversampling<float>>, 6> oversamplers;
    std::atomic<int> requestedOSIndex{ 0 }, requestedOSQual{ 0 };
    int currentOSIndex = -1, currentOSQual = -1;
    void onOSParamChanged(int i, int q);
    inline int getOversamplerArrayIndex(int i, int q) const noexcept { return (i - 1) + q * 3; }

    std::array<OriginalSVFBandSplit, 2> svfSplit, svfSplitOS;
    std::array<SimpleBandSplit, 2> simpleSplit, simpleSplitOS;
    void processBandSplit(int ch, bool useOS, bool useSVF, double x,
        double& l, double& m, double& h, double& G, double& GR) noexcept;

    std::array<DCBlocker, 2> dcBlocker;
    std::array<TiltEQ, 2> tiltEQ;
    std::array<TransientDetector, 2> transientDet;
    std::array<LevelFollower, 2> inputFollower, outputFollower;

    LUFSMeter lufsMeter;
    StereoCorrelationMeter corrMeter;
    DCOffsetMeter dcOffsetMeter;
    MeterAccumulator<float> inputMeter[2], outputMeter[2];

    AdvancedLimiter advancedLimiter;
    SubHarmonicGenerator subHarmonic;
    MultiStageSaturation multiSat;

    static constexpr int DRY_DELAY_MAX = 16384;
    float dryDelayL[DRY_DELAY_MAX] = {}, dryDelayR[DRY_DELAY_MAX] = {};
    int dryDelayWrite = 0, dryDelayLen = 0;
    void pushDry(float l, float r) noexcept;
    void peekDry(float& l, float& r) const noexcept;

    double agcRmsIn = 0, agcRmsOut = 0, agcCoeffSlow = 0, agcCoeffFast = 0;
    juce::SmoothedValue<double, juce::ValueSmoothingTypes::Multiplicative> agcGainSmooth;
    juce::SmoothedValue<double, juce::ValueSmoothingTypes::Multiplicative> inputGain, outputGain;
    juce::SmoothedValue<double, juce::ValueSmoothingTypes::Linear> effectWet, curveSmoother, toneSmoother;
    std::array<double, 2> inRmsAcc = { 0, 0 }, outRmsAcc = { 0, 0 };
    double rmsCoeff = 0.0;
    float peakHoldInL = 0, peakHoldInR = 0, peakHoldOutL = 0, peakHoldOutR = 0;
    int peakHoldCounter = 0, peakHoldSamples = 0;
    double dryRMS = 0.0, wetRMS = 0.0, levelMatchGain = 1.0;
    juce::AudioBuffer<float> dryBuffer;

    std::atomic<float> inLevelL{ 0 }, inLevelR{ 0 }, outLevelL{ 0 }, outLevelR{ 0 };
    std::atomic<float> inPeakL{ 0 }, inPeakR{ 0 }, outPeakL{ 0 }, outPeakR{ 0 };
    std::atomic<float> inRmsL{ 0 }, inRmsR{ 0 }, outRmsL{ 0 }, outRmsR{ 0 };
    std::atomic<int> inOversL{ 0 }, inOversR{ 0 }, outOversL{ 0 }, outOversR{ 0 };
    std::atomic<float> effectMeter{ 0 }, limiterGR{ 0 };
    std::atomic<float> correlation{ 1.0f };
    std::atomic<float> momentaryLUFS{ -100.0f }, shortTermLUFS{ -100.0f }, integratedLUFS{ -100.0f };
    std::atomic<float> dcOffset{ 0.0f };

    double currentSampleRate = 44100.0, currentBlockSize = 512;
    bool uiVisible = true;

    struct OSListener : public juce::AudioProcessorValueTreeState::Listener
    {
        explicit OSListener(JSInflatorProcessor& p) : proc(p) {}
        void parameterChanged(const juce::String&, float) override
        {
            int idx = static_cast<int>(proc.apvts.getRawParameterValue(ParamID::OS)->load() + 0.5f);
            int qual = static_cast<int>(proc.apvts.getRawParameterValue(ParamID::OS_QUAL)->load() + 0.5f);
            proc.onOSParamChanged(idx, qual);
        }
        JSInflatorProcessor& proc;
    } osListener{ *this };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(JSInflatorProcessor)
};