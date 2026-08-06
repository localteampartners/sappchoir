#include "PluginProcessor.h"

#include <sapp/sounds/DiagnosticInstrument.h>

#include "../core/SappLinkCCMap.h"
#include "../core/VowelLayers.h"
#include "PluginEditor.h"

namespace sappchoir {

using namespace sapp::choir;
using sapp::sounds::MidiEvent;

namespace {
constexpr int kMaxArticulations = 16;
}

juce::AudioProcessorValueTreeState::ParameterLayout SappChoirProcessor::makeLayout()
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
    return layout;
}

SappChoirProcessor::SappChoirProcessor()
    : juce::AudioProcessor(BusesProperties().withOutput(
          "Output", juce::AudioChannelSet::stereo(), true)),
      apvts_(*this, nullptr, "SappChoir", makeLayout())
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

    eventScratch_.reserve(512);

    const auto& table = sapplink::mappings();
    static_assert(std::tuple_size<decltype(ccSlews_)>::value == size_t(sapplink::kNumMappings),
                  "ccSlews_ must match the SappLink mapping table size");
    for (size_t i = 0; i < table.size(); ++i)
        ccSlews_[i].parameter = apvts_.getParameter(table[i].paramId);

    loadDiagnosticInstrument();
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

SappChoirProcessor::~SappChoirProcessor() = default;

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
            // SappLink CC-in (any channel): mapped CCs also steer parameters.
            // The event still reaches the engine below (vowel crossfades, SFZ
            // CC conditions, native CC1/CC11/CC64 behavior stay untouched).
            handleSappLinkCc(msg.getControllerNumber(), msg.getControllerValue());
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
    midi.clear();
}

// ------------------------------------------------------------- instruments --

void SappChoirProcessor::loadDiagnosticInstrument()
{
    const uint64_t generation = ++loadGeneration_;
    loading_ = true;
    {
        const juce::ScopedLock sl(loadLock_);
        loadStatus_ = "Generating built-in choir...";
    }
    std::thread([this, generation] {
        // Built-in: the diagnostic instrument refiltered into vowel layers.
        auto inst = makeVowelInstrument(sapp::sounds::makeDiagnosticInstrument(), kVowelCc);
        sapp::sounds::LoadResult result;
        result.instrument = inst;
        result.ok = true;
        juce::MessageManager::callAsync([this, result = std::move(result), generation]() mutable {
            finishLoad(std::move(result), {}, generation);
        });
    }).detach();
}

void SappChoirProcessor::loadSfzInstrument(const juce::File& sfzFile)
{
    const uint64_t generation = ++loadGeneration_;
    loading_ = true;
    {
        const juce::ScopedLock sl(loadLock_);
        loadStatus_ = "Loading " + sfzFile.getFileName() + "...";
    }
    const juce::String path = sfzFile.getFullPathName();
    std::thread([this, path, generation] {
        sapp::sounds::InstrumentLoader loader;
        auto result = loader.loadSfz(path.toStdString());
        // Vowel morph: generated formant layers unless the library ships its
        // own crossfades on the Vowel CC. Runs here, off the audio thread.
        if (result.ok && result.instrument != nullptr)
            result.instrument = makeVowelInstrument(result.instrument, kVowelCc);
        juce::MessageManager::callAsync([this, result = std::move(result), path, generation]() mutable {
            finishLoad(std::move(result), path, generation);
        });
    }).detach();
}

void SappChoirProcessor::finishLoad(sapp::sounds::LoadResult result,
                                    const juce::String& path, uint64_t generation)
{
    if (generation != loadGeneration_.load()) return;  // superseded
    loading_ = false;

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
    }
    if (onInstrumentChanged) onInstrumentChanged();
}

juce::String SappChoirProcessor::currentInstrumentName() const
{
    const juce::ScopedLock sl(loadLock_);
    return instrumentName_;
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
        apvts_.replaceState(state);
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
