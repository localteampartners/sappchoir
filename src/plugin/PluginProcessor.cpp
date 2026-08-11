#include "PluginProcessor.h"

#include <sapp/sounds/DiagnosticInstrument.h>

#include "../core/SappLinkCCMap.h"
#include "../core/VowelLayers.h"
#include "PluginEditor.h"
#include "SoundsPanel.h"

namespace sappchoir {

using namespace sapp::choir;
using sapp::sounds::MidiEvent;

namespace {
constexpr int kMaxArticulations = 16;
}

void logLine(const juce::String& message)
{
    juce::Logger::writeToLog(message);
#if JUCE_WINDOWS
    // JUCE's fallback logger is OutputDebugString on Windows — invisible to a
    // station box redirecting the host's output. stderr is what it greps.
    std::fputs((message + "\n").toRawUTF8(), stderr);
    std::fflush(stderr);
#endif
    if (const char* path = std::getenv(kLogEnvVar))
        if (path[0] != 0)
            juce::File(juce::String::fromUTF8(path)).appendText(message + "\n");
}

juce::AudioProcessorValueTreeState::ParameterLayout
SappChoirProcessor::makeLayout(std::vector<sapp::sfzlib::Entry>& outLibrary)
{
    using P = juce::AudioParameterFloat;
    using Range = juce::NormalisableRange<float>;
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    // Parameter IDs are compatibility contracts — never reuse or renumber.
    layout.add(std::make_unique<P>(juce::ParameterID{"vowel", 1}, "Vowel",
                                   Range{0.0f, 1.0f, 0.001f}, 0.35f));
    layout.add(std::make_unique<P>(juce::ParameterID{"dynamics", 1}, "Dynamics",
                                   Range{0.0f, 1.0f, 0.001f}, 0.65f));
    layout.add(std::make_unique<P>(juce::ParameterID{"expression", 1}, "Expression",
                                   Range{0.0f, 1.0f, 0.001f}, 1.0f));
    layout.add(std::make_unique<P>(juce::ParameterID{"breath", 1}, "Breath",
                                   Range{0.0f, 1.0f, 0.001f}, 0.25f));
    layout.add(std::make_unique<P>(juce::ParameterID{"ensemble", 1}, "Ensemble",
                                   Range{0.0f, 1.0f, 0.001f}, 0.5f));
    layout.add(std::make_unique<P>(juce::ParameterID{"width", 1}, "Width",
                                   Range{0.0f, 2.0f, 0.001f}, 1.2f));
    layout.add(std::make_unique<P>(juce::ParameterID{"earlyLevel", 1}, "Early Stone",
                                   Range{0.0f, 1.0f, 0.001f}, 0.28f));
    layout.add(std::make_unique<P>(juce::ParameterID{"tailLevel", 1}, "Cathedral Level",
                                   Range{0.0f, 1.0f, 0.001f}, 0.45f));
    layout.add(std::make_unique<P>(juce::ParameterID{"spaceSize", 1}, "Cathedral Size",
                                   Range{0.2f, 1.5f, 0.001f}, 1.15f));
    layout.add(std::make_unique<P>(juce::ParameterID{"spaceDecay", 1}, "Cathedral Decay",
                                   Range{1.0f, 20.0f, 0.01f, 0.5f}, 6.5f));
    layout.add(std::make_unique<P>(juce::ParameterID{"spaceDamping", 1}, "Stone Damping",
                                   Range{0.0f, 1.0f, 0.001f}, 0.55f));
    layout.add(std::make_unique<juce::AudioParameterBool>(
        juce::ParameterID{"legato", 1}, "Legato", true));
    layout.add(std::make_unique<P>(juce::ParameterID{"masterGain", 1}, "Master Gain",
                                   Range{-24.0f, 12.0f, 0.1f}, 0.0f));
    layout.add(std::make_unique<juce::AudioParameterBool>(
        juce::ParameterID{"limiter", 1}, "Safety Limiter", true));
    layout.add(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID{"quality", 1}, "Quality",
        juce::StringArray{"Draft", "Normal"}, 1));
    layout.add(std::make_unique<juce::AudioParameterInt>(
        juce::ParameterID{"articulation", 1}, "Articulation", 0, kMaxArticulations - 1, 0));

    // APPENDED LAST (sapptune issue #20) so every pre-existing automation
    // index holds. Choice 0 keeps whatever is loaded; choice k loads library
    // entry k-1 (case-insensitive sort order — see SfzLibrary.h). The list is
    // fixed for this instance; a rescan shows up on the next instantiation.
    outLibrary = sapp::sfzlib::loadOrScan(sapp::sfzlib::resolveRoot(
        SoundsPanel::samplesRoot().getFullPathName().toStdString()));
    juce::StringArray instrumentChoices;
    instrumentChoices.add("(keep current)");
    for (const auto& entry : outLibrary)
        instrumentChoices.add(juce::String::fromUTF8(entry.label.c_str()));
    if (outLibrary.empty())
        instrumentChoices.add("(no SFZ library found)");  // avoid a 1-step param
    layout.add(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID{"instrument", 1}, "Instrument", instrumentChoices, 0));

    // Suite-wide `clean` convention (CC 3): 0 = every modeled imperfection as
    // designed, 1 = none. Appended AFTER `instrument` so no pre-existing
    // automation index moves. Default 0 keeps the historical sound — and, as
    // issue #1's postmortem demands, no parameter of this plugin may default
    // to a value that silences it.
    layout.add(std::make_unique<P>(juce::ParameterID{"clean", 1}, "Clean",
                                   Range{0.0f, 1.0f, 0.001f}, 0.0f));
    return layout;
}

SappChoirProcessor::SappChoirProcessor()
    : juce::AudioProcessor(BusesProperties().withOutput(
          "Output", juce::AudioChannelSet::stereo(), true)),
      apvts_(*this, nullptr, "SappChoir", makeLayout(sfzLibrary_))
{
    auto raw = [this](const char* id) { return apvts_.getRawParameterValue(id); };
    pVowel_ = raw("vowel");
    pDynamics_ = raw("dynamics");
    pExpression_ = raw("expression");
    pBreath_ = raw("breath");
    pEnsemble_ = raw("ensemble");
    pWidth_ = raw("width");
    pEarly_ = raw("earlyLevel");
    pTail_ = raw("tailLevel");
    pSpaceSize_ = raw("spaceSize");
    pSpaceDecay_ = raw("spaceDecay");
    pSpaceDamping_ = raw("spaceDamping");
    pLegato_ = raw("legato");
    pMaster_ = raw("masterGain");
    pLimiter_ = raw("limiter");
    pQuality_ = raw("quality");
    pArticulation_ = raw("articulation");
    pClean_ = raw("clean");

    eventScratch_.reserve(512);

    const auto& table = sapplink::mappings();
    static_assert(std::tuple_size<decltype(ccSlews_)>::value == size_t(sapplink::kNumMappings),
                  "ccSlews_ must match the SappLink mapping table size");
    for (size_t i = 0; i < table.size(); ++i)
        ccSlews_[i].parameter = apvts_.getParameter(table[i].paramId);

    // Readiness readout (issue #1): a headless host polls this instead of
    // guessing a settle window. Outside the APVTS on purpose — see the
    // declaration. Appended last, after every APVTS parameter.
    libraryReady_ = new juce::AudioParameterBool(
        juce::ParameterID{"libraryReady", 1}, "Library Ready", false,
        juce::AudioParameterBoolAttributes().withAutomatable(false));
    addParameter(libraryReady_);

    // Host-automatable SFZ selection: the callback may fire on the audio
    // thread, so it only stores an index; the LOADER THREAD applies it.
    apvts_.addParameterListener("instrument", this);

    // The loader thread owns every instrument install. Started before the
    // construction diagnostic is queued so nothing waits on the host.
    loaderThread_ = std::thread([this] { loaderLoop(); });

    // The 30 Hz timer is an editor convenience ONLY (it fires the
    // onInstrumentChanged hook on the message thread). Nothing about loading
    // depends on it — see the threading note in the header.
    startTimerHz(30);

    // Which build did the host just load, and what is it enumerating? One
    // line at construction turns "the wrong sound came out" from guesswork
    // into a log grep.
    logLine("SappChoir-build: version=" SAPPCHOIR_VERSION
            " root=\"" + SoundsPanel::samplesRoot().getFullPathName()
            + "\" instruments=" + juce::String(int(sfzLibrary_.size())));

    loadDiagnosticInstrument();
}

// --------------------------------------------- `instrument` choice param --

void SappChoirProcessor::parameterChanged(const juce::String& parameterId,
                                          float newValue)
{
    if (parameterId != juce::StringRef("instrument")
        || applyingInstrumentChoice_.load(std::memory_order_acquire))
        return;
    pendingInstrumentChoice_.store(int(newValue + 0.5f));
    // Not ready from the instant the host asks for a different instrument —
    // otherwise a host that writes the parameter and immediately polls would
    // read the PREVIOUS instrument's "ready" and render too early.
    if (libraryReady_ != nullptr && libraryReady_->get())
        *libraryReady_ = false;
    queueCv_.notify_all();
}

void SappChoirProcessor::timerCallback()
{
    // Editor convenience ONLY. Loading does not run here (see LoadJob).
    if (instrumentChangedFlag_.exchange(false) && onInstrumentChanged)
        onInstrumentChanged();
}

// The loader thread: the one place instruments are installed. Runs whether or
// not the host has a message loop, which is the entire point (issue #1).
void SappChoirProcessor::loaderLoop()
{
    while (!loaderStop_.load(std::memory_order_acquire)) {
        // MIDI program select first, then an explicit parameter move: when
        // both land in the same tick the parameter (the deliberate host move)
        // wins because it is enqueued last.
        const int programSelect = pendingProgramSelect_.exchange(-1);
        if (programSelect >= 0)
            applyInstrumentChoice(programSelect + 1);  // entry -> choice
        const int choice = pendingInstrumentChoice_.exchange(-1);
        if (choice >= 0)
            applyInstrumentChoice(choice);

        LoadJob job;
        bool haveJob = false;
        {
            std::lock_guard<std::mutex> lock(queueMutex_);
            if (!loadQueue_.empty()) {
                job = std::move(loadQueue_.front());
                loadQueue_.pop_front();
                haveJob = true;
            }
        }
        if (haveJob) {
            performLoad(std::move(job));
            jobsOutstanding_.fetch_sub(1);
            loading_.store(jobsOutstanding_.load() > 0);
            publishReadiness();
            continue;
        }

        loading_.store(false);
        publishReadiness();
        logAudioSourceIfNeeded();
        std::unique_lock<std::mutex> lock(queueMutex_);
        queueCv_.wait_for(lock, std::chrono::milliseconds(5));
    }
}

void SappChoirProcessor::enqueueLoad(LoadJob job)
{
    jobsOutstanding_.fetch_add(1);
    loading_.store(true);
    if (libraryReady_ != nullptr && libraryReady_->get())
        *libraryReady_ = false;
    {
        std::lock_guard<std::mutex> lock(queueMutex_);
        loadQueue_.push_back(std::move(job));
    }
    queueCv_.notify_all();
}

void SappChoirProcessor::performLoad(LoadJob job)
{
    if (job.generation != loadGeneration_.load())
        return;  // a newer load was queued before this one started

    if (job.kind == LoadJob::Kind::Diagnostic) {
        // Built-in: the diagnostic instrument refiltered into vowel layers.
        sapp::sounds::LoadResult result;
        result.instrument =
            makeVowelInstrument(sapp::sounds::makeDiagnosticInstrument(), kVowelCc);
        result.ok = result.instrument != nullptr;
        finishLoad(std::move(result), {}, job.generation, job.syncParameter);
        return;
    }

    sapp::sounds::InstrumentLoader loader;
    auto result = loader.loadSfz(job.path.toStdString());
    // Vowel morph: generated formant layers unless the library ships its own
    // crossfades on the Vowel CC.
    if (result.ok && result.instrument != nullptr)
        result.instrument = makeVowelInstrument(result.instrument, kVowelCc);
    finishLoad(std::move(result), job.path, job.generation, job.syncParameter);
}

void SappChoirProcessor::publishReadiness()
{
    if (libraryReady_ == nullptr) return;
    const bool ready = jobsOutstanding_.load() == 0
                       && pendingInstrumentChoice_.load() < 0
                       && pendingProgramSelect_.load() < 0
                       && installCount_.load() > 0;
    if (libraryReady_->get() != ready)
        *libraryReady_ = ready;
}

bool SappChoirProcessor::libraryReady() const
{
    return libraryReady_ != nullptr && libraryReady_->get();
}

void SappChoirProcessor::logInstalled(const juce::String& what, bool ok)
{
    logLine(juce::String("SappChoir-instrument: ") + (ok ? "loaded" : "FAILED")
            + " source=\"" + what + "\" build=" SAPPCHOIR_VERSION);
}

void SappChoirProcessor::logAudioSourceIfNeeded()
{
    // Voices started from silence: name the instrument that produced them.
    // This is the line that makes "the plugin is playing its default sound"
    // — or nothing at all — visible in the wild instead of only audible.
    if (!audioBatchStarted_.exchange(false)) return;
    const double nowMs = juce::Time::getMillisecondCounterHiRes();
    if (nowMs - lastAudioSourceLogMs_ < 3000.0) return;
    lastAudioSourceLogMs_ = nowMs;

    juce::String source, name;
    {
        const juce::ScopedLock sl(loadLock_);
        source = sfzPath_;
        name = instrumentName_;
    }
    if (source.isEmpty())
        // ASCII only: these lines end up in host logs with every encoding.
        source = "DIAGNOSTIC(no SFZ loaded - the built-in choir is sounding)";
    logLine("SappChoir-audio-source: instrument=\"" + source
            + "\" name=\"" + name + "\" build=" SAPPCHOIR_VERSION
            + " ready=" + juce::String(libraryReady() ? 1 : 0));
}

void SappChoirProcessor::applyInstrumentChoice(int choiceIndex)
{
    if (choiceIndex <= 0 || choiceIndex > int(sfzLibrary_.size()))
        return;  // "(keep current)" / placeholder / out of range
    const auto& entry = sfzLibrary_[size_t(choiceIndex - 1)];
    const juce::File file(juce::String::fromUTF8(entry.path.c_str()));
    if (file.getFullPathName() == currentInstrumentPath())
        return;  // already loaded
    if (!file.existsAsFile()) {
        // Graceful miss: the library changed since the index was written.
        // Say so — a silent no-op here is exactly how this class of bug hides.
        {
            const juce::ScopedLock sl(loadLock_);
            loadStatus_ = "Missing: " + file.getFullPathName();
        }
        logLine("SappChoir-instrument: MISSING choice=" + juce::String(choiceIndex)
                + " path=\"" + file.getFullPathName() + "\" build=" SAPPCHOIR_VERSION);
        instrumentChangedFlag_.store(true);
        return;
    }
    loadSfzInstrument(file);
}

void SappChoirProcessor::syncInstrumentParameter(const juce::String& path)
{
    auto* parameter = apvts_.getParameter("instrument");
    if (parameter == nullptr) return;
    int choice = 0;  // "" / unknown path -> "(keep current)"
    const auto pathStd = path.toStdString();
    for (size_t i = 0; i < sfzLibrary_.size(); ++i)
        if (sfzLibrary_[i].path == pathStd) { choice = int(i) + 1; break; }
    applyingInstrumentChoice_.store(true, std::memory_order_release);
    parameter->setValueNotifyingHost(parameter->convertTo0to1(float(choice)));
    applyingInstrumentChoice_.store(false, std::memory_order_release);
}

bool SappChoirProcessor::rescanSfzLibrary() const
{
    const auto root = sapp::sfzlib::resolveRoot(
        SoundsPanel::samplesRoot().getFullPathName().toStdString());
    return sapp::sfzlib::writeIndex(root, sapp::sfzlib::scan(root));
}

void SappChoirProcessor::handleSappLinkCc(int ccNumber, int ccValue)
{
    const auto* mapping = sapplink::findMapping(ccNumber);
    if (mapping == nullptr)
        return;
    const auto index = size_t(mapping - sapplink::mappings().data());
    auto& slew = ccSlews_[index];
    if (slew.parameter == nullptr)
        return;
    slew.target = slew.parameter->convertTo0to1(sapplink::ccToEngineering(*mapping, ccValue));
    if (!slew.active)
        slew.current = slew.parameter->getValue();
    slew.active = true;
}

void SappChoirProcessor::advanceCcSlews(int numSamples)
{
    // ~15 ms approach per step, applied through the same normalized-value
    // path host automation uses — never straight into the DSP.
    const float coefficient =
        1.0f - std::exp(-float(numSamples) / (0.015f * float(getSampleRate() > 0 ? getSampleRate() : 48000.0)));
    for (auto& slew : ccSlews_) {
        if (!slew.active || slew.parameter == nullptr)
            continue;
        slew.current += (slew.target - slew.current) * coefficient;
        if (std::abs(slew.target - slew.current) < 1.0e-4f) {
            slew.current = slew.target;
            slew.active = false;
        }
        slew.parameter->setValueNotifyingHost(slew.current);
    }
}

SappChoirProcessor::~SappChoirProcessor()
{
    stopTimer();
    apvts_.removeParameterListener("instrument", this);
    // Join the loader thread before anything it touches goes away. The old
    // detached-thread + callAsync design left closures capturing `this` alive
    // past destruction — a crash waiting for the next pump.
    loaderStop_.store(true, std::memory_order_release);
    queueCv_.notify_all();
    if (loaderThread_.joinable())
        loaderThread_.join();
}

void SappChoirProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    engine_.prepare(sampleRate, juce::jmax(64, samplesPerBlock));
    pushParamsToEngine();
}

void SappChoirProcessor::releaseResources() {}

bool SappChoirProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    return layouts.getMainOutputChannelSet() == juce::AudioChannelSet::stereo();
}

void SappChoirProcessor::pushParamsToEngine()
{
    ChoirParams p;
    p.vowel = pVowel_->load();
    p.dynamics = pDynamics_->load();
    p.expression = pExpression_->load();
    p.breath = pBreath_->load();
    p.ensemble = pEnsemble_->load();
    p.width = pWidth_->load();
    p.earlyLevel = pEarly_->load();
    p.tailLevel = pTail_->load();
    p.spaceSize = pSpaceSize_->load();
    p.spaceDecay = pSpaceDecay_->load();
    p.spaceDamping = pSpaceDamping_->load();
    p.legato = pLegato_->load();
    // The engine owns the `clean` contract (ChoirEngine::cleanScale) — hand
    // it the raw value.
    p.clean = pClean_->load();
    p.masterGainDb = pMaster_->load();
    p.limiter = pLimiter_->load() > 0.5f;
    p.quality = int(pQuality_->load());
    engine_.setParams(p);
}

void SappChoirProcessor::processBlock(juce::AudioBuffer<float>& buffer,
                                      juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    advanceCcSlews(buffer.getNumSamples());
    pushParamsToEngine();

    keyboardState.processNextMidiBuffer(midi, 0, buffer.getNumSamples(), true);

    eventScratch_.clear();

    // Knob→CC bridging: moving Dynamics/Expression behaves like riding CC1/11.
    const float dynParam = pDynamics_->load();
    if (lastDynParam_ >= 0.0f && std::abs(dynParam - lastDynParam_) > 0.004f) {
        MidiEvent e;
        e.type = MidiEvent::Type::Controller;
        e.frame = 0;
        e.note = 1;
        e.value = uint8_t(juce::jlimit(0, 127, int(dynParam * 127.0f + 0.5f)));
        eventScratch_.push_back(e);
    }
    lastDynParam_ = dynParam;
    const float exprParam = pExpression_->load();
    if (lastExprParam_ >= 0.0f && std::abs(exprParam - lastExprParam_) > 0.004f) {
        MidiEvent e;
        e.type = MidiEvent::Type::Controller;
        e.frame = 0;
        e.note = 11;
        e.value = uint8_t(juce::jlimit(0, 127, int(exprParam * 127.0f + 0.5f)));
        eventScratch_.push_back(e);
    }
    lastExprParam_ = exprParam;

    // Automatable articulation parameter.
    const int artParam = int(pArticulation_->load());
    if (artParam != lastArticulationParam_) {
        lastArticulationParam_ = artParam;
        engine_.selectArticulation(artParam);
    }

    for (const auto metadata : midi) {
        const auto msg = metadata.getMessage();
        MidiEvent e;
        e.frame = uint32_t(juce::jmax(0, metadata.samplePosition));
        if (msg.isNoteOn()) {
            e.type = MidiEvent::Type::NoteOn;
            e.note = uint8_t(msg.getNoteNumber());
            e.value = uint8_t(msg.getVelocity());
        } else if (msg.isNoteOff()) {
            e.type = MidiEvent::Type::NoteOff;
            e.note = uint8_t(msg.getNoteNumber());
        } else if (msg.isController()) {
            e.type = MidiEvent::Type::Controller;
            e.note = uint8_t(msg.getControllerNumber());
            e.value = uint8_t(msg.getControllerValue());
            // Bank select arms the next program change:
            // entry = ((CC0 << 7) | CC32) * 128 + program (sapptune #20).
            if (msg.getControllerNumber() == 0)
                bankMsb_ = uint8_t(msg.getControllerValue());
            else if (msg.getControllerNumber() == 32)
                bankLsb_ = uint8_t(msg.getControllerValue());
            // SappLink CC-in (any channel): mapped CCs also steer parameters.
            // The event still reaches the engine below (vowel crossfades, SFZ
            // CC conditions, native CC1/CC11/CC64 behavior stay untouched).
            handleSappLinkCc(msg.getControllerNumber(), msg.getControllerValue());
        } else if (msg.isProgramChange()) {
            // Program change selects an SFZ library entry (single-timbral:
            // any channel); the load happens on the message thread (timer).
            const int bank = (int(bankMsb_) << 7) | int(bankLsb_);
            const int entry = bank * 128 + msg.getProgramChangeNumber();
            if (entry < int(sfzLibrary_.size())) {
                pendingProgramSelect_.store(entry);
                // Not ready from the instant the host asks (sappkeys #4).
                // publishReadiness() already accounts for the queued select,
                // but it only runs on the loader thread — a host that sends
                // the program change and polls in the same breath would read
                // the OUTGOING library's "ready" in the gap. Clearing here,
                // on the calling thread, closes it. Writing a parameter from
                // processBlock is this processor's normal path already
                // (advanceCcSlews does it every block).
                if (libraryReady_ != nullptr && libraryReady_->get())
                    *libraryReady_ = false;
            }
            continue;  // consumed; the engine has no program-change concept
        } else if (msg.isPitchWheel()) {
            e.type = MidiEvent::Type::PitchBend;
            e.bend14 = int16_t(msg.getPitchWheelValue() - 8192);
        } else if (msg.isAllNotesOff()) {
            e.type = MidiEvent::Type::AllNotesOff;
        } else if (msg.isAllSoundOff()) {
            e.type = MidiEvent::Type::AllSoundOff;
        } else {
            continue;
        }
        eventScratch_.push_back(e);
    }
    std::stable_sort(eventScratch_.begin(), eventScratch_.end(),
                     [](const MidiEvent& a, const MidiEvent& b) { return a.frame < b.frame; });

    buffer.clear();
    if (buffer.getNumChannels() >= 2) {
        engine_.process(eventScratch_.data(), int(eventScratch_.size()),
                        buffer.getWritePointer(0), buffer.getWritePointer(1),
                        buffer.getNumSamples());
    }

    // Silence → voices: flag it so the loader thread names what just sounded.
    const int voices = engine_.sampler().activeVoiceCount();
    if (voices > 0 && lastVoiceCount_ == 0)
        audioBatchStarted_.store(true, std::memory_order_relaxed);
    lastVoiceCount_ = voices;

    midi.clear();
}

// ------------------------------------------------------------- instruments --

void SappChoirProcessor::loadDiagnosticInstrument()
{
    LoadJob job;
    job.kind = LoadJob::Kind::Diagnostic;
    job.generation = ++loadGeneration_;
    // The construction diagnostic is generation 1 and must stay quiet about
    // the `instrument` parameter; every later diagnostic is a deliberate
    // "go back to the built-in choir" and does move it.
    job.syncParameter = job.generation > 1;
    {
        const juce::ScopedLock sl(loadLock_);
        loadStatus_ = "Generating built-in choir...";
    }
    enqueueLoad(std::move(job));
}

void SappChoirProcessor::loadSfzInstrument(const juce::File& sfzFile)
{
    LoadJob job;
    job.kind = LoadJob::Kind::Sfz;
    job.path = sfzFile.getFullPathName();
    job.generation = ++loadGeneration_;
    {
        const juce::ScopedLock sl(loadLock_);
        loadStatus_ = "Loading " + sfzFile.getFileName() + "...";
    }
    enqueueLoad(std::move(job));
}

// Runs on the LOADER THREAD (never the message thread — issue #1).
void SappChoirProcessor::finishLoad(sapp::sounds::LoadResult result,
                                    const juce::String& path, uint64_t generation,
                                    bool syncParameter)
{
    if (generation != loadGeneration_.load()) return;  // superseded

    bool installed = false;
    {
        const juce::ScopedLock sl(loadLock_);
        if (!result.ok || result.instrument == nullptr) {
            loadStatus_ = "Load failed";
            for (const auto& d : result.diagnostics)
                if (d.severity == sapp::sounds::Severity::Error) {
                    loadStatus_ = "Load failed: " + juce::String(d.message);
                    break;
                }
        } else {
            engine_.setInstrument(result.instrument);
            engine_.collectRetired();
            sfzPath_ = path;
            instrumentName_ = juce::String(result.instrument->definition.name);
            loadStatus_ = result.missingSamples.empty()
                              ? "Ready"
                              : juce::String(result.missingSamples.size()) + " samples missing";
            lastArticulationParam_ = -1;  // re-apply articulation on next block
            installed = true;
        }
    }
    if (installed)
        installCount_.fetch_add(1);
    logInstalled(path.isEmpty() ? juce::String("DIAGNOSTIC(built-in choir)") : path,
                 installed);

    // Reflect the loaded instrument in the `instrument` parameter (guarded —
    // this must not schedule another load).
    if (syncParameter)
        syncInstrumentParameter(installed ? path : currentInstrumentPath());
    instrumentChangedFlag_.store(true);
}

juce::String SappChoirProcessor::currentInstrumentName() const
{
    const juce::ScopedLock sl(loadLock_);
    return instrumentName_;
}

juce::String SappChoirProcessor::currentInstrumentPath() const
{
    const juce::ScopedLock sl(loadLock_);
    return sfzPath_;
}

juce::String SappChoirProcessor::loadStatus() const
{
    const juce::ScopedLock sl(loadLock_);
    return loadStatus_;
}

juce::StringArray SappChoirProcessor::articulationNames() const
{
    juce::StringArray names;
    if (auto inst = engine_.currentInstrument())
        for (const auto& a : inst->definition.articulations)
            names.add(juce::String(a.name));
    return names;
}

int SappChoirProcessor::currentArticulation() const
{
    return int(pArticulation_->load());
}

void SappChoirProcessor::selectArticulation(int index)
{
    if (auto* param = apvts_.getParameter("articulation")) {
        param->beginChangeGesture();
        param->setValueNotifyingHost(
            param->convertTo0to1(float(juce::jlimit(0, kMaxArticulations - 1, index))));
        param->endChangeGesture();
    }
}

// -------------------------------------------------------------------- state --

void SappChoirProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    auto state = apvts_.copyState();
    state.setProperty("sfzPath", sfzPath_, nullptr);
    state.setProperty("stateVersion", 1, nullptr);
    if (auto xml = state.createXml())
        copyXmlToBinary(*xml, destData);
}

void SappChoirProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary(data, sizeInBytes)) {
        auto state = juce::ValueTree::fromXml(*xml);
        if (!state.isValid()) return;
        // The chosen SFZ persists BY PATH (indices shift when the library
        // changes): suppress the `instrument` choice value coming back with
        // the tree so it cannot race the path-based load below. finishLoad
        // re-syncs the parameter once the real instrument is in.
        applyingInstrumentChoice_.store(true, std::memory_order_release);
        apvts_.replaceState(state);
        applyingInstrumentChoice_.store(false, std::memory_order_release);
        pendingInstrumentChoice_.store(-1);
        const juce::String path = state.getProperty("sfzPath", "").toString();
        if (path.isNotEmpty() && juce::File(path).existsAsFile())
            loadSfzInstrument(juce::File(path));
        else
            loadDiagnosticInstrument();
    }
}

juce::AudioProcessorEditor* SappChoirProcessor::createEditor()
{
    return new SappChoirEditor(*this);
}

} // namespace sappchoir

// JUCE plugin entry point.
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new sappchoir::SappChoirProcessor();
}
