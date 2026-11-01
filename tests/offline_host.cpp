// offline_host.cpp - headless test host for AmpDualRect.
//   * renders a synthetic guitar DI through the real processor for a set of channel / mode / mod
//     settings and writes WAV files, reporting CPU time per second of audio
//   * silence-sleep and switching tests
//   * saves PNG snapshots of the editor
// usage: AmpDualRectTestHost <output dir>
#include <juce_audio_utils/juce_audio_utils.h>
#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <chrono>
#include <random>

// Karplus-Strong plucked strings: a palm-muted E5 riff and an open chord, ~4 s at 48 kHz
static juce::AudioBuffer<float> makeDI (double fs)
{
    const int n = (int) (4.0 * fs);
    juce::AudioBuffer<float> b (1, n);
    b.clear();
    std::mt19937 rng (7);
    std::uniform_real_distribution<float> U (-1.0f, 1.0f);
    auto pluck = [&] (double t0, double freq, double amp, double decay)
    {
        const int start = (int) (t0 * fs);
        const int period = std::max (2, (int) std::lround (fs / freq));
        std::vector<float> line ((size_t) period);
        for (auto& v : line) v = U (rng);
        float prev = 0.0f;
        for (int i = start, k = 0; i < n; ++i, ++k)
        {
            const size_t idx = (size_t) (k % period);
            const float cur = line[idx];
            const float out = (float) decay * 0.5f * (cur + prev);
            prev = cur; line[idx] = out;
            b.addSample (0, i, (float) amp * cur);
            if (k > (int) (fs * 1.5)) break;
        }
    };
    const double e2 = 82.41, b2 = 123.47, e3 = 164.81;
    for (int i = 0; i < 8; ++i) { pluck (0.25 * i, e2, 0.25, 0.992); pluck (0.25 * i, b2, 0.18, 0.992); }
    for (double f : { e2, b2, e3, 207.65, 246.94, 329.63 }) pluck (2.2, f, 0.12, 0.9985);
    return b;
}

static void writeWav (const juce::File& f, const juce::AudioBuffer<float>& b, double fs)
{
    f.deleteFile();
    juce::WavAudioFormat wav;
    std::unique_ptr<juce::FileOutputStream> os (f.createOutputStream());
    if (os == nullptr) return;
    std::unique_ptr<juce::AudioFormatWriter> w (wav.createWriterFor (os.get(), fs, (unsigned) b.getNumChannels(), 24, {}, 0));
    if (w == nullptr) return;
    os.release();
    w->writeFromAudioSampleBuffer (b, 0, b.getNumSamples());
}

static void setParam (AmpDualRectProcessor& p, const char* id, float value)
{
    auto* prm = p.apvts.getParameter (id);
    prm->setValueNotifyingHost (prm->convertTo0to1 (value));
}

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI init;
    const juce::File outDir (argc > 1 ? juce::String (argv[1]) : juce::File::getCurrentWorkingDirectory().getFullPathName());
    outDir.createDirectory();
    const double fs = 48000.0;
    const int block = 256;
    const auto di = makeDI (fs);
    const bool snapshotsOnly = argc > 2 && juce::String (argv[2]) == "ui";

    struct Case { const char* name; int channel, orMode, rdMode; float gain, master, scary; bool deep, tight, defizz; int tubes, rect, power; bool midShift = false; };
    const Case cases[] = {
        { "orange_cln",            0, 0, 0, 2.0f, 1.5f, 0.0f, false, false, false, 0, 0, 0 },
        { "orange_norm",           0, 1, 0, 6.0f, 4.0f, 0.0f, false, false, false, 0, 0, 0 },
        { "orange_mod",            0, 2, 0, 6.0f, 3.0f, 0.0f, false, false, false, 0, 0, 0 },
        { "red_norm",              1, 0, 0, 6.5f, 3.0f, 0.0f, false, false, false, 0, 0, 0 },
        { "red_vint",              1, 0, 1, 6.5f, 4.0f, 0.0f, false, false, false, 0, 0, 0 },
        { "red_norm_tight_defizz", 1, 0, 0, 6.5f, 3.0f, 0.0f, false, true,  true,  0, 0, 0 },
        { "red_norm_scary6",       1, 0, 0, 6.5f, 3.0f, 6.0f, false, false, false, 0, 0, 0 },
        { "red_norm_scary6_deep",  1, 0, 0, 6.5f, 3.0f, 6.0f, true,  false, false, 0, 0, 0 },
        { "red_norm_scary10_all",  1, 0, 0, 6.5f, 3.0f, 10.0f, true, true,  true,  0, 0, 0 },
        { "red_norm_el34_tube_spongy", 1, 0, 0, 6.5f, 4.0f, 0.0f, false, false, false, 1, 1, 1 },
        { "red_norm_midshift",     1, 0, 0, 6.5f, 3.0f, 0.0f, false, false, false, 0, 0, 0, true },
    };
    if (! snapshotsOnly)
    {
        for (auto& c : cases)
        {
            AmpDualRectProcessor proc;
            setParam (proc, ids::channel, (float) c.channel);
            setParam (proc, ids::orMode, (float) c.orMode);
            setParam (proc, ids::rdMode, (float) c.rdMode);
            setParam (proc, ids::orGain, c.gain); setParam (proc, ids::rdGain, c.gain);
            setParam (proc, ids::orMaster, c.master); setParam (proc, ids::rdMaster, c.master);
            setParam (proc, ids::scaryGain, c.scary);
            setParam (proc, ids::scaryDeep, c.deep ? 1.0f : 0.0f);
            setParam (proc, ids::tight, c.tight ? 1.0f : 0.0f);
            setParam (proc, ids::defizz, c.defizz ? 1.0f : 0.0f);
            setParam (proc, ids::midShift, c.midShift ? 1.0f : 0.0f);
            setParam (proc, ids::tubes, (float) c.tubes);
            setParam (proc, ids::rectifier, (float) c.rect);
            setParam (proc, ids::power, (float) c.power);
            if (auto* q = std::getenv ("AMP_QUALITY")) setParam (proc, ids::quality, (float) std::atoi (q));
            proc.setPlayConfigDetails (2, 2, fs, block);
            proc.prepareToPlay (fs, block);
            juce::AudioBuffer<float> out (2, di.getNumSamples());
            juce::MidiBuffer midi;
            const auto t0 = std::chrono::steady_clock::now();
            for (int pos = 0; pos < di.getNumSamples(); pos += block)
            {
                const int m = std::min (block, di.getNumSamples() - pos);
                juce::AudioBuffer<float> buf (2, m);
                buf.copyFrom (0, 0, di, 0, pos, m); buf.copyFrom (1, 0, di, 0, pos, m);
                proc.processBlock (buf, midi);
                out.copyFrom (0, pos, buf, 0, 0, m); out.copyFrom (1, pos, buf, 1, 0, m);
            }
            const double secs = std::chrono::duration<double> (std::chrono::steady_clock::now() - t0).count();
            float peak = out.getMagnitude (0, out.getNumSamples());
            bool finite = true;
            for (int i = 0; i < out.getNumSamples(); ++i) if (! std::isfinite (out.getSample (0, i))) { finite = false; break; }
            std::printf ("%-26s peak %6.1f dBFS  rms %6.1f dBFS  CPU %5.1f%% of real time  latency %d smp  %s\n", c.name,
                         juce::Decibels::gainToDecibels (peak), juce::Decibels::gainToDecibels (out.getRMSLevel (0, 0, out.getNumSamples())),
                         100.0 * secs / (di.getNumSamples() / fs), proc.getLatencySamples(), finite ? "" : "NON-FINITE!");
            writeWav (outDir.getChildFile (juce::String (c.name) + ".wav"), out, fs);
        }
        writeWav (outDir.getChildFile ("di_input.wav"), di, fs);

        // silence-sleep test: DI, 2 s of digital silence, DI again
        {
            AmpDualRectProcessor proc;
            proc.setPlayConfigDetails (2, 2, fs, block);
            proc.prepareToPlay (fs, block);
            juce::MidiBuffer midi;
            const int seg = di.getNumSamples(), total = seg * 2 + (int) (2.0 * fs);
            juce::AudioBuffer<float> out (2, total);
            double idle = 0.0; int sleptBlocks = 0;
            for (int pos = 0; pos < total; pos += block)
            {
                const int m = std::min (block, total - pos);
                juce::AudioBuffer<float> buf (2, m);
                for (int i = 0; i < m; ++i)
                {
                    const int k = pos + i;
                    const float v = k < seg ? di.getSample (0, k) : (k < seg + (int) (2.0 * fs) ? 0.0f : di.getSample (0, k - seg - (int) (2.0 * fs)));
                    buf.setSample (0, i, v); buf.setSample (1, i, v);
                }
                const auto t0 = std::chrono::steady_clock::now();
                proc.processBlock (buf, midi);
                const double dt = std::chrono::duration<double> (std::chrono::steady_clock::now() - t0).count();
                if (pos > seg + (int) (1.0 * fs) && pos + m < seg + (int) (2.0 * fs)) { idle += dt; if (proc.sleepingMeter.load()) ++sleptBlocks; }
                out.copyFrom (0, pos, buf, 0, 0, m); out.copyFrom (1, pos, buf, 1, 0, m);
            }
            std::printf ("silence sleep              asleep in %d of %d silent blocks, CPU while silent %.1f%% of real time\n",
                         sleptBlocks, (int) (1.0 * fs / block) - 1, 100.0 * idle / 1.0);
            writeWav (outDir.getChildFile ("silence_test.wav"), out, fs);
        }

        // switching test: channel, modes, mods, Mr. Scary in/out and power switches every 0.4 s while playing
        {
            AmpDualRectProcessor proc;
            proc.setPlayConfigDetails (2, 2, fs, block);
            proc.prepareToPlay (fs, block);
            juce::MidiBuffer midi;
            const int total = (int) (8.0 * fs);
            juce::AudioBuffer<float> out (2, total);
            int step = -1;
            for (int pos = 0; pos < total; pos += block)
            {
                const int s = (int) (pos / (0.4 * fs));
                if (s != step)
                {
                    step = s;
                    setParam (proc, ids::channel, (float) (s % 2));
                    setParam (proc, ids::orMode, (float) ((s / 2) % 3));
                    setParam (proc, ids::rdMode, (float) ((s / 3) % 2));
                    setParam (proc, ids::tight, (float) ((s / 4) % 2));
                    setParam (proc, ids::defizz, (float) ((s / 5) % 2));
                    setParam (proc, ids::scaryGain, (s / 6) % 2 ? 7.0f : 0.0f);
                    setParam (proc, ids::scaryDeep, (float) ((s / 7) % 2));
                    setParam (proc, ids::tubes, (float) ((s / 8) % 2));
                    setParam (proc, ids::rectifier, (float) ((s / 9) % 2));
                    setParam (proc, ids::power, (float) ((s / 10) % 2));
                    setParam (proc, ids::midShift, (float) ((s / 3) % 2));
                }
                const int m = std::min (block, total - pos);
                juce::AudioBuffer<float> buf (2, m);
                for (int i = 0; i < m; ++i) { const float v = di.getSample (0, (pos + i) % di.getNumSamples()); buf.setSample (0, i, v); buf.setSample (1, i, v); }
                proc.processBlock (buf, midi);
                out.copyFrom (0, pos, buf, 0, 0, m); out.copyFrom (1, pos, buf, 1, 0, m);
            }
            bool finite = true;
            for (int i = 0; i < total; ++i) if (! std::isfinite (out.getSample (0, i))) { finite = false; break; }
            // click detector: largest sample-to-sample jump relative to the signal's typical jump
            double maxJump = 0.0, sumJump = 0.0;
            for (int i = 1; i < total; ++i) { const double d = std::fabs (out.getSample (0, i) - out.getSample (0, i - 1)); maxJump = std::max (maxJump, d); sumJump += d; }
            std::printf ("switching test             peak %6.1f dBFS  %s  largest step %.3f (mean %.4f)\n",
                         juce::Decibels::gainToDecibels (out.getMagnitude (0, total)), finite ? "finite" : "NON-FINITE!", maxJump, sumJump / total);
            writeWav (outDir.getChildFile ("switching_test.wav"), out, fs);
        }
    }

    // editor snapshots: RED (default) and ORANGE with the mods and Mr. Scary engaged
    for (int shot = 0; shot < 2; ++shot)
    {
        AmpDualRectProcessor proc;
        if (shot == 1)
        {
            setParam (proc, ids::channel, 0.0f);
            setParam (proc, ids::orMode, 2.0f);
            setParam (proc, ids::tight, 1.0f);
            setParam (proc, ids::defizz, 1.0f);
            setParam (proc, ids::midShift, 1.0f);
            setParam (proc, ids::scaryGain, 6.0f);
            setParam (proc, ids::scaryDeep, 1.0f);
            setParam (proc, ids::rectifier, 1.0f);
            setParam (proc, ids::tubes, 1.0f);
        }
        proc.setPlayConfigDetails (2, 2, fs, block);
        proc.prepareToPlay (fs, block);
        std::unique_ptr<juce::AudioProcessorEditor> ed (proc.createEditor());
        ed->setSize (AmpUi::kW, AmpUi::kH);
        juce::MessageManager::getInstance()->runDispatchLoopUntil (300);
        auto img = ed->createComponentSnapshot (ed->getLocalBounds(), true, 1.0f);
        juce::PNGImageFormat png;
        const auto f = outDir.getChildFile (shot == 0 ? "ui_red.png" : "ui_orange_mods.png");
        f.deleteFile();
        if (auto os = std::unique_ptr<juce::FileOutputStream> (f.createOutputStream())) png.writeImageToStream (img, *os);
        ed.reset();
    }
    return 0;
}
