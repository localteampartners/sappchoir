#pragma once
// SappChoir editor — "candlelit cathedral".
// Near-black stone, candle-amber accents, arched panels, and a large
// vowel morph wheel in the centre. Vector-drawn controls.

#include <juce_audio_utils/juce_audio_utils.h>

#include "PluginProcessor.h"

namespace sappchoir {

// ------------------------------------------------------------------ palette --
namespace palette {
const juce::Colour background{0xff120e14};   // night-stone violet-black
const juce::Colour panel{0xff1c1620};
const juce::Colour panelEdge{0xff2c2331};
const juce::Colour candle{0xffd99a45};       // candle amber
const juce::Colour candleBright{0xffffca7d};
const juce::Colour parchment{0xffece1cd};
const juce::Colour dim{0xff8a7d74};
const juce::Colour wine{0xff5d2f40};
const juce::Colour shadow{0xff08060a};
} // namespace palette

// ------------------------------------------------------------ look and feel --
class CathedralLookAndFeel : public juce::LookAndFeel_V4
{
public:
    CathedralLookAndFeel();
    void drawRotarySlider(juce::Graphics&, int x, int y, int w, int h,
                          float sliderPos, float rotaryStartAngle,
                          float rotaryEndAngle, juce::Slider&) override;
    void drawComboBox(juce::Graphics&, int width, int height, bool isButtonDown,
                      int buttonX, int buttonY, int buttonW, int buttonH,
                      juce::ComboBox&) override;
    void drawButtonBackground(juce::Graphics&, juce::Button&,
                              const juce::Colour& backgroundColour,
                              bool highlighted, bool down) override;
    juce::Font getComboBoxFont(juce::ComboBox&) override;
    juce::Font getPopupMenuFont() override;
};

// ------------------------------------------------------------- labeled knob --
class Knob : public juce::Component
{
public:
    Knob(juce::AudioProcessorValueTreeState& state, const juce::String& paramId,
         const juce::String& title, bool big = false);
    void resized() override;
    juce::Slider slider;

private:
    juce::Label label_;
    bool big_;
};

// -------------------------------------------------------------- vowel wheel --
// The signature control: a large wheel that morphs oo → oh → ah → eh.
// Drag rotates; the glow tracks the vowel colour. Bound to the "vowel"
// parameter (which the engine turns into the Vowel CC for the crossfades).
class VowelWheel : public juce::Component, private juce::Timer
{
public:
    explicit VowelWheel(juce::AudioProcessorValueTreeState& state);
    ~VowelWheel() override;
    void paint(juce::Graphics&) override;
    void mouseDown(const juce::MouseEvent&) override;
    void mouseDrag(const juce::MouseEvent&) override;
    void mouseUp(const juce::MouseEvent&) override;
    void mouseDoubleClick(const juce::MouseEvent&) override;

private:
    void timerCallback() override;
    void applyDrag(juce::Point<float> position);
    juce::RangedAudioParameter* param_;
    float dragStartValue_ = 0.0f;
    juce::Point<float> dragStart_;
    float glow_ = 0.0f;
};

// ------------------------------------------------------- keyswitch keyboard --
class ChoirKeyboard : public juce::MidiKeyboardComponent
{
public:
    ChoirKeyboard(SappChoirProcessor& processor, juce::MidiKeyboardState& state);

protected:
    void drawWhiteNote(int midiNoteNumber, juce::Graphics&, juce::Rectangle<float>,
                       bool isDown, bool isOver, juce::Colour lineColour,
                       juce::Colour textColour) override;
    void drawBlackNote(int midiNoteNumber, juce::Graphics&, juce::Rectangle<float>,
                       bool isDown, bool isOver, juce::Colour noteFillColour) override;

private:
    bool keyswitchInfo(int note, bool& isActive) const;
    SappChoirProcessor& processor_;
};

// ------------------------------------------------------------------- editor --
class SappChoirEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit SappChoirEditor(SappChoirProcessor&);
    ~SappChoirEditor() override;

    void paint(juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;
    void rebuildArticulationChips();
    void chooseSfz();

    SappChoirProcessor& processor_;
    CathedralLookAndFeel lookAndFeel_;

    juce::Label title_, subtitle_, instrumentName_, status_;
    juce::TextButton loadButton_{"LOAD SFZ"};
    juce::TextButton diagButton_{"BUILT-IN"};

    juce::Label voicesHeader_, vowelHeader_, ensembleHeader_, spaceHeader_;
    juce::OwnedArray<juce::TextButton> articulationChips_;

    std::unique_ptr<VowelWheel> vowelWheel_;
    std::unique_ptr<Knob> dynamics_, expression_, breath_, ensemble_, width_;
    std::unique_ptr<Knob> spaceSize_, spaceDecay_, spaceDamping_, early_, tail_;
    std::unique_ptr<Knob> master_;
    juce::ComboBox quality_;
    juce::ToggleButton limiter_{"limiter"};
    juce::ToggleButton legato_{"legato"};
    std::unique_ptr<ChoirKeyboard> keyboard_;

    juce::Label voicesLabel_;
    float meterL_ = 0.0f, meterR_ = 0.0f;
    juce::Rectangle<int> meterArea_;

    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ComboAttachment = juce::AudioProcessorValueTreeState::ComboBoxAttachment;
    using ButtonAttachment = juce::AudioProcessorValueTreeState::ButtonAttachment;
    std::vector<std::unique_ptr<SliderAttachment>> sliderAttachments_;
    std::unique_ptr<ComboAttachment> qualityAttachment_;
    std::unique_ptr<ButtonAttachment> limiterAttachment_, legatoAttachment_;

    std::unique_ptr<juce::FileChooser> fileChooser_;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SappChoirEditor)
};

} // namespace sappchoir
