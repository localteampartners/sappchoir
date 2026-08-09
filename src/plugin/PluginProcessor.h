#pragma once
// SappChoir plugin processor: JUCE wrapper around ChoirEngine.
// Owns parameters (APVTS), host state, MIDI conversion, and async
// instrument loading (SFZ load + vowel-layer generation happen on a
// background thread). All sampler/choir DSP lives below in
// sappchoir_core / SappSounds.

#include <array>
#include <atomic>
#include <memory>
#include <thread>

#include <juce_audio_utils/juce_audio_utils.h>

#include <sapp/sounds/InstrumentLoader.h>

#include "../core/ChoirEngine.h"
#include "../core/SfzLibrary.h"

namespace sappchoir {

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

    // Async instrument management (message thread).
    void loadSfzInstrument(const juce::File& sfzFile);
    void loadDiagnosticInstrument();

    // Host-automatable SFZ selection (sapptune issue #20): the `instrument`
    // AudioParameterChoice enumerates the library scanned at construction
    // (choice 0 = "(keep current)", choice k loads sfzLibrary()[k-1]).
    // The list is FIXED per instance; rescanSfzLibrary() rewrites the index
    // for the NEXT instantiation.
    const std::vector<sapp::sfzlib::Entry>& sfzLibrary() const { return sfzLibrary_; }
    bool rescanSfzLibrary() const;
    juce::String currentInstrumentName() const;
    juce::String currentInstrumentPath() const { return sfzPath_; }
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
    void finishLoad(sapp::sounds::LoadResult result, const juce::String& path,
                    uint64_t generation);

    // --- `instrument` choice parameter plumbing (sapptune issue #20) --------
    // parameterChanged may fire on the audio thread: it only stores an index;
    // the 30 Hz timer applies it on the message thread (SFZ loads must never
    // run on the audio thread).
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
    std::array<CcSlew, 11> ccSlews_;
    void handleSappLinkCc(int ccNumber, int ccValue);
    void advanceCcSlews(int numSamples);

    std::vector<sapp::sounds::MidiEvent> eventScratch_;

    // `instrument` choice apply state. pendingProgramSelect_ holds a library
    // entry index armed by MIDI bank-select + program change (any channel —
    // SappChoir is single-timbral). -1 = nothing pending.
    std::atomic<int> pendingInstrumentChoice_{-1};
    std::atomic<int> pendingProgramSelect_{-1};
    uint8_t bankMsb_ = 0, bankLsb_ = 0;      // CC0 / CC32 (audio thread only)
    bool applyingInstrumentChoice_ = false;  // message-thread reentry guard

    juce::String sfzPath_;                 // "" = diagnostic instrument
    juce::String instrumentName_{"(loading)"};
    juce::String loadStatus_{"starting"};
    std::atomic<bool> loading_{false};
    std::atomic<uint64_t> loadGeneration_{0};
    juce::CriticalSection loadLock_;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SappChoirProcessor)
};

} // namespace sappchoir
