#pragma once
// SappChoir plugin processor: JUCE wrapper around ChoirEngine.
// Owns parameters (APVTS), host state, MIDI conversion, and instrument
// loading. All sampler/choir DSP lives below in sappchoir_core / SappSounds.
//
// THREADING, and why it is shaped this way (issue #1):
// instrument installs used to run on the JUCE message thread
// (MessageManager::callAsync + a juce::Timer). A plugin embedded in a
// headless host has a MessageManager that nothing ever pumps, so NOTHING
// loaded — not even the built-in default — and the station rendered digital
// silence for months. Every install now happens on a dedicated LOADER
// THREAD this processor owns, which runs whether or not the host has a
// message loop. The 30 Hz timer survives only to fire the editor's
// onInstrumentChanged hook; if it never runs, nothing about the sound
// changes.

#include <array>
#include <atomic>
#include <condition_variable>
#include <deque>
#include <memory>
#include <mutex>
#include <thread>

#include <juce_audio_utils/juce_audio_utils.h>

#include <sapp/sounds/InstrumentLoader.h>

#include "../core/ChoirEngine.h"
#include "../core/SfzLibrary.h"

namespace sappchoir {

/// Append a diagnostic line to the host's JUCE logger, to stderr on Windows
/// (where JUCE's fallback logger is OutputDebugString and invisible to a
/// station box), and to $SAPP_CHOIR_LOG when that names a file.
void logLine(const juce::String& message);

/// Environment override for the diagnostic log destination.
inline constexpr const char* kLogEnvVar = "SAPP_CHOIR_LOG";

class SappChoirProcessor : public juce::AudioProcessor,
                           private juce::AudioProcessorValueTreeState::Listener,
                           private juce::Timer
{
public:
    SappChoirProcessor();
    ~SappChoirProcessor() override;

    // --- AudioProcessor -----------------------------------------------------
    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "SappChoir"; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 20.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return "Default"; }
    void changeProgramName(int, const juce::String&) override {}

    void getStateInformation(juce::MemoryBlock& destData) override;
    void setStateInformation(const void* data, int sizeInBytes) override;

    // --- SappChoir ----------------------------------------------------------
    juce::AudioProcessorValueTreeState& valueTree() { return apvts_; }
    sapp::choir::ChoirEngine& engine() { return engine_; }

    // Async instrument management. These only ENQUEUE — the loader thread
    // performs every install, so they work with no message loop at all.
    void loadSfzInstrument(const juce::File& sfzFile);
    void loadDiagnosticInstrument();

    /// Readiness readout, also exposed as the read-only `libraryReady` host
    /// parameter: false from the instant a load is requested, true once that
    /// instrument is installed. A headless host polls this instead of
    /// guessing a settle window.
    bool libraryReady() const;

    // Host-automatable SFZ selection (sapptune issue #20): the `instrument`
    // AudioParameterChoice enumerates the library scanned at construction
    // (choice 0 = "(keep current)", choice k loads sfzLibrary()[k-1]).
    // The list is FIXED per instance; rescanSfzLibrary() rewrites the index
    // for the NEXT instantiation.
    const std::vector<sapp::sfzlib::Entry>& sfzLibrary() const { return sfzLibrary_; }
    bool rescanSfzLibrary() const;
    juce::String currentInstrumentName() const;
    juce::String currentInstrumentPath() const;
    juce::String loadStatus() const;
    bool isLoading() const { return loading_.load(); }

    // Articulations of the loaded instrument (message/UI thread).
    juce::StringArray articulationNames() const;
    int currentArticulation() const;
    void selectArticulation(int index);

    juce::MidiKeyboardState keyboardState;

    std::function<void()> onInstrumentChanged;  // editor hook (message thread)

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout
        makeLayout(std::vector<sapp::sfzlib::Entry>& outLibrary);
    void pushParamsToEngine();

    // --- the loader thread: the ONE place instruments are installed --------
    struct LoadJob {
        enum class Kind { Diagnostic, Sfz };
        Kind kind = Kind::Diagnostic;
        juce::String path;       // Sfz only
        uint64_t generation = 0; // superseded jobs are dropped
        // The CONSTRUCTION diagnostic must never write the `instrument`
        // parameter: a host that selected an SFZ microseconds earlier would
        // see its choice reset to "(keep current)" (sapporchestra #2).
        bool syncParameter = true;
    };
    void loaderLoop();
    void enqueueLoad(LoadJob job);
    void performLoad(LoadJob job);
    void finishLoad(sapp::sounds::LoadResult result, const juce::String& path,
                    uint64_t generation, bool syncParameter);
    void publishReadiness();
    void logInstalled(const juce::String& what, bool ok);
    void logAudioSourceIfNeeded();

    // --- `instrument` choice parameter plumbing (sapptune issue #20) --------
    // parameterChanged may fire on the audio thread: it only stores an index;
    // the LOADER THREAD applies it (SFZ loads must never run on the audio
    // thread, and must not depend on the message thread either — issue #1).
    void parameterChanged(const juce::String& parameterId, float newValue) override;
    void timerCallback() override;
    void applyInstrumentChoice(int choiceIndex);
    // Reflect a loaded path back into the parameter without re-triggering a
    // load (guarded). "" or an unknown path selects choice 0.
    void syncInstrumentParameter(const juce::String& path);

    // Library snapshot behind the `instrument` choice list. Declared BEFORE
    // apvts_: makeLayout(sfzLibrary_) fills it while building the layout.
    std::vector<sapp::sfzlib::Entry> sfzLibrary_;

    juce::AudioProcessorValueTreeState apvts_;
    sapp::choir::ChoirEngine engine_;

    // Cached raw parameter pointers (audio thread reads).
    std::atomic<float>* pVowel_ = nullptr;
    std::atomic<float>* pDynamics_ = nullptr;
    std::atomic<float>* pExpression_ = nullptr;
    std::atomic<float>* pBreath_ = nullptr;
    std::atomic<float>* pEnsemble_ = nullptr;
    std::atomic<float>* pWidth_ = nullptr;
    std::atomic<float>* pEarly_ = nullptr;
    std::atomic<float>* pTail_ = nullptr;
    std::atomic<float>* pSpaceSize_ = nullptr;
    std::atomic<float>* pSpaceDecay_ = nullptr;
    std::atomic<float>* pSpaceDamping_ = nullptr;
    std::atomic<float>* pLegato_ = nullptr;
    std::atomic<float>* pMaster_ = nullptr;
    std::atomic<float>* pLimiter_ = nullptr;
    std::atomic<float>* pQuality_ = nullptr;
    std::atomic<float>* pArticulation_ = nullptr;
    std::atomic<float>* pClean_ = nullptr;

    // Readiness readout. Outside the APVTS on purpose: host state must never
    // be able to restore a stale "ready". Appended after every APVTS
    // parameter, non-automatable.
    juce::AudioParameterBool* libraryReady_ = nullptr;

    // Knob→CC bridging: moving Dynamics/Expression injects the matching CC.
    // (The Vowel wheel needs no bridge — ChoirEngine injects the Vowel CC
    // whenever the parameter moves.)
    float lastDynParam_ = -1.0f, lastExprParam_ = -1.0f;
    int lastArticulationParam_ = -1;

    // SappLink CC-in (see src/core/SappLinkCCMap.h): mapped controllers land
    // as slew targets; each block moves the APVTS parameter a fraction of the
    // way — the same normalized path host automation uses — so 7-bit CC steps
    // don't zipper. CC 1/11/64 are engine-native and never appear here.
    struct CcSlew {
        juce::RangedAudioParameter* parameter = nullptr;
        float target = 0.0f, current = 0.0f;
        bool active = false;
    };
    std::array<CcSlew, 12> ccSlews_;
    void handleSappLinkCc(int ccNumber, int ccValue);
    void advanceCcSlews(int numSamples);

    std::vector<sapp::sounds::MidiEvent> eventScratch_;

    // `instrument` choice apply state. pendingProgramSelect_ holds a library
    // entry index armed by MIDI bank-select + program change (any channel —
    // SappChoir is single-timbral). -1 = nothing pending.
    std::atomic<int> pendingInstrumentChoice_{-1};
    std::atomic<int> pendingProgramSelect_{-1};
    uint8_t bankMsb_ = 0, bankLsb_ = 0;      // CC0 / CC32 (audio thread only)
    std::atomic<bool> applyingInstrumentChoice_{false};  // reentry guard

    juce::String sfzPath_;                 // "" = diagnostic instrument
    juce::String instrumentName_{"(loading)"};
    juce::String loadStatus_{"starting"};
    std::atomic<bool> loading_{false};
    std::atomic<uint64_t> loadGeneration_{0};
    std::atomic<int> installCount_{0};      // instruments actually installed
    mutable juce::CriticalSection loadLock_;

    // Loader thread + its queue. Every install runs here, message loop or no
    // message loop (issue #1).
    std::deque<LoadJob> loadQueue_;
    std::mutex queueMutex_;
    std::condition_variable queueCv_;
    std::atomic<int> jobsOutstanding_{0};
    std::atomic<bool> loaderStop_{false};
    std::thread loaderThread_;

    // Editor hook, fired from the timer on the message thread. Nothing about
    // loading depends on it.
    std::atomic<bool> instrumentChangedFlag_{false};

    // Identity logging: a voice batch starting from silence names what
    // produced it (sappkeys #1 / sapptune #21).
    std::atomic<bool> audioBatchStarted_{false};
    int lastVoiceCount_ = 0;               // audio thread only
    double lastAudioSourceLogMs_ = 0.0;    // loader thread only

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SappChoirProcessor)
};

} // namespace sappchoir
