#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_graphics/juce_graphics.h>

namespace bf::gui {

class BeatFixLookAndFeel : public juce::LookAndFeel_V4 {
public:
    BeatFixLookAndFeel() {
        setColour(juce::ResizableWindow::backgroundColourId, juce::Colour(0xFF0B0E14));
        setColour(juce::Label::textColourId, juce::Colour(0xFFE2E8F0));
        setColour(juce::TextEditor::backgroundColourId, juce::Colour(0xFF161B26));
        setColour(juce::TextEditor::textColourId, juce::Colour(0xFFFFFFFF));
        setColour(juce::TextEditor::outlineColourId, juce::Colour(0xFF2D3748));
        setColour(juce::ComboBox::backgroundColourId, juce::Colour(0xFF161B26));
        setColour(juce::ComboBox::textColourId, juce::Colour(0xFFFFFFFF));
        setColour(juce::ComboBox::outlineColourId, juce::Colour(0xFF2D3748));
        setColour(juce::PopupMenu::backgroundColourId, juce::Colour(0xFF121620));
        setColour(juce::PopupMenu::textColourId, juce::Colour(0xFFE2E8F0));
        setColour(juce::PopupMenu::highlightedBackgroundColourId, juce::Colour(0xFF2A3447));
        setColour(juce::PopupMenu::highlightedTextColourId, juce::Colour(0xFFFFFFFF));
    }

    void drawButtonBackground(juce::Graphics& g,
                              juce::Button& button,
                              const juce::Colour& backgroundColour,
                              bool shouldDrawButtonAsHighlighted,
                              bool shouldDrawButtonAsDown) override {
        auto bounds = button.getLocalBounds().toFloat().reduced(0.5f);
        float cornerRadius = 6.0f;

        bool isAccented = button.getProperties().contains("accentRed") ||
                          (button.findColour(juce::TextButton::buttonColourId) == juce::Colour(0xFFE53E3E)) ||
                          (button.findColour(juce::TextButton::buttonColourId) == juce::Colour(0xFFFF334B));

        juce::Colour baseColour = isAccented ? juce::Colour(0xFFFF334B) :
                                  (backgroundColour != button.findColour(juce::TextButton::buttonColourId) ? backgroundColour : juce::Colour(0xFF161B26));

        if (shouldDrawButtonAsDown) {
            baseColour = baseColour.darker(0.2f);
        } else if (shouldDrawButtonAsHighlighted) {
            baseColour = baseColour.brighter(0.15f);
        }

        // Fill background
        g.setColour(baseColour);
        g.fillRoundedRectangle(bounds, cornerRadius);

        // Border outline
        juce::Colour outlineColour = isAccented ? juce::Colour(0xFFFF5C70) : juce::Colour(0xFF252D3D);
        if (shouldDrawButtonAsHighlighted) {
            outlineColour = outlineColour.brighter(0.25f);
        }
        g.setColour(outlineColour);
        g.drawRoundedRectangle(bounds, cornerRadius, 1.0f);
    }

    void drawButtonText(juce::Graphics& g,
                        juce::TextButton& button,
                        bool /*shouldDrawButtonAsHighlighted*/,
                        bool /*shouldDrawButtonAsDown*/) override {
        auto font = getTextButtonFont(button, button.getHeight());
        g.setFont(font);

        bool isAccented = button.getProperties().contains("accentRed") ||
                          (button.findColour(juce::TextButton::buttonColourId) == juce::Colour(0xFFE53E3E)) ||
                          (button.findColour(juce::TextButton::buttonColourId) == juce::Colour(0xFFFF334B));

        g.setColour(isAccented ? juce::Colours::white :
                    button.findColour(button.getToggleState() ? juce::TextButton::textColourOnId : juce::TextButton::textColourOffId));

        const int yIndent = juce::jmin(4, button.proportionOfHeight(0.3f));
        const int cornerSize = juce::jmin(button.getHeight(), button.getWidth()) / 2;
        const int fontHeight = juce::roundToInt(font.getHeight() * 0.6f);
        const int leftIndent = juce::jmin(fontHeight, 2 + cornerSize / (button.isConnectedOnLeft() ? 4 : 2));
        const int rightIndent = juce::jmin(fontHeight, 2 + cornerSize / (button.isConnectedOnRight() ? 4 : 2));

        g.drawFittedText(button.getButtonText(),
                         leftIndent, yIndent,
                         button.getWidth() - leftIndent - rightIndent,
                         button.getHeight() - yIndent * 2,
                         juce::Justification::centred, 2);
    }

    void drawComboBox(juce::Graphics& g, int width, int height, bool /*isButtonDown*/,
                      int /*buttonX*/, int /*buttonY*/, int /*buttonW*/, int /*buttonH*/,
                      juce::ComboBox& box) override {
        auto bounds = juce::Rectangle<int>(0, 0, width, height).toFloat().reduced(0.5f);
        g.setColour(juce::Colour(0xFF161B26));
        g.fillRoundedRectangle(bounds, 6.0f);

        g.setColour(box.hasKeyboardFocus(false) ? juce::Colour(0xFFFF334B) : juce::Colour(0xFF252D3D));
        g.drawRoundedRectangle(bounds, 6.0f, 1.0f);

        // Arrow
        auto arrowZone = juce::Rectangle<float>((float)width - 24.0f, 0.0f, 20.0f, (float)height);
        juce::Path path;
        path.startNewSubPath(arrowZone.getCentreX() - 4.0f, arrowZone.getCentreY() - 2.0f);
        path.lineTo(arrowZone.getCentreX(), arrowZone.getCentreY() + 2.5f);
        path.lineTo(arrowZone.getCentreX() + 4.0f, arrowZone.getCentreY() - 2.0f);

        g.setColour(juce::Colour(0xFFA0AEC0));
        g.strokePath(path, juce::PathStrokeType(1.5f));
    }

    void drawProgressBar(juce::Graphics& g, juce::ProgressBar& /*bar*/,
                         int width, int height,
                         double progress, const juce::String& textToShow) override {
        auto bounds = juce::Rectangle<int>(0, 0, width, height).toFloat().reduced(0.5f);
        g.setColour(juce::Colour(0xFF161B26));
        g.fillRoundedRectangle(bounds, 6.0f);
        g.setColour(juce::Colour(0xFF252D3D));
        g.drawRoundedRectangle(bounds, 6.0f, 1.0f);

        if (progress > 0.0) {
            auto progressBounds = bounds.reduced(2.0f);
            progressBounds.setWidth(static_cast<float>(progressBounds.getWidth() * std::clamp(progress, 0.0, 1.0)));
            g.setColour(juce::Colour(0xFFFF334B));
            g.fillRoundedRectangle(progressBounds, 4.0f);
        }

        g.setColour(juce::Colours::white);
        g.setFont(juce::Font(12.0f, juce::Font::bold));
        juce::String displayStr = textToShow.isNotEmpty() ? textToShow :
                                  (progress > 0.0 ? juce::String(juce::roundToInt(progress * 100.0)) + "%" : "0%");
        g.drawFittedText(displayStr, 0, 0, width, height, juce::Justification::centred, 1);
    }
};

} // namespace bf::gui
