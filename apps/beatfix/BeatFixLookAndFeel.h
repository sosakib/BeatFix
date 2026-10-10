#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_graphics/juce_graphics.h>
#include <cmath>

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

    // --- Vector Icon Drawing Utilities ---

    static void drawFolderIcon(juce::Graphics& g, juce::Rectangle<float> r, juce::Colour col) {
        g.setColour(col);
        float x = r.getX(), y = r.getY(), w = r.getWidth(), h = r.getHeight();
        juce::Path p;
        p.startNewSubPath(x, y + h * 0.28f);
        p.lineTo(x + w * 0.38f, y + h * 0.28f);
        p.lineTo(x + w * 0.48f, y + h * 0.44f);
        p.lineTo(x + w, y + h * 0.44f);
        p.lineTo(x + w, y + h);
        p.lineTo(x, y + h);
        p.closeSubPath();
        g.strokePath(p, juce::PathStrokeType(1.4f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }

    static void drawSaveIcon(juce::Graphics& g, juce::Rectangle<float> r, juce::Colour col) {
        g.setColour(col);
        float x = r.getX(), y = r.getY(), w = r.getWidth(), h = r.getHeight();
        juce::Path p;
        p.addRoundedRectangle(x, y, w, h, 1.8f);
        g.strokePath(p, juce::PathStrokeType(1.4f));
        g.fillRect(x + w * 0.22f, y + 1.0f, w * 0.56f, h * 0.35f);
        g.drawHorizontalLine(juce::roundToInt(y + h * 0.72f), x + 2.0f, x + w - 2.0f);
    }

    static void drawPlayIcon(juce::Graphics& g, juce::Rectangle<float> r, juce::Colour col) {
        g.setColour(col);
        juce::Path p;
        p.addTriangle(r.getX(), r.getY(),
                      r.getRight(), r.getCentreY(),
                      r.getX(), r.getBottom());
        g.fillPath(p);
    }

    static void drawPauseIcon(juce::Graphics& g, juce::Rectangle<float> r, juce::Colour col) {
        g.setColour(col);
        float bar_w = r.getWidth() * 0.32f;
        g.fillRoundedRectangle(r.getX(), r.getY(), bar_w, r.getHeight(), 1.0f);
        g.fillRoundedRectangle(r.getRight() - bar_w, r.getY(), bar_w, r.getHeight(), 1.0f);
    }

    static void drawStopIcon(juce::Graphics& g, juce::Rectangle<float> r, juce::Colour col) {
        g.setColour(col);
        g.fillRoundedRectangle(r, 1.5f);
    }

    static void drawExportIcon(juce::Graphics& g, juce::Rectangle<float> r, juce::Colour col) {
        g.setColour(col);
        float cx = r.getCentreX(), cy = r.getCentreY();
        juce::Path p;
        // Arrow head
        p.startNewSubPath(cx, r.getY());
        p.lineTo(cx - r.getWidth() * 0.4f, cy);
        p.startNewSubPath(cx, r.getY());
        p.lineTo(cx + r.getWidth() * 0.4f, cy);
        // Arrow stem
        p.startNewSubPath(cx, r.getY());
        p.lineTo(cx, r.getY() + r.getHeight() * 0.65f);
        // Base tray
        p.startNewSubPath(r.getX(), cy + r.getHeight() * 0.2f);
        p.lineTo(r.getX(), r.getBottom());
        p.lineTo(r.getRight(), r.getBottom());
        p.lineTo(r.getRight(), cy + r.getHeight() * 0.2f);
        g.strokePath(p, juce::PathStrokeType(1.4f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }

    static void drawMenuIcon(juce::Graphics& g, juce::Rectangle<float> r, juce::Colour col) {
        g.setColour(col);
        float cx = r.getCentreX();
        float d = 3.0f;
        g.fillEllipse(cx - 1.5f, r.getY() + 3.0f, d, d);
        g.fillEllipse(cx - 1.5f, r.getCentreY() - 1.5f, d, d);
        g.fillEllipse(cx - 1.5f, r.getBottom() - 6.0f, d, d);
    }

    static void drawChevronDown(juce::Graphics& g, juce::Rectangle<float> r, juce::Colour col) {
        g.setColour(col);
        juce::Path p;
        p.startNewSubPath(r.getX(), r.getY());
        p.lineTo(r.getCentreX(), r.getBottom());
        p.lineTo(r.getRight(), r.getY());
        g.strokePath(p, juce::PathStrokeType(1.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }

    static void drawChevronUp(juce::Graphics& g, juce::Rectangle<float> r, juce::Colour col) {
        g.setColour(col);
        juce::Path p;
        p.startNewSubPath(r.getX(), r.getBottom());
        p.lineTo(r.getCentreX(), r.getY());
        p.lineTo(r.getRight(), r.getBottom());
        g.strokePath(p, juce::PathStrokeType(1.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }

    static void drawMusicNoteIcon(juce::Graphics& g, juce::Rectangle<float> r, juce::Colour col) {
        g.setColour(col);
        float x = r.getX(), y = r.getY(), w = r.getWidth(), h = r.getHeight();
        g.fillEllipse(x + w * 0.05f, y + h * 0.62f, w * 0.36f, h * 0.30f);
        g.fillEllipse(x + w * 0.58f, y + h * 0.48f, w * 0.36f, h * 0.30f);
        juce::Path p;
        p.startNewSubPath(x + w * 0.38f, y + h * 0.68f);
        p.lineTo(x + w * 0.38f, y + h * 0.15f);
        p.lineTo(x + w * 0.91f, y + h * 0.05f);
        p.lineTo(x + w * 0.91f, y + h * 0.54f);
        g.strokePath(p, juce::PathStrokeType(1.8f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }

    static void drawTargetIcon(juce::Graphics& g, juce::Rectangle<float> r, juce::Colour col) {
        g.setColour(col);
        g.drawEllipse(r, 1.5f);
        g.fillEllipse(r.getCentreX() - 2.5f, r.getCentreY() - 2.5f, 5.0f, 5.0f);
    }

    static void drawInfoIcon(juce::Graphics& g, juce::Rectangle<float> r, juce::Colour col) {
        g.setColour(col);
        g.drawEllipse(r, 1.2f);
        g.fillEllipse(r.getCentreX() - 1.0f, r.getY() + 2.5f, 2.0f, 2.0f);
        g.drawVerticalLine(juce::roundToInt(r.getCentreX()), r.getY() + 5.5f, r.getBottom() - 3.0f);
    }

    static void drawWaveformIcon(juce::Graphics& g, juce::Rectangle<float> r, juce::Colour col) {
        g.setColour(col);
        juce::Path p;
        float x = r.getX(), y = r.getY(), w = r.getWidth(), h = r.getHeight();
        p.startNewSubPath(x, y + h * 0.5f);
        p.quadraticTo(x + w * 0.25f, y, x + w * 0.5f, y + h * 0.5f);
        p.quadraticTo(x + w * 0.75f, y + h, x + w, y + h * 0.5f);
        g.strokePath(p, juce::PathStrokeType(1.8f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }

    static void drawSearchIcon(juce::Graphics& g, juce::Rectangle<float> r, juce::Colour col) {
        g.setColour(col);
        float d = r.getWidth() * 0.68f;
        g.drawEllipse(r.getX(), r.getY(), d, d, 1.5f);
        juce::Path p;
        p.startNewSubPath(r.getX() + d * 0.85f, r.getY() + d * 0.85f);
        p.lineTo(r.getRight(), r.getBottom());
        g.strokePath(p, juce::PathStrokeType(1.8f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }

    // --- Button Rendering ---

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

        g.setColour(baseColour);
        g.fillRoundedRectangle(bounds, cornerRadius);

        juce::Colour outlineColour = isAccented ? juce::Colour(0xFFFF5C70) : juce::Colour(0xFF252D3D);
        if (shouldDrawButtonAsHighlighted) {
            outlineColour = outlineColour.brighter(0.25f);
        }
        g.setColour(outlineColour);
        g.drawRoundedRectangle(bounds, cornerRadius, 1.0f);
    }

    static float measureTextWidth(const juce::Font& f, const juce::String& text) {
        juce::GlyphArrangement ga;
        ga.addLineOfText(f, text, 0.0f, 0.0f);
        return ga.getBoundingBox(0, -1, true).getWidth();
    }

    void drawButtonText(juce::Graphics& g,
                        juce::TextButton& button,
                        bool /*shouldDrawButtonAsHighlighted*/,
                        bool /*shouldDrawButtonAsDown*/) override {
        auto text = button.getButtonText().trim();
        auto bounds = button.getLocalBounds().toFloat();

        bool isAccented = button.getProperties().contains("accentRed") ||
                          (button.findColour(juce::TextButton::buttonColourId) == juce::Colour(0xFFE53E3E)) ||
                          (button.findColour(juce::TextButton::buttonColourId) == juce::Colour(0xFFFF334B));

        juce::Colour textColour = isAccented ? juce::Colours::white :
                                  button.findColour(button.getToggleState() ? juce::TextButton::textColourOnId : juce::TextButton::textColourOffId);

        g.setFont(juce::Font(13.0f, juce::Font::bold));

        if (text == "Open") {
            float iconW = 14.0f, iconH = 12.0f, gap = 6.0f;
            float textW = measureTextWidth(g.getCurrentFont(), text);
            float totalW = iconW + gap + textW;
            float startX = bounds.getCentreX() - totalW * 0.5f;
            drawFolderIcon(g, juce::Rectangle<float>(startX, bounds.getCentreY() - iconH * 0.5f, iconW, iconH), textColour);
            g.setColour(textColour);
            g.drawText(text, juce::Rectangle<float>(startX + iconW + gap, bounds.getY(), textW + 4, bounds.getHeight()), juce::Justification::centredLeft);
        } else if (text == "Save") {
            float iconW = 13.0f, iconH = 13.0f, gap = 6.0f;
            float textW = measureTextWidth(g.getCurrentFont(), text);
            float totalW = iconW + gap + textW;
            float startX = bounds.getCentreX() - totalW * 0.5f;
            drawSaveIcon(g, juce::Rectangle<float>(startX, bounds.getCentreY() - iconH * 0.5f, iconW, iconH), textColour);
            g.setColour(textColour);
            g.drawText(text, juce::Rectangle<float>(startX + iconW + gap, bounds.getY(), textW + 4, bounds.getHeight()), juce::Justification::centredLeft);
        } else if (text == "Play") {
            float iconW = 11.0f, iconH = 12.0f, gap = 6.0f;
            float textW = measureTextWidth(g.getCurrentFont(), text);
            float totalW = iconW + gap + textW;
            float startX = bounds.getCentreX() - totalW * 0.5f;
            drawPlayIcon(g, juce::Rectangle<float>(startX, bounds.getCentreY() - iconH * 0.5f, iconW, iconH), textColour);
            g.setColour(textColour);
            g.drawText(text, juce::Rectangle<float>(startX + iconW + gap, bounds.getY(), textW + 4, bounds.getHeight()), juce::Justification::centredLeft);
        } else if (text == "Pause") {
            float iconW = 10.0f, iconH = 12.0f, gap = 6.0f;
            float textW = measureTextWidth(g.getCurrentFont(), text);
            float totalW = iconW + gap + textW;
            float startX = bounds.getCentreX() - totalW * 0.5f;
            drawPauseIcon(g, juce::Rectangle<float>(startX, bounds.getCentreY() - iconH * 0.5f, iconW, iconH), textColour);
            g.setColour(textColour);
            g.drawText(text, juce::Rectangle<float>(startX + iconW + gap, bounds.getY(), textW + 4, bounds.getHeight()), juce::Justification::centredLeft);
        } else if (text == "Stop") {
            float iconW = 10.0f, iconH = 10.0f, gap = 6.0f;
            float textW = measureTextWidth(g.getCurrentFont(), text);
            float totalW = iconW + gap + textW;
            float startX = bounds.getCentreX() - totalW * 0.5f;
            drawStopIcon(g, juce::Rectangle<float>(startX, bounds.getCentreY() - iconH * 0.5f, iconW, iconH), textColour);
            g.setColour(textColour);
            g.drawText(text, juce::Rectangle<float>(startX + iconW + gap, bounds.getY(), textW + 4, bounds.getHeight()), juce::Justification::centredLeft);
        } else if (text == "Export") {
            float iconW = 12.0f, iconH = 12.0f, gap = 6.0f, chevW = 8.0f;
            float textW = measureTextWidth(g.getCurrentFont(), text);
            float totalW = iconW + gap + textW + gap + chevW;
            float startX = bounds.getCentreX() - totalW * 0.5f;
            drawExportIcon(g, juce::Rectangle<float>(startX, bounds.getCentreY() - iconH * 0.5f, iconW, iconH), textColour);
            g.setColour(textColour);
            g.drawText(text, juce::Rectangle<float>(startX + iconW + gap, bounds.getY(), textW + 2, bounds.getHeight()), juce::Justification::centredLeft);
            drawChevronDown(g, juce::Rectangle<float>(startX + iconW + gap + textW + gap, bounds.getCentreY() - 2.5f, chevW, 5.0f), textColour);
        } else if (text.isEmpty() || text == "menu") {
            drawMenuIcon(g, bounds, textColour);
        } else if (text == "up") {
            drawChevronUp(g, juce::Rectangle<float>(bounds.getCentreX() - 4.0f, bounds.getCentreY() - 2.5f, 8.0f, 5.0f), textColour);
        } else if (text == "down") {
            drawChevronDown(g, juce::Rectangle<float>(bounds.getCentreX() - 4.0f, bounds.getCentreY() - 2.5f, 8.0f, 5.0f), textColour);
        } else {
            g.setColour(textColour);
            g.drawFittedText(text, bounds.toNearestInt(), juce::Justification::centred, 1);
        }
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
        auto arrowZone = juce::Rectangle<float>((float)width - 20.0f, 0.0f, 14.0f, (float)height);
        drawChevronDown(g, juce::Rectangle<float>(arrowZone.getCentreX() - 3.5f, arrowZone.getCentreY() - 2.0f, 7.0f, 4.0f), juce::Colour(0xFFA0AEC0));
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
