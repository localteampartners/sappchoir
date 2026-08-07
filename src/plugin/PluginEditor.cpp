#include "PluginEditor.h"

#include "../core/VowelLayers.h"
#include "SoundsPanel.h"

namespace sappchoir {

namespace {

juce::String midiNoteName(int note)
{
    static const char* names[12] = {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};
    return juce::String(names[((note % 12) + 12) % 12]) + juce::String(note / 12 - 1);
}

juce::Font titleFont(float height)
{
    return juce::Font(juce::FontOptions{"Georgia", height, juce::Font::plain});
}
juce::Font uiFont(float height, bool bold = false)
{
    return juce::Font(juce::FontOptions{height, bold ? juce::Font::bold : juce::Font::plain});
}

// A gothic pointed arch path within `bounds` (apex at top centre).
juce::Path pointedArch(juce::Rectangle<float> bounds)
{
    juce::Path arch;
    const float w = bounds.getWidth(), h = bounds.getHeight();
    const float x = bounds.getX(), y = bounds.getY();
    arch.startNewSubPath(x, y + h);
    arch.lineTo(x, y + h * 0.42f);
    arch.quadraticTo(x + w * 0.02f, y + h * 0.10f, x + w * 0.5f, y);
    arch.quadraticTo(x + w * 0.98f, y + h * 0.10f, x + w, y + h * 0.42f);
    arch.lineTo(x + w, y + h);
    return arch;
}

} // namespace

// ------------------------------------------------------------ look and feel --

CathedralLookAndFeel::CathedralLookAndFeel()
{
    setColour(juce::Slider::textBoxTextColourId, palette::dim);
    setColour(juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    setColour(juce::Label::textColourId, palette::parchment);
    setColour(juce::ComboBox::backgroundColourId, palette::panel);
    setColour(juce::ComboBox::textColourId, palette::parchment);
    setColour(juce::ComboBox::outlineColourId, palette::panelEdge);
    setColour(juce::ComboBox::arrowColourId, palette::candle);
    setColour(juce::PopupMenu::backgroundColourId, palette::panel);
    setColour(juce::PopupMenu::textColourId, palette::parchment);
    setColour(juce::PopupMenu::highlightedBackgroundColourId, palette::wine);
    setColour(juce::TextButton::buttonColourId, palette::panel);
    setColour(juce::TextButton::textColourOffId, palette::parchment);
    setColour(juce::TextButton::textColourOnId, palette::background);
    setColour(juce::ToggleButton::textColourId, palette::dim);
    setColour(juce::ToggleButton::tickColourId, palette::candle);
    setColour(juce::ToggleButton::tickDisabledColourId, palette::panelEdge);
}

void CathedralLookAndFeel::drawRotarySlider(juce::Graphics& g, int x, int y, int w, int h,
                                            float sliderPos, float startAngle,
                                            float endAngle, juce::Slider& slider)
{
    const auto bounds = juce::Rectangle<float>(float(x), float(y), float(w), float(h)).reduced(4.0f);
    const float radius = juce::jmin(bounds.getWidth(), bounds.getHeight()) * 0.5f;
    const auto centre = bounds.getCentre();
    const float angle = startAngle + sliderPos * (endAngle - startAngle);
    const float arcThickness = juce::jmax(2.4f, radius * 0.085f);
    const float arcRadius = radius - arcThickness * 0.5f;

    // Body: candlelit cap, lit from the lower left like a flame beneath.
    const float capRadius = arcRadius - arcThickness * 1.7f;
    juce::ColourGradient bodyGrad(palette::panelEdge.brighter(0.10f),
                                  centre.x - capRadius * 0.4f, centre.y + capRadius * 0.5f,
                                  palette::shadow, centre.x, centre.y - capRadius, true);
    g.setGradientFill(bodyGrad);
    g.fillEllipse(centre.x - capRadius, centre.y - capRadius, capRadius * 2, capRadius * 2);
    g.setColour(palette::shadow.withAlpha(0.6f));
    g.drawEllipse(centre.x - capRadius, centre.y - capRadius, capRadius * 2, capRadius * 2, 1.0f);

    // Track arc.
    juce::Path track;
    track.addCentredArc(centre.x, centre.y, arcRadius, arcRadius, 0.0f,
                        startAngle, endAngle, true);
    g.setColour(palette::shadow);
    g.strokePath(track, juce::PathStrokeType(arcThickness, juce::PathStrokeType::curved,
                                             juce::PathStrokeType::rounded));

    // Value arc with a candle glow.
    juce::Path value;
    value.addCentredArc(centre.x, centre.y, arcRadius, arcRadius, 0.0f,
                        startAngle, angle, true);
    g.setColour(palette::candle.withAlpha(0.25f));
    g.strokePath(value, juce::PathStrokeType(arcThickness * 2.2f, juce::PathStrokeType::curved,
                                             juce::PathStrokeType::rounded));
    g.setColour(slider.isEnabled() ? palette::candleBright : palette::dim);
    g.strokePath(value, juce::PathStrokeType(arcThickness, juce::PathStrokeType::curved,
                                             juce::PathStrokeType::rounded));

    // Pointer.
    juce::Path pointer;
    pointer.addRoundedRectangle(-arcThickness * 0.5f, -capRadius + arcThickness,
                                arcThickness, capRadius * 0.42f, arcThickness * 0.4f);
    g.setColour(palette::candleBright);
    g.fillPath(pointer, juce::AffineTransform::rotation(angle).translated(centre.x, centre.y));
}

void CathedralLookAndFeel::drawComboBox(juce::Graphics& g, int width, int height, bool,
                                        int, int, int, int, juce::ComboBox& box)
{
    const auto bounds = juce::Rectangle<float>(0, 0, float(width), float(height)).reduced(0.5f);
    g.setColour(palette::panel);
    g.fillRoundedRectangle(bounds, 4.0f);
    g.setColour(box.hasKeyboardFocus(true) ? palette::candle : palette::panelEdge);
    g.drawRoundedRectangle(bounds, 4.0f, 1.0f);
    juce::Path arrow;
    const float ax = float(width) - 14.0f, ay = float(height) * 0.5f;
    arrow.addTriangle(ax - 4, ay - 2.5f, ax + 4, ay - 2.5f, ax, ay + 3.5f);
    g.setColour(palette::candle);
    g.fillPath(arrow);
}

void CathedralLookAndFeel::drawButtonBackground(juce::Graphics& g, juce::Button& button,
                                                const juce::Colour&, bool highlighted,
                                                bool down)
{
    auto bounds = button.getLocalBounds().toFloat().reduced(0.5f);
    const bool on = button.getToggleState();
    juce::Colour fill = on ? palette::candle : palette::panel;
    if (down) fill = fill.darker(0.2f);
    else if (highlighted) fill = fill.brighter(0.08f);
    g.setColour(fill);
    g.fillRoundedRectangle(bounds, 5.0f);
    g.setColour(on ? palette::candleBright : palette::panelEdge);
    g.drawRoundedRectangle(bounds, 5.0f, 1.0f);
}

juce::Font CathedralLookAndFeel::getComboBoxFont(juce::ComboBox&) { return uiFont(13.0f); }
juce::Font CathedralLookAndFeel::getPopupMenuFont() { return uiFont(13.5f); }

// -------------------------------------------------------------------- knob ---

Knob::Knob(juce::AudioProcessorValueTreeState&, const juce::String&,
           const juce::String& title, bool big)
    : big_(big)
{
    slider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    slider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    slider.setPopupDisplayEnabled(true, true, nullptr);
    addAndMakeVisible(slider);

    label_.setText(title, juce::dontSendNotification);
    label_.setJustificationType(juce::Justification::centred);
    label_.setFont(uiFont(big ? 13.0f : 11.0f, big));
    label_.setColour(juce::Label::textColourId, big ? palette::parchment : palette::dim);
    label_.setInterceptsMouseClicks(false, false);
    addAndMakeVisible(label_);
}

void Knob::resized()
{
    auto bounds = getLocalBounds();
    label_.setBounds(bounds.removeFromBottom(big_ ? 18 : 14));
    slider.setBounds(bounds);
}

// -------------------------------------------------------------- vowel wheel --

VowelWheel::VowelWheel(juce::AudioProcessorValueTreeState& state)
    : param_(state.getParameter("vowel"))
{
    startTimerHz(30);
    setMouseCursor(juce::MouseCursor::UpDownLeftRightResizeCursor);
}
VowelWheel::~VowelWheel() { stopTimer(); }

void VowelWheel::timerCallback() { repaint(); }

void VowelWheel::applyDrag(juce::Point<float> position)
{
    // Combined vertical + horizontal drag, like a big rotary.
    const float delta = (dragStart_.y - position.y) + (position.x - dragStart_.x);
    param_->setValueNotifyingHost(
        juce::jlimit(0.0f, 1.0f, dragStartValue_ + delta / 260.0f));
}

void VowelWheel::mouseDown(const juce::MouseEvent& e)
{
    param_->beginChangeGesture();
    dragStart_ = e.position;
    dragStartValue_ = param_->getValue();
    glow_ = 1.0f;
}
void VowelWheel::mouseDrag(const juce::MouseEvent& e) { applyDrag(e.position); }
void VowelWheel::mouseUp(const juce::MouseEvent&) { param_->endChangeGesture(); }
void VowelWheel::mouseDoubleClick(const juce::MouseEvent&)
{
    param_->setValueNotifyingHost(param_->getDefaultValue());
}

void VowelWheel::paint(juce::Graphics& g)
{
    const auto bounds = getLocalBounds().toFloat();
    const auto centre = bounds.getCentre();
    // Reserve a margin so the vowel labels render inside the component.
    const float radius = juce::jmin(bounds.getWidth(), bounds.getHeight()) * 0.5f - 28.0f;
    const float value = param_ != nullptr ? param_->getValue() : 0.35f;

    // Sweep: 270° from lower-left (oo) to lower-right (eh).
    const float startAngle = -juce::MathConstants<float>::pi * 0.75f;
    const float endAngle = -startAngle;
    const float angle = startAngle + value * (endAngle - startAngle);

    glow_ = juce::jmax(0.55f, glow_ * 0.95f);

    // Halo — candlelight behind the wheel.
    juce::ColourGradient halo(palette::candle.withAlpha(0.16f * glow_), centre.x, centre.y,
                              palette::candle.withAlpha(0.0f), centre.x, centre.y - radius * 1.35f,
                              true);
    g.setGradientFill(halo);
    g.fillEllipse(centre.x - radius * 1.3f, centre.y - radius * 1.3f,
                  radius * 2.6f, radius * 2.6f);

    // Stone ring.
    const float ringThickness = radius * 0.16f;
    const float ringRadius = radius - ringThickness * 0.5f;
    juce::Path track;
    track.addCentredArc(centre.x, centre.y, ringRadius, ringRadius, 0.0f,
                        startAngle, endAngle, true);
    g.setColour(palette::shadow);
    g.strokePath(track, juce::PathStrokeType(ringThickness, juce::PathStrokeType::curved,
                                             juce::PathStrokeType::rounded));

    // Lit arc up to the pointer.
    juce::Path lit;
    lit.addCentredArc(centre.x, centre.y, ringRadius, ringRadius, 0.0f,
                      startAngle, angle, true);
    g.setColour(palette::candle.withAlpha(0.30f));
    g.strokePath(lit, juce::PathStrokeType(ringThickness * 1.9f, juce::PathStrokeType::curved,
                                           juce::PathStrokeType::rounded));
    g.setColour(palette::candleBright);
    g.strokePath(lit, juce::PathStrokeType(ringThickness * 0.8f, juce::PathStrokeType::curved,
                                           juce::PathStrokeType::rounded));

    // Inner face.
    const float faceRadius = radius - ringThickness * 1.9f;
    juce::ColourGradient face(palette::panel.brighter(0.10f),
                              centre.x - faceRadius * 0.35f, centre.y + faceRadius * 0.45f,
                              palette::shadow, centre.x, centre.y - faceRadius, true);
    g.setGradientFill(face);
    g.fillEllipse(centre.x - faceRadius, centre.y - faceRadius, faceRadius * 2, faceRadius * 2);
    g.setColour(palette::panelEdge);
    g.drawEllipse(centre.x - faceRadius, centre.y - faceRadius, faceRadius * 2, faceRadius * 2, 1.2f);

    // Pointer.
    juce::Path pointer;
    pointer.addRoundedRectangle(-ringThickness * 0.28f, -radius + ringThickness * 0.2f,
                                ringThickness * 0.56f, ringThickness * 2.1f, 2.0f);
    g.setColour(palette::candleBright);
    g.fillPath(pointer, juce::AffineTransform::rotation(angle).translated(centre.x, centre.y));

    // Vowel labels around the ring; the nearest one glows.
    g.setFont(uiFont(radius * 0.16f, true));
    for (int v = 0; v < sapp::choir::kNumVowels; ++v) {
        const float t = float(v) / float(sapp::choir::kNumVowels - 1);
        const float a = startAngle + t * (endAngle - startAngle);
        const float lx = centre.x + std::sin(a) * (radius + ringThickness * 1.6f);
        const float ly = centre.y - std::cos(a) * (radius + ringThickness * 1.6f);
        const float near = 1.0f - juce::jmin(1.0f, std::abs(value - t) * 3.0f);
        g.setColour(palette::dim.interpolatedWith(palette::candleBright, near));
        g.drawText(juce::String(sapp::choir::kVowelNames[v]).toUpperCase(),
                   juce::Rectangle<float>(lx - 26, ly - 10, 52, 20),
                   juce::Justification::centred);
    }

    // Current vowel, large, in the face.
    const float position = value * float(sapp::choir::kNumVowels - 1);
    const int nearest = juce::jlimit(0, sapp::choir::kNumVowels - 1, int(position + 0.5f));
    g.setColour(palette::parchment.withAlpha(0.9f));
    g.setFont(titleFont(faceRadius * 0.62f));
    g.drawText(juce::String(sapp::choir::kVowelNames[nearest]),
               getLocalBounds(), juce::Justification::centred);
    g.setColour(palette::dim);
    g.setFont(uiFont(faceRadius * 0.14f, true));
    g.drawText("VOWEL", int(centre.x - 40), int(centre.y + faceRadius * 0.34f), 80, 14,
               juce::Justification::centred);
}

// ---------------------------------------------------------------- keyboard ---

ChoirKeyboard::ChoirKeyboard(SappChoirProcessor& processor,
                             juce::MidiKeyboardState& state)
    : juce::MidiKeyboardComponent(state, juce::MidiKeyboardComponent::horizontalKeyboard),
      processor_(processor)
{
    setColour(juce::MidiKeyboardComponent::whiteNoteColourId, juce::Colour(0xffe7ddc8));
    setColour(juce::MidiKeyboardComponent::blackNoteColourId, juce::Colour(0xff251d28));
    setColour(juce::MidiKeyboardComponent::keySeparatorLineColourId, juce::Colour(0xff9a8f7c));
    setColour(juce::MidiKeyboardComponent::mouseOverKeyOverlayColourId,
              palette::candle.withAlpha(0.35f));
    setColour(juce::MidiKeyboardComponent::keyDownOverlayColourId,
              palette::candle.withAlpha(0.6f));
    setColour(juce::MidiKeyboardComponent::shadowColourId, palette::shadow.withAlpha(0.7f));
    setAvailableRange(12, 108);
    setKeyWidth(13.0f);
    setScrollButtonsVisible(false);
}

bool ChoirKeyboard::keyswitchInfo(int note, bool& isActive) const
{
    isActive = false;
    auto inst = processor_.engine().currentInstrument();
    if (!inst) return false;
    const auto& def = inst->definition;
    if (def.keyswitchLo < 0 || note < def.keyswitchLo || note > def.keyswitchHi)
        return false;
    sapp::sounds::DiagnosticSnapshot snap;
    if (processor_.engine().sampler().diagnostics().read(snap))
        isActive = snap.activeKeyswitch == note;
    return true;
}

void ChoirKeyboard::drawWhiteNote(int note, juce::Graphics& g,
                                  juce::Rectangle<float> area, bool isDown,
                                  bool isOver, juce::Colour lineColour,
                                  juce::Colour textColour)
{
    bool active = false;
    if (keyswitchInfo(note, active)) {
        g.setColour(active ? palette::candle : palette::wine.brighter(0.1f));
        g.fillRect(area);
        if (isDown) { g.setColour(palette::candleBright.withAlpha(0.5f)); g.fillRect(area); }
        g.setColour(lineColour);
        g.fillRect(area.withWidth(1.0f));
        return;
    }
    juce::MidiKeyboardComponent::drawWhiteNote(note, g, area, isDown, isOver,
                                               lineColour, textColour);
}

void ChoirKeyboard::drawBlackNote(int note, juce::Graphics& g,
                                  juce::Rectangle<float> area, bool isDown,
                                  bool isOver, juce::Colour fill)
{
    bool active = false;
    if (keyswitchInfo(note, active)) {
        g.setColour(active ? palette::candle : palette::wine);
        g.fillRect(area);
        if (isDown) { g.setColour(palette::candleBright.withAlpha(0.5f)); g.fillRect(area); }
        return;
    }
    juce::MidiKeyboardComponent::drawBlackNote(note, g, area, isDown, isOver, fill);
}

// ------------------------------------------------------------------- editor --

SappChoirEditor::SappChoirEditor(SappChoirProcessor& processor)
    : juce::AudioProcessorEditor(&processor), processor_(processor)
{
    setLookAndFeel(&lookAndFeel_);

    auto& state = processor_.valueTree();

    title_.setText("SappChoir", juce::dontSendNotification);
    title_.setFont(titleFont(30.0f));
    title_.setColour(juce::Label::textColourId, palette::candleBright);
    addAndMakeVisible(title_);

    subtitle_.setText("VOCAL CATHEDRAL", juce::dontSendNotification);
    subtitle_.setFont(uiFont(10.0f));
    subtitle_.setColour(juce::Label::textColourId, palette::dim);
    addAndMakeVisible(subtitle_);

    instrumentName_.setFont(uiFont(15.0f, true));
    instrumentName_.setColour(juce::Label::textColourId, palette::parchment);
    instrumentName_.setJustificationType(juce::Justification::centredRight);
    addAndMakeVisible(instrumentName_);

    status_.setFont(uiFont(11.0f));
    status_.setColour(juce::Label::textColourId, palette::dim);
    status_.setJustificationType(juce::Justification::centredRight);
    addAndMakeVisible(status_);

    loadButton_.onClick = [this] { chooseSfz(); };
    addAndMakeVisible(loadButton_);
    diagButton_.onClick = [this] { processor_.loadDiagnosticInstrument(); };
    addAndMakeVisible(diagButton_);
    soundsButton_.onClick = [this] { openSoundsPanel(); };
    addAndMakeVisible(soundsButton_);

    auto header = [&](juce::Label& label, const juce::String& text) {
        label.setText(text, juce::dontSendNotification);
        label.setFont(uiFont(10.5f, true));
        label.setColour(juce::Label::textColourId, palette::dim);
        addAndMakeVisible(label);
    };
    header(voicesHeader_, "VOICES");
    header(vowelHeader_, "VOWEL MORPH");
    header(ensembleHeader_, "ENSEMBLE");
    header(spaceHeader_, "SANCTUARY");

    vowelWheel_ = std::make_unique<VowelWheel>(state);
    // A hidden slider attachment is unnecessary — the wheel talks to the
    // parameter directly.
    addAndMakeVisible(*vowelWheel_);

    auto knob = [&](const juce::String& id, const juce::String& text, bool big = false) {
        auto k = std::make_unique<Knob>(state, id, text, big);
        sliderAttachments_.push_back(
            std::make_unique<SliderAttachment>(state, id, k->slider));
        addAndMakeVisible(*k);
        return k;
    };
    dynamics_ = knob("dynamics", "DYNAMICS", true);
    expression_ = knob("expression", "EXPRESSION");
    breath_ = knob("breath", "AIR", true);
    ensemble_ = knob("ensemble", "ENSEMBLE", true);
    width_ = knob("width", "WIDTH");
    spaceSize_ = knob("spaceSize", "SIZE");
    spaceDecay_ = knob("spaceDecay", "DECAY");
    spaceDamping_ = knob("spaceDamping", "DAMP");
    early_ = knob("earlyLevel", "EARLY");
    tail_ = knob("tailLevel", "TAIL");
    master_ = knob("masterGain", "MASTER");

    quality_.addItemList({"Draft", "Normal"}, 1);
    qualityAttachment_ = std::make_unique<ComboAttachment>(state, "quality", quality_);
    addAndMakeVisible(quality_);

    limiterAttachment_ = std::make_unique<ButtonAttachment>(state, "limiter", limiter_);
    addAndMakeVisible(limiter_);
    legatoAttachment_ = std::make_unique<ButtonAttachment>(state, "legato", legato_);
    addAndMakeVisible(legato_);

    keyboard_ = std::make_unique<ChoirKeyboard>(processor_, processor_.keyboardState);
    addAndMakeVisible(*keyboard_);

    voicesLabel_.setFont(uiFont(11.0f));
    voicesLabel_.setColour(juce::Label::textColourId, palette::dim);
    addAndMakeVisible(voicesLabel_);

    processor_.onInstrumentChanged = [this] { rebuildArticulationChips(); };
    rebuildArticulationChips();

    startTimerHz(24);
    setResizable(true, true);
    setResizeLimits(780, 520, 1600, 1070);
    getConstrainer()->setFixedAspectRatio(940.0 / 620.0);
    setSize(940, 620);
}

SappChoirEditor::~SappChoirEditor()
{
    processor_.onInstrumentChanged = nullptr;
    setLookAndFeel(nullptr);
}

SoundsPanel& SappChoirEditor::ensureSoundsPanel()
{
    if (soundsPanel_ == nullptr) {
        soundsPanel_ = std::make_unique<SoundsPanel>(
            processor_, [this] { soundsPanel_->setVisible(false); });
        addChildComponent(*soundsPanel_);
    }
    return *soundsPanel_;
}

void SappChoirEditor::openSoundsPanel()
{
    auto& panel = ensureSoundsPanel();
    panel.setBounds(getLocalBounds().reduced(14));
    panel.setVisible(true);
    panel.toFront(true);
}

void SappChoirEditor::chooseSfz()
{
    fileChooser_ = std::make_unique<juce::FileChooser>(
        "Load SFZ instrument", juce::File(), "*.sfz");
    fileChooser_->launchAsync(juce::FileBrowserComponent::openMode |
                                  juce::FileBrowserComponent::canSelectFiles,
                              [this](const juce::FileChooser& chooser) {
                                  const auto file = chooser.getResult();
                                  if (file.existsAsFile())
                                      processor_.loadSfzInstrument(file);
                              });
}

void SappChoirEditor::rebuildArticulationChips()
{
    articulationChips_.clear();
    const auto names = processor_.articulationNames();
    auto inst = processor_.engine().currentInstrument();
    for (int i = 0; i < names.size(); ++i) {
        auto* chip = articulationChips_.add(new juce::TextButton());
        juce::String text = names[i];
        if (inst && size_t(i) < inst->definition.articulations.size()) {
            const int ks = inst->definition.articulations[size_t(i)].keyswitch;
            if (ks >= 0) text << "   " << midiNoteName(ks);
        }
        chip->setButtonText(text);
        chip->setClickingTogglesState(false);
        chip->onClick = [this, i] { processor_.selectArticulation(i); };
        addAndMakeVisible(chip);
    }
    instrumentName_.setText(processor_.currentInstrumentName(), juce::dontSendNotification);
    resized();
    repaint();
}

void SappChoirEditor::timerCallback()
{
    status_.setText(processor_.loadStatus(), juce::dontSendNotification);
    instrumentName_.setText(processor_.currentInstrumentName(), juce::dontSendNotification);

    sapp::sounds::DiagnosticSnapshot snap;
    if (processor_.engine().sampler().diagnostics().read(snap)) {
        voicesLabel_.setText(juce::String(snap.activeVoices) + " voices",
                             juce::dontSendNotification);
        meterL_ = juce::jmax(snap.lastPeakL, meterL_ * 0.86f);
        meterR_ = juce::jmax(snap.lastPeakR, meterR_ * 0.86f);

        // Reflect the sounding articulation in the chip states.
        if (auto inst = processor_.engine().currentInstrument()) {
            const auto& arts = inst->definition.articulations;
            for (int i = 0; i < articulationChips_.size() && size_t(i) < arts.size(); ++i)
                articulationChips_[i]->setToggleState(
                    arts[size_t(i)].keyswitch == snap.activeKeyswitch &&
                        snap.activeKeyswitch >= 0,
                    juce::dontSendNotification);
        }
    }
    keyboard_->repaint();
    repaint(meterArea_);
}

void SappChoirEditor::paint(juce::Graphics& g)
{
    // Night stone with candle warmth pooling at the bottom.
    juce::ColourGradient grad(palette::background.brighter(0.05f), 0.0f, 0.0f,
                              palette::background.darker(0.3f), 0.0f, float(getHeight()),
                              false);
    g.setGradientFill(grad);
    g.fillAll();

    const float scale = float(getWidth()) / 940.0f;
    auto s = [scale](int v) { return int(float(v) * scale); };

    // Two candle glows low in the frame.
    auto candleGlow = [&](float cx, float cy, float r, float alpha) {
        juce::ColourGradient glow(palette::candle.withAlpha(alpha), cx, cy,
                                  palette::candle.withAlpha(0.0f), cx, cy - r, true);
        g.setGradientFill(glow);
        g.fillEllipse(cx - r, cy - r, r * 2, r * 2);
    };
    candleGlow(float(s(180)), float(getHeight()) - float(s(40)), float(s(240)), 0.05f);
    candleGlow(float(s(760)), float(getHeight()) - float(s(30)), float(s(280)), 0.06f);

    // Faint arched windows behind the header.
    g.setColour(palette::panelEdge.withAlpha(0.5f));
    for (int i = 0; i < 3; ++i) {
        auto arch = pointedArch(juce::Rectangle<float>(
            float(getWidth()) - float(s(300 - i * 92)), float(s(8)),
            float(s(56)), float(s(48))));
        g.strokePath(arch, juce::PathStrokeType(1.0f));
    }

    // Panels: the vowel panel is itself a pointed arch; side panels are stone.
    auto stonePanel = [&](juce::Rectangle<int> r) {
        g.setColour(palette::panel.withAlpha(0.75f));
        g.fillRoundedRectangle(r.toFloat(), 8.0f);
        g.setColour(palette::panelEdge);
        g.drawRoundedRectangle(r.toFloat(), 8.0f, 1.0f);
    };
    stonePanel({s(14), s(74), s(188), s(368)});    // voices / articulations
    stonePanel({s(628), s(74), s(298), s(368)});   // ensemble + sanctuary

    const juce::Rectangle<float> archBounds(float(s(214)), float(s(74)),
                                            float(s(402)), float(s(368)));
    auto arch = pointedArch(archBounds);
    g.setColour(palette::panel.withAlpha(0.8f));
    g.fillPath(arch);
    g.setColour(palette::panelEdge.brighter(0.15f));
    g.strokePath(arch, juce::PathStrokeType(1.4f));

    // Gold hairline under the header.
    g.setColour(palette::candle.withAlpha(0.35f));
    g.fillRect(s(14), s(66), getWidth() - s(28), 1);

    // Peak meter.
    if (!meterArea_.isEmpty()) {
        g.setColour(palette::shadow);
        g.fillRoundedRectangle(meterArea_.toFloat(), 3.0f);
        auto bar = [&](float level, juce::Rectangle<int> r) {
            const float db = juce::Decibels::gainToDecibels(level, -60.0f);
            const float norm = juce::jlimit(0.0f, 1.0f, (db + 60.0f) / 60.0f);
            auto fill = r.toFloat();
            fill = fill.removeFromLeft(fill.getWidth() * norm);
            g.setColour(db > -3.0f ? palette::wine.brighter(0.4f) : palette::candle);
            g.fillRoundedRectangle(fill, 2.0f);
        };
        auto inner = meterArea_.reduced(2);
        bar(meterL_, inner.removeFromTop(inner.getHeight() / 2).reduced(0, 1));
        bar(meterR_, inner.reduced(0, 1));
    }
}

void SappChoirEditor::resized()
{
    const float scale = float(getWidth()) / 940.0f;
    auto s = [scale](int v) { return int(float(v) * scale); };

    title_.setBounds(s(18), s(10), s(240), s(34));
    subtitle_.setBounds(s(21), s(42), s(240), s(16));
    loadButton_.setBounds(s(266), s(20), s(92), s(28));
    diagButton_.setBounds(s(364), s(20), s(84), s(28));
    soundsButton_.setBounds(s(456), s(20), s(112), s(28));
    if (soundsPanel_ != nullptr && soundsPanel_->isVisible())
        soundsPanel_->setBounds(getLocalBounds().reduced(14));
    instrumentName_.setBounds(getWidth() - s(330), s(12), s(314), s(24));
    status_.setBounds(getWidth() - s(330), s(36), s(314), s(18));

    // Voices panel (articulations).
    voicesHeader_.setBounds(s(26), s(82), s(160), s(16));
    int chipY = s(104);
    for (auto* chip : articulationChips_) {
        chip->setBounds(s(26), chipY, s(164), s(30));
        chipY += s(36);
    }

    // Vowel arch: wheel + performance knobs.
    vowelHeader_.setBounds(s(360), s(96), s(120), s(16));
    vowelWheel_->setBounds(s(302), s(118), s(226), s(226));
    dynamics_->setBounds(s(238), s(330), s(106), s(104));
    expression_->setBounds(s(360), s(342), s(96), s(92));
    breath_->setBounds(s(480), s(330), s(106), s(104));

    // Ensemble + sanctuary panel.
    ensembleHeader_.setBounds(s(640), s(82), s(160), s(16));
    ensemble_->setBounds(s(644), s(100), s(110), s(112));
    width_->setBounds(s(772), s(108), s(94), s(104));
    spaceHeader_.setBounds(s(640), s(226), s(160), s(16));
    const int knobW = s(54), knobH = s(78);
    int hx = s(640);
    for (auto* k : {spaceSize_.get(), spaceDecay_.get(), spaceDamping_.get(),
                    early_.get(), tail_.get()}) {
        k->setBounds(hx, s(246), knobW, knobH);
        hx += knobW + s(2);
    }
    master_->setBounds(s(772), s(336), s(84), s(94));
    quality_.setBounds(s(644), s(346), s(100), s(26));
    legato_.setBounds(s(644), s(384), s(84), s(24));

    // Footer strip.
    const int keyboardY = s(462);
    keyboard_->setBounds(s(14), keyboardY, getWidth() - s(28), s(96));
    keyboard_->setKeyWidth(float(keyboard_->getWidth()) / 56.5f);

    voicesLabel_.setBounds(s(14), s(572), s(90), s(20));
    meterArea_ = {s(110), s(576), s(150), s(14)};
    limiter_.setBounds(getWidth() - s(150), s(572), s(90), s(24));
}

} // namespace sappchoir
