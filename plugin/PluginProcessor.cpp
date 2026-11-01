#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "DefaultCab.h"

//==============================================================================
// Background builder: constructing and DC-settling an amp model takes ~0.1 - 0.3 s, so it is never
// done on the audio thread when the quality / oversampling changes.
class AmpDualRectProcessor::Builder : public juce::Thread
{
public:
    explicit Builder (AmpDualRectProcessor& o) : juce::Thread ("AmpDualRect builder"), owner (o) {}
    void run() override
    {
        while (! threadShouldExit())
        {
            const int stages = owner.requestedStages.exchange (-1);
            if (stages < 0) { wait (200); continue; }
            owner.engineBusy = true;
            const int gen = owner.generation.load();
            auto e = owner.buildEngine (owner.requestedRate.load(), stages, owner.currentParams());
            {
                const juce::SpinLock::ScopedLockType l (owner.engineLock);
                // publish only if nothing newer was requested and prepareToPlay has not been called meanwhile
                if (owner.requestedStages.load() < 0 && gen == owner.generation.load()) { owner.pending = std::move (e); owner.pendingFresh = true; }
            }
            owner.engineBusy = false;
        }
    }
private:
    AmpDualRectProcessor& owner;
};

//==============================================================================
AmpDualRectProcessor::AmpDualRectProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "STATE", createParameterLayout())
{
    auto raw = [this] (const char* id) { return apvts.getRawParameterValue (id); };
    pInGain = raw (ids::inGain); pGateOn = raw (ids::gateOn); pGateThr = raw (ids::gateThresh); pGateDec = raw (ids::gateDecay); pGateHpf = raw (ids::gateHpf);
    pChannel = raw (ids::channel); pOrMode = raw (ids::orMode); pRdMode = raw (ids::rdMode);
    pTubes = raw (ids::tubes); pRect = raw (ids::rectifier); pPower = raw (ids::power);
    pTight = raw (ids::tight); pDefizz = raw (ids::defizz); pDeep = raw (ids::scaryDeep); pMidShift = raw (ids::midShift);
    for (int i = 0; i < kNumPots; ++i) pPot[i] = raw (ids::pots[i]);
    pQuality = raw (ids::quality); pIrOn = raw (ids::irOn); pLowCut = raw (ids::lowCut); pHighCut = raw (ids::highCut); pOut = raw (ids::outLevel);

    builder = std::make_unique<Builder> (*this);
    builder->startThread (juce::Thread::Priority::low);
}

AmpDualRectProcessor::~AmpDualRectProcessor()
{
    builder->stopThread (5000);
    cancelPendingUpdate();
}

bool AmpDualRectProcessor::isBusesLayoutSupported (const BusesLayout& l) const
{
    const auto in = l.getMainInputChannelSet(), out = l.getMainOutputChannelSet();
    if (out != juce::AudioChannelSet::mono() && out != juce::AudioChannelSet::stereo()) return false;
    return in == juce::AudioChannelSet::mono() || in == juce::AudioChannelSet::stereo();
}

//==============================================================================
int AmpDualRectProcessor::stagesFor (double hostRate, int quality)
{
    const double target = quality == 1 ? 192000.0 : (quality == 2 ? 48000.0 : 96000.0);
    int s = 0;
    while (hostRate * (double) (1 << s) < target * 0.9 && s < 4) ++s;
    return s;
}

AmpDualRectProcessor::Switches AmpDualRectProcessor::wantedSwitches() const
{
    Switches s;
    s.channel = juce::jlimit (0, 1, (int) std::lround (pChannel->load()));
    s.orMode = juce::jlimit (0, 2, (int) std::lround (pOrMode->load()));
    s.rdMode = juce::jlimit (0, 1, (int) std::lround (pRdMode->load()));
    s.el34 = pTubes->load() > 0.5f;
    s.tubeRect = pRect->load() > 0.5f;
    s.spongy = pPower->load() > 0.5f;
    s.tight = pTight->load() > 0.5f;
    s.defizz = pDefizz->load() > 0.5f;
    s.deep = pDeep->load() > 0.5f;
    s.midShift = pMidShift->load() > 0.5f;
    s.scaryOn = drect::scaryEngaged ((double) pPot[12]->load() * 0.1);
    return s;
}

drect::Params AmpDualRectProcessor::makeParams (const Switches& s, const double* v) const
{
    drect::Params p;
    p.channel = s.channel; p.orMode = s.orMode; p.rdMode = s.rdMode;
    p.el34 = s.el34; p.tubeRect = s.tubeRect; p.spongy = s.spongy;
    p.tight = s.tight; p.defizz = s.defizz; p.deep = s.deep; p.midShift = s.midShift;
    p.orGain = v[0]; p.orTreble = v[1]; p.orMid = v[2]; p.orBass = v[3]; p.orPres = v[4]; p.orMaster = v[5];
    p.rdGain = v[6]; p.rdTreble = v[7]; p.rdMid = v[8]; p.rdBass = v[9]; p.rdPres = v[10]; p.rdMaster = v[11];
    // the Mr. Scary stage is switched in or out only under a duck (see processSub); while it is in,
    // the smoothed knob is kept above the click-out point
    p.scary = s.scaryOn ? std::max (v[12], 0.006) : 0.0;
    return p;
}

drect::Params AmpDualRectProcessor::currentParams() const
{
    double v[kNumPots];
    for (int i = 0; i < kNumPots; ++i) v[i] = (double) pPot[i]->load() * 0.1;
    return makeParams (wantedSwitches(), v);
}

static bool sameParams (const drect::Params& a, const drect::Params& b)
{
    return a.channel == b.channel && a.orMode == b.orMode && a.rdMode == b.rdMode && a.el34 == b.el34 && a.tubeRect == b.tubeRect
        && a.spongy == b.spongy && a.tight == b.tight && a.defizz == b.defizz && a.deep == b.deep && a.midShift == b.midShift
        && a.orGain == b.orGain && a.orTreble == b.orTreble && a.orMid == b.orMid && a.orBass == b.orBass && a.orPres == b.orPres && a.orMaster == b.orMaster
        && a.rdGain == b.rdGain && a.rdTreble == b.rdTreble && a.rdMid == b.rdMid && a.rdBass == b.rdBass && a.rdPres == b.rdPres && a.rdMaster == b.rdMaster
        && a.scary == b.scary;
}

std::unique_ptr<AmpDualRectProcessor::Engine> AmpDualRectProcessor::buildEngine (double rate, int stages, const drect::Params& p) const
{
    auto e = std::make_unique<Engine>();
    e->stages = stages;
    e->coreRate = rate * (double) (1 << stages);
    e->amp.setParams (p);
    e->amp.prepare (e->coreRate);
    return e;
}

void AmpDualRectProcessor::requestEngine (int stages)
{
    requestedRate = hostRate;
    requestedStages = stages;
    builder->notify();
}

void AmpDualRectProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    hostRate = sampleRate;
    maxBlock = std::max (samplesPerBlock, kChunk);
    requestedStages = -1;
    ++generation;                                       // invalidates any build that is still running for the old rate

    // oversamplers for 1..4 stages (2x..16x), all prepared once
    oversamplers.clear();
    oversamplers.resize (5);
    for (int s = 1; s <= 4; ++s)
    {
        oversamplers[(size_t) s] = std::make_unique<juce::dsp::Oversampling<float>> (1, (size_t) s,
            juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR, true, true);
        oversamplers[(size_t) s]->initProcessing ((size_t) kChunk);
    }

    // committed switch state and pots start at the current parameter values (no click on load)
    const auto p = currentParams();
    committed = wantedSwitches();
    for (int i = 0; i < kNumPots; ++i) potCur[i] = (double) pPot[i]->load() * 0.1;
    duckState = 0; holdLeft = 0;
    ampAsleep = false; quietSamples = 0;

    const int stages = stagesFor (sampleRate, (int) pQuality->load());
    {
        auto e = buildEngine (sampleRate, stages, p);          // synchronous: only at (re)initialisation
        const juce::SpinLock::ScopedLockType l (engineLock);
        active = std::move (e); pending.reset(); pendingFresh = false;
    }
    applied = p;
    latencyToReport = stages > 0 ? (int) std::lround (oversamplers[(size_t) stages]->getLatencyInSamples()) : 0;
    coreRateMeter = active->coreRate;
    setLatencySamples (latencyToReport.load());

    // signal-path state
    gate.prepare (sampleRate);
    inGainSm.reset (sampleRate, 0.02);  inGainSm.setCurrentAndTargetValue (juce::Decibels::decibelsToGain (pInGain->load()));
    outGainSm.reset (sampleRate, 0.02); outGainSm.setCurrentAndTargetValue (juce::Decibels::decibelsToGain (pOut->load()));
    irMix.reset (sampleRate, 0.015);    irMix.setCurrentAndTargetValue (pIrOn->load() > 0.5f ? 1.0f : 0.0f);
    irWasOn = pIrOn->load() > 0.5f;
    work.setSize (2, maxBlock); ampOut.setSize (1, maxBlock);
    juce::dsp::ProcessSpec spec { sampleRate, (juce::uint32) maxBlock, 2 };
    convolution.prepare (spec);
    for (auto& f : hp) f.reset();
    for (auto& f : lp) f.reset();
    lastLowCut = lastHighCut = -1.0f;

    dryL.assign ((size_t) maxBlock, 0.0f);
    if (irPath.isEmpty() || ! loadIR (juce::File (irPath))) useDefaultIR();
}

//==============================================================================
void AmpDualRectProcessor::applyAmpParams (bool force)
{
    if (active == nullptr) return;
    const auto p = makeParams (committed, potCur);
    if (! force && sameParams (p, applied)) return;
    active->amp.setParams (p, force);
    applied = p;
}

// in: gated signal at the host rate (full-scale units); out: speaker signal at the host rate (full-scale units)
void AmpDualRectProcessor::processAmpChunk (const float* in, float* out, int m)
{
    auto& eng = *active;
    if (eng.stages == 0)
    {
        for (int i = 0; i < m; ++i) out[i] = (float) (eng.amp.process ((double) in[i] * kInVoltsPerFS) * kOutScale);
        return;
    }
    float tmp[kChunk];
    for (int i = 0; i < m; ++i) tmp[i] = in[i];
    float* chans[1] = { tmp };
    juce::dsp::AudioBlock<float> block (chans, 1, (size_t) m);
    auto& os = *oversamplers[(size_t) eng.stages];
    auto up = os.processSamplesUp (block);
    float* u = up.getChannelPointer (0);
    const size_t nu = up.getNumSamples();
    for (size_t i = 0; i < nu; ++i) u[i] = (float) (eng.amp.process ((double) u[i] * kInVoltsPerFS) * kOutScale);
    os.processSamplesDown (block);
    for (int i = 0; i < m; ++i) out[i] = tmp[i];
}

void AmpDualRectProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    const int total = buffer.getNumSamples();
    if (total <= maxBlock) { processSub (buffer); return; }
    // host delivered more than it announced: process in aliased pieces (no allocation)
    for (int pos = 0; pos < total; pos += maxBlock)
    {
        juce::AudioBuffer<float> piece (buffer.getArrayOfWritePointers(), buffer.getNumChannels(), pos, std::min (maxBlock, total - pos));
        processSub (piece);
    }
}

void AmpDualRectProcessor::processSub (juce::AudioBuffer<float>& buffer)
{
    const int numSamples = buffer.getNumSamples();
    const int nIn = getTotalNumInputChannels(), nOut = getTotalNumOutputChannels();

    // ---- adopt a freshly built engine, if there is one (never blocks) ----
    {
        const juce::SpinLock::ScopedTryLockType l (engineLock);
        if (l.isLocked() && pendingFresh && pending != nullptr)
        {
            std::swap (active, pending);       // old engine ends up in 'pending' and is freed by the builder
            pendingFresh = false;
            if (active->stages > 0) oversamplers[(size_t) active->stages]->reset();
            // the new engine was built with the parameters at request time; bring switches in line
            applied = active->amp.getParams();
            applyAmpParams (true);
            latencyToReport = active->stages > 0 ? (int) std::lround (oversamplers[(size_t) active->stages]->getLatencyInSamples()) : 0;
            coreRateMeter = active->coreRate;
            triggerAsyncUpdate();
        }
    }
    if (active == nullptr || numSamples == 0)
    {
        for (int c = 0; c < nOut; ++c) buffer.clear (c, 0, numSamples);
        return;
    }
    // quality change -> request a rebuild in the background
    {
        const int want = stagesFor (hostRate, (int) pQuality->load());
        if (want != active->stages && requestedStages.load() != want && ! engineBusy.load()) requestEngine (want);
    }

    // ---- control-rate parameter reads ----
    inGainSm.setTargetValue (juce::Decibels::decibelsToGain (pInGain->load()));
    outGainSm.setTargetValue (juce::Decibels::decibelsToGain (pOut->load()));
    gate.setEnabled (pGateOn->load() > 0.5f);
    gate.setThresholdDb (pGateThr->load());
    gate.setReleaseMs (pGateDec->load());
    gate.setDetectorHz (pGateHpf->load());
    const Switches want = wantedSwitches();
    const bool irOn = pIrOn->load() > 0.5f;
    irMix.setTargetValue (irOn ? 1.0f : 0.0f);
    if (irOn && ! irWasOn) convolution.reset();
    irWasOn = irOn;

    // ---- mono sum of the inputs ----
    auto* mono = ampOut.getWritePointer (0);          // reused as scratch for the whole block (size >= numSamples)
    if (nIn >= 2) { const float* a = buffer.getReadPointer (0); const float* b = buffer.getReadPointer (1); for (int i = 0; i < numSamples; ++i) mono[i] = 0.5f * (a[i] + b[i]); }
    else if (nIn == 1) { const float* a = buffer.getReadPointer (0); for (int i = 0; i < numSamples; ++i) mono[i] = a[i]; }
    else juce::FloatVectorOperations::clear (mono, numSamples);

    // ---- input gain + gate + amp, in chunks ----
    const double chunkAlpha = 1.0 - std::exp (-(double) kChunk / (hostRate * 0.012));      // pot smoothing (12 ms)
    float minGate = 1.0f;
    const int holdSamples = (int) (0.025 * hostRate);
    for (int pos = 0; pos < numSamples; pos += kChunk)
    {
        const int m = std::min (kChunk, numSamples - pos);
        float* x = mono + pos;

        // pots: smoothed, applied once per chunk and only when changed
        for (int i = 0; i < kNumPots; ++i)
        {
            const double t = (double) pPot[i]->load() * 0.1;
            potCur[i] += (t - potCur[i]) * chunkAlpha;
            if (std::fabs (t - potCur[i]) < 1e-4) potCur[i] = t;
        }
        // switches (channel / mode LDRs, mods, Mr. Scary in/out, rectifier, power, tube swap):
        // duck the output, change while silent, then fade back in
        float duckFrom = 1.0f, duckTo = 1.0f;
        if (duckState == 0 && want != committed) duckState = 1;
        if (duckState == 1) { duckFrom = 1.0f; duckTo = 0.0f; }
        else if (duckState == 2) { duckFrom = duckTo = 0.0f; }
        else if (duckState == 3) { duckFrom = 0.0f; duckTo = 1.0f; }
        applyAmpParams (false);

        // input gain + noise gate
        for (int i = 0; i < m; ++i)
        {
            x[i] = gate.process (x[i] * inGainSm.getNextValue());
            minGate = std::min (minGate, (float) gate.currentGain());
        }
        float inPeak = 0.0f;
        for (int i = 0; i < m; ++i) inPeak = std::max (inPeak, std::fabs (x[i]));
        if (ampAsleep && (inPeak > 1.0e-6f || duckState != 0)) ampAsleep = false;      // -120 dBFS: wake up
        if (ampAsleep)
            juce::FloatVectorOperations::clear (x, m);
        else
        {
            processAmpChunk (x, x, m);
            float outPeak = 0.0f;
            for (int i = 0; i < m; ++i) outPeak = std::max (outPeak, std::fabs (x[i]));
            if (inPeak < 1.0e-6f && outPeak < 1.0e-5f) quietSamples += m; else quietSamples = 0;
            if (quietSamples > (int) (0.5 * hostRate)) ampAsleep = true;               // 0.5 s below -100 dBFS
        }
        const float step = (duckTo - duckFrom) / (float) m;
        float g = duckFrom;
        for (int i = 0; i < m; ++i) { g += step; x[i] *= g; }

        // state machine advance (after the chunk); a tube swap re-biases the power amp, so hold longer
        if (duckState == 1)
        {
            // a tube swap re-biases the power amp and a rectifier / power change moves the supply: hold longer
            const bool slow = want.el34 != committed.el34 || want.tubeRect != committed.tubeRect || want.spongy != committed.spongy;
            committed = want;
            duckState = 2; holdLeft = slow ? holdSamples * 4 : holdSamples;
        }
        else if (duckState == 2) { holdLeft -= m; if (holdLeft <= 0) duckState = 3; }
        else if (duckState == 3) duckState = 0;
    }
    gateGainMeter = minGate;
    sleepingMeter = ampAsleep;

    // ---- IR loader (defeatable, click-free) ----
    float* l = work.getWritePointer (0);
    float* r = work.getWritePointer (1);
    for (int i = 0; i < numSamples; ++i) l[i] = r[i] = mono[i];
    if (irMix.isSmoothing() || irMix.getTargetValue() > 0.5f)
    {
        const bool xfade = irMix.isSmoothing();
        if (xfade) std::copy (mono, mono + numSamples, dryL.begin());
        juce::dsp::AudioBlock<float> blk (work.getArrayOfWritePointers(), 2, (size_t) numSamples);
        juce::dsp::ProcessContextReplacing<float> ctx (blk);
        convolution.process (ctx);
        if (xfade)
            for (int i = 0; i < numSamples; ++i) { const float w = irMix.getNextValue(); l[i] = dryL[(size_t) i] * (1.0f - w) + l[i] * w; r[i] = dryL[(size_t) i] * (1.0f - w) + r[i] * w; }
    }

    // ---- low cut / high cut (12 dB/oct) ----
    const float lc = pLowCut->load(), hc = pHighCut->load();
    if (lc != lastLowCut || hc != lastHighCut)
    {
        lastLowCut = lc; lastHighCut = hc;
        auto hpc = juce::dsp::IIR::Coefficients<float>::makeHighPass (hostRate, std::min ((double) lc, 0.45 * hostRate), 0.7071f);
        auto lpc = juce::dsp::IIR::Coefficients<float>::makeLowPass  (hostRate, std::min ((double) hc, 0.45 * hostRate), 0.7071f);
        for (int c = 0; c < 2; ++c) { *hp[c].coefficients = *hpc; *lp[c].coefficients = *lpc; }
    }
    const bool useHp = lc > 20.5f, useLp = hc < 19990.0f;
    float* ch[2] = { l, r };
    for (int c = 0; c < 2; ++c)
        for (int i = 0; i < numSamples; ++i)
        {
            float s = ch[c][i];
            if (useHp) s = hp[c].processSample (s);
            if (useLp) s = lp[c].processSample (s);
            ch[c][i] = s;
        }
    if (! useHp) for (auto& f : hp) f.reset();
    if (! useLp) for (auto& f : lp) f.reset();

    // ---- output level + write out ----
    float peak = 0.0f;
    for (int i = 0; i < numSamples; ++i)
    {
        const float g = outGainSm.getNextValue();
        l[i] *= g; r[i] *= g;
        peak = std::max (peak, std::max (std::fabs (l[i]), std::fabs (r[i])));
    }
    if (nOut >= 2) { buffer.copyFrom (0, 0, l, numSamples); buffer.copyFrom (1, 0, r, numSamples); }
    else if (nOut == 1) { auto* o = buffer.getWritePointer (0); for (int i = 0; i < numSamples; ++i) o[i] = 0.5f * (l[i] + r[i]); }
    for (int c = 2; c < nOut; ++c) buffer.clear (c, 0, numSamples);
    outPeakMeter = std::max (outPeakMeter.load() * 0.9f, peak);
}

void AmpDualRectProcessor::handleAsyncUpdate()
{
    setLatencySamples (latencyToReport.load());
}

//==============================================================================
// Scale an IR so that its mean power gain across the guitar band (100 Hz - 6 kHz) is unity.
// This keeps cab-on and cab-off at similar loudness regardless of how the IR was normalised.
static void normaliseIR (juce::AudioBuffer<float>& b, double sampleRate)
{
    const int order = 15, N = 1 << order;           // spectrum estimate from the first 32768 samples
    juce::dsp::FFT fft (order);
    std::vector<float> data ((size_t) N * 2);
    double worst = 0.0;
    for (int c = 0; c < b.getNumChannels(); ++c)
    {
        std::fill (data.begin(), data.end(), 0.0f);
        const int n = std::min (b.getNumSamples(), N);
        std::copy (b.getReadPointer (c), b.getReadPointer (c) + n, data.begin());
        fft.performRealOnlyForwardTransform (data.data(), true);
        double sum = 0.0; int count = 0;
        const int k0 = (int) std::ceil (100.0 * N / sampleRate), k1 = (int) std::floor (6000.0 * N / sampleRate);
        for (int k = std::max (k0, 1); k <= std::min (k1, N / 2 - 1); ++k)
        {
            const double re = data[(size_t) (2 * k)], im = data[(size_t) (2 * k + 1)];
            sum += re * re + im * im; ++count;
        }
        if (count > 0) worst = std::max (worst, sum / count);
    }
    if (worst > 1e-20) b.applyGain ((float) (1.0 / std::sqrt (worst)));
}

bool AmpDualRectProcessor::loadIR (const juce::File& file)
{
    if (! file.existsAsFile()) return false;
    juce::AudioFormatManager fm; fm.registerBasicFormats();
    std::unique_ptr<juce::AudioFormatReader> reader (fm.createReaderFor (file));
    if (reader == nullptr || reader->lengthInSamples <= 0) return false;
    const int len = (int) std::min<juce::int64> (reader->lengthInSamples, (juce::int64) (reader->sampleRate * 6.0));   // at most 6 s
    const int chans = (int) std::min<unsigned int> (reader->numChannels, 2u);
    juce::AudioBuffer<float> buf (chans, len);
    reader->read (&buf, 0, len, 0, true, chans > 1);
    normaliseIR (buf, reader->sampleRate);
    convolution.loadImpulseResponse (std::move (buf), reader->sampleRate, juce::dsp::Convolution::Stereo::yes, juce::dsp::Convolution::Trim::yes, juce::dsp::Convolution::Normalise::no);
    const juce::ScopedLock sl (irLock);
    irPath = file.getFullPathName();
    irDisplay = file.getFileName();
    return true;
}

void AmpDualRectProcessor::useDefaultIR()
{
    auto buf = defaultcab::make (96000.0);
    normaliseIR (buf, 96000.0);
    convolution.loadImpulseResponse (std::move (buf), 96000.0, juce::dsp::Convolution::Stereo::no, juce::dsp::Convolution::Trim::no, juce::dsp::Convolution::Normalise::no);
    const juce::ScopedLock sl (irLock);
    irPath = {};
    irDisplay = "Built-in cab (synthetic)";
}

juce::String AmpDualRectProcessor::getIRName() const
{
    const juce::ScopedLock sl (irLock);
    return irDisplay;
}

//==============================================================================
void AmpDualRectProcessor::getStateInformation (juce::MemoryBlock& dest)
{
    auto state = apvts.copyState();
    {
        const juce::ScopedLock sl (irLock);
        state.setProperty ("irPath", irPath, nullptr);
    }
    state.setProperty ("uiWidth", editorWidth.load(), nullptr);
    if (auto xml = state.createXml()) copyXmlToBinary (*xml, dest);
}

void AmpDualRectProcessor::setStateInformation (const void* data, int size)
{
    if (auto xml = getXmlFromBinary (data, size))
        if (xml->hasTagName (apvts.state.getType()))
        {
            auto vt = juce::ValueTree::fromXml (*xml);
            const auto path = vt.getProperty ("irPath").toString();
            editorWidth = juce::jlimit (800, 2800, (int) vt.getProperty ("uiWidth", 1440));
            apvts.replaceState (vt);
            if (path.isEmpty() || ! loadIR (juce::File (path))) useDefaultIR();
        }
}

juce::AudioProcessorEditor* AmpDualRectProcessor::createEditor() { return new AmpDualRectEditor (*this); }

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new AmpDualRectProcessor(); }
