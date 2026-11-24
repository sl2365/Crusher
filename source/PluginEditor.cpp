#include "PluginEditor.h"
#include <BinaryData.h>
#include <algorithm>
#include <cmath>

namespace
{
const juce::Colour knobTrackColour(0xff343940);
const juce::Colour knobBodyColour(0xff2b3036);
const juce::Colour knobLabelColour(0xff8d949e);
const juce::Colour knobValueColour(knobLabelColour.brighter(0.18f));
const juce::Colour controlAccentColour(0xff65cbb9);
const juce::Colour knobIndicatorColour(0xffd2b84f);
const juce::Colour tempoSyncLedColour(0xff55c96b);
const juce::Colour linkLedColour(0xffffad42);
const juce::Colour bipolarLedColour(0xffd96b72);
const juce::Colour unipolarNegativeLedColour(0xff8f68cf);
const juce::Colour powerLedColour(0xff82d16a);
const juce::Colour presetAccentColour(0xff72b9a9);
// Theme is bipolar: image on the left, charcoal in the centre, muted
// distortion-red on the right. The dark control panels remain opaque.
const juce::Colour themeNeutralColour(0xff24292f);
const juce::Colour themeDistortionRed(0xff8e414b);

float themeImageOpacity(double themePercent) noexcept
{
    return juce::jlimit(0.0f, 1.0f,
        static_cast<float>((50.0 - themePercent) / 50.0));
}

juce::Colour themeSolidColour(double themePercent)
{
    return themeNeutralColour.interpolatedWith(themeDistortionRed,
        juce::jlimit(0.0f, 1.0f,
            static_cast<float>((themePercent - 50.0) / 50.0)));
}

constexpr auto presetFileExtension = ".crusherpreset";
constexpr auto sequencerTargetDragPrefix = "crusher-sequencer-target:";

// Use the same readable help string for a parameter label and its control.
// Keep a short tip on a single line; wrap longer help to at most two lines.
juce::String formatControlHelp(juce::String help)
{
    help = help.replace("\r", " ").trim();
    const int firstLineBreak = help.indexOfChar('\n');
    if (firstLineBreak >= 0 && ! help.substring(0, firstLineBreak).contains(" | "))
        help = help.replaceSection(firstLineBreak, 1, " | ");
    help = help.replaceCharacter('\n', ' ').trim();
    const juce::Font font(juce::FontOptions(11.5f));
    constexpr float availableWidth = 650.0f;
    if (juce::GlyphArrangement::getStringWidth(font, help) <= availableWidth)
        return help;
    // Prefer to end the first row at a word boundary, preserving NAME | text.
    int split = -1;
    for (int i = 0; i < help.length(); ++i)
        if (help[i] == ' '
            && juce::GlyphArrangement::getStringWidth(font,
                   help.substring(0, i)) <= availableWidth)
            split = i;
    if (split <= 0)
        return help;
    auto second = help.substring(split + 1).trimStart();
    while (juce::GlyphArrangement::getStringWidth(font, second)
               > availableWidth && second.length() > 5)
        second = second.dropLastCharacters(1);
    if (second != help.substring(split + 1).trimStart())
        second = second.trimEnd() + "...";
    return help.substring(0, split) + "\n" + second;
}

juce::Array<juce::File> presetFilesInFolder(const juce::File& folder)
{
    juce::Array<juce::File> files;
    if (! folder.isDirectory())
        return files;

    folder.findChildFiles(files, juce::File::findFiles, false);
    files.removeIf([](const juce::File& file)
    {
        return ! file.hasFileExtension(presetFileExtension);
    });
    std::sort(files.begin(), files.end(),
              [](const juce::File& first, const juce::File& second)
              {
                  return first.getFileNameWithoutExtension().compareNatural(
                             second.getFileNameWithoutExtension(), true) < 0;
              });
    return files;
}

juce::Path makeStarPath(juce::Rectangle<float> bounds)
{
    juce::Path path;
    const auto centre = bounds.getCentre();
    const float outerRadius = 0.5f * juce::jmin(bounds.getWidth(),
                                                bounds.getHeight());
    const float innerRadius = outerRadius * 0.43f;
    for (int point = 0; point < 10; ++point)
    {
        const float radius = (point % 2 == 0) ? outerRadius : innerRadius;
        const float angle = -juce::MathConstants<float>::halfPi
            + static_cast<float>(point)
                * juce::MathConstants<float>::pi / 5.0f;
        const juce::Point<float> position(
            centre.x + std::cos(angle) * radius,
            centre.y + std::sin(angle) * radius);
        if (point == 0)
            path.startNewSubPath(position);
        else
            path.lineTo(position);
    }
    path.closeSubPath();
    return path;
}

constexpr std::array<const char*, 23> distortionSummaries
{
    "Dense tanh clipping with strong static level compensation",
    "Open arctangent saturation with brighter, gentler harmonics",
    "Asymmetric valve-style soft saturation with even harmonics",
    "Full-wave folding followed by DC-safe filtering",
    "Signal-responsive broken-contact pops over compact saturation",
    "Sharp hard clipping charged with signal-tracked white noise",
    "Cascaded Class-A-style preamp and power-stage overdrive",
    "Uneven frequency-shaped waveshaping with nonlinear ripples",
    "Different positive and negative curves for strong even harmonics",
    "Upper-mid pre-emphasis feeding sharp ridged saturation",
    "Bit-depth and sample-rate reduction with quiet deterministic dither",
    "Short resonant comb recurrence feeding bounded saturation",
    "Compressed wavefolding with a strained asymmetric edge",
    "Sliced, upper-mid-emphasised clipping with a metallic blade",
    "Bright high-band excitation over restrained clean saturation",
    "Clean flat-threshold clipping without added noise or tone filtering",
    "High-gain asymmetric fuzz with compressed sustain and a restrained top end",
    "Mechanical and tube waveshaping with optional tone-shaping controls",
    "Asymmetric diode-inspired clipping with envelope-sensitive response and filtering",
    "Combined rate reduction, 1-16-bit quantisation, clipping, rectification and noise",
    "Companding and predictive/spectral digital codec artifacts with frame damage",
    "Frequency-selective, character-morphing saturation with dynamic harshness control",
    "Multi-stage variable-curve saturation with spectral emphasis and transient preservation"
};
constexpr std::array<const char*, 23> distortionDetails
{
    "DIST increases density and compression through a level-compensated soft curve.",
    "DIST moves smoothly from clean into open saturation without darkening the top end.",
    "DIST moves from clean warmth into rounded compression and asymmetric colour.",
    "DIST moves from the dry signal into octave-rich electronic crunch.",
    "DIST increases crack density, broken-contact edge and compact grit.",
    "DIST increases electrical bite and noise while keeping the input bounded.",
    "DIST moves from warm edge into raw drive with a restrained amp-like top end.",
    "DIST increases shifting ridges, asymmetric grit and midrange emphasis.",
    "DIST progressively separates the two polarities without leaving a DC offset.",
    "DIST intensifies high-frequency teeth and narrow, cutting harmonic ridges.",
    "DIST lowers effective resolution and update rate for increasingly coarse digital texture.",
    "DIST shortens and strengthens the resonant path for metallic, pitched colour.",
    "DIST pushes a smooth curve into folded, tense and increasingly unstable harmonics.",
    "DIST widens the dead zone and sharpens the bright, sliced transient edge.",
    "DIST adds clear upper harmonics and presence while leaving the low band comparatively intact.",
    "DIST lowers the clipping threshold for progressively harder, squared-off transients.",
    "DIST increases asymmetric clipping, dense sustain and dark post-clip fuzz colour.",
    "MECH and TUBE provide the main character; BODY, BIAS, FOLD and TONE add variation.",
    "DRIVE increases diode-style clipping; ENV and HP/LP filters shape its response.",
    "DRIVE sets the wet amount; RATE, BITS, CLIP, RECTIFY and NOISE combine digital damage.",
    "DRIVE sets the wet blend; CODEC chooses PCM, mu-law, A-law, speech or spectral loss.",
    "DRIVE sets the wet blend; CHARACTER, BODY, PRESENCE, BITE and SMOOTH suit voice and drums.",
    "DRIVE intensifies cascaded waveshapers; STAGES, VARIATION, EMPH, BIAS and PUNCH reshape them."
};
static_assert(distortionSummaries.size() == distortionDetails.size());
// A sized std::array silently fills missing initializers with null pointers.
// Catch incomplete text tables at compile time before a new model can crash
// the host when its description is appended to a JUCE String.
static_assert([]() constexpr
{
    for (const auto* summary : distortionSummaries)
        if (summary == nullptr) return false;
    for (const auto* detail : distortionDetails)
        if (detail == nullptr) return false;
    return true;
}(), "Every distortion type must have a summary and detail");

juce::Colour filterColourForId(int filterIndex, int typeId)
{
    static constexpr std::array<juce::uint32, 8> filterOneColours
    {
        0xff1479b8, 0xff159b91, 0xff3d6406, 0xffa46c3f,
        0xffc14521, 0xff5968d8, 0xff82436e, 0xff6e5fa8
    };
    static constexpr std::array<juce::uint32, 8> filterTwoColours
    {
        0xff82436e, 0xffb04378, 0xffc23d82, 0xff6e5fa8,
        0xff5968d8, 0xff159b91, 0xffa46c3f, 0xffc14521
    };
    const auto& colours = filterIndex == 2 ? filterTwoColours
                                            : filterOneColours;
    return juce::Colour(colours[static_cast<size_t>(juce::jlimit(
        0, static_cast<int>(colours.size()) - 1, typeId))]);
}

struct InterfaceSettings
{
    int scalePercent = 100;
    double themePercent = 50.0;
    bool sequencerOpen = false;
    bool sequencerAnimate = false;
};

InterfaceSettings loadInterfaceSettings()
{
    InterfaceSettings settings;
    const auto file = CrusherAudioProcessor::getSettingsDirectory()
        .getChildFile("Settings.ini");
    if (! file.existsAsFile())
        return settings;

    juce::StringArray lines;
    lines.addLines(file.loadFileAsString());
    bool inGuiSection = false;
    for (auto line : lines)
    {
        line = line.trim();
        if (line.startsWithChar('['))
        {
            inGuiSection = line.equalsIgnoreCase("[GUI]");
            continue;
        }
        if (! inGuiSection)
            continue;

        const int separator = line.indexOfChar('=');
        if (separator <= 0)
            continue;
        const auto key = line.substring(0, separator).trim();
        const auto value = line.substring(separator + 1).trim();
        if (key.equalsIgnoreCase("ScalePercent"))
            settings.scalePercent = juce::jlimit(75, 300, value.getIntValue());
        else if (key.equalsIgnoreCase("ThemePercent"))
            settings.themePercent = juce::jlimit(0.0, 100.0,
                                                  static_cast<double>(value.getFloatValue()));
        else if (key.equalsIgnoreCase("SequencerOpen"))
            settings.sequencerOpen = value.getIntValue() != 0;
        else if (key.equalsIgnoreCase("SequencerAnimate"))
            settings.sequencerAnimate = value.getIntValue() != 0;
    }
    return settings;
}

class PresetPopupList final : public juce::PopupMenu::CustomComponent,
                              private juce::ListBoxModel
{
public:
    PresetPopupList(juce::Array<juce::File> filesToUse,
                    const juce::File& currentFile,
                    std::function<void(const juce::File&)> selectionCallback)
        : juce::PopupMenu::CustomComponent(false),
          files(std::move(filesToUse)),
          onSelected(std::move(selectionCallback)),
          list("Preset quick list", this)
    {
        list.setRowHeight(rowHeight);
        list.setColour(juce::ListBox::backgroundColourId,
                       juce::Colour(0xff14181d));
        list.setColour(juce::ListBox::outlineColourId,
                       juce::Colours::transparentBlack);
        list.setOutlineThickness(0);
        addAndMakeVisible(list);
        list.updateContent();

        const int currentRow = files.indexOf(currentFile);
        if (currentRow >= 0)
        {
            list.selectRow(currentRow, false, false);
            rowToReveal = currentRow;
        }
    }

    void getIdealSize(int& idealWidth, int& idealHeight) override
    {
        idealWidth = 224;
        idealHeight = juce::jmax(1, juce::jmin(maximumVisibleRows,
                                               files.size())) * rowHeight + 4;
    }

    void resized() override
    {
        // Leave the same two-pixel frame visible as the Mode and Type menus.
        list.setBounds(getLocalBounds().reduced(2));
        if (rowToReveal >= 0)
        {
            list.scrollToEnsureRowIsOnscreen(rowToReveal);
            rowToReveal = -1;
        }
    }

private:
    int getNumRows() override
    {
        return files.size();
    }

    void paintListBoxItem(int row, juce::Graphics& g, int width, int height,
                          bool rowIsSelected) override
    {
        if (! juce::isPositiveAndBelow(row, files.size()))
            return;

        if (rowIsSelected)
        {
            g.setColour(juce::Colour(0xff2a3037));
            g.fillRoundedRectangle(3.0f, 1.0f,
                                   static_cast<float>(width - 6),
                                   static_cast<float>(height - 2), 2.0f);
        }

        g.setColour(rowIsSelected
            ? knobIndicatorColour.brighter(0.28f)
            : knobValueColour);
        g.setFont(getLookAndFeel().getPopupMenuFont());
        g.drawFittedText(files.getReference(row).getFileNameWithoutExtension(),
                         8, 0, juce::jmax(0, width - 16), height,
                         juce::Justification::centredLeft, 1, 0.75f);
    }

    void listBoxItemClicked(int row, const juce::MouseEvent&) override
    {
        chooseRow(row);
    }

    void returnKeyPressed(int row) override
    {
        chooseRow(row);
    }

    void chooseRow(int row)
    {
        if (! juce::isPositiveAndBelow(row, files.size()))
            return;

        if (onSelected)
            onSelected(files.getReference(row));
        triggerMenuItem();
    }

    static constexpr int rowHeight = 20;
    static constexpr int maximumVisibleRows = 20;
    juce::Array<juce::File> files;
    std::function<void(const juce::File&)> onSelected;
    juce::ListBox list;
    int rowToReveal = -1;
};
}

class CrusherAudioProcessorEditor::FolderTreeItem final
    : public juce::TreeViewItem
{
public:
    FolderTreeItem(juce::File directoryToUse,
                   std::function<void(const juce::File&, bool)>
                       selectionCallback,
                   std::function<void(const juce::File&, bool,
                                      juce::Point<int>)> contextCallback,
                   bool favouriteItem = false,
                   int depth = 0)
        : directory(std::move(directoryToUse)),
          onSelected(std::move(selectionCallback)),
          onContext(std::move(contextCallback)),
          isFavouriteItem(favouriteItem)
    {
        if (isFavouriteItem || depth >= 32)
            return;

        if (depth == 0)
            addSubItem(new FolderTreeItem(
                directory, onSelected, onContext, true, depth + 1));

        juce::Array<juce::File> childDirectories;
        directory.findChildFiles(childDirectories, juce::File::findDirectories,
                                 false);
        std::sort(childDirectories.begin(), childDirectories.end(),
                  [](const juce::File& first, const juce::File& second)
                  {
                      return first.getFileName().compareNatural(
                                 second.getFileName(), true) < 0;
                  });
        for (const auto& child : childDirectories)
            if (! child.isSymbolicLink())
                addSubItem(new FolderTreeItem(
                    child, onSelected, onContext, false, depth + 1));
    }

    bool mightContainSubItems() override
    {
        return getNumSubItems() > 0;
    }

    juce::String getUniqueName() const override
    {
        return isFavouriteItem
            ? directory.getFullPathName() + "::Favourites"
            : directory.getFullPathName();
    }

    int getItemHeight() const override
    {
        return 22;
    }

    void paintItem(juce::Graphics& g, int width, int height) override
    {
        if (isSelected())
        {
            g.setColour(presetAccentColour.withAlpha(0.30f));
            g.fillRoundedRectangle(0.0f, 1.0f, static_cast<float>(width),
                                   static_cast<float>(height - 2), 3.0f);
        }

        if (isFavouriteItem)
        {
            g.setColour(isSelected() ? knobIndicatorColour
                                     : knobLabelColour.brighter(0.12f));
            g.fillPath(makeStarPath({ 2.0f,
                static_cast<float>(height) * 0.5f - 6.0f,
                12.0f, 12.0f }));
        }
        else
        {
            const auto folderBounds = juce::Rectangle<float>(
                2.0f, static_cast<float>(height) * 0.5f - 5.0f,
                13.0f, 10.0f);
            g.setColour(isSelected() ? knobIndicatorColour
                                     : knobLabelColour.brighter(0.12f));
            g.fillRoundedRectangle(folderBounds, 1.5f);
            g.fillRect(folderBounds.getX() + 1.5f,
                       folderBounds.getY() - 2.0f, 5.0f, 3.0f);
        }
        g.setColour(isSelected() ? juce::Colours::white
                                 : knobValueColour);
        g.setFont(juce::Font(juce::FontOptions(11.0f)));
        g.drawFittedText(isFavouriteItem ? "Favourites"
                                         : directory.getFileName(),
                         21, 0, juce::jmax(0, width - 24), height,
                         juce::Justification::centredLeft, 1);
    }

    void itemSelectionChanged(bool isNowSelected) override
    {
        if (isNowSelected && onSelected)
            onSelected(directory, isFavouriteItem);
        repaintItem();
    }

    void itemClicked(const juce::MouseEvent& event) override
    {
        if (event.mods.isPopupMenu() && onContext)
            onContext(directory, isFavouriteItem,
                      event.getScreenPosition());
    }

    FolderTreeItem* findItemForDirectory(const juce::File& target,
                                         bool findFavourites = false)
    {
        if (isFavouriteItem == findFavourites
            && (findFavourites || directory == target))
            return this;

        for (int index = 0; index < getNumSubItems(); ++index)
            if (auto* child = dynamic_cast<FolderTreeItem*>(getSubItem(index)))
                if (auto* match = child->findItemForDirectory(
                        target, findFavourites))
                    return match;
        return nullptr;
    }

private:
    juce::File directory;
    std::function<void(const juce::File&, bool)> onSelected;
    std::function<void(const juce::File&, bool, juce::Point<int>)> onContext;
    bool isFavouriteItem = false;
};

namespace
{
void setParameterActualValue(juce::RangedAudioParameter* parameter,
                             float actualValue)
{
    if (parameter == nullptr)
        return;
    parameter->beginChangeGesture();
    parameter->setValueNotifyingHost(parameter->convertTo0to1(actualValue));
    parameter->endChangeGesture();
}
}

class CrusherAudioProcessorEditor::SequencerPanel final
    : public juce::Component
{
public:
    class StepGrid final : public juce::Component,
                           public juce::SettableTooltipClient
    {
    public:
        StepGrid(CrusherAudioProcessor& audioProcessor,
                 LookAndFeel& sharedLookAndFeelToUse,
                 LookAndFeel& compactLookAndFeelToUse)
            : processor(audioProcessor),
              sharedPopupLookAndFeel(sharedLookAndFeelToUse),
              compactPopupLookAndFeel(compactLookAndFeelToUse)
        {
            for (int step = 0; step < crusher::sequencer::stepCount; ++step)
            {
                steps[static_cast<size_t>(step)]
                    = processor.parameters.getParameter(
                        crusher::sequencer::stepParameterId(step));
                gateSteps[static_cast<size_t>(step)]
                    = processor.parameters.getParameter(
                        crusher::sequencer::gateStepParameterId(step));
            }
            start = processor.parameters.getRawParameterValue("sequencerStart");
            end = processor.parameters.getRawParameterValue("sequencerEnd");
            peak = processor.parameters.getRawParameterValue("sequencerPeak");
            move = processor.parameters.getRawParameterValue("sequencerMove");
            attack = processor.parameters.getRawParameterValue(
                "sequencerAttack");
            release = processor.parameters.getRawParameterValue(
                "sequencerRelease");
            setMouseCursor(juce::MouseCursor::CrosshairCursor);
            setTooltip("Click or drag vertically to set the sequencer steps");
        }

        void setAccent(juce::Colour colour)
        {
            accent = colour;
            repaint();
        }

        void setPolarityMode(int newMode)
        {
            newMode = juce::jlimit(0, 2, newMode);
            if (polarityMode == newMode)
                return;

            polarityMode = newMode;
            repaint();
        }

        void paint(juce::Graphics& graphics) override
        {
            const auto whole = getLocalBounds().toFloat();
            graphics.setColour(juce::Colour(0xff10151a));
            graphics.fillRoundedRectangle(whole, 5.0f);
            const auto graph = graphBounds();
            const float columnWidth = graph.getWidth()
                                    / crusher::sequencer::stepCount;
            const bool isBipolar = polarityMode == 1;
            const bool isNegative = polarityMode == 2;
            const int first = juce::jlimit(0, 30,
                juce::roundToInt(start != nullptr ? start->load() : 1.0f) - 1);
            const int last = juce::jlimit(first + 1, 31,
                juce::roundToInt(end != nullptr ? end->load() : 32.0f) - 1);
            const int active = processor.getActiveSequencerStep();

            std::array<float, crusher::sequencer::stepCount> values {};
            float patternMinimum = 1.0f;
            float patternMaximum = 0.0f;
            for (int step = 0; step < crusher::sequencer::stepCount; ++step)
            {
                const auto index = static_cast<size_t>(step);
                values[index] = steps[index] != nullptr
                    ? steps[index]->convertFrom0to1(steps[index]->getValue())
                    : 1.0f;
                patternMinimum = juce::jmin(patternMinimum, values[index]);
                patternMaximum = juce::jmax(patternMaximum, values[index]);
            }
            const float peakValue = peak != nullptr ? peak->load() : 0.0f;
            const float moveValue = move != nullptr ? move->load() : 0.0f;
            for (auto& value : values)
                value = crusher::sequencer::applyDepthOffset(
                    value, patternMinimum, patternMaximum,
                    peakValue, moveValue, isBipolar);

            const auto displayedMagnitude = [isBipolar](float canonical)
            {
                return isBipolar ? juce::jlimit(0.0f, 1.0f, canonical)
                    : juce::jlimit(0.0f, 1.0f, 2.0f * canonical - 1.0f);
            };
            const auto displayY = [graph, isBipolar, isNegative,
                                   &displayedMagnitude](float canonical)
            {
                const float displayed = displayedMagnitude(canonical);
                if (isBipolar)
                    return graph.getBottom() - graph.getHeight() * displayed;
                return isNegative ? graph.getY()
                                      + graph.getHeight() * displayed
                                  : graph.getBottom()
                                      - graph.getHeight() * displayed;
            };
            if (isBipolar)
            {
                graphics.setColour(accent.withAlpha(0.22f));
                graphics.drawHorizontalLine(
                    juce::roundToInt(graph.getCentreY()), graph.getX(),
                    graph.getRight());
            }

            graphics.setFont(juce::FontOptions(10.5f, juce::Font::bold));
            for (int step = 0; step < crusher::sequencer::stepCount; ++step)
            {
                const float x = graph.getX() + columnWidth * step;
                const auto cell = juce::Rectangle<float>(
                    x, graph.getY(), columnWidth, graph.getHeight());
                if ((step / 4) % 2 != 0)
                {
                    graphics.setColour(juce::Colour(0x0dffffff));
                    graphics.fillRect(cell);
                }
                if (step == active)
                {
                    graphics.setColour(accent.withAlpha(0.22f));
                    graphics.fillRect(cell);
                }
                const float displayed = displayedMagnitude(
                    values[static_cast<size_t>(step)]);
                graphics.setColour(accent.withAlpha(
                    step == active ? 0.95f : 0.68f));
                if (isBipolar)
                {
                    const float y = graph.getBottom()
                                  - graph.getHeight() * displayed;
                    graphics.fillRect(x + 1.0f,
                        juce::jmin(y, graph.getCentreY()),
                        juce::jmax(1.0f, columnWidth - 2.0f),
                        std::abs(y - graph.getCentreY()));
                }
                else
                {
                    const float y = displayY(values[static_cast<size_t>(step)]);
                    graphics.fillRect(x + 1.0f,
                        isNegative ? graph.getY() : y,
                        juce::jmax(1.0f, columnWidth - 2.0f),
                        juce::jmax(0.0f, graph.getHeight() * displayed));
                }
                graphics.setColour(juce::Colour(0x553b4650));
                graphics.drawVerticalLine(juce::roundToInt(x + columnWidth),
                                           graph.getY(), graph.getBottom());
                if (step % 4 == 0)
                {
                    graphics.setColour(knobLabelColour);
                    graphics.drawText(juce::String(step + 1),
                        juce::roundToInt(x), 0,
                        juce::roundToInt(columnWidth * 4.0f), 12,
                        juce::Justification::centredLeft);
                }
            }

            const float attackValue = juce::jlimit(
                0.0f, 1.0f, attack != nullptr ? attack->load() : 0.0f);
            const float releaseValue = juce::jlimit(
                0.0f, 1.0f, release != nullptr ? release->load() : 0.0f);
            juce::Path envelope;
            constexpr int pointsPerStep = 16;
            for (int step = 0; step < crusher::sequencer::stepCount; ++step)
            {
                const int previous = (step - 1
                    + crusher::sequencer::stepCount)
                    % crusher::sequencer::stepCount;
                for (int point = 0; point <= pointsPerStep; ++point)
                {
                    const float position = static_cast<float>(point)
                                         / static_cast<float>(pointsPerStep);
                    const float value = crusher::sequencer::transition(
                        values[static_cast<size_t>(previous)],
                        values[static_cast<size_t>(step)], position,
                        attackValue, releaseValue);
                    const float x = graph.getX() + columnWidth
                        * (static_cast<float>(step) + position);
                    const float y = displayY(value);
                    if (step == 0 && point == 0)
                        envelope.startNewSubPath(x, y);
                    else
                        envelope.lineTo(x, y);
                }
            }
            graphics.setColour(juce::Colour(0xb0000000));
            graphics.strokePath(envelope, juce::PathStrokeType(3.2f));
            graphics.setColour(accent.brighter(0.50f));
            graphics.strokePath(envelope, juce::PathStrokeType(1.35f));
            for (int step = 0; step < crusher::sequencer::stepCount; ++step)
            {
                if (step >= first && step <= last)
                    continue;
                const float x = graph.getX() + columnWidth * step;
                graphics.setColour(juce::Colour(0xb0101419));
                graphics.fillRect(x, graph.getY(), columnWidth,
                                  graph.getHeight());
            }
            graphics.setColour(accent.withAlpha(0.72f));
            graphics.drawRoundedRectangle(graph, 4.0f, 1.1f);

            const auto gateRow = gateBounds();
            const float gateColumnWidth = gateRow.getWidth()
                                        / crusher::sequencer::stepCount;
            for (int step = 0; step < crusher::sequencer::stepCount; ++step)
            {
                const auto index = static_cast<size_t>(step);
                const float x = gateRow.getX() + gateColumnWidth * step;
                const auto cell = juce::Rectangle<float>(
                    x + 1.0f, gateRow.getY(),
                    juce::jmax(1.0f, gateColumnWidth - 2.0f),
                    gateRow.getHeight());
                const auto mode = gateSteps[index] != nullptr
                    ? crusher::sequencer::gateStepModeFromChoice(
                        gateSteps[index]->convertFrom0to1(
                            gateSteps[index]->getValue()))
                    : crusher::sequencer::GateStepMode::rest;
                const auto cellAccent = accent.withAlpha(
                    step == active ? 0.95f : 0.72f);

                graphics.setColour(juce::Colour(0xff111820));
                graphics.fillRoundedRectangle(cell, 2.0f);
                if (mode == crusher::sequencer::GateStepMode::shortStep)
                {
                    auto fill = cell.reduced(2.0f);
                    fill.setWidth(fill.getWidth() * 0.5f);
                    graphics.setColour(cellAccent);
                    graphics.fillRoundedRectangle(fill, 1.5f);
                }
                else if (mode == crusher::sequencer::GateStepMode::longStep
                         || mode == crusher::sequencer::GateStepMode::rest)
                {
                    graphics.setColour(cellAccent);
                    graphics.fillRoundedRectangle(cell.reduced(2.0f), 1.5f);
                    if (mode == crusher::sequencer::GateStepMode::rest)
                    {
                        const auto centre = cell.getCentre();
                        const float halfWidth = cell.getWidth() * 0.22f;
                        const float halfHeight = cell.getHeight() * 0.19f;
                        juce::Path arrow;
                        arrow.startNewSubPath(centre.x - halfWidth, centre.y);
                        arrow.lineTo(centre.x + halfWidth, centre.y);
                        arrow.startNewSubPath(centre.x + halfWidth, centre.y);
                        arrow.lineTo(centre.x + halfWidth - halfHeight,
                                     centre.y - halfHeight);
                        arrow.startNewSubPath(centre.x + halfWidth, centre.y);
                        arrow.lineTo(centre.x + halfWidth - halfHeight,
                                     centre.y + halfHeight);
                        graphics.setColour(juce::Colour(0xff10151a)
                                               .withAlpha(0.88f));
                        graphics.strokePath(
                            arrow, juce::PathStrokeType(1.25f));
                    }
                }

                graphics.setColour(
                    mode == crusher::sequencer::GateStepMode::off
                        ? knobTrackColour : cellAccent.brighter(0.18f));
                graphics.drawRoundedRectangle(
                    cell, 2.0f, step == active ? 1.4f : 0.8f);
                if (step < first || step > last)
                {
                    graphics.setColour(juce::Colour(0xb0101419));
                    graphics.fillRoundedRectangle(cell, 2.0f);
                }
            }
        }

        void mouseDown(const juce::MouseEvent& event) override
        {
            if (event.mods.isPopupMenu())
            {
                if (gateBounds().contains(event.position))
                    showGateMenu();
                else
                    showPatternMenu();
                return;
            }
            lastEditedCell = -1;
            editingGateRow = gateBounds().contains(event.position);
            if (editingGateRow)
            {
                cycleGateMode(event.position);
                return;
            }
            editAt(event.position);
        }

        void mouseDrag(const juce::MouseEvent& event) override
        {
            if (editingGateRow)
                return;
            editAt(event.position);
        }

        void mouseUp(const juce::MouseEvent&) override
        {
            lastEditedCell = -1;
            editingGateRow = false;
        }

        void applyWaveform(crusher::sequencer::Waveform waveform)
        {
            const bool isBipolar = polarityMode == 1;
            const auto values = crusher::sequencer::makeWaveform(
                waveform, isBipolar);
            for (int step = 0; step < crusher::sequencer::stepCount; ++step)
                setParameterActualValue(steps[static_cast<size_t>(step)],
                                        values[static_cast<size_t>(step)]);
            repaint();
        }

        void nudge(int direction)
        {
            std::array<float, crusher::sequencer::stepCount> values {};
            for (int step = 0; step < crusher::sequencer::stepCount; ++step)
            {
                const auto index = static_cast<size_t>(step);
                values[index] = steps[index]->convertFrom0to1(
                    steps[index]->getValue());
            }
            for (int step = 0; step < crusher::sequencer::stepCount; ++step)
            {
                const int source = (step - direction
                    + crusher::sequencer::stepCount)
                    % crusher::sequencer::stepCount;
                setParameterActualValue(steps[static_cast<size_t>(step)],
                                        values[static_cast<size_t>(source)]);
            }
            repaint();
        }

        void nudgeGates(int direction)
        {
            std::array<float, crusher::sequencer::stepCount> values {};
            for (int step = 0; step < crusher::sequencer::stepCount; ++step)
            {
                const auto index = static_cast<size_t>(step);
                values[index] = gateSteps[index]->convertFrom0to1(
                    gateSteps[index]->getValue());
            }
            for (int step = 0; step < crusher::sequencer::stepCount; ++step)
            {
                const int source = (step - direction
                    + crusher::sequencer::stepCount)
                    % crusher::sequencer::stepCount;
                setParameterActualValue(gateSteps[static_cast<size_t>(step)],
                                        values[static_cast<size_t>(source)]);
            }
            repaint();
        }

        std::function<void()> onSaveRequested;

    private:
        juce::Rectangle<float> graphBounds() const
        {
            const auto gate = gateBounds();
            auto graph = getLocalBounds().toFloat().reduced(5.0f)
                .withTrimmedTop(12.0f);
            graph.setBottom(gate.getY() - 4.0f);
            return graph;
        }

        juce::Rectangle<float> gateBounds() const
        {
            const auto inner = getLocalBounds().toFloat().reduced(5.0f);
            const float columnWidth = inner.getWidth()
                                    / crusher::sequencer::stepCount;
            const float height = juce::jmax(
                8.0f, std::floor(columnWidth * 0.60f));
            return { inner.getX(), inner.getBottom() - height,
                     inner.getWidth(), height };
        }

        void cycleGateMode(juce::Point<float> position)
        {
            const auto row = gateBounds();
            const int step = juce::jlimit(
                0, crusher::sequencer::stepCount - 1,
                static_cast<int>(std::floor(
                    (position.x - row.getX()) / row.getWidth()
                        * crusher::sequencer::stepCount)));
            auto* parameter = gateSteps[static_cast<size_t>(step)];
            if (parameter == nullptr)
                return;
            const int current = static_cast<int>(
                crusher::sequencer::gateStepModeFromChoice(
                    parameter->convertFrom0to1(parameter->getValue())));
            const int next = (current + 1)
                % (static_cast<int>(
                    crusher::sequencer::GateStepMode::rest) + 1);
            setParameterActualValue(parameter, static_cast<float>(next));
            repaint();
        }

        void editAt(juce::Point<float> position)
        {
            const auto graph = graphBounds();
            if (! graph.contains(position))
                return;
            const float column = (position.x - graph.getX()) / graph.getWidth()
                               * crusher::sequencer::stepCount;
            const int step = juce::jlimit(0,
                crusher::sequencer::stepCount - 1,
                static_cast<int>(std::floor(column)));
            const bool isBipolar = polarityMode == 1;
            const bool isNegative = polarityMode == 2;
            const float displayed = isBipolar
                ? juce::jlimit(0.0f, 1.0f,
                    (graph.getBottom() - position.y) / graph.getHeight())
                : juce::jlimit(0.0f, 1.0f,
                    (isNegative ? position.y - graph.getY()
                                : graph.getBottom() - position.y)
                        / graph.getHeight());
            const float canonical = isBipolar
                ? displayed : 0.5f + 0.5f * displayed;
            setParameterActualValue(steps[static_cast<size_t>(step)],
                                    canonical);
            lastEditedCell = step;
            repaint();
        }

        void showPatternMenu()
        {
            compactPopupLookAndFeel.setPopupBorderColour(
                sharedPopupLookAndFeel.getPopupBorderColour());
            juce::PopupMenu menu;
            menu.setLookAndFeel(&compactPopupLookAndFeel);
            menu.addItem(1, "Zero");
            menu.addItem(2, "Max");
            menu.addItem(3, "Min");
            menu.addItem(4, "Random");
            menu.addSeparator();
            menu.addItem(5, "Reset");
            menu.addItem(6, "Save Sequence...");
            menu.addSeparator();
            menu.addItem(7, "Copy");
            menu.addItem(8, "Paste", clipboardValid);
            const juce::Component::SafePointer<StepGrid> safeThis(this);
            menu.showMenuAsync(
                juce::PopupMenu::Options()
                    .withTargetComponent(this)
                    .withMinimumWidth(128)
                    .withStandardItemHeight(20),
                [safeThis](int result)
                {
                    if (safeThis == nullptr || result == 0)
                        return;
                    auto& grid = *safeThis;
                    const bool isBipolar = grid.polarityMode == 1;
                    if (result >= 1 && result <= 4)
                    {
                        juce::Random random;
                        for (auto* parameter : grid.steps)
                        {
                            const float value = result == 1 ? 0.5f
                                : result == 2 ? 1.0f
                                : result == 3 ? (isBipolar ? 0.0f : 0.5f)
                                : (isBipolar ? random.nextFloat()
                                             : 0.5f
                                                 + 0.5f * random.nextFloat());
                            setParameterActualValue(parameter, value);
                        }
                    }
                    else if (result == 5)
                    {
                        for (int step = 0;
                             step < crusher::sequencer::stepCount; ++step)
                        {
                            auto* parameter = grid.steps[static_cast<size_t>(step)];
                            const auto saved = grid.processor.getSavedParameterValue(
                                crusher::sequencer::stepParameterId(step));
                            const float value = saved.has_value()
                                ? *saved
                                : parameter->convertFrom0to1(
                                      parameter->getDefaultValue());
                            setParameterActualValue(parameter, value);
                        }
                    }
                    else if (result == 6
                             && grid.onSaveRequested != nullptr)
                    {
                        grid.onSaveRequested();
                    }
                    else if (result == 7)
                    {
                        for (size_t index = 0; index < grid.steps.size(); ++index)
                            clipboard[index]
                                = grid.steps[index]->convertFrom0to1(
                                    grid.steps[index]->getValue());
                        clipboardValid = true;
                    }
                    else if (result == 8 && clipboardValid)
                    {
                        for (size_t index = 0; index < grid.steps.size(); ++index)
                            setParameterActualValue(grid.steps[index],
                                                    clipboard[index]);
                    }
                    grid.repaint();
                });
        }

        void showGateMenu()
        {
            compactPopupLookAndFeel.setPopupBorderColour(
                sharedPopupLookAndFeel.getPopupBorderColour());
            juce::PopupMenu menu;
            menu.setLookAndFeel(&compactPopupLookAndFeel);
            menu.addItem(1, "Short");
            menu.addItem(2, "Long");
            menu.addItem(3, "Rest");
            menu.addItem(4, "Off");
            menu.addItem(5, "Random");
            menu.addSeparator();
            menu.addItem(6, "Reset");
            menu.addSeparator();
            menu.addItem(7, "Copy");
            menu.addItem(8, "Paste", gateClipboardValid);
            const juce::Component::SafePointer<StepGrid> safeThis(this);
            menu.showMenuAsync(
                juce::PopupMenu::Options()
                    .withTargetComponent(this)
                    .withMinimumWidth(128)
                    .withStandardItemHeight(20),
                [safeThis](int result)
                {
                    if (safeThis == nullptr || result == 0)
                        return;
                    auto& grid = *safeThis;
                    if (result >= 1 && result <= 4)
                    {
                        const std::array<crusher::sequencer::GateStepMode, 4>
                            modes
                            {
                                crusher::sequencer::GateStepMode::shortStep,
                                crusher::sequencer::GateStepMode::longStep,
                                crusher::sequencer::GateStepMode::rest,
                                crusher::sequencer::GateStepMode::off
                            };
                        const auto mode = modes[static_cast<size_t>(result - 1)];
                        for (auto* parameter : grid.gateSteps)
                            setParameterActualValue(
                                parameter, static_cast<float>(mode));
                    }
                    else if (result == 5)
                    {
                        juce::Random random;
                        for (auto* parameter : grid.gateSteps)
                            setParameterActualValue(parameter,
                                static_cast<float>(random.nextInt(4)));
                    }
                    else if (result == 6)
                    {
                        for (int step = 0;
                             step < crusher::sequencer::stepCount; ++step)
                        {
                            auto* parameter = grid.gateSteps[
                                static_cast<size_t>(step)];
                            const auto saved = grid.processor
                                .getSavedParameterValue(
                                    crusher::sequencer::gateStepParameterId(
                                        step));
                            const float value = saved.has_value()
                                ? *saved
                                : parameter->convertFrom0to1(
                                      parameter->getDefaultValue());
                            setParameterActualValue(parameter, value);
                        }
                    }
                    else if (result == 7)
                    {
                        for (size_t index = 0;
                             index < grid.gateSteps.size(); ++index)
                            gateClipboard[index] = grid.gateSteps[index]
                                ->convertFrom0to1(
                                    grid.gateSteps[index]->getValue());
                        gateClipboardValid = true;
                    }
                    else if (result == 8 && gateClipboardValid)
                    {
                        for (size_t index = 0;
                             index < grid.gateSteps.size(); ++index)
                            setParameterActualValue(grid.gateSteps[index],
                                                    gateClipboard[index]);
                    }
                    grid.repaint();
                });
        }

        CrusherAudioProcessor& processor;
        LookAndFeel& sharedPopupLookAndFeel;
        LookAndFeel& compactPopupLookAndFeel;
        std::array<juce::RangedAudioParameter*,
                   crusher::sequencer::stepCount> steps {};
        std::array<juce::RangedAudioParameter*,
                   crusher::sequencer::stepCount> gateSteps {};
        std::atomic<float>* start = nullptr;
        std::atomic<float>* end = nullptr;
        std::atomic<float>* peak = nullptr;
        std::atomic<float>* move = nullptr;
        std::atomic<float>* attack = nullptr;
        std::atomic<float>* release = nullptr;
        juce::Colour accent { controlAccentColour };
        int polarityMode = 0;
        int lastEditedCell = -1;
        bool editingGateRow = false;
        inline static std::array<float,
                                 crusher::sequencer::stepCount> clipboard {};
        inline static bool clipboardValid = false;
        inline static std::array<float,
                                 crusher::sequencer::stepCount>
            gateClipboard {};
        inline static bool gateClipboardValid = false;
    };

    class TargetList final : public juce::Component,
                             public juce::DragAndDropTarget,
                             public juce::SettableTooltipClient
    {
    public:
        TargetList(CrusherAudioProcessor& audioProcessor,
                   std::function<void(const juce::String&)> helpCallback)
            : processor(audioProcessor), showHelp(std::move(helpCallback))
        {
            for (int slot = 0; slot < crusher::sequencer::targetSlotCount;
                 ++slot)
            {
                choices[static_cast<size_t>(slot)]
                    = processor.parameters.getParameter(
                        crusher::sequencer::targetParameterId(slot));
                enabled[static_cast<size_t>(slot)]
                    = processor.parameters.getParameter(
                        crusher::sequencer::targetEnabledParameterId(slot));
                depths[static_cast<size_t>(slot)]
                    = processor.parameters.getParameter(
                        crusher::sequencer::targetDepthParameterId(slot));
            }
            setTooltip("Drop a Crusher knob label here. The small knob sets that target's individual modulation depth.");
        }

        bool isInterestedInDragSource(const SourceDetails& details) override
        {
            const auto description = details.description.toString();
            return description.startsWith(sequencerTargetDragPrefix)
                && crusher::sequencer::targetChoiceForParameter(
                    description.fromFirstOccurrenceOf(
                        sequencerTargetDragPrefix, false, false)) > 0;
        }

        void itemDragEnter(const SourceDetails&) override
        {
            dragOver = true;
            repaint();
        }

        void itemDragExit(const SourceDetails&) override
        {
            dragOver = false;
            repaint();
        }

        void itemDropped(const SourceDetails& details) override
        {
            dragOver = false;
            const int requested = crusher::sequencer::targetChoiceForParameter(
                details.description.toString().fromFirstOccurrenceOf(
                    sequencerTargetDragPrefix, false, false));
            if (requested <= 0)
                return;
            int freeSlot = -1;
            for (int slot = 0; slot < crusher::sequencer::targetSlotCount;
                 ++slot)
            {
                const int existing = choiceForSlot(slot);
                if (existing == requested)
                {
                    setParameterActualValue(enabled[static_cast<size_t>(slot)], 1.0f);
                    repaint();
                    return;
                }
                if (existing == 0 && freeSlot < 0)
                    freeSlot = slot;
            }
            if (freeSlot >= 0)
            {
                setParameterActualValue(choices[static_cast<size_t>(freeSlot)],
                                        static_cast<float>(requested));
                setParameterActualValue(enabled[static_cast<size_t>(freeSlot)],
                                        1.0f);
            }
            repaint();
        }

        void paint(juce::Graphics& graphics) override
        {
            graphics.setColour(juce::Colour(0xff10151a));
            graphics.fillRoundedRectangle(getLocalBounds().toFloat(), 5.0f);
            graphics.setColour(dragOver
                ? controlAccentColour.brighter(0.25f)
                : controlAccentColour.withAlpha(0.65f));
            graphics.drawRoundedRectangle(getLocalBounds().toFloat().reduced(0.5f),
                                           5.0f, dragOver ? 2.0f : 1.0f);
            graphics.setColour(knobLabelColour);
            graphics.setFont(juce::FontOptions(10.5f, juce::Font::bold));
            graphics.drawText("TARGETS", 5, 3, getWidth() - 10, 14,
                              juce::Justification::centred);
            graphics.setColour(knobIndicatorColour);
            graphics.setFont(juce::FontOptions(12.5f, juce::Font::bold));
            graphics.drawText("!!!", randomButtonBounds(),
                              juce::Justification::centred, false);
            const auto slots = assignedSlots();
            const auto depthColour = filterColourForId(1, 0);
            if (slots.empty())
            {
                graphics.setColour(knobLabelColour.darker(0.25f));
                graphics.setFont(juce::FontOptions(10.5f));
                graphics.drawFittedText("DROP PARAMETER LABELS HERE",
                    8, 28, getWidth() - 16, getHeight() - 36,
                    juce::Justification::centred, 2);
                return;
            }
            const int visibleRows = juce::jmax(1, (getHeight() - 20) / rowHeight);
            const int maximumFirst = juce::jmax(
                0, static_cast<int>(slots.size()) - visibleRows);
            firstVisibleRow = juce::jlimit(0, maximumFirst, firstVisibleRow);
            for (int visibleRow = 0;
                 visibleRow < visibleRows
                    && firstVisibleRow + visibleRow
                        < static_cast<int>(slots.size());
                 ++visibleRow)
            {
                const int slot = slots[static_cast<size_t>(
                    firstVisibleRow + visibleRow)];
                const int y = 20 + visibleRow * rowHeight;
                const bool isEnabled = enabledForSlot(slot);
                graphics.setColour(isEnabled
                                             ? controlAccentColour.withAlpha(0.18f)
                                             : juce::Colour(0xff171c21));
                graphics.fillRoundedRectangle(4.0f, static_cast<float>(y),
                    static_cast<float>(getWidth() - 8),
                    static_cast<float>(rowHeight - 2), 2.5f);
                graphics.setColour(isEnabled ? controlAccentColour
                                             : knobTrackColour);
                graphics.fillEllipse(8.0f, static_cast<float>(y + 6), 6.0f, 6.0f);

                const auto knob = depthBoundsForVisibleRow(visibleRow).toFloat();
                const float rowAlpha = isEnabled ? 1.0f : 0.42f;
                graphics.setColour(juce::Colour(0xff090c0f).withMultipliedAlpha(
                    rowAlpha));
                graphics.fillEllipse(knob);
                graphics.setColour(depthColour.withAlpha(
                    0.42f * rowAlpha));
                graphics.drawEllipse(knob.reduced(0.5f), 1.0f);
                const float depth = depthForSlot(slot);
                if (depth > 0.0001f)
                {
                    constexpr float startAngle =
                        juce::MathConstants<float>::pi * 1.20f;
                    constexpr float arcLength =
                        juce::MathConstants<float>::pi * 1.60f;
                    juce::Path arc;
                    const auto arcBounds = knob.reduced(2.5f);
                    arc.addCentredArc(
                        arcBounds.getCentreX(), arcBounds.getCentreY(),
                        arcBounds.getWidth() * 0.5f,
                        arcBounds.getHeight() * 0.5f, 0.0f,
                        startAngle, startAngle + arcLength * depth, true);
                    graphics.setColour(depthColour.withAlpha(
                        0.90f * rowAlpha));
                    graphics.strokePath(
                        arc, juce::PathStrokeType(
                            1.7f, juce::PathStrokeType::curved,
                            juce::PathStrokeType::rounded));
                }

                graphics.setColour(isEnabled ? knobValueColour
                                             : knobLabelColour.darker(0.35f));
                graphics.setFont(juce::FontOptions(10.5f,
                    isEnabled ? juce::Font::bold : juce::Font::plain));
                graphics.drawFittedText(crusher::sequencer::targetName(
                        choiceForSlot(slot)), 38, y + 1, getWidth() - 61,
                    rowHeight - 4, juce::Justification::centredLeft, 1);
                graphics.setColour(knobLabelColour);
                const float closeX = static_cast<float>(getWidth() - 12);
                const float closeY = static_cast<float>(y)
                                   + 0.5f * static_cast<float>(rowHeight - 2);
                graphics.drawLine(closeX - 3.0f, closeY - 3.0f,
                                  closeX + 3.0f, closeY + 3.0f, 1.25f);
                graphics.drawLine(closeX + 3.0f, closeY - 3.0f,
                                  closeX - 3.0f, closeY + 3.0f, 1.25f);
            }
            if (canScrollUp())
            {
                graphics.setColour(controlAccentColour);
                const auto bounds = scrollUpBounds().toFloat();
                const auto centre = bounds.getCentre();
                graphics.drawLine(centre.x - 4.0f, centre.y + 2.0f,
                                  centre.x, centre.y - 2.0f, 1.6f);
                graphics.drawLine(centre.x, centre.y - 2.0f,
                                  centre.x + 4.0f, centre.y + 2.0f, 1.6f);
            }
            if (canScrollDown())
            {
                graphics.setColour(controlAccentColour);
                const auto bounds = scrollDownBounds().toFloat();
                const auto centre = bounds.getCentre();
                graphics.drawLine(centre.x - 4.0f, centre.y - 2.0f,
                                  centre.x, centre.y + 2.0f, 1.6f);
                graphics.drawLine(centre.x, centre.y + 2.0f,
                                  centre.x + 4.0f, centre.y - 2.0f, 1.6f);
            }
        }

        void mouseDown(const juce::MouseEvent& event) override
        {
            if (randomButtonBounds().contains(event.getPosition()))
            {
                randomiseTargets();
                return;
            }
            if (canScrollUp() && scrollUpBounds().contains(event.getPosition()))
            {
                --firstVisibleRow;
                repaint();
                return;
            }
            if (canScrollDown()
                && scrollDownBounds().contains(event.getPosition()))
            {
                ++firstVisibleRow;
                repaint();
                return;
            }
            if (event.y < 20)
                return;
            const int row = (event.y - 20) / rowHeight + firstVisibleRow;
            const auto slots = assignedSlots();
            if (! juce::isPositiveAndBelow(row,
                                           static_cast<int>(slots.size())))
                return;
            const int slot = slots[static_cast<size_t>(row)];
            const int visibleRow = row - firstVisibleRow;
            if (depthBoundsForVisibleRow(visibleRow).contains(event.getPosition()))
            {
                draggedDepthParameter = depths[static_cast<size_t>(slot)];
                if (draggedDepthParameter != nullptr)
                {
                    dragStartDepth = depthForSlot(slot);
                    dragStartY = event.y;
                    draggedDepthParameter->beginChangeGesture();
                }
            }
            else if (event.x >= getWidth() - 24)
                setParameterActualValue(choices[static_cast<size_t>(slot)], 0.0f);
            else
                setParameterActualValue(enabled[static_cast<size_t>(slot)],
                                        enabledForSlot(slot) ? 0.0f : 1.0f);
            repaint();
        }

        void mouseDrag(const juce::MouseEvent& event) override
        {
            if (draggedDepthParameter == nullptr)
                return;

            const float sensitivity = event.mods.isShiftDown() ? 360.0f
                                                                : 120.0f;
            const float depth = juce::jlimit(
                0.0f, 1.0f,
                dragStartDepth
                    + static_cast<float>(dragStartY - event.y) / sensitivity);
            draggedDepthParameter->setValueNotifyingHost(
                draggedDepthParameter->convertTo0to1(depth));
            setDepthTooltip(depth);
            repaint();
        }

        void mouseUp(const juce::MouseEvent&) override
        {
            if (draggedDepthParameter != nullptr)
            {
                draggedDepthParameter->endChangeGesture();
                draggedDepthParameter = nullptr;
            }
        }

        void mouseDoubleClick(const juce::MouseEvent& event) override
        {
            const int row = (event.y - 20) / rowHeight + firstVisibleRow;
            const auto slots = assignedSlots();
            if (event.y < 20
                || ! juce::isPositiveAndBelow(
                    row, static_cast<int>(slots.size())))
                return;

            const int visibleRow = row - firstVisibleRow;
            if (! depthBoundsForVisibleRow(visibleRow).contains(
                    event.getPosition()))
                return;

            auto* parameter = depths[static_cast<size_t>(
                slots[static_cast<size_t>(row)])];
            if (parameter == nullptr)
                return;
            if (draggedDepthParameter != nullptr)
            {
                draggedDepthParameter->endChangeGesture();
                draggedDepthParameter = nullptr;
            }
            parameter->beginChangeGesture();
            parameter->setValueNotifyingHost(parameter->getDefaultValue());
            parameter->endChangeGesture();
            setDepthTooltip(parameter->convertFrom0to1(
                parameter->getValue()));
            repaint();
        }

        void mouseMove(const juce::MouseEvent& event) override
        {
            if (randomButtonBounds().contains(event.getPosition()))
            {
                setMouseCursor(juce::MouseCursor::PointingHandCursor);
                const auto tooltip =
                    "Randomise target assignments, enabled states and individual Depths";
                setTooltip(tooltip);
                if (showHelp != nullptr)
                    showHelp("RANDOMISE TARGETS\n" + juce::String(tooltip));
                return;
            }
            if ((canScrollUp()
                    && scrollUpBounds().contains(event.getPosition()))
                || (canScrollDown()
                    && scrollDownBounds().contains(event.getPosition())))
            {
                setMouseCursor(juce::MouseCursor::PointingHandCursor);
                const bool upwards = scrollUpBounds().contains(
                    event.getPosition());
                const auto tooltip = upwards ? "Show earlier targets"
                                             : "Show later targets";
                setTooltip(tooltip);
                if (showHelp != nullptr)
                    showHelp("TARGETS\n" + juce::String(tooltip));
                return;
            }

            const int row = (event.y - 20) / rowHeight + firstVisibleRow;
            const auto slots = assignedSlots();
            const int visibleRow = row - firstVisibleRow;
            if (event.y >= 20
                && juce::isPositiveAndBelow(
                    row, static_cast<int>(slots.size()))
                && depthBoundsForVisibleRow(visibleRow).contains(
                    event.getPosition()))
            {
                setMouseCursor(juce::MouseCursor::UpDownResizeCursor);
                setDepthTooltip(depthForSlot(
                    slots[static_cast<size_t>(row)]));
            }
            else
            {
                setMouseCursor(juce::MouseCursor::NormalCursor);
                setTooltip("Drop an Crusher knob label here. The small knob sets that target's individual modulation depth.");
            }
        }

        void mouseWheelMove(const juce::MouseEvent& event,
                            const juce::MouseWheelDetails& wheel) override
        {
            const auto slots = assignedSlots();
            const int row = (event.y - 20) / rowHeight + firstVisibleRow;
            const int visibleRow = row - firstVisibleRow;
            if (event.y >= 20
                && juce::isPositiveAndBelow(
                    row, static_cast<int>(slots.size()))
                && depthBoundsForVisibleRow(visibleRow).contains(
                    event.getPosition()))
            {
                auto* parameter = depths[static_cast<size_t>(
                    slots[static_cast<size_t>(row)])];
                if (parameter != nullptr)
                {
                    const float depth = juce::jlimit(
                        0.0f, 1.0f,
                        parameter->convertFrom0to1(parameter->getValue())
                            + wheel.deltaY * 0.12f);
                    parameter->beginChangeGesture();
                    parameter->setValueNotifyingHost(
                        parameter->convertTo0to1(depth));
                    parameter->endChangeGesture();
                    setDepthTooltip(depth);
                    repaint();
                }
                return;
            }

            const int visibleRows = juce::jmax(1, (getHeight() - 20) / rowHeight);
            const int maximumFirst = juce::jmax(
                0, static_cast<int>(slots.size()) - visibleRows);
            firstVisibleRow = juce::jlimit(
                0, maximumFirst,
                firstVisibleRow + (wheel.deltaY > 0.0f ? -1 : 1));
            repaint();
        }

    private:
        void randomiseTargets()
        {
            juce::Random random;
            const auto randomise = [&random](juce::RangedAudioParameter* parameter)
            {
                if (parameter == nullptr)
                    return;
                parameter->beginChangeGesture();
                parameter->setValueNotifyingHost(random.nextFloat());
                parameter->endChangeGesture();
            };

            for (int slot = 0; slot < crusher::sequencer::targetSlotCount;
                 ++slot)
            {
                const auto index = static_cast<size_t>(slot);
                randomise(choices[index]);
                randomise(enabled[index]);
                randomise(depths[index]);
            }

            firstVisibleRow = 0;
            repaint();
            if (showHelp != nullptr)
                showHelp(
                    "RANDOMISE TARGETS\nTarget assignments, enabled states and individual Depths were randomised.");
        }

        juce::Rectangle<int> randomButtonBounds() const
        {
            return { 0, 2, 24, 16 };
        }

        int choiceForSlot(int slot) const
        {
            auto* parameter = choices[static_cast<size_t>(slot)];
            return parameter != nullptr ? juce::roundToInt(
                parameter->convertFrom0to1(parameter->getValue())) : 0;
        }

        bool enabledForSlot(int slot) const
        {
            auto* parameter = enabled[static_cast<size_t>(slot)];
            return parameter != nullptr
                && parameter->convertFrom0to1(parameter->getValue()) >= 0.5f;
        }

        float depthForSlot(int slot) const
        {
            auto* parameter = depths[static_cast<size_t>(slot)];
            return parameter != nullptr
                ? juce::jlimit(0.0f, 1.0f,
                    parameter->convertFrom0to1(parameter->getValue()))
                : 1.0f;
        }

        juce::Rectangle<int> depthBoundsForVisibleRow(int visibleRow) const
        {
            return { 18, 21 + visibleRow * rowHeight, 16, 16 };
        }

        void setDepthTooltip(float depth)
        {
            const auto tooltip = "Target Depth: modulation amount ("
                + juce::String(juce::roundToInt(depth * 100.0f)) + "%)";
            setTooltip(tooltip);
            if (showHelp != nullptr)
                showHelp("TARGET DEPTH\n" + tooltip);
        }

        std::vector<int> assignedSlots() const
        {
            std::vector<int> result;
            for (int slot = 0; slot < crusher::sequencer::targetSlotCount;
                 ++slot)
                if (choiceForSlot(slot) > 0)
                    result.push_back(slot);
            return result;
        }

        int visibleRowCount() const
        {
            return juce::jmax(1, (getHeight() - 20) / rowHeight);
        }

        bool canScrollUp() const
        {
            return firstVisibleRow > 0;
        }

        bool canScrollDown() const
        {
            return firstVisibleRow + visibleRowCount()
                < static_cast<int>(assignedSlots().size());
        }

        juce::Rectangle<int> scrollUpBounds() const
        {
            return { getWidth() - 22, 1, 20, 18 };
        }

        juce::Rectangle<int> scrollDownBounds() const
        {
            return { getWidth() - 22, getHeight() - 14, 20, 13 };
        }

        CrusherAudioProcessor& processor;
        std::function<void(const juce::String&)> showHelp;
        std::array<juce::RangedAudioParameter*,
                   crusher::sequencer::targetSlotCount> choices {};
        std::array<juce::RangedAudioParameter*,
                   crusher::sequencer::targetSlotCount> enabled {};
        std::array<juce::RangedAudioParameter*,
                   crusher::sequencer::targetSlotCount> depths {};
        juce::RangedAudioParameter* draggedDepthParameter = nullptr;
        float dragStartDepth = 1.0f;
        int dragStartY = 0;
        bool dragOver = false;
        int firstVisibleRow = 0;
        static constexpr int rowHeight = 20;
    };

    SequencerPanel(CrusherAudioProcessor& audioProcessor,
                   LookAndFeel& sharedLookAndFeel,
                   std::function<juce::Colour()> surroundColourProvider,
                   juce::Image backgroundArtwork,
                   std::function<float()> backgroundOpacityProvider,
                   std::function<void(const juce::String&)> helpCallback)
        : processor(audioProcessor),
          surroundColour(std::move(surroundColourProvider)),
          backdropImage(std::move(backgroundArtwork)),
          backdropOpacity(std::move(backgroundOpacityProvider)),
          showHelp(std::move(helpCallback)),
          grid(audioProcessor, sharedLookAndFeel, patternMenuLookAndFeel),
          targets(audioProcessor, showHelp)
    {
        setLookAndFeel(&sharedLookAndFeel);
        addMouseListener(this, true);
        addAndMakeVisible(grid);
        addAndMakeVisible(targets);
        const std::array<juce::Component*, 33> components
        {{ &hostSync, &enabledButton, &bipolarButton,
           &animateButton, &gateModeButton,
           &waveformBox, &userSequenceBox, &directionBox,
           &rateSlider, &startSlider, &endSlider, &attackSlider,
           &releaseSlider, &peakSlider, &moveSlider, &colourSlider,
           &saturationSlider, &luminanceSlider,
           &startLabel, &endLabel, &attackLabel, &releaseLabel,
           &peakLabel, &moveLabel, &shortGateLabel, &longGateLabel,
           &nudgeLeft, &nudgeRight, &gateNudgeLeft, &gateNudgeRight,
           &rangeLinkButton,
           &shortGateSlider, &longGateSlider }};
        for (auto* component : components)
            addAndMakeVisible(component);

        hostSync.setButtonText("HOST SYNC");
        enabledButton.setButtonText("ON");
        bipolarButton.setButtonText("UNIPOLAR +");
        animateButton.setButtonText("ANIMATE");
        hostSync.setName("Host Sync");
        enabledButton.setName("Sequencer On");
        bipolarButton.setName("Sequencer Polarity");
        animateButton.setName("Animate");
        gateModeButton.setName("Gate Mode");
        waveformBox.setName("Waveform");
        userSequenceBox.setName("User Sequence");
        directionBox.setName("Direction");
        rateSlider.setName("Rate");
        startSlider.setName("Start");
        endSlider.setName("End");
        rangeLinkButton.setName("Start / End Link");
        colourSlider.setName("Hue");
        saturationSlider.setName("Saturation");
        luminanceSlider.setName("Luminance");
        attackSlider.setName("Attack");
        releaseSlider.setName("Release");
        peakSlider.setName("Depth");
        moveSlider.setName("Offset");
        shortGateSlider.setName("Short");
        longGateSlider.setName("Long");
        grid.setName("Steps");
        targets.setName("Targets");
        for (auto* button : { &hostSync, &enabledButton, &bipolarButton,
                              &animateButton })
        {
            button->setClickingTogglesState(true);
            button->setColour(juce::ToggleButton::textColourId,
                              knobLabelColour);
            button->getProperties().set("activeColour",
                                        controlAccentColour.toString());
            button->getProperties().set("sequencerLabelStyle", true);
        }
        hostSync.getProperties().set("activeColour",
                                     tempoSyncLedColour.toString());
        enabledButton.getProperties().set("activeColour",
                                          linkLedColour.toString());
        bipolarButton.setClickingTogglesState(false);
        bipolarButton.setToggleState(true, juce::dontSendNotification);
        bipolarButton.getProperties().set("fixedLedCentreX", 9.0f);
        gateModeButton.setClickingTogglesState(false);
        gateModeButton.setToggleState(true, juce::dontSendNotification);
        gateModeButton.getProperties().set("fixedLedCentreX", 9.0f);
        gateModeButton.setColour(juce::ToggleButton::textColourId,
                                 knobLabelColour);
        gateModeButton.getProperties().set("sequencerLabelStyle", true);
        nudgeLeft.setName("Nudge pattern left");
        nudgeRight.setName("Nudge pattern right");
        gateNudgeLeft.setName("Nudge Gates left");
        gateNudgeRight.setName("Nudge Gates right");
        for (auto* button : { &nudgeLeft, &nudgeRight,
                              &gateNudgeLeft, &gateNudgeRight })
            button->setClickingTogglesState(false);
        nudgeLeft.getProperties().set("presetArrowDirection", -1);
        nudgeRight.getProperties().set("presetArrowDirection", 1);
        gateNudgeLeft.getProperties().set("presetArrowDirection", -1);
        gateNudgeRight.getProperties().set("presetArrowDirection", 1);
        for (auto* button : { &nudgeLeft, &nudgeRight,
                              &gateNudgeLeft, &gateNudgeRight })
            button->getProperties().set("compactPresetArrow", true);
        rangeLinkButton.setButtonText({});
        rangeLinkButton.setClickingTogglesState(true);
        rangeLinkButton.getProperties().set("rangeLinkIcon", true);
        rangeLinkButton.getProperties().set("compactRangeLink", true);
        rangeLinkButton.getProperties().set("activeColour",
                                             controlAccentColour.toString());
        nudgeLeft.onClick = [this] { grid.nudge(-1); };
        nudgeRight.onClick = [this] { grid.nudge(1); };
        gateNudgeLeft.onClick = [this] { grid.nudgeGates(-1); };
        gateNudgeRight.onClick = [this] { grid.nudgeGates(1); };

        hostSync.setTooltip("Synchronise the sequencer clock to the host tempo");
        enabledButton.setTooltip("Enable or disable sequencer modulation");
        bipolarButton.setTooltip(
            "Cycle target movement between Unipolar +, Bipolar and Unipolar -");
        animateButton.setTooltip(
            "Animate assigned controls without changing their stored values");
        animateButton.onClick = [this]
        {
            updateAnimatedControls();
            if (onAnimateChanged != nullptr)
                onAnimateChanged();
        };
        waveformBox.setTooltip("Replace the steps with a generated waveform");
        userSequenceBox.setTooltip("Load a saved user sequence");
        rateSlider.setTooltip("Set the sequencer rate");
        startSlider.setTooltip("Set the first step in the playback range");
        rangeLinkButton.setTooltip("Link Start and End so the range moves without changing length");
        endSlider.setTooltip("Set the last step in the playback range");
        directionBox.setTooltip("Choose the sequencer playback direction");
        colourSlider.setTooltip("Choose the hue used by the steps and assigned target labels");
        saturationSlider.setTooltip("Adjust the saturation of the sequencer colour");
        luminanceSlider.setTooltip("Adjust only the luminance of the sequencer steps");
        attackSlider.setTooltip("Smooth the rising edge between sequence steps");
        releaseSlider.setTooltip("Smooth the falling edge between sequence steps");
        peakSlider.setTooltip("Shrink or expand the sequence shape; 0% leaves it unchanged");
        moveSlider.setTooltip("Offset the complete sequence up or down");
        nudgeLeft.setTooltip("Move the step values one position left");
        nudgeRight.setTooltip("Move the step values one position right");
        gateNudgeLeft.setTooltip("Move the Gate states one position left");
        gateNudgeRight.setTooltip("Move the Gate states one position right");
        shortGateSlider.setTooltip(
            "Set how much of a step remains open in Short Gate mode");
        longGateSlider.setTooltip(
            "Set how much of a step remains open in Long Gate mode");
        gateModeButton.setTooltip(
            "Choose whether Gate acts before the effect, on the effect signal, on the complete output, or is bypassed");

        const auto configureTargetLabel = [](ParameterDragLabel& label,
                                             const juce::String& text,
                                             const juce::String& parameterId)
        {
            label.setText(text, juce::dontSendNotification);
            label.setJustificationType(juce::Justification::centred);
            label.setFont(juce::Font(juce::FontOptions(
                10.5f, juce::Font::bold)));
            label.setColour(juce::Label::textColourId, knobLabelColour);
            label.setInterceptsMouseClicks(true, false);
            label.parameterIdProvider = [parameterId] { return parameterId; };
            label.setTooltip("Drag " + text
                             + " onto a sequencer target slot");
        };
        configureTargetLabel(startLabel, "START", "sequencerStart");
        configureTargetLabel(endLabel, "END", "sequencerEnd");
        configureTargetLabel(attackLabel, "ATTACK", "sequencerAttack");
        configureTargetLabel(releaseLabel, "RELEASE", "sequencerRelease");
        configureTargetLabel(peakLabel, "DEPTH", "sequencerPeak");
        configureTargetLabel(moveLabel, "OFFSET", "sequencerMove");
        configureTargetLabel(shortGateLabel, "SHORT",
                             "sequencerGateShortLength");
        configureTargetLabel(longGateLabel, "LONG",
                             "sequencerGateLongLength");
        startLabel.setTooltip(startSlider.getTooltip());
        endLabel.setTooltip(endSlider.getTooltip());
        attackLabel.setTooltip(attackSlider.getTooltip());
        releaseLabel.setTooltip(releaseSlider.getTooltip());
        peakLabel.setTooltip(peakSlider.getTooltip());
        moveLabel.setTooltip(moveSlider.getTooltip());
        shortGateLabel.setTooltip(shortGateSlider.getTooltip());
        longGateLabel.setTooltip(longGateSlider.getTooltip());

        waveformBox.addItem("Saw", 1);
        waveformBox.addItem("Saw Double", 2);
        waveformBox.addItem("Saw Down", 3);
        waveformBox.addItem("Saw Down Double", 4);
        waveformBox.addItem("Sine", 5);
        waveformBox.addItem("Sine Double", 6);
        waveformBox.addItem("Triangle", 7);
        waveformBox.addItem("Triangle Double", 8);
        waveformBox.addItem("Pulse 25", 9);
        waveformBox.addItem("Pulse 25 Double", 10);
        waveformBox.addItem("Square", 11);
        waveformBox.addItem("Square Double", 12);
        waveformBox.addItem("Pulse 75", 13);
        waveformBox.addItem("Pulse 75 Double", 14);
        waveformBox.setTextWhenNothingSelected("WAVEFORM");
        waveformBox.onChange = [this]
        {
            const int selected = waveformBox.getSelectedItemIndex();
            if (selected >= 0)
                grid.applyWaveform(
                    static_cast<crusher::sequencer::Waveform>(selected));
            waveformBox.setSelectedId(0, juce::dontSendNotification);
        };

        directionBox.addItemList(
            { "Loop", "Bounce", "Reverse", "Played", "Random" }, 1);
        for (auto* box : { &waveformBox, &userSequenceBox, &directionBox })
        {
            box->setJustificationType(juce::Justification::centred);
            box->setColour(juce::ComboBox::backgroundColourId,
                           juce::Colour(0xff19151f));
            box->setColour(juce::ComboBox::textColourId, knobValueColour);
            box->setColour(juce::ComboBox::outlineColourId, knobTrackColour);
            box->setColour(juce::ComboBox::arrowColourId,
                           knobIndicatorColour);
            box->setScrollWheelEnabled(true);
            box->getProperties().set("compactClosedCombo", true);
        }
        directionAttachment = std::make_unique<
            juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
                processor.parameters, "sequencerDirection", directionBox);
        hostSyncAttachment = std::make_unique<
            juce::AudioProcessorValueTreeState::ButtonAttachment>(
                processor.parameters, "sequencerHostSync", hostSync);
        enabledAttachment = std::make_unique<
            juce::AudioProcessorValueTreeState::ButtonAttachment>(
                processor.parameters, "sequencerEnabled", enabledButton);
        enabledButton.onStateChange = [this]
        {
            updateSequencerControlEnablement();
        };
        updateSequencerControlEnablement();
        auto* bipolarParameter = processor.parameters.getParameter(
            "sequencerBipolar");
        auto* negativeParameter = processor.parameters.getParameter(
            "sequencerUnipolarNegative");
        jassert(bipolarParameter != nullptr && negativeParameter != nullptr);
        if (bipolarParameter != nullptr && negativeParameter != nullptr)
        {
            bipolarModeAttachment = std::make_unique<juce::ParameterAttachment>(
                *bipolarParameter,
                [this](float)
                {
                    if (! updatingPolarityParameters)
                        syncPolarityFromParameters();
                }, nullptr);
            negativeModeAttachment = std::make_unique<juce::ParameterAttachment>(
                *negativeParameter,
                [this](float)
                {
                    if (! updatingPolarityParameters)
                        syncPolarityFromParameters();
                }, nullptr);
            bipolarButton.onClick = [this] { cyclePolarityMode(); };
            bipolarModeAttachment->sendInitialUpdate();
            negativeModeAttachment->sendInitialUpdate();
            syncPolarityFromParameters();
        }
        rangeLinkAttachment = std::make_unique<
            juce::AudioProcessorValueTreeState::ButtonAttachment>(
                processor.parameters, "sequencerRangeLink", rangeLinkButton);
        if (auto* gateModeParameter = processor.parameters.getParameter(
                "sequencerGateMode"))
        {
            gateModeAttachment = std::make_unique<juce::ParameterAttachment>(
                *gateModeParameter,
                [this](float)
                {
                    if (! updatingGateModeParameter)
                        syncGateModeFromParameter();
                }, nullptr);
            gateModeButton.onClick = [this] { cycleGateMode(); };
            gateModeAttachment->sendInitialUpdate();
            syncGateModeFromParameter();
        }

        configureKnob(rateSlider, 0.0, 13.0, 1.0);
        configureKnob(startSlider, 1.0, 31.0, 1.0);
        configureKnob(endSlider, 2.0, 32.0, 1.0);
        configureKnob(attackSlider, 0.0, 1.0, 0.001);
        configureKnob(releaseSlider, 0.0, 1.0, 0.001);
        attackSlider.textFromValueFunction
            = releaseSlider.textFromValueFunction
            = [](double value)
            {
                return juce::String(juce::roundToInt(value * 100.0)) + "%";
            };
        attackSlider.valueFromTextFunction
            = releaseSlider.valueFromTextFunction
            = [](const juce::String& text)
            {
                const double entered = text.getDoubleValue();
                return juce::jlimit(0.0, 1.0,
                                    entered > 1.0 || text.containsChar('%')
                                        ? entered * 0.01 : entered);
            };
        attackSlider.setTextBoxStyle(
            juce::Slider::TextBoxBelow, false, 52, 14);
        releaseSlider.setTextBoxStyle(
            juce::Slider::TextBoxBelow, false, 52, 14);
        configureKnob(shortGateSlider, 0.10, 0.60, 0.01);
        configureKnob(longGateSlider, 0.65, 0.95, 0.01);
        shortGateSlider.setDoubleClickReturnValue(true, 0.50);
        longGateSlider.setDoubleClickReturnValue(true, 0.90);
        configureVerticalSlider(peakSlider, -1.0, 1.0, 0.001);
        configureVerticalSlider(moveSlider, -1.0, 1.0, 0.001);
        peakSlider.setScrollWheelEnabled(true);
        moveSlider.setScrollWheelEnabled(true);
        peakSlider.setCentreDetent(0.0, 0.03);
        peakSlider.setDoubleClickReturnValue(true, 0.0);
        moveSlider.setCentreDetent(0.0, 0.03);
        colourSlider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
        colourSlider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
        colourSlider.setRange(0.0, 1.0, 0.001);
        colourSlider.setValue(0.5, juce::dontSendNotification);
        colourSlider.onValueChange = [this] { updateAccent(); };
        colourSlider.setDoubleClickReturnValue(true, 0.5);
        saturationSlider.setSliderStyle(
            juce::Slider::RotaryHorizontalVerticalDrag);
        saturationSlider.setTextBoxStyle(
            juce::Slider::NoTextBox, false, 0, 0);
        saturationSlider.setRange(0.0, 1.0, 0.001);
        saturationSlider.setValue(0.5, juce::dontSendNotification);
        saturationSlider.onValueChange = [this] { updateAccent(); };
        saturationSlider.setDoubleClickReturnValue(true, 0.5);
        luminanceSlider.setSliderStyle(
            juce::Slider::RotaryHorizontalVerticalDrag);
        luminanceSlider.setTextBoxStyle(
            juce::Slider::NoTextBox, false, 0, 0);
        luminanceSlider.setRange(0.2, 1.0, 0.001);
        luminanceSlider.setValue(0.5, juce::dontSendNotification);
        luminanceSlider.onValueChange = [this] { updateAccent(); };
        luminanceSlider.setDoubleClickReturnValue(true, 0.5);

        colourAttachment = makeSliderAttachment(
            "sequencerHue", colourSlider);
        saturationAttachment = makeSliderAttachment(
            "sequencerSaturation", saturationSlider);
        luminanceAttachment = makeSliderAttachment(
            "sequencerLuminance", luminanceSlider);

        rateAttachment = makeSliderAttachment("sequencerRate", rateSlider);
        startAttachment = makeSliderAttachment("sequencerStart", startSlider);
        endAttachment = makeSliderAttachment("sequencerEnd", endSlider);
        attackAttachment = makeSliderAttachment("sequencerAttack", attackSlider);
        releaseAttachment = makeSliderAttachment("sequencerRelease", releaseSlider);
        peakAttachment = makeSliderAttachment("sequencerPeak", peakSlider);
        moveAttachment = makeSliderAttachment("sequencerMove", moveSlider);
        shortGateAttachment = makeSliderAttachment(
            "sequencerGateShortLength", shortGateSlider);
        longGateAttachment = makeSliderAttachment(
            "sequencerGateLongLength", longGateSlider);
        configureSavedReset(rateSlider, "sequencerRate");
        configureSavedReset(startSlider, "sequencerStart");
        configureSavedReset(endSlider, "sequencerEnd");
        configureSavedReset(attackSlider, "sequencerAttack");
        configureSavedReset(releaseSlider, "sequencerRelease");
        configureSavedReset(peakSlider, "sequencerPeak");
        configureSavedReset(moveSlider, "sequencerMove");
        configureSavedReset(shortGateSlider, "sequencerGateShortLength");
        configureSavedReset(longGateSlider, "sequencerGateLongLength");
        linkedRangeLength = juce::jmax(
            1, juce::roundToInt(endSlider.getValue()
                               - startSlider.getValue()));
        rangeLinkButton.onClick = [this]
        {
            if (rangeLinkButton.getToggleState())
                linkedRangeLength = juce::jmax(
                    1, juce::roundToInt(endSlider.getValue()
                                       - startSlider.getValue()));
        };
        startSlider.onValueChange = [this]
        {
            if (! rangeLinkButton.getToggleState() || updatingLinkedRange)
                return;
            const juce::ScopedValueSetter<bool> guard(updatingLinkedRange, true);
            const int startValue = juce::roundToInt(startSlider.getValue());
            const int requestedEnd = startValue + linkedRangeLength;
            if (requestedEnd <= crusher::sequencer::stepCount)
                endSlider.setValue(requestedEnd, juce::sendNotificationSync);
            else
                startSlider.setValue(crusher::sequencer::stepCount
                                         - linkedRangeLength,
                                     juce::sendNotificationSync);
        };
        endSlider.onValueChange = [this]
        {
            if (! rangeLinkButton.getToggleState() || updatingLinkedRange)
                return;
            const juce::ScopedValueSetter<bool> guard(updatingLinkedRange, true);
            const int endValue = juce::roundToInt(endSlider.getValue());
            const int requestedStart = endValue - linkedRangeLength;
            if (requestedStart >= 1)
                startSlider.setValue(requestedStart, juce::sendNotificationSync);
            else
                endSlider.setValue(1 + linkedRangeLength,
                                   juce::sendNotificationSync);
        };
        rateSlider.textFromValueFunction = [](double value)
        {
            static const std::array<const char*, 14> names
            { "1/128", "1/64T", "1/64", "1/32T", "1/32", "1/16T",
              "1/16", "1/8T", "1/8", "1/4T", "1/4", "1/2T", "1/2", "1/1" };
            return juce::String(names[static_cast<size_t>(juce::jlimit(
                0, 13, juce::roundToInt(value)))]);
        };
        peakSlider.textFromValueFunction = moveSlider.textFromValueFunction
            = [](double value) { return juce::String(value * 100.0, 0) + "%"; };
        shortGateSlider.textFromValueFunction
            = longGateSlider.textFromValueFunction
            = [](double value) { return juce::String(value * 100.0, 0) + "%"; };
        attackSlider.updateText();
        releaseSlider.updateText();

        grid.onSaveRequested = [this] { saveUserSequence(); };
        userSequenceBox.setTextWhenNothingSelected("USER");
        userSequenceBox.onChange = [this] { loadSelectedUserSequence(); };
        refreshUserSequences();
        updateAccent();
    }

    ~SequencerPanel() override
    {
        removeMouseListener(this);
        setLookAndFeel(nullptr);
    }

    double getHue() const noexcept { return colourSlider.getValue(); }
    juce::Colour getAccent() const noexcept { return accent; }
    bool isTargetAssigned(const juce::String& parameterId) const
    {
        const int requested = crusher::sequencer::targetChoiceForParameter(
            parameterId);
        if (requested <= 0)
            return false;

        for (int slot = 0; slot < crusher::sequencer::targetSlotCount;
             ++slot)
        {
            auto* parameter = processor.parameters.getParameter(
                crusher::sequencer::targetParameterId(slot));
            if (parameter != nullptr
                && juce::roundToInt(parameter->convertFrom0to1(
                       parameter->getValue())) == requested)
                return true;
        }
        return false;
    }
    double getSaturation() const noexcept
    {
        return saturationSlider.getValue();
    }
    double getLuminance() const noexcept
    {
        return luminanceSlider.getValue();
    }
    bool getAnimate() const noexcept
    {
        return animateButton.getToggleState();
    }
    void setAnimate(bool shouldAnimate)
    {
        animateButton.setToggleState(shouldAnimate,
                                     juce::dontSendNotification);
        updateAnimatedControls();
    }
    std::function<void()> onAnimateChanged;
    void setHue(double hue)
    {
        colourSlider.setValue(juce::jlimit(0.0, 1.0, hue),
                              juce::dontSendNotification);
        updateAccent();
    }
    void setSaturation(double saturation)
    {
        saturationSlider.setValue(juce::jlimit(0.0, 1.0, saturation),
                                  juce::dontSendNotification);
        updateAccent();
    }
    void setLuminance(double luminance)
    {
        luminanceSlider.setValue(juce::jlimit(0.2, 1.0, luminance),
                                 juce::dontSendNotification);
        updateAccent();
    }
    void updateTargetHighlights()
    {
        const auto update = [this](ParameterDragLabel& label)
        {
            const auto parameterId = label.parameterIdProvider != nullptr
                ? label.parameterIdProvider() : juce::String();
            label.setTargetAppearance(isTargetAssigned(parameterId), accent);
        };
        update(startLabel);
        update(endLabel);
        update(attackLabel);
        update(releaseLabel);
        update(peakLabel);
        update(moveLabel);
        update(shortGateLabel);
        update(longGateLabel);
    }
    void timerTick()
    {
        updateSequencerControlEnablement();
        if (! updatingPolarityParameters)
            syncPolarityFromParameters();
        if (! updatingGateModeParameter)
            syncGateModeFromParameter();
        grid.repaint();
        targets.repaint();
        updateAnimatedControls();
    }

    void paint(juce::Graphics& graphics) override
    {
        const auto surround = surroundColour != nullptr
            ? surroundColour() : juce::Colour(0xff24292f);
        graphics.setColour(surround);
        graphics.fillRoundedRectangle(getLocalBounds().toFloat(), 12.0f);
        // The artwork shares one image coordinate system with the main panel,
        // but is clipped to this sequencer surround. The 8 px gap stays clear.
        if (backdropImage.isValid() && backdropOpacity != nullptr)
        {
            const float opacity = backdropOpacity();
            if (opacity > 0.0f)
            {
                graphics.saveState();
                juce::Path clip;
                clip.addRoundedRectangle(getLocalBounds().toFloat(), 12.0f);
                graphics.reduceClipRegion(clip);
                graphics.setOpacity(opacity);
                // Main image is (8,52,910,470) in editor coordinates.
                // Sequencer begins at (8,276): local image origin (0,-224).
                graphics.drawImage(backdropImage,
                    juce::Rectangle<float>(0.0f, -224.0f, 910.0f, 470.0f),
                    juce::RectanglePlacement::fillDestination);
                graphics.restoreState();
            }
        }
        const auto panelColour = juce::Colour(0xff171b20);
        const auto drawPanel = [&graphics, panelColour](juce::Rectangle<float> area)
        {
            graphics.setColour(panelColour);
            graphics.fillRoundedRectangle(area, 8.0f);
            graphics.setColour(knobTrackColour);
            graphics.drawRoundedRectangle(area, 8.0f, 1.0f);
        };
        drawPanel({ 10.0f, 10.0f, static_cast<float>(getWidth() - 20), 68.0f });
        drawPanel({ 10.0f, 84.0f, static_cast<float>(getWidth() - 20), 152.0f });
        graphics.setColour(sequencerControlsEnabled
            ? knobLabelColour : knobLabelColour.darker(0.48f));
        graphics.setFont(juce::FontOptions(10.5f, juce::Font::bold));
        graphics.drawText("RATE", 243, 11, 60, 13,
                          juce::Justification::centred);
        graphics.drawText("DIRECTION", 443, 11, 106, 13,
                          juce::Justification::centred);
        graphics.drawText("HUE", 773, 11, 46, 13,
                          juce::Justification::centred);
        graphics.drawText("SAT", 815, 11, 38, 13,
                          juce::Justification::centred);
        graphics.drawText("LUM", 853, 11, 38, 13,
                          juce::Justification::centred);
        graphics.drawText("STEPS", 16, 190, 68, 13,
                          juce::Justification::centred);
        graphics.drawText("GATE", 82, 190, 68, 13,
                          juce::Justification::centred);
    }

    void resized() override
    {
        hostSync.setBounds(24, 14, 88, 24);
        animateButton.setBounds(150, 14, 68, 24);
        waveformBox.setBounds(23, 45, 90, 20);
        userSequenceBox.setBounds(139, 45, 90, 20);
        rateSlider.setBounds(251, 26, 44, 48);
        startLabel.setBounds(299, 11, 60, 13);
        startSlider.setBounds(307, 26, 44, 48);
        rangeLinkButton.setBounds(358, 33, 18, 18);
        endLabel.setBounds(379, 11, 60, 13);
        endSlider.setBounds(387, 26, 44, 48);
        directionBox.setBounds(451, 33, 90, 20);
        shortGateLabel.setBounds(553, 11, 60, 13);
        shortGateSlider.setBounds(561, 26, 44, 48);
        longGateLabel.setBounds(611, 11, 60, 13);
        longGateSlider.setBounds(619, 26, 44, 48);
        gateModeButton.setBounds(676, 31, 96, 24);
        colourSlider.setBounds(781, 29, 30, 30);
        saturationSlider.setBounds(819, 29, 30, 30);
        luminanceSlider.setBounds(857, 29, 30, 30);
        enabledButton.setBounds(20, 88, 34, 24);
        bipolarButton.setBounds(56, 88, 84, 24);
        attackLabel.setBounds(16, 118, 68, 13);
        releaseLabel.setBounds(82, 118, 68, 13);
        peakLabel.setBounds(149, 88, 64, 13);
        moveLabel.setBounds(149, 156, 64, 13);
        attackSlider.setBounds(24, 131, 52, 49);
        releaseSlider.setBounds(90, 131, 52, 49);
        peakSlider.setBounds(167, 102, 28, 58);
        moveSlider.setBounds(167, 170, 28, 62);
        nudgeLeft.setBounds(27, 204, 20, 20);
        nudgeRight.setBounds(53, 204, 20, 20);
        gateNudgeLeft.setBounds(93, 204, 20, 20);
        gateNudgeRight.setBounds(119, 204, 20, 20);
        grid.setBounds(210, 92, 554, 136);
        targets.setBounds(774, 92, getWidth() - 792, 136);
    }

private:
    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    void setPolarityMode(int polarityMode)
    {
        polarityMode = juce::jlimit(0, 2, polarityMode);
        displayedPolarityMode = polarityMode;
        grid.setPolarityMode(polarityMode);

        const bool isBipolar = polarityMode == 1;
        const bool isNegative = polarityMode == 2;
        bipolarButton.setButtonText(isBipolar ? "BIPOLAR"
            : (isNegative ? "UNIPOLAR -" : "UNIPOLAR +"));
        bipolarButton.getProperties().set(
            "activeColour",
            (isBipolar ? bipolarLedColour
                       : (isNegative ? unipolarNegativeLedColour
                                     : controlAccentColour)).toString());
        bipolarButton.setToggleState(true, juce::dontSendNotification);
        bipolarButton.repaint();
    }

    void syncPolarityFromParameters()
    {
        const auto* bipolar = processor.parameters.getRawParameterValue(
            "sequencerBipolar");
        const auto* negative = processor.parameters.getRawParameterValue(
            "sequencerUnipolarNegative");
        const bool isBipolar = bipolar != nullptr && bipolar->load() >= 0.5f;
        const bool isNegative = ! isBipolar && negative != nullptr
            && negative->load() >= 0.5f;
        const int polarityMode = isBipolar ? 1 : (isNegative ? 2 : 0);
        if (displayedPolarityMode == polarityMode)
            return;
        setPolarityMode(polarityMode);
    }

    void cyclePolarityMode()
    {
        if (bipolarModeAttachment == nullptr
            || negativeModeAttachment == nullptr)
            return;

        const int nextMode = (juce::jlimit(0, 2, displayedPolarityMode) + 1) % 3;

        // Advance the label, LED and grid as one state. The two persisted
        // booleans are then written as a canonical pair without exposing the
        // intermediate pair through either attachment callback.
        setPolarityMode(nextMode);
        const juce::ScopedValueSetter<bool> guard(
            updatingPolarityParameters, true);
        negativeModeAttachment->setValueAsCompleteGesture(
            nextMode == 2 ? 1.0f : 0.0f);
        bipolarModeAttachment->setValueAsCompleteGesture(
            nextMode == 1 ? 1.0f : 0.0f);
    }

    void setGateMode(int gateMode)
    {
        gateMode = juce::jlimit(0, 3, gateMode);
        displayedGateMode = gateMode;
        const auto text = gateMode == 0 ? "INPUT"
                        : gateMode == 1 ? "EFFECT"
                        : gateMode == 2 ? "OUTPUT"
                                        : "OFF";
        const auto colour = gateMode == 0 ? linkLedColour
                          : gateMode == 1 ? controlAccentColour
                          : gateMode == 2 ? bipolarLedColour
                                          : juce::Colour(0xff596069);
        gateModeButton.setButtonText(text);
        gateModeButton.getProperties().set(
            "activeColour", colour.toString());
        gateModeButton.setToggleState(
            gateMode != 3, juce::dontSendNotification);
        gateModeButton.repaint();
    }

    void syncGateModeFromParameter()
    {
        const auto* gateMode = processor.parameters.getRawParameterValue(
            "sequencerGateMode");
        const int mode = gateMode != nullptr
            ? juce::jlimit(0, 3, juce::roundToInt(gateMode->load())) : 0;
        if (displayedGateMode != mode)
            setGateMode(mode);
    }

    void cycleGateMode()
    {
        if (gateModeAttachment == nullptr)
            return;

        const int nextMode = (juce::jlimit(0, 3, displayedGateMode) + 1) % 4;
        setGateMode(nextMode);
        const juce::ScopedValueSetter<bool> guard(
            updatingGateModeParameter, true);
        gateModeAttachment->setValueAsCompleteGesture(
            static_cast<float>(nextMode));
    }

    void mouseEnter(const juce::MouseEvent& event) override
    {
        if (showHelp == nullptr)
            return;

        const auto isWithin = [](juce::Component* component,
                                 const juce::Component& owner)
        {
            for (auto* current = component; current != nullptr;
                 current = current->getParentComponent())
                if (current == &owner)
                    return true;
            return false;
        };
        juce::String controlTitle;
        const std::array<juce::Slider*, 12> sequencerSliders
        {{ &rateSlider, &startSlider, &endSlider,
           &attackSlider, &releaseSlider, &peakSlider,
           &moveSlider, &colourSlider, &saturationSlider,
           &luminanceSlider, &shortGateSlider, &longGateSlider }};
        for (auto* slider : sequencerSliders)
            if (isWithin(event.eventComponent, *slider))
            {
                controlTitle = slider->getName().trim();
                break;
            }

        for (auto* component = event.eventComponent;
             component != nullptr && component != this;
             component = component->getParentComponent())
        {
            if (auto* tooltipClient = dynamic_cast<juce::TooltipClient*>(component))
            {
                const auto tooltip = tooltipClient->getTooltip();
                if (tooltip.isNotEmpty())
                {
                    auto title = controlTitle.isNotEmpty()
                        ? controlTitle : component->getName().trim();
                    if (title.isEmpty())
                        if (auto* label = dynamic_cast<juce::Label*>(component))
                            title = label->getText().trim();
                    if (title.isEmpty())
                        title = "Sequencer";
                    showHelp(title.toUpperCase() + "\n" + tooltip);
                    return;
                }
            }
        }
    }

    void configureKnob(juce::Slider& slider, double minimum,
                       double maximum, double interval)
    {
        slider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
        slider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 72, 18);
        slider.setRange(minimum, maximum, interval);
        slider.setMouseDragSensitivity(80);
        slider.setColour(juce::Slider::rotarySliderFillColourId,
                         knobIndicatorColour);
        slider.setColour(juce::Slider::textBoxTextColourId, knobValueColour);
        slider.setColour(juce::Slider::textBoxBackgroundColourId,
                         juce::Colours::transparentBlack);
        slider.setColour(juce::Slider::textBoxOutlineColourId,
                         juce::Colours::transparentBlack);
    }

    void updateSequencerControlEnablement()
    {
        const bool shouldEnable = enabledButton.getToggleState();
        if (sequencerControlsEnabled == shouldEnable)
            return;

        sequencerControlsEnabled = shouldEnable;
        const std::array<juce::Component*, 34> controls
        {{ &hostSync, &bipolarButton, &animateButton, &gateModeButton,
           &waveformBox, &userSequenceBox, &directionBox,
           &rateSlider, &startSlider, &endSlider, &attackSlider,
           &releaseSlider, &peakSlider, &moveSlider, &colourSlider,
           &saturationSlider, &luminanceSlider,
           &startLabel, &endLabel, &attackLabel, &releaseLabel,
           &peakLabel, &moveLabel, &shortGateLabel, &longGateLabel,
           &nudgeLeft, &nudgeRight, &gateNudgeLeft, &gateNudgeRight,
           &rangeLinkButton, &shortGateSlider, &longGateSlider,
           &grid, &targets }};
        for (auto* control : controls)
        {
            control->setEnabled(shouldEnable);
            control->setAlpha(shouldEnable ? 1.0f : 0.48f);
        }
        repaint();
    }

    void configureVerticalSlider(juce::Slider& slider, double minimum,
                                 double maximum, double interval)
    {
        slider.setSliderStyle(juce::Slider::LinearVertical);
        slider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
        slider.setRange(minimum, maximum, interval);
        slider.setMouseDragSensitivity(80);
        slider.setColour(juce::Slider::rotarySliderFillColourId,
                         knobIndicatorColour);
    }

    std::unique_ptr<SliderAttachment> makeSliderAttachment(
        const juce::String& id, juce::Slider& slider)
    {
        return std::make_unique<SliderAttachment>(processor.parameters,
                                                   id, slider);
    }

    void configureSavedReset(DetentedSlider& slider, juce::String id)
    {
        slider.onSavedStateReset = [this, &slider, id = std::move(id)]
        {
            const auto saved = processor.getSavedParameterValue(id);
            if (! saved.has_value())
                return false;
            slider.setValue(*saved, juce::sendNotificationSync);
            return true;
        };
    }

    void updateAccent()
    {
        // Target labels and the three colour controls must remain readable.
        // LUM deliberately affects only the step display.
        accent = juce::Colour::fromHSV(
            static_cast<float>(colourSlider.getValue()),
            static_cast<float>(saturationSlider.getValue()),
            0.72f, 1.0f);
        const auto stepAccent = juce::Colour::fromHSV(
            static_cast<float>(colourSlider.getValue()),
            static_cast<float>(saturationSlider.getValue()),
            static_cast<float>(luminanceSlider.getValue()), 1.0f);
        colourSlider.setColour(juce::Slider::rotarySliderFillColourId, accent);
        saturationSlider.setColour(juce::Slider::rotarySliderFillColourId,
                                   accent);
        luminanceSlider.setColour(juce::Slider::rotarySliderFillColourId,
                                  accent);
        grid.setAccent(stepAccent);
        updateTargetHighlights();
        repaint();
    }

    void updateAnimatedControls()
    {
        const auto update = [this](juce::Slider& slider,
                                   const juce::String& parameterId)
        {
            const auto value = animateButton.getToggleState()
                ? processor.getSequencerAnimatedNormalised(parameterId)
                : std::nullopt;
            auto& sliderProperties = slider.getProperties();
            if (value.has_value())
            {
                const bool changed = ! sliderProperties.contains("animatedNormalised")
                    || std::abs(static_cast<float>(
                           sliderProperties["animatedNormalised"]) - *value)
                           > 0.000001f;
                if (changed)
                {
                    sliderProperties.set("animatedNormalised", *value);
                    slider.repaint();
                }
            }
            else if (sliderProperties.contains("animatedNormalised"))
            {
                sliderProperties.remove("animatedNormalised");
                slider.repaint();
            }
        };
        update(attackSlider, "sequencerAttack");
        update(releaseSlider, "sequencerRelease");
        update(peakSlider, "sequencerPeak");
        update(moveSlider, "sequencerMove");
        update(startSlider, "sequencerStart");
        update(endSlider, "sequencerEnd");
        update(shortGateSlider, "sequencerGateShortLength");
        update(longGateSlider, "sequencerGateLongLength");
    }

    void refreshUserSequences()
    {
        const auto selectedText = userSequenceBox.getText();
        userSequenceFiles.clear();
        const auto directory = CrusherAudioProcessor::getSequencesDirectory();
        directory.createDirectory();
        directory.findChildFiles(userSequenceFiles, juce::File::findFiles,
                                 false, "*.crusherseq");
        std::sort(userSequenceFiles.begin(), userSequenceFiles.end(),
            [](const juce::File& left, const juce::File& right)
            {
                return left.getFileNameWithoutExtension().compareNatural(
                           right.getFileNameWithoutExtension(), true) < 0;
            });
        userSequenceBox.clear(juce::dontSendNotification);
        for (int index = 0; index < userSequenceFiles.size(); ++index)
            userSequenceBox.addItem(
                userSequenceFiles[index].getFileNameWithoutExtension(),
                index + 1);
        userSequenceBox.setTextWhenNothingSelected(
            selectedText.isNotEmpty() ? selectedText : "USER");
    }

    void loadSelectedUserSequence()
    {
        const int index = userSequenceBox.getSelectedId() - 1;
        if (! juce::isPositiveAndBelow(index, userSequenceFiles.size()))
            return;
        const auto xml = juce::XmlDocument::parse(userSequenceFiles[index]);
        if (xml == nullptr || ! xml->hasTagName("CrusherSequence"))
            return;
        if (xml->hasAttribute("gateShort"))
            shortGateSlider.setValue(
                xml->getDoubleAttribute("gateShort", 0.50),
                juce::sendNotificationSync);
        if (xml->hasAttribute("gateLong"))
            longGateSlider.setValue(
                xml->getDoubleAttribute("gateLong", 0.90),
                juce::sendNotificationSync);
        for (auto* stepElement : xml->getChildWithTagNameIterator("Step"))
        {
            const int step = stepElement->getIntAttribute("index", -1);
            if (! juce::isPositiveAndBelow(step,
                                           crusher::sequencer::stepCount))
                continue;
            setParameterActualValue(processor.parameters.getParameter(
                crusher::sequencer::stepParameterId(step)),
                static_cast<float>(stepElement->getDoubleAttribute("value", 1.0)));
            if (stepElement->hasAttribute("gate"))
                setParameterActualValue(processor.parameters.getParameter(
                    crusher::sequencer::gateStepParameterId(step)),
                    static_cast<float>(stepElement->getDoubleAttribute(
                        "gate", static_cast<double>(
                            static_cast<int>(
                                crusher::sequencer::GateStepMode::rest)))));
        }
        grid.repaint();
    }

    void saveUserSequence()
    {
        sequenceFileChooser = std::make_unique<juce::FileChooser>(
            "Save Crusher User Sequence",
            CrusherAudioProcessor::getSequencesDirectory()
                .getChildFile("New Sequence.crusherseq"), "*.crusherseq");
        const juce::Component::SafePointer<SequencerPanel> safeThis(this);
        sequenceFileChooser->launchAsync(
            juce::FileBrowserComponent::saveMode
                | juce::FileBrowserComponent::canSelectFiles,
            [safeThis](const juce::FileChooser& chooser)
            {
                if (safeThis == nullptr)
                    return;
                auto file = chooser.getResult();
                if (file == juce::File())
                    return;
                if (! file.hasFileExtension(".crusherseq"))
                    file = file.withFileExtension(".crusherseq");
                auto xml = std::make_unique<juce::XmlElement>(
                    "CrusherSequence");
                xml->setAttribute("gateShort",
                    safeThis->shortGateSlider.getValue());
                xml->setAttribute("gateLong",
                    safeThis->longGateSlider.getValue());
                for (int step = 0; step < crusher::sequencer::stepCount;
                     ++step)
                {
                    auto* element = xml->createNewChildElement("Step");
                    element->setAttribute("index", step);
                    const auto read = [&safeThis](const juce::String& id)
                    {
                        auto* parameter = safeThis->processor.parameters
                            .getParameter(id);
                        return parameter != nullptr
                            ? parameter->convertFrom0to1(parameter->getValue())
                            : 0.0f;
                    };
                    element->setAttribute("value", read(
                        crusher::sequencer::stepParameterId(step)));
                    element->setAttribute("gate", read(
                        crusher::sequencer::gateStepParameterId(step)));
                }
                if (xml->writeTo(file))
                    safeThis->refreshUserSequences();
                safeThis->sequenceFileChooser.reset();
            });
    }

    CrusherAudioProcessor& processor;
    std::function<juce::Colour()> surroundColour;
    juce::Image backdropImage;
    std::function<float()> backdropOpacity;
    std::function<void(const juce::String&)> showHelp;
    LookAndFeel patternMenuLookAndFeel { true };
    StepGrid grid;
    TargetList targets;
    juce::ToggleButton hostSync;
    juce::ToggleButton enabledButton;
    juce::ToggleButton bipolarButton;
    juce::ToggleButton animateButton;
    juce::ToggleButton gateModeButton;
    juce::ToggleButton rangeLinkButton;
    juce::ComboBox waveformBox;
    juce::ComboBox userSequenceBox;
    juce::ComboBox directionBox;
    juce::ToggleButton nudgeLeft;
    juce::ToggleButton nudgeRight;
    juce::ToggleButton gateNudgeLeft;
    juce::ToggleButton gateNudgeRight;
    DetentedSlider rateSlider;
    DetentedSlider startSlider;
    DetentedSlider endSlider;
    DetentedSlider attackSlider;
    DetentedSlider releaseSlider;
    DetentedSlider peakSlider;
    DetentedSlider moveSlider;
    DetentedSlider shortGateSlider;
    DetentedSlider longGateSlider;
    ParameterDragLabel startLabel;
    ParameterDragLabel endLabel;
    ParameterDragLabel attackLabel;
    ParameterDragLabel releaseLabel;
    ParameterDragLabel peakLabel;
    ParameterDragLabel moveLabel;
    ParameterDragLabel shortGateLabel;
    ParameterDragLabel longGateLabel;
    juce::Slider colourSlider;
    juce::Slider saturationSlider;
    juce::Slider luminanceSlider;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment>
        hostSyncAttachment, enabledAttachment,
        rangeLinkAttachment;
    std::unique_ptr<juce::ParameterAttachment> bipolarModeAttachment,
        negativeModeAttachment, gateModeAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment>
        directionAttachment;
    std::unique_ptr<SliderAttachment> rateAttachment, startAttachment,
        endAttachment, attackAttachment, releaseAttachment, peakAttachment,
        moveAttachment, colourAttachment, saturationAttachment,
        luminanceAttachment, shortGateAttachment, longGateAttachment;
    juce::Array<juce::File> userSequenceFiles;
    std::unique_ptr<juce::FileChooser> sequenceFileChooser;
    juce::Colour accent { 0xff34d6c6 };
    int linkedRangeLength = crusher::sequencer::stepCount - 1;
    int displayedPolarityMode = -1;
    int displayedGateMode = -1;
    bool updatingPolarityParameters = false;
    bool updatingGateModeParameter = false;
    bool updatingLinkedRange = false;
    bool sequencerControlsEnabled = true;
};

void CrusherAudioProcessorEditor::PresetDisplayLabel::paint(
    juce::Graphics& g)
{
    const auto bounds = getLocalBounds().toFloat().reduced(0.5f);
    g.setColour(findColour(juce::Label::backgroundColourId));
    g.fillRoundedRectangle(bounds, 3.0f);
    g.setColour(findColour(juce::Label::outlineColourId));
    g.drawRoundedRectangle(bounds, 3.0f, 1.0f);
    g.setColour(isEnabled()
        ? findColour(juce::Label::textColourId)
        : findColour(juce::Label::textColourId).darker(0.45f));
    g.setFont(getFont());
    g.drawFittedText(getText(), getLocalBounds().reduced(6, 1),
                     getJustificationType(), 1, 0.72f);
}

void CrusherAudioProcessorEditor::PresetDisplayLabel::mouseDown(
    const juce::MouseEvent& event)
{
    if (isEnabled() && event.mods.isLeftButtonDown() && onShowMenu != nullptr)
        onShowMenu();
}

void CrusherAudioProcessorEditor::PresetDisplayLabel::mouseWheelMove(
    const juce::MouseEvent&, const juce::MouseWheelDetails& wheel)
{
    if (! isEnabled() || onStep == nullptr
        || std::abs(wheel.deltaY) < 0.0001f)
        return;

    const int direction = wheel.deltaY > 0.0f ? -1 : 1;
    const auto now = juce::Time::getMillisecondCounter();
    if (direction == lastWheelDirection && now - lastWheelTimeMs < 100u)
        return;

    lastWheelDirection = direction;
    lastWheelTimeMs = now;
    onStep(direction);
}

void CrusherAudioProcessorEditor::ParameterDragLabel::setTargetAppearance(
    bool assigned, juce::Colour accent)
{
    if (targetAssigned == assigned && targetAccent == accent)
        return;

    targetAssigned = assigned;
    targetAccent = accent;
    setColour(juce::Label::textColourId,
              targetAssigned ? targetAccent : knobLabelColour);
    repaint();
}

void CrusherAudioProcessorEditor::ParameterDragLabel::paint(
    juce::Graphics& graphics)
{
    const auto parameterId = parameterIdProvider != nullptr
        ? parameterIdProvider() : juce::String();
    const bool draggable = crusher::sequencer::targetChoiceForParameter(
        parameterId) > 0;
    if (! draggable)
    {
        if (! dragHandleOnly)
            juce::Label::paint(graphics);
        return;
    }

    auto colour = targetAssigned ? targetAccent : knobLabelColour;
    if (! isEnabled())
        colour = colour.darker(0.42f);

    const auto caption = dragHandleOnly ? juce::String()
                                        : getText().trim();
    graphics.setColour(colour);
    const auto labelFont = getFont();
    graphics.setFont(labelFont);
    if (caption.isEmpty())
    {
        graphics.drawFittedText("::", getLocalBounds(),
                                juce::Justification::centred, 1, 0.72f);
        return;
    }

    // Keep the caption itself centred over its knob.  The drag marker belongs
    // to this same label component, but sits one measured space outside the
    // centred caption so it cannot pull the visible control name sideways.
    graphics.drawFittedText(caption, getLocalBounds(),
                            getJustificationType(), 1, 0.72f);
    const float captionWidth = juce::GlyphArrangement::getStringWidth(
        labelFont, caption);
    const float markerAdvance = juce::GlyphArrangement::getStringWidth(
        labelFont, "::");
    const float gap = juce::GlyphArrangement::getStringWidth(labelFont, " ");
    const float captionLeft = static_cast<float>(getWidth()) * 0.5f
                            - captionWidth * 0.5f;
    const float markerX = dragHandleRight
        ? captionLeft + captionWidth + gap
        : captionLeft - gap - markerAdvance;
    // Give the glyphs a little drawing room beyond their typographic advance;
    // otherwise JUCE can clip the outer colon at small GUI scales.
    constexpr float markerPadding = 2.0f;
    const int markerLeft = juce::roundToInt(markerX - markerPadding);
    const int markerWidth = juce::jmax(
        1, juce::roundToInt(markerAdvance + markerPadding * 2.0f));
    graphics.drawText("::", markerLeft, 0, markerWidth, getHeight(),
                      getJustificationType(), false);
}

void CrusherAudioProcessorEditor::ParameterDragLabel::mouseDrag(
    const juce::MouseEvent& event)
{
    if (event.getDistanceFromDragStart() < 4)
        return;
    const auto parameterId = parameterIdProvider != nullptr
        ? parameterIdProvider() : juce::String();
    if (crusher::sequencer::targetChoiceForParameter(parameterId) <= 0)
        return;
    if (auto* container = juce::DragAndDropContainer::findParentDragContainerFor(this))
        if (! container->isDragAndDropActive())
            container->startDragging(
                juce::var(juce::String(sequencerTargetDragPrefix) + parameterId),
                this);
}

void CrusherAudioProcessorEditor::FilterShapeButton::paintButton(
    juce::Graphics& g, bool highlighted, bool down)
{
    const bool selected = getToggleState();
    const auto fill = selected ? knobIndicatorColour.withAlpha(0.24f)
        : juce::Colour(0xff1c2229);
    g.setColour(highlighted || down ? fill.brighter(0.12f) : fill);
    g.fillRoundedRectangle(getLocalBounds().toFloat(), 2.6f);
    g.setColour(selected ? knobIndicatorColour : knobTrackColour);
    g.drawRoundedRectangle(getLocalBounds().toFloat().reduced(0.5f),
                           2.6f, selected ? 1.2f : 0.7f);
    juce::Path curve;
    if (mode == 0) // Low-pass: horizontal left, steep fall on the right.
    {
        curve.startNewSubPath(2.8f, 4.6f);
        curve.lineTo(7.0f, 4.6f);
        curve.quadraticTo(9.6f, 4.6f, 11.3f, 11.8f);
        curve.lineTo(14.2f, 12.0f);
    }
    else if (mode == 1) // Band-pass: resonant peak.
    {
        curve.startNewSubPath(2.5f, 12.0f);
        curve.lineTo(4.7f, 11.9f);
        curve.quadraticTo(7.8f, 1.4f, 9.1f, 4.9f);
        curve.quadraticTo(11.5f, 12.1f, 14.3f, 12.1f);
    }
    else // High-pass: rise on the left, horizontal at the top right.
    {
        curve.startNewSubPath(2.8f, 12.0f);
        curve.lineTo(5.2f, 11.8f);
        curve.quadraticTo(7.0f, 4.7f, 10.0f, 4.7f);
        curve.lineTo(14.2f, 4.7f);
    }
    g.setColour(selected ? knobIndicatorColour : knobLabelColour);
    g.strokePath(curve, juce::PathStrokeType(1.35f,
        juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
}

class CrusherAudioProcessorEditor::EnvMeter final : public juce::Component,
                                                      public juce::SettableTooltipClient
{
public:
    explicit EnvMeter(CrusherAudioProcessor& p) : processor(p) {}
    void paint(juce::Graphics& g) override
    {
        const auto area = getLocalBounds().toFloat();
        g.setColour(juce::Colour(0xff11171d));
        g.fillRoundedRectangle(area, 3.0f);
        g.setColour(knobTrackColour);
        g.drawRoundedRectangle(area.reduced(0.5f), 3.0f, 0.7f);
        const float value = juce::jlimit(0.0f, 1.0f, processor.getEnvelopeMeter());
        if (value > 0.00001f)
        {
            g.setColour(knobIndicatorColour);
            g.fillRoundedRectangle(area.reduced(2.0f).withWidth(
                juce::jmax(0.0f, (area.getWidth() - 4.0f) * value)), 2.0f);
        }
    }
private:
    CrusherAudioProcessor& processor;
};

// Global seven-band graph, independent of the chosen distortion model.
// A normal drag changes frequency/gain with host gestures; the wheel changes
// Q. Right-click chooses the filter shape or assigns that point's X/Y
// parameter to the existing sequencer; double-click resets the band.
class CrusherAudioProcessorEditor::EQGraph final : public juce::Component,
                                                     public juce::SettableTooltipClient,
                                                     private juce::Timer
{
public:
    explicit EQGraph(CrusherAudioProcessor& p) : processor(p)
    {
        for (int i = 0; i < crusher::eq::bandCount; ++i)
        {
            const juce::String prefix = "eq" + juce::String(i + 1);
            auto& b = handles[static_cast<size_t>(i)];
            b.frequency = processor.parameters.getParameter(prefix + "Freq");
            b.gain = processor.parameters.getParameter(prefix + "Gain");
            b.q = processor.parameters.getParameter(prefix + "Q");
            b.shape = processor.parameters.getParameter(prefix + "Shape");
        }
        setName("EQ graph");
        setTooltip("EQ GRAPH | Drag a point for frequency and gain; right-click for shape, SEQ targets and ENV targets.");
        startTimerHz(24);
    }

    void paint(juce::Graphics& g) override
    {
        auto area = graphArea();
        g.setColour(juce::Colour(0xff11171d));
        g.fillRoundedRectangle(getLocalBounds().toFloat(), 4.0f);
        g.setColour(juce::Colour(0xff39424a));
        g.drawRoundedRectangle(getLocalBounds().toFloat().reduced(0.5f), 4.0f, 1.0f);
        for (int n = -2; n <= 2; ++n)
        {
            const float y = area.getCentreY() - n * area.getHeight() / 6.0f;
            g.setColour(n == 0 ? juce::Colour(0xff58605c) : juce::Colour(0xff29323b));
            g.drawHorizontalLine(juce::roundToInt(y), area.getX(), area.getRight());
        }
        for (float hz : { 50.0f, 200.0f, 1000.0f, 5000.0f, 16000.0f })
        {
            const float x = xForFrequency(hz);
            g.setColour(juce::Colour(0xff29323b));
            g.drawVerticalLine(juce::roundToInt(x), area.getY(), area.getBottom());
        }
        g.setColour(juce::Colour(0xff7c8993));
        g.setFont(juce::Font(juce::FontOptions(8.0f)));
        g.drawFittedText("20", juce::Rectangle<int>(juce::roundToInt(area.getX()),
            juce::roundToInt(area.getBottom()) + 1, 20, 11), juce::Justification::left, 1);
        g.drawFittedText("1K", juce::Rectangle<int>(juce::roundToInt(xForFrequency(1000.0f)) - 11,
            juce::roundToInt(area.getBottom()) + 1, 22, 11), juce::Justification::centred, 1);
        g.drawFittedText("20K", juce::Rectangle<int>(juce::roundToInt(area.getRight()) - 25,
            juce::roundToInt(area.getBottom()) + 1, 25, 11), juce::Justification::right, 1);
        const double sampleRate = std::max(8000.0, processor.getSampleRate());
        std::array<crusher::eq::Coefficients, crusher::eq::bandCount> coeffs {};
        for (int i = 0; i < crusher::eq::bandCount; ++i)
            coeffs[static_cast<size_t>(i)] = crusher::eq::makeCoefficients(
                displayBand(i), sampleRate, false);
        juce::Path response;
        for (int x = 0; x <= juce::roundToInt(area.getWidth()); x += 2)
        {
            const float position = area.getX() + static_cast<float>(x);
            const float frequency = frequencyAtX(position);
            double magnitude = 1.0;
            for (const auto& coefficients : coeffs)
                magnitude *= coefficients.magnitude(frequency, sampleRate);
            const float db = juce::jlimit(-18.0f, 18.0f,
                static_cast<float>(20.0 * std::log10(std::max(1.e-10, magnitude))));
            const float y = yForGain(db);
            if (x == 0) response.startNewSubPath(position, y);
            else response.lineTo(position, y);
        }
        g.setColour(knobIndicatorColour);
        g.strokePath(response, juce::PathStrokeType(1.7f));
        constexpr std::array<juce::uint32, crusher::eq::bandCount> colours
            {{ 0xffe5b75e, 0xff66cfb2, 0xffb29bf2, 0xff6eafe4,
               0xffd8809c, 0xffa6cc68, 0xffef995b }};
        for (int i = 0; i < crusher::eq::bandCount; ++i)
        {
            const auto b = displayBand(i);
            const juce::Point<float> point(xForFrequency(b.frequency), yForGain(b.gainDb));
            const float radius = i == selectedBand ? 6.1f : 4.6f;
            g.setColour(juce::Colour(0xff171b20));
            g.fillEllipse(point.x - radius - 1.3f, point.y - radius - 1.3f,
                radius * 2.0f + 2.6f, radius * 2.0f + 2.6f);
            g.setColour(juce::Colour(colours[static_cast<size_t>(i)]));
            g.fillEllipse(point.x - radius, point.y - radius,
                radius * 2.0f, radius * 2.0f);
            if (i == selectedBand)
            {
                g.setColour(juce::Colours::white.withAlpha(0.72f));
                g.drawEllipse(point.x - radius - 2.0f, point.y - radius - 2.0f,
                    radius * 2.0f + 4.0f, radius * 2.0f + 4.0f, 0.9f);
            }
        }
    }

    void mouseDown(const juce::MouseEvent& event) override
    {
        selectedBand = nearestBand(event.position);
        if (event.mods.isRightButtonDown())
        {
            showShapeMenu();
            return;
        }
        activeFrequency = handles[static_cast<size_t>(selectedBand)].frequency;
        activeGain = handles[static_cast<size_t>(selectedBand)].gain;
        if (activeFrequency != nullptr && activeGain != nullptr)
        {
            activeFrequency->beginChangeGesture();
            activeGain->beginChangeGesture();
            dragging = true;
            mouseDrag(event);
        }
        repaint();
    }

    void mouseDrag(const juce::MouseEvent& event) override
    {
        if (! dragging) return;
        const auto area = graphArea();
        const float x = juce::jlimit(area.getX(), area.getRight(), event.position.x);
        const float y = juce::jlimit(area.getY(), area.getBottom(), event.position.y);
        writeValue(activeFrequency, frequencyAtX(x));
        writeValue(activeGain, gainAtY(y));
        repaint();
    }

    void mouseUp(const juce::MouseEvent&) override
    {
        if (! dragging) return;
        if (activeFrequency != nullptr) activeFrequency->endChangeGesture();
        if (activeGain != nullptr) activeGain->endChangeGesture();
        activeFrequency = activeGain = nullptr;
        dragging = false;
    }

    void mouseDoubleClick(const juce::MouseEvent& event) override
    {
        selectedBand = nearestBand(event.position);
        const auto index = static_cast<size_t>(selectedBand);
        auto& b = handles[index];
        setWithGesture(b.frequency, crusher::eq::defaultFrequencies[index]);
        setWithGesture(b.gain, 0.0f);
        setWithGesture(b.q, 1.0f);
        setWithGesture(b.shape, 0.0f);
        repaint();
    }

    void mouseWheelMove(const juce::MouseEvent& event,
                        const juce::MouseWheelDetails& wheel) override
    {
        if (std::abs(wheel.deltaY) < 0.0001f) return;
        selectedBand = nearestBand(event.position);
        auto* q = handles[static_cast<size_t>(selectedBand)].q;
        if (q == nullptr) return;
        const float oldQ = q->convertFrom0to1(q->getValue());
        const float next = juce::jlimit(0.35f, 8.0f, oldQ
            + (wheel.deltaY > 0.0f ? 0.15f : -0.15f));
        setWithGesture(q, next);
        repaint();
    }

    static void setWithGesture(juce::RangedAudioParameter* parameter,
                               float plainValue)
    {
        if (parameter == nullptr) return;
        parameter->beginChangeGesture();
        writeValue(parameter, plainValue);
        parameter->endChangeGesture();
    }

private:
    struct BandHandles
    {
        juce::RangedAudioParameter* frequency = nullptr;
        juce::RangedAudioParameter* gain = nullptr;
        juce::RangedAudioParameter* q = nullptr;
        juce::RangedAudioParameter* shape = nullptr;
    };

    static void writeValue(juce::RangedAudioParameter* parameter,
                           float plainValue)
    {
        if (parameter != nullptr)
            parameter->setValueNotifyingHost(
                parameter->convertTo0to1(plainValue));
    }
    static float parameterValue(juce::RangedAudioParameter* parameter) noexcept
    {
        return parameter != nullptr ? parameter->convertFrom0to1(parameter->getValue())
                                    : 0.0f;
    }
    crusher::eq::Band readBand(int index) const noexcept
    {
        const auto& b = handles[static_cast<size_t>(index)];
        return { parameterValue(b.frequency), parameterValue(b.gain),
                 parameterValue(b.q), juce::roundToInt(parameterValue(b.shape)) };
    }
    crusher::eq::Band displayBand(int index) const noexcept
    {
        auto band = readBand(index);
        const auto& handle = handles[static_cast<size_t>(index)];
        const auto prefix = "eq" + juce::String(index + 1);
        if (const auto value = processor.getSequencerAnimatedNormalised(prefix + "Freq"))
            if (handle.frequency != nullptr)
                band.frequency = handle.frequency->convertFrom0to1(*value);
        if (const auto value = processor.getSequencerAnimatedNormalised(prefix + "Gain"))
            if (handle.gain != nullptr)
                band.gainDb = handle.gain->convertFrom0to1(*value);
        return band;
    }
    juce::Rectangle<float> graphArea() const noexcept
    {
        return { 11.0f, 8.0f, static_cast<float>(juce::jmax(1, getWidth() - 22)),
                 static_cast<float>(juce::jmax(1, getHeight() - 25)) };
    }
    float xForFrequency(float hz) const noexcept
    {
        const auto area = graphArea();
        const float norm = std::log(juce::jlimit(20.0f, 20000.0f, hz) / 20.0f)
                          / std::log(1000.0f);
        return area.getX() + norm * area.getWidth();
    }
    float frequencyAtX(float x) const noexcept
    {
        const auto area = graphArea();
        const float norm = juce::jlimit(0.0f, 1.0f, (x - area.getX())
                                      / area.getWidth());
        return 20.0f * std::pow(1000.0f, norm);
    }
    float yForGain(float db) const noexcept
    {
        const auto area = graphArea();
        return area.getCentreY() - juce::jlimit(-18.0f, 18.0f, db)
            * area.getHeight() / 36.0f;
    }
    float gainAtY(float y) const noexcept
    {
        const auto area = graphArea();
        return juce::jlimit(-18.0f, 18.0f,
            (area.getCentreY() - y) * 36.0f / area.getHeight());
    }
    int nearestBand(juce::Point<float> point) const noexcept
    {
        float distance = 1.0e9f;
        int closest = 0;
        for (int i = 0; i < crusher::eq::bandCount; ++i)
        {
            const auto b = displayBand(i);
            const float dx = xForFrequency(b.frequency) - point.x;
            const float dy = yForGain(b.gainDb) - point.y;
            const float candidate = dx * dx + dy * dy;
            if (candidate < distance) { distance = candidate; closest = i; }
        }
        return closest;
    }
    // Assign an EQ coordinate just like dropping a knob label onto the
    // target list: reuse an existing slot, otherwise take the first empty one.
    // Both directions are independent and share the normal 16-slot limit.
    bool assignSequencerTarget(int band, bool vertical)
    {
        const int requested = vertical
            ? crusher::sequencer::eqVerticalTargetChoice(band)
            : crusher::sequencer::eqHorizontalTargetChoice(band);
        int freeSlot = -1;
        for (int slot = 0; slot < crusher::sequencer::targetSlotCount; ++slot)
        {
            auto* choice = processor.parameters.getParameter(
                crusher::sequencer::targetParameterId(slot));
            if (choice == nullptr)
                continue;
            const int existing = juce::roundToInt(
                choice->convertFrom0to1(choice->getValue()));
            if (existing == requested)
            {
                setParameterActualValue(processor.parameters.getParameter(
                    crusher::sequencer::targetEnabledParameterId(slot)), 1.0f);
                return true;
            }
            if (existing == 0 && freeSlot < 0)
                freeSlot = slot;
        }
        if (freeSlot < 0)
            return false;
        setParameterActualValue(processor.parameters.getParameter(
            crusher::sequencer::targetParameterId(freeSlot)),
            static_cast<float>(requested));
        setParameterActualValue(processor.parameters.getParameter(
            crusher::sequencer::targetEnabledParameterId(freeSlot)), 1.0f);
        return true;
    }

    bool isTargetAssigned(int choice) const
    {
        for (int slot = 0; slot < crusher::sequencer::targetSlotCount; ++slot)
        {
            auto* target = processor.parameters.getParameter(
                crusher::sequencer::targetParameterId(slot));
            if (target != nullptr && juce::roundToInt(
                target->convertFrom0to1(target->getValue())) == choice)
                return true;
        }
        return false;
    }

    bool hasTargetSlotAvailable() const
    {
        for (int slot = 0; slot < crusher::sequencer::targetSlotCount; ++slot)
        {
            auto* target = processor.parameters.getParameter(
                crusher::sequencer::targetParameterId(slot));
            if (target != nullptr && juce::roundToInt(
                target->convertFrom0to1(target->getValue())) == 0)
                return true;
        }
        return false;
    }

    void showShapeMenu()
    {
        juce::PopupMenu menu;
        const auto& b = handles[static_cast<size_t>(selectedBand)];
        const int active = juce::roundToInt(parameterValue(b.shape));
        const juce::StringArray names { "Bell", "Low Shelf", "High Shelf",
                                        "Notch", "Low Cut", "High Cut" };
        for (int i = 0; i < names.size(); ++i)
            menu.addItem(i + 1, names[i], true, active == i);

        menu.addSeparator();
        const int vert = crusher::sequencer::eqVerticalTargetChoice(selectedBand);
        const int horz = crusher::sequencer::eqHorizontalTargetChoice(selectedBand);
        const bool available = hasTargetSlotAvailable();
        menu.addItem(101, "Set Seq Target Gain", available || isTargetAssigned(vert),
                     isTargetAssigned(vert));
        menu.addItem(102, "Set Seq Target Freq", available || isTargetAssigned(horz),
                     isTargetAssigned(horz));
        if (! available && ! isTargetAssigned(vert) && ! isTargetAssigned(horz))
            menu.addItem(103, "All 16 target slots occupied", false);
        menu.addSeparator();
        const auto gainId = "eq" + juce::String(selectedBand + 1) + "Gain";
        const auto freqId = "eq" + juce::String(selectedBand + 1) + "Freq";
        const bool assignedGain = processor.isEnvelopeTargetAssigned(gainId);
        const bool assignedFreq = processor.isEnvelopeTargetAssigned(freqId);
        const bool envAvailable = processor.getEnvelopeAssignmentCount()
            < processor.getEnvelopeTargetLimit();
        menu.addItem(201, "Set Env Target Gain", envAvailable || assignedGain,
                     assignedGain);
        menu.addItem(202, "Set Env Target Freq", envAvailable || assignedFreq,
                     assignedFreq);
        if (assignedGain || assignedFreq)
        {
            menu.addItem(203, "Remove Env Target Gain", assignedGain);
            menu.addItem(204, "Remove Env Target Freq", assignedFreq);
        }

        const int captured = selectedBand;
        juce::Component::SafePointer<EQGraph> safe(this);
        menu.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(this),
            [safe, captured](int result)
            {
                if (safe == nullptr || result == 0)
                    return;
                safe->selectedBand = captured;
                if (result == 101 || result == 102)
                    safe->assignSequencerTarget(captured, result == 101);
                else if (result >= 201 && result <= 204)
                {
                    const auto prefix = "eq" + juce::String(captured + 1);
                    const auto targetId = prefix
                        + ((result == 201 || result == 203) ? "Gain" : "Freq");
                    if (result == 201 || result == 202)
                        safe->processor.assignEnvelopeTarget(targetId);
                    else
                        safe->processor.clearEnvelopeTarget(targetId);
                }
                else if (result >= 1 && result <= 6)
                    setWithGesture(safe->handles[static_cast<size_t>(captured)].shape,
                                   static_cast<float>(result - 1));
                safe->repaint();
            });
    }
    void timerCallback() override { if (isShowing() && ! dragging) repaint(); }

    CrusherAudioProcessor& processor;
    std::array<BandHandles, crusher::eq::bandCount> handles {};
    int selectedBand = 2;
    bool dragging = false;
    juce::RangedAudioParameter* activeFrequency = nullptr;
    juce::RangedAudioParameter* activeGain = nullptr;
};

CrusherAudioProcessorEditor::LookAndFeel::LookAndFeel(
    bool compactPopupFont)
    : useCompactPopupFont(compactPopupFont)
{
    setColour(juce::Slider::textBoxTextColourId, knobValueColour);
    setColour(juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
    setColour(juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    setColour(juce::PopupMenu::backgroundColourId, juce::Colour(0xff14181d));
    setColour(juce::PopupMenu::textColourId, knobValueColour);
    setColour(juce::PopupMenu::highlightedBackgroundColourId,
              juce::Colour(0xff2a3037));
    setColour(juce::PopupMenu::highlightedTextColourId,
              knobValueColour);
}

juce::Font CrusherAudioProcessorEditor::LookAndFeel::getPopupMenuFont()
{
    auto font = juce::LookAndFeel_V4::getPopupMenuFont();
    if (useCompactPopupFont)
        font.setHeight(juce::jmax(9.0f, font.getHeight() - 1.5f));
    font.setHeight(font.getHeight() * zoomPopupScale);
    return font;
}

juce::Font CrusherAudioProcessorEditor::LookAndFeel::getComboBoxFont(
    juce::ComboBox& box)
{
    auto font = juce::LookAndFeel_V4::getComboBoxFont(box);
    if (static_cast<bool>(box.getProperties().getWithDefault(
            "compactClosedCombo", false)))
        font.setHeight(font.getHeight() * 0.85f);
    return font;
}

juce::Label* CrusherAudioProcessorEditor::LookAndFeel::createSliderTextBox(juce::Slider& slider)
{
    auto* label = juce::LookAndFeel_V4::createSliderTextBox(slider);
    label->setFont(juce::Font(juce::FontOptions(12.5f)));
    return label;
}

void CrusherAudioProcessorEditor::LookAndFeel::drawRotarySlider(
    juce::Graphics& g, int x, int y, int width, int height,
    float position, float startAngle, float endAngle, juce::Slider& slider)
{
    if (slider.getProperties().contains("animatedNormalised"))
        position = juce::jlimit(0.0f, 1.0f, static_cast<float>(
            slider.getProperties()["animatedNormalised"]));
    const auto radius = static_cast<float>(juce::jmin(width, height)) * 0.40f + 2.0f;
    const auto centre = juce::Point<float>(static_cast<float>(x + width) * 0.5f,
                                           static_cast<float>(y + height) * 0.5f);
    const auto angle = startAngle + position * (endAngle - startAngle);
    const bool enabled = slider.isEnabled();
    const auto trackColour = enabled ? knobTrackColour : juce::Colour(0xff25292e);
    const auto bodyColour = enabled ? knobBodyColour : juce::Colour(0xff202328);
    auto indicatorColour = enabled ? slider.findColour(juce::Slider::rotarySliderFillColourId)
                                   : juce::Colour(0xff555b63);

    if (enabled && slider.isMouseOverOrDragging())
        indicatorColour = indicatorColour.brighter(0.10f);

    float fillStartAngle = startAngle;
    float fillEndAngle = angle;
    if (static_cast<bool>(slider.getProperties().getWithDefault("hasCentreDetent", false)))
    {
        const double centreDetent = static_cast<double>(slider.getProperties()
            .getWithDefault("centreDetentValue", 0.0));
        const auto normalisedCentre = static_cast<float>(slider.getNormalisableRange()
            .convertTo0to1(centreDetent));
        const float centreAngle = startAngle + normalisedCentre * (endAngle - startAngle);
        fillStartAngle = juce::jmin(angle, centreAngle);
        fillEndAngle = juce::jmax(angle, centreAngle);
    }

    constexpr float ringThickness = 2.0f;
    juce::Path track;
    track.addCentredArc(centre.x, centre.y, radius, radius,
                        0.0f, startAngle, endAngle, true);
    g.setColour(trackColour);
    g.strokePath(track, juce::PathStrokeType(ringThickness,
                                              juce::PathStrokeType::curved,
                                              juce::PathStrokeType::rounded));

    if (std::abs(fillEndAngle - fillStartAngle) > 0.0001f)
    {
        juce::Path fill;
        fill.addCentredArc(centre.x, centre.y, radius, radius,
                           0.0f, fillStartAngle, fillEndAngle, true);
        g.setColour(indicatorColour);
        g.strokePath(fill, juce::PathStrokeType(ringThickness,
                                                 juce::PathStrokeType::curved,
                                                 juce::PathStrokeType::rounded));
    }

    g.setColour(bodyColour);
    g.fillEllipse(centre.x - radius * 0.72f,
                  centre.y - radius * 0.72f,
                  radius * 1.44f,
                  radius * 1.44f);
    g.setColour(indicatorColour);
    g.drawLine(centre.x, centre.y,
               centre.x + std::sin(angle) * radius * 0.58f,
               centre.y - std::cos(angle) * radius * 0.58f,
               2.0f);
}

void CrusherAudioProcessorEditor::LookAndFeel::drawLinearSlider(
    juce::Graphics& graphics, int x, int y, int width, int height,
    float sliderPosition, float minSliderPosition, float maxSliderPosition,
    juce::Slider::SliderStyle style, juce::Slider& slider)
{
    if (slider.getProperties().contains("animatedNormalised"))
    {
        const float animated = juce::jlimit(
            0.0f, 1.0f, static_cast<float>(
                slider.getProperties()["animatedNormalised"]));
        sliderPosition = style == juce::Slider::LinearVertical
            ? static_cast<float>(y + height - 1)
                - animated * static_cast<float>(height - 2)
            : static_cast<float>(x + 1)
                + animated * static_cast<float>(width - 2);
    }
    if (style != juce::Slider::LinearVertical
        && style != juce::Slider::LinearHorizontal)
    {
        juce::LookAndFeel_V4::drawLinearSlider(
            graphics, x, y, width, height, sliderPosition,
            minSliderPosition, maxSliderPosition, style, slider);
        return;
    }

    const bool enabled = slider.isEnabled();
    const auto accent = enabled
        ? slider.findColour(juce::Slider::rotarySliderFillColourId)
        : juce::Colour(0xff555b63);
    if (style == juce::Slider::LinearHorizontal)
    {
        const float left = static_cast<float>(x + 1);
        const float right = static_cast<float>(x + width - 1);
        const float centreX = 0.5f * (left + right);
        const float centreY = static_cast<float>(y + height / 2);
        graphics.setColour(enabled
            ? juce::Colour(0xffb5bac0).withAlpha(0.86f)
            : juce::Colour(0xff555b63).withAlpha(0.58f));
        graphics.drawLine(left, centreY, right, centreY, 1.25f);
        graphics.drawLine(centreX, centreY - 4.0f,
                          centreX, centreY + 4.0f, 1.0f);
        graphics.setColour(accent);
        graphics.fillRoundedRectangle(sliderPosition - 2.0f,
                                      centreY - 6.0f,
                                      4.0f, 12.0f, 1.0f);
        return;
    }

    const float centreX = static_cast<float>(x + width / 2);
    const float top = static_cast<float>(y + 1);
    const float bottom = static_cast<float>(y + height - 1);
    const float centreY = 0.5f * (top + bottom);
    graphics.setColour(enabled
        ? knobTrackColour.withAlpha(0.90f)
        : juce::Colour(0xff555b63).withAlpha(0.58f));
    graphics.drawLine(centreX, top, centreX, bottom, 1.25f);
    graphics.setColour(accent.withAlpha(enabled ? 0.92f : 0.42f));
    graphics.drawLine(centreX, centreY, centreX, sliderPosition, 2.6f);
    graphics.setColour(enabled
        ? knobTrackColour.withAlpha(0.90f)
        : juce::Colour(0xff555b63).withAlpha(0.58f));
    graphics.drawLine(centreX - 4.0f, centreY,
                      centreX + 4.0f, centreY, 1.0f);
    graphics.setColour(accent);
    graphics.fillRoundedRectangle(centreX - 6.0f,
                                  sliderPosition - 2.0f,
                                  12.0f, 4.0f, 1.0f);
}

void CrusherAudioProcessorEditor::LookAndFeel::drawToggleButton(
    juce::Graphics& g, juce::ToggleButton& button,
    bool shouldDrawButtonAsHighlighted, bool shouldDrawButtonAsDown)
{
    const auto bounds = button.getLocalBounds().toFloat();
    const bool ledOnly = static_cast<bool>(button.getProperties().getWithDefault("ledOnly", false));
    const bool statusLedOnly = static_cast<bool>(
        button.getProperties().getWithDefault("statusLedOnly", false));
    const bool radioLed = static_cast<bool>(
        button.getProperties().getWithDefault("radioLed", false));
    const bool snowflakeIcon = static_cast<bool>(
        button.getProperties().getWithDefault("snowflakeIcon", false));
    const bool listIcon = static_cast<bool>(
        button.getProperties().getWithDefault("listIcon", false));
    const int presetArrowDirection = static_cast<int>(
        button.getProperties().getWithDefault("presetArrowDirection", 0));
    const bool saveIcon = static_cast<bool>(
        button.getProperties().getWithDefault("saveIcon", false));
    const bool saveAsIcon = static_cast<bool>(
        button.getProperties().getWithDefault("saveAsIcon", false));
    const bool folderIcon = static_cast<bool>(
        button.getProperties().getWithDefault("folderIcon", false));
    const bool rangeLinkIcon = static_cast<bool>(
        button.getProperties().getWithDefault("rangeLinkIcon", false));
    const bool randomIcon = static_cast<bool>(
        button.getProperties().getWithDefault("randomIcon", false));
    const bool mixLockIcon = static_cast<bool>(
        button.getProperties().getWithDefault("mixLockIcon", false));
    const bool sequencerLabelStyle = static_cast<bool>(
        button.getProperties().getWithDefault("sequencerLabelStyle", false));
    const bool standardTextButton = static_cast<bool>(
        button.getProperties().getWithDefault("standardTextButton", false));
    const bool filterSelectorTextButton = static_cast<bool>(
        button.getProperties().getWithDefault("filterSelectorTextButton", false));
    const bool filterSelectorThemeRed = static_cast<bool>(
        button.getProperties().getWithDefault("filterSelectorThemeRed", false));
    const bool hasPowerLed = static_cast<bool>(
        button.getProperties().getWithDefault("hasPowerLed", false));
    const bool active = button.getToggleState();
    const bool enabled = button.isEnabled();
    auto activeColour = ledOnly ? juce::Colour(0xffff9363) : controlAccentColour;
    const auto configuredColour = button.getProperties()
        .getWithDefault("activeColour", juce::var()).toString();
    if (configuredColour.isNotEmpty())
        activeColour = juce::Colour::fromString(configuredColour);

    if (shouldDrawButtonAsHighlighted)
        activeColour = activeColour.brighter(0.10f);
    if (shouldDrawButtonAsDown)
        activeColour = activeColour.darker(0.15f);

    if (radioLed)
    {
        // Simple amber radio indicator beside its text, not a power symbol.
        const auto dot = juce::Rectangle<float>(10.0f, 10.0f)
                            .withCentre(juce::Point<float>(9.0f,
                                                           bounds.getCentreY()));
        g.setColour(active ? activeColour.withAlpha(0.18f)
                           : juce::Colour(0xff24282e));
        g.fillEllipse(dot.expanded(2.0f));
        g.setColour(active ? activeColour : juce::Colour(0xff59616a));
        g.drawEllipse(dot, 1.2f);
        if (active)
            g.fillEllipse(dot.reduced(2.4f));
        g.setColour(active ? knobValueColour.brighter(0.12f)
                           : knobLabelColour);
        g.setFont(juce::Font(juce::FontOptions(9.0f, juce::Font::bold)));
        g.drawText(button.getButtonText(),
                   button.getLocalBounds().withTrimmedLeft(18),
                   juce::Justification::centredLeft, false);
        return;
    }

    if (filterSelectorTextButton)
    {
        // EQ/ENV use the theme-bar red. Other selectors, including PHAT,
        // retain their original yellow styling and 2.6px corners.
        const auto selectorColour = filterSelectorThemeRed
            ? themeDistortionRed : knobIndicatorColour;
        const auto fill = active ? selectorColour.withAlpha(0.24f)
                                 : juce::Colour(0xff1c2229);
        g.setColour(shouldDrawButtonAsHighlighted || shouldDrawButtonAsDown
                        ? fill.brighter(0.12f) : fill);
        g.fillRoundedRectangle(bounds, 2.6f);
        g.setColour(active ? selectorColour : knobTrackColour);
        g.drawRoundedRectangle(bounds.reduced(0.5f), 2.6f,
                               active ? 1.2f : 0.7f);
        g.setColour(active ? selectorColour : knobLabelColour);
        g.setFont(juce::Font(juce::FontOptions(9.0f, juce::Font::bold)));
        auto textBounds = button.getLocalBounds();
        // Reserve the left 18px for the separate, clickable EQ/ENV LED.
        if (hasPowerLed)
            textBounds = textBounds.withTrimmedLeft(18);
        g.drawFittedText(button.getButtonText(), textBounds.reduced(2, 2),
                         juce::Justification::centred, 1);
        return;
    }

    if (snowflakeIcon || listIcon || presetArrowDirection != 0
        || saveIcon || saveAsIcon || folderIcon || rangeLinkIcon
        || randomIcon || mixLockIcon || standardTextButton)
    {
        const auto buttonBounds = bounds.reduced(0.5f);
        if (! mixLockIcon)
        {
            g.setColour(active && enabled ? activeColour.darker(0.32f)
                                         : juce::Colour(0xff242a30));
            g.fillRoundedRectangle(buttonBounds, 3.0f);
            g.setColour(active && enabled ? activeColour.brighter(0.12f)
                                         : (shouldDrawButtonAsHighlighted
                                                ? knobLabelColour
                                                : juce::Colour(0xff59616a)));
            g.drawRoundedRectangle(buttonBounds, 3.0f, 1.0f);
        }

        const auto iconColour = mixLockIcon
            ? (active && enabled ? activeColour
                                 : juce::Colour(0xff69717b))
            : presetArrowDirection != 0
            ? knobIndicatorColour.withAlpha(enabled ? 0.9f : 0.2f)
            : (enabled
                   ? (active ? juce::Colours::white : knobValueColour)
                   : knobLabelColour.darker(0.45f));
        g.setColour(iconColour);
        if (mixLockIcon)
        {
            const auto centre = bounds.getCentre();
            const auto body = juce::Rectangle<float>(10.0f, 8.0f)
                .withCentre(centre.translated(0.0f, 2.5f));
            const auto shackle = juce::Rectangle<float>(7.0f, 8.0f)
                .withCentre(centre.translated(0.0f, -2.0f));
            g.drawRoundedRectangle(shackle, 3.5f, 1.5f);
            g.fillRoundedRectangle(body, 1.5f);
            g.setColour(juce::Colour(0xff242a30));
            g.fillEllipse(centre.x - 1.0f, centre.y + 1.0f,
                          2.0f, 2.0f);
        }
        else if (randomIcon)
        {
            g.setFont(juce::Font(juce::FontOptions(
                15.0f, juce::Font::bold)));
            g.drawText("!!!", button.getLocalBounds(),
                       juce::Justification::centred, false);
        }
        else if (standardTextButton)
        {
            g.setFont(juce::Font(juce::FontOptions(9.0f, juce::Font::bold)));
            g.drawFittedText(button.getButtonText(),
                             button.getLocalBounds().reduced(5, 2),
                             juce::Justification::centred, 1);
        }
        else if (rangeLinkIcon)
        {
            const auto centre = bounds.getCentre();
            const float iconScale = static_cast<bool>(
                button.getProperties().getWithDefault(
                    "compactRangeLink", false)) ? 0.75f : 1.0f;
            const auto upper = juce::Rectangle<float>(
                10.0f * iconScale, 7.0f * iconScale)
                .withCentre(centre.translated(-3.5f * iconScale,
                                              -2.5f * iconScale));
            const auto lower = juce::Rectangle<float>(
                10.0f * iconScale, 7.0f * iconScale)
                .withCentre(centre.translated(3.5f * iconScale,
                                              2.5f * iconScale));
            g.drawRoundedRectangle(upper, 3.0f * iconScale,
                                   1.4f * iconScale);
            g.drawRoundedRectangle(lower, 3.0f * iconScale,
                                   1.4f * iconScale);
            g.drawLine(centre.x - 2.0f * iconScale,
                       centre.y - 1.5f * iconScale,
                       centre.x + 2.0f * iconScale,
                       centre.y + 1.5f * iconScale,
                       1.4f * iconScale);
        }
        else if (snowflakeIcon)
        {
            const auto centre = bounds.getCentre();
            const float radius = juce::jmin(bounds.getWidth(), bounds.getHeight())
                               * 0.275f;
            for (int axis = 0; axis < 3; ++axis)
            {
                const float angle = static_cast<float>(axis)
                                  * juce::MathConstants<float>::pi / 3.0f;
                const juce::Point<float> direction(std::cos(angle),
                                                    std::sin(angle));
                g.drawLine(centre.x - direction.x * radius,
                           centre.y - direction.y * radius,
                           centre.x + direction.x * radius,
                           centre.y + direction.y * radius, 1.5f);
            }
            g.fillEllipse(centre.x - 1.25f, centre.y - 1.25f, 2.5f, 2.5f);
        }
        else if (listIcon)
        {
            const float left = bounds.getX() + bounds.getWidth() * 0.26f;
            const float lineLeft = bounds.getX() + bounds.getWidth() * 0.43f;
            const float lineRight = bounds.getRight() - bounds.getWidth() * 0.22f;
            for (int row = 0; row < 3; ++row)
            {
                const float y = bounds.getY() + bounds.getHeight()
                              * (0.31f + static_cast<float>(row) * 0.19f);
                g.fillEllipse(left - 1.25f, y - 1.25f, 2.5f, 2.5f);
                g.drawLine(lineLeft, y, lineRight, y, 1.5f);
            }
        }
        else if (presetArrowDirection != 0)
        {
            const auto centre = bounds.getCentre();
            const float arrowScale = static_cast<bool>(
                button.getProperties().getWithDefault(
                    "compactPresetArrow", false)) ? 0.75f : 1.0f;
            const float innerOffset = 2.0f * arrowScale;
            const float pointOffset = 3.0f * arrowScale;
            const float halfHeight = 7.0f * arrowScale;
            juce::Path arrow;
            if (presetArrowDirection < 0)
            {
                arrow.startNewSubPath(centre.x + innerOffset,
                                      centre.y - halfHeight);
                arrow.lineTo(centre.x - pointOffset, centre.y);
                arrow.lineTo(centre.x + innerOffset,
                             centre.y + halfHeight);
            }
            else
            {
                arrow.startNewSubPath(centre.x - innerOffset,
                                      centre.y - halfHeight);
                arrow.lineTo(centre.x + pointOffset, centre.y);
                arrow.lineTo(centre.x - innerOffset,
                             centre.y + halfHeight);
            }
            g.strokePath(arrow, juce::PathStrokeType(2.0f * arrowScale));
        }
        else if (folderIcon)
        {
            const auto icon = juce::Rectangle<float>(14.0f, 10.0f)
                .withCentre(bounds.getCentre().translated(0.0f, 1.0f));
            g.fillRoundedRectangle(icon, 1.5f);
            g.fillRect(icon.getX() + 1.5f, icon.getY() - 2.0f,
                       5.0f, 3.0f);
        }
        else
        {
            const auto icon = bounds.withSizeKeepingCentre(14.0f, 14.0f);

            g.drawRoundedRectangle(icon, 1.5f, 1.4f);
            g.fillRect(icon.getX() + 3.0f, icon.getY(),
                       icon.getWidth() - 6.0f, 5.0f);
            g.setColour(juce::Colour(0xff242a30));
            g.fillRect(icon.getX() + 5.0f, icon.getY() + 1.0f,
                       icon.getWidth() - 9.0f, 3.0f);
            g.setColour(iconColour);
            g.drawRect(icon.getX() + 3.5f, icon.getBottom() - 7.0f,
                       icon.getWidth() - 7.0f, 5.0f, 1.2f);

            if (saveAsIcon)
            {
                const auto pencilStart = juce::Point<float>(
                    icon.getRight() - 4.0f, icon.getBottom() + 2.5f);
                const auto pencilEnd = juce::Point<float>(
                    icon.getRight() + 3.0f, icon.getBottom() - 4.5f);
                g.drawLine({ pencilStart, pencilEnd }, 2.8f);
                g.fillEllipse(pencilEnd.x - 1.6f, pencilEnd.y - 1.6f,
                              3.2f, 3.2f);
                g.drawLine(pencilStart.x - 1.3f, pencilStart.y + 1.3f,
                           pencilStart.x + 1.3f, pencilStart.y - 1.3f,
                           1.3f);
            }
        }
        return;
    }

    const bool centredLed = ledOnly || statusLedOnly;
    const float diameter = centredLed
                         ? juce::jmin(bounds.getWidth(), bounds.getHeight()) - 6.0f
                         : 10.0f;
    const juce::Font toggleFont(sequencerLabelStyle
        ? juce::FontOptions(10.5f, juce::Font::bold)
        : juce::FontOptions(11.0f));
    const float textWidth = statusLedOnly ? 0.0f
        : juce::GlyphArrangement::getStringWidth(toggleFont, button.getButtonText());
    const float contentWidth = diameter + 5.0f + textWidth;
    const float contentX = bounds.getCentreX() - contentWidth * 0.5f;
    const float fixedLedCentreX = static_cast<float>(button.getProperties()
        .getWithDefault("fixedLedCentreX", -1.0f));
    const juce::Point<float> centre = centredLed
        ? bounds.getCentre()
        : juce::Point<float>(fixedLedCentreX >= 0.0f
                                 ? fixedLedCentreX
                                 : contentX + diameter * 0.5f,
                             bounds.getCentreY());
    const auto ledBounds = juce::Rectangle<float>(diameter, diameter).withCentre(centre);
    if (active && enabled)
    {
        g.setColour(activeColour.withAlpha(0.20f));
        g.fillEllipse(ledBounds.expanded(3.0f));
    }

    if (statusLedOnly)
    {
        const auto disabledActiveColour = activeColour.darker(0.72f);
        g.setColour(enabled
            ? (active ? activeColour : juce::Colour(0xff343940))
            : (active ? disabledActiveColour : juce::Colour(0xff282c31)));
        g.fillEllipse(ledBounds);
        g.setColour(enabled
            ? (active ? activeColour.brighter(0.30f) : juce::Colour(0xff596069))
            : (active ? disabledActiveColour.brighter(0.12f)
                      : juce::Colour(0xff41474e)));
        g.drawEllipse(ledBounds, 1.0f);
        if (button.getButtonText().isNotEmpty())
        {
            g.setColour(enabled
                ? (active ? juce::Colour(0xff17201b) : knobValueColour)
                : knobValueColour.darker(0.45f));
            g.setFont(juce::Font(juce::FontOptions(8.0f, juce::Font::bold)));
            g.drawText(button.getButtonText(), ledBounds.toNearestInt(),
                       juce::Justification::centred, false);
        }
        return;
    }

    if (ledOnly)
    {
        g.setColour(juce::Colour(0xff24282e));
        g.fillEllipse(ledBounds);
        g.setColour(active ? activeColour : juce::Colour(0xff69717b));
        g.drawEllipse(ledBounds, 1.0f);

        const float iconRadius = diameter * 0.285f;
        const float gapAngle = juce::MathConstants<float>::pi * 0.23f;
        juce::Path powerRing;
        powerRing.addCentredArc(centre.x, centre.y, iconRadius, iconRadius, 0.0f,
                               gapAngle,
                               juce::MathConstants<float>::twoPi - gapAngle,
                               true);
        g.strokePath(powerRing, juce::PathStrokeType(1.8f,
                                                     juce::PathStrokeType::curved,
                                                     juce::PathStrokeType::rounded));
        g.drawLine(centre.x, centre.y - iconRadius * 0.82f,
                   centre.x, centre.y + iconRadius * 0.10f, 1.8f);
        return;
    }

    g.setColour(active ? activeColour : juce::Colour(0xff343940));
    g.fillEllipse(ledBounds);
    g.setColour(active ? activeColour.brighter(0.30f) : juce::Colour(0xff596069));
    g.drawEllipse(ledBounds, 1.0f);

    g.setColour(button.isEnabled()
        ? button.findColour(juce::ToggleButton::textColourId)
        : knobLabelColour.darker(0.35f));
    g.setFont(toggleFont);
    const int textX = juce::roundToInt(fixedLedCentreX >= 0.0f
        ? centre.x + diameter * 0.5f + 5.0f
        : contentX + diameter + 5.0f);
    g.drawFittedText(button.getButtonText(), textX, 0,
                     juce::jmax(0, button.getWidth() - textX), button.getHeight(),
                     juce::Justification::centredLeft, 1);
}

void CrusherAudioProcessorEditor::LookAndFeel::drawPopupMenuItem(
    juce::Graphics& g, const juce::Rectangle<int>& area,
    bool isSeparator, bool isActive, bool isHighlighted, bool isTicked,
    bool hasSubMenu, const juce::String& text,
    const juce::String& shortcutKeyText, const juce::Drawable* icon,
    const juce::Colour* textColour)
{
    const auto insetArea = area.withTrimmedLeft(3).withTrimmedRight(3);
    juce::LookAndFeel_V4::drawPopupMenuItem(
        g, insetArea, isSeparator, isActive, isHighlighted, isTicked,
        hasSubMenu, text, shortcutKeyText, icon, textColour);
}

void CrusherAudioProcessorEditor::LookAndFeel::drawPopupMenuBackground(
    juce::Graphics& g, int width, int height)
{
    g.fillAll(juce::Colour(0xff14181d));
    g.setColour(popupBorderColour);
    g.drawRect(0, 0, width, height,
               juce::jmax(1, juce::roundToInt(2.0f * zoomPopupScale)));
}

juce::PopupMenu::Options
CrusherAudioProcessorEditor::LookAndFeel::getOptionsForComboBoxPopupMenu(
    juce::ComboBox& box, juce::Label& label)
{
    return juce::PopupMenu::Options()
        .withTargetComponent(&box)
        .withTargetScreenArea(box.getScreenBounds())
        .withPreferredPopupDirection(
            juce::PopupMenu::Options::PopupDirection::downwards)
        .withMinimumWidth(box.getWidth())
        .withMaximumNumColumns(1)
        .withStandardItemHeight(label.getHeight());
}

CrusherAudioProcessorEditor::CrusherAudioProcessorEditor(CrusherAudioProcessor& p)
    : AudioProcessorEditor(&p), processor(p)
{
    const auto storedSettings = loadInterfaceSettings();
    const auto presetsDirectory = CrusherAudioProcessor::getPresetsDirectory();
    loadFavouritePresets();
    const auto storedPresetFile = processor.getCurrentPresetFile();
    const auto storedPresetFolder = storedPresetFile.getParentDirectory();
    if (storedPresetFile != juce::File()
        && (storedPresetFolder == presetsDirectory
            || storedPresetFolder.isAChildOf(presetsDirectory)))
    {
        currentPresetFile = storedPresetFile;
        selectedPresetFolder = storedPresetFolder;
    }

    logoDrawable = juce::Drawable::createFromImageData(
        BinaryData::logo_svg, static_cast<size_t>(BinaryData::logo_svgSize));
#if defined(CRUSHER_HAS_BACKGROUND) && CRUSHER_HAS_BACKGROUND
    themeBackdrop = juce::ImageCache::getFromMemory(
        BinaryData::crusher_background_jpg,
        static_cast<size_t>(BinaryData::crusher_background_jpgSize));
#endif
    setLookAndFeel(&lookAndFeel);
    addAndMakeVisible(content);
    content.addMouseListener(this, true);
    content.setInterceptsMouseClicks(false, true);

    versionLabel.setText("v1.44.1", juce::dontSendNotification);
    versionLabel.setJustificationType(juce::Justification::centred);
    versionLabel.setColour(juce::Label::textColourId, knobIndicatorColour);
    versionLabel.setFont(juce::Font(juce::FontOptions(8.5f,
                                                     juce::Font::bold)));
    content.addAndMakeVisible(versionLabel);

    mechDial = addDial(
        "mech", "MECH",
        "Hump: asymmetric hard-knee drive before the filter.");
    tubeDial = addDial(
        "tube", "TUBE",
        "Hump: rounded parabolic saturation after the filter.");
    humpBodyDial = addDial("humpBody", "BODY", "Hump: cut or boost bass before MECH; 50% is neutral.");
    humpBiasDial = addDial("humpBias", "BIAS", "Hump: adjust asymmetric drive before MECH; 50% leaves the sound unchanged.");
    humpFoldDial = addDial("humpFold", "FOLD", "Hump: add optional waveform folding after TUBE; 0% is neutral.");
    humpToneDial = addDial("humpTone", "TONE", "Hump: darker or brighter tone after TUBE; 50% is unchanged.");
    ampSoftDial = addDial("ampSoftClip", "SOFT CLIP", "Amp-only: smooth the output distortion up to 100%.");
    ampTopDial = addDial("ampTopBoost", "TOP BOOST", "Amp-only: add strong pre-drive bite and post-drive presence.");
    ampBiasDial = addDial("ampBias", "BIAS", "Amp-only: adjust pre/post-drive asymmetry and even harmonics.");
    bumpyRippleDial = addDial("bumpyRipple", "RIPPLE", "Bumpy: vary the strength of harmonic ripples, 50% matches the original.");
    bumpyRateDial = addDial("bumpyRate", "RATE", "Bumpy: stretch or compress the ripple harmonics; 50% is neutral.");
    bumpyContourDial = addDial("bumpyContour", "CONTOUR", "Bumpy: pre-emphasise midrange before distortion; 50% is original.");
    bumpyBiasDial = addDial("bumpyBias", "BIAS", "Bumpy: adjust positive and negative wave symmetry; centre is neutral.");
    bumpyToneDial = addDial("bumpyTone", "TONE", "Bumpy: post-distortion dark/bright balance; centre is neutral.");
    combTimeDial = addDial("combTime", "TIME", "Comb: vary the short resonant delay length; centre is original.");
    combFeedbackDial = addDial("combFeedback", "FEEDBACK", "Comb: increase or decrease the resonant feedback; 50% is original.");
    combDampDial = addDial("combDamp", "DAMP", "Comb: high-frequency loss in the feedback path; 50% is original.");
    combSpreadDial = addDial("combSpread", "SPREAD", "Comb: stereo delay offset, from mono to wide; 50% is original.");
    combToneDial = addDial("combTone", "TONE", "Comb: darken or brighten the distorted output; centre is neutral.");
    crackleDensityDial = addDial("crackleDensity", "DENSITY", "Crackle: how often random crackle bursts occur; 50% is original.");
    crackleBurstDial = addDial("crackleBurst", "BURST", "Crackle: set the intensity of transient crackles; 50% is original.");
    crackleDecayDial = addDial("crackleDecay", "DECAY", "Crackle: make individual bursts shorter or longer; 50% is original.");
    crackleGritDial = addDial("crackleGrit", "GRIT", "Crackle: adjust the saturation harshness between smooth and rough; centre is original.");
    crackleToneDial = addDial("crackleTone", "TONE", "Crackle: adjust dark/bright harmonic balance after distortion; centre is neutral.");
    diodeInputHPDial = addDial("diodeInputHP", "IN HP", "Diode Clip: high-pass before saturation, shaping bass-driven distortion.");
    diodeEnvelopeDial = addDial("diodeEnvelope", "ENV", "Diode Clip: response speed of the signal-dependent clipping threshold.");
    diodeShapeDial = addDial("diodeShape", "DIODE", "Diode Clip: change the positive and negative clipping curves.");
    diodeOutputHPDial = addDial("diodeOutputHP", "OUT HP", "Diode Clip: high-pass the processed distortion to remove low frequencies.");
    diodeOutputLPDial = addDial("diodeOutputLP", "OUT LP", "Diode Clip: low-pass the processed distortion to soften top-end harmonics.");
    codecModeDial = addDial("codecMode", "CODEC", "Codec: 0-19 PCM, 20-39 mu-law, 40-59 A-law, 60-79 predictive speech, 80-100 spectral loss.");
    codecRateDial = addDial("codecRate", "RATE", "Codec: lower capture rate, from host rate to about 800 Hz.");
    codecBitsDial = addDial("codecBits", "BITS", "Codec: quantizer resolution from 2 to 12 bits.");
    codecDamageDial = addDial("codecDamage", "DAMAGE", "Codec: increase signal-dependent frame faults, grain and spectral noise fill.");
    codecToneDial = addDial("codecTone", "TONE", "Codec: dark/bright post-filter; centre is neutral.");
    vocalCharacterDial = addDial("vocalCharacter", "CHARACTER", "Vocal: morph from rounded warmth through gritty saturation to hard-edged clipping.");
    vocalBodyDial = addDial("vocalBody", "BODY", "Vocal: cut or boost low-mid body; 50% is neutral, with wider frequency reach at the extremes.");
    vocalPresenceDial = addDial("vocalPresence", "PRESENCE", "Vocal: cut or boost upper-mid intelligibility and snare attack, even with strong saturation.");
    vocalBiteDial = addDial("vocalBite", "BITE", "Vocal: sweep the upper-band edge from restrained to aggressive harmonics and attack.");
    vocalSmoothDial = addDial("vocalSmooth", "SMOOTH", "Vocal: tame upper-band harshness and progressively soften the post-distortion tone.");
    cascadeStagesDial = addDial("cascadeStages", "STAGES", "Cascade: morph between 1 and 16 consecutive saturation stages.");
    cascadeVariationDial = addDial("cascadeVariation", "VARIATION", "Cascade: move from soft to rounded or hard-edged stage combinations.");
    cascadeEmphasisDial = addDial("cascadeEmphasis", "EMPH", "Cascade: change pre-distortion emphasis from dark to forward and bright.");
    cascadeBiasDial = addDial("cascadeBias", "BIAS", "Cascade: introduce asymmetric harmonics; centre is neutral.");
    cascadePunchDial = addDial("cascadePunch", "PUNCH", "Cascade: preserve more original drum or vocal attack through the saturation chain.");
    loFiRateDial = addDial("loFiRate", "RATE", "Lo-Fi: capture-rate reduction, from host rate to 180 Hz. Right-click for PRE/POST anti-alias settings.");
    loFiBitsDial = addDial("loFiBits", "BITS", "Lo-Fi: continuous 1-bit through 16-bit quantisation.");
    loFiClipDial = addDial("loFiClip", "CLIP", "Lo-Fi: increase hard-clipping intensity before downsampling.");
    loFiRectifyDial = addDial("loFiRectify", "RECTIFY", "Lo-Fi: morph toward full-wave rectification before downsampling.");
    loFiNoiseDial = addDial("loFiNoise", "NOISE", "Lo-Fi: add controlled noise before quantisation.");
    pixelatedRateDial = addDial("pixelatedRate", "RATE", "Pixelated: sample-and-hold rate; 50% follows the original DRIVE response.");
    pixelatedBitsDial = addDial("pixelatedBits", "BITS", "Pixelated: quantisation bit depth, low for grit and high for cleaner resolution; centre is original.");
    pixelatedDitherDial = addDial("pixelatedDither", "DITHER", "Pixelated: noise added before quantisation; 50% matches the original.");
    pixelatedJitterDial = addDial("pixelatedJitter", "JITTER", "Pixelated: stutter left / irregular flutter right; 50% is original timing.");
    pixelatedToneDial = addDial("pixelatedTone", "TONE", "Pixelated: post-crush high-frequency tilt; centre is neutral.");
    rectifyWaveDial = addDial("rectifyWave", "WAVE", "Rectify: positive half-wave to original full-wave to negative half-wave; 50% is original.");
    rectifyBiasDial = addDial("rectifyBias", "BIAS", "Rectify: offset the signal before rectification for asymmetric harmonics; 50% is neutral.");
    rectifyCurveDial = addDial("rectifyCurve", "CURVE", "Rectify: change the rectification curve from firm to soft; 50% is original.");
    rectifyFoldDial = addDial("rectifyFold", "FOLD", "Rectify: fold rectified peaks back into the waveform; 0% is original.");
    rectifyToneDial = addDial("rectifyTone", "TONE", "Rectify: darken or brighten the final sound; 50% is neutral.");
    softClipThresholdDial = addDial("softClipThreshold", "THRESH", "Soft Clip: shift the clipping onset; 50% is original.");
    softClipKneeDial = addDial("softClipKnee", "KNEE", "Soft Clip: blend from rounded arctangent through original tanh to firmer clipping.");
    softClipBiasDial = addDial("softClipBias", "BIAS", "Soft Clip: vary waveform asymmetry and even harmonics; centre is neutral.");
    softClipBodyDial = addDial("softClipBody", "BODY", "Soft Clip: add or remove low frequencies before saturation; centre is neutral.");
    softClipToneDial = addDial("softClipTone", "TONE", "Soft Clip: darken or brighten post-distortion output; centre is neutral.");
    tubeBiasDial = addDial("tubeBias", "BIAS", "Tube: adjust waveform asymmetry and even harmonics; 50% is neutral.");
    tubeHeatDial = addDial("tubeHeat", "HEAT", "Tube: vary saturation intensity and harmonic compression; 50% is original.");
    tubeSagDial = addDial("tubeSag", "SAG", "Tube: control signal-responsive power sag and transient lift; 50% is neutral.");
    tubeBodyDial = addDial("tubeBody", "BODY", "Tube: cut or boost bass before the tube waveshaper; 50% is neutral.");
    tubeToneDial = addDial("tubeTone", "TONE", "Tube: adjust post-drive warmth or brightness; 50% is neutral.");
    stressedPressDial = addDial("stressedPress", "PRESS", "Stressed: vary compression pressure; centre retains the original tanh drive.");
    stressedFoldDial = addDial("stressedFold", "FOLD", "Stressed: reduce or intensify the sine-wave folding; centre is original.");
    stressedTensionDial = addDial("stressedTension", "TENSION", "Stressed: vary fold harmonic spacing and pitch character; centre is original.");
    stressedBiasDial = addDial("stressedBias", "BIAS", "Stressed: change input symmetry and even-order harmonics; centre is neutral.");
    stressedToneDial = addDial("stressedTone", "TONE", "Stressed: darken or brighten the finished distortion; centre is neutral.");
    saturationCurveDial = addDial("saturationCurve", "CURVE", "Saturation: soft arctangent to denser tanh curve, original at 50%.");
    saturationBiasDial = addDial("saturationBias", "BIAS", "Saturation: add asymmetric harmonics, 50% is neutral.");
    saturationWarmDial = addDial("saturationWarm", "WARM", "Saturation: vary harmonic drive density, 50% retains the original.");
    saturationBodyDial = addDial("saturationBody", "BODY", "Saturation: cut or boost bass before distortion, neutral at 50%.");
    saturationToneDial = addDial("saturationTone", "TONE", "Saturation: darken or brighten after distortion, neutral at 50%.");
    samuraiEdgeDial = addDial("samuraiEdge", "EDGE", "Samurai: change treble emphasis before clipping; 50% retains the original.");
    samuraiGateDial = addDial("samuraiGate", "GATE", "Samurai: adjust the dead-zone cut through the waveform; 50% is original.");
    samuraiBladeDial = addDial("samuraiBlade", "BLADE", "Samurai: control the saturation hardness; 50% retains the original.");
    samuraiRingDial = addDial("samuraiRing", "RING", "Samurai: vary metallic sine harmonics; 50% is original.");
    samuraiToneDial = addDial("samuraiTone", "TONE", "Samurai: darken or brighten after distortion; 50% is neutral.");
    razorEdgeDial = addDial("razorEdge", "EDGE", "Razor: high-frequency drive emphasis; 50% is original.");
    razorFreqDial = addDial("razorFreq", "FREQ", "Razor: move the edge-emphasis frequency; 50% is the original 2.1 kHz.");
    razorRingDial = addDial("razorRing", "RING", "Razor: vary the metallic sine harmonics; 50% is original.");
    razorBiteDial = addDial("razorBite", "BITE", "Razor: drive into the waveshaper for sharper clipping; 50% is original.");
    razorToneDial = addDial("razorTone", "TONE", "Razor: darken or brighten the processed signal; 50% is neutral.");
    lightningSparkDial = addDial("lightningSpark", "SPARK", "Lightning: set the signal-reactive electrical noise level; 50% is original.");
    lightningReactDial = addDial("lightningReact", "REACT", "Lightning: vary how strongly incoming signal level excites spark noise; 50% is original.");
    lightningClipDial = addDial("lightningClip", "CLIP", "Lightning: move the hard clipping ceiling; 50% is original.");
    lightningEdgeDial = addDial("lightningEdge", "EDGE", "Lightning: adjust optional sharper, more complex waveform harmonics; 50% is original.");
    lightningToneDial = addDial("lightningTone", "TONE", "Lightning: post-distortion dark/bright balance; 50% is unchanged.");
    freshFrequencyDial = addDial("freshFrequency", "FREQ", "Fresh: frequency where high-band excitation begins; centre preserves original 3.4 kHz.");
    freshExciteDial = addDial("freshExcite", "EXCITE", "Fresh: vary high-frequency saturation before blending; 50% is original.");
    freshAirDial = addDial("freshAir", "AIR", "Fresh: blend less or more excited high-frequency harmonics; 50% is original.");
    freshSaturationDial = addDial("freshSaturation", "SAT", "Fresh: control the final soft saturation strength; 50% is original.");
    freshToneDial = addDial("freshTone", "TONE", "Fresh: darken or brighten the sound after excitation; centre is neutral.");
    fuzzGateDial = addDial("fuzzGate", "GATE", "Fuzz: quiet-signal cutoff; centre disables gating, left soft, right hard.");
    fuzzBiasDial = addDial("fuzzBias", "BIAS", "Fuzz: move the asymmetry offset; 50% retains the original.");
    fuzzClipDial = addDial("fuzzClip", "CLIP", "Fuzz: vary clip threshold and harshness; 50% retains the original.");
    fuzzBodyDial = addDial("fuzzBody", "BODY", "Fuzz: cut or boost low frequencies before clipping; 50% is neutral.");
    fuzzToneDial = addDial("fuzzTone", "TONE", "Fuzz: shift the dark post-filter cutoff; centre retains 5.2 kHz.");
    hardClipThresholdDial = addDial("hardClipThreshold", "THRESH", "Hard Clip: adjust the clipping rails; 50% is the original threshold.");
    hardClipBiasDial = addDial("hardClipBias", "BIAS", "Hard Clip: shift clipping symmetry for stronger even harmonics; 50% is neutral.");
    hardClipKneeDial = addDial("hardClipKnee", "KNEE", "Hard Clip: softer shoulder below 50%, sharper squared clipping above; centre is original.");
    hardClipFoldDial = addDial("hardClipFold", "FOLD", "Hard Clip: fold back waveform peaks beyond the clipping rails; zero is off.");
    hardClipToneDial = addDial("hardClipTone", "TONE", "Hard Clip: darken or brighten harmonics after clipping; centre is neutral.");
    asymBiasDial = addDial("asymBias", "BIAS", "Asym: shift clipping symmetry and even-harmonic colour.");
    asymSkewDial = addDial("asymSkew", "SKEW", "Asym: balance positive and negative half-cycle saturation.");
    asymShapeDial = addDial("asymShape", "SHAPE", "Asym: move from rounded atan saturation through tanh to hard clipping.");
    asymToneDial = addDial("asymTone", "TONE", "Asym: darken or add presence after waveshaping; centre is neutral.");
    filterCutoffDial = addDial(
        "filterCutoff", "CUT",
        "Set shared LP / BP / HP frequency, 80 Hz to 16 kHz.");
    filterResonanceDial = addDial(
        "filterResonance", "RES",
        "Set shared LP / BP / HP resonance, Q from 1 to 7.");
    compAmountDial = addDial(
        "compAmount", "COMP",
        "Hump compressor input gain and soft limiting, up to 40 dB drive.");
    driveDial = addDial(
        "drive", "DIST",
        "Model-specific DIST amount for the selected original distortion type.");
    distOutputDial = addDial("distOutput", "OUTPUT",
        "Shared distortion wet level, -12 to +12 dB before EQ post and compressor. 0 dB is original.");
    distOutputDial->slider.setCentreDetent(0.0, 0.6);
    distOutputDial->slider.setDoubleClickReturnValue(true, 0.0);
    mixDial = addDial(
        "mix", "MIX",
        "Blend the dry signal with the selected distortion, filter and compressor.");
    mixDial->slider.setCentreDetent(50.0, 2.0);
    mixDial->slider.setDoubleClickReturnValue(true, 50.0);
    outputDial = addDial(
        "output", "OUT",
        "Adjust the final output level. The 0 dB position has a tactile detent.");
    outputDial->slider.setCentreDetent(0.0, 0.6);
    outputDial->slider.setDoubleClickReturnValue(true, 0.0);
    themeDial = addDial(
        "", "THEME",
        "Left: background image; centre: charcoal; right: muted distortion red.",
        false);
    themeDial->label.setJustificationType(juce::Justification::centred);
    themeDial->slider.setRange(0.0, 100.0, 0.1);
    themeDial->slider.setValue(storedSettings.themePercent,
                               juce::dontSendNotification);
    themeDial->slider.setDoubleClickReturnValue(true, 50.0);
    themeDial->slider.setTextBoxStyle(
        juce::Slider::NoTextBox, false, 0, 0);
    themeDial->slider.onValueChange = [this]
    {
        information.setText(
            "THEME | Left: image; centre: charcoal; right: muted red.",
            juce::dontSendNotification);
        repaint();
        if (sequencerPanel != nullptr)
            sequencerPanel->repaint();
    };

    browserMixDial = addDial(
        "mix", "MIX",
        "Browser copy of the main Mix control. It changes the same Mix level without becoming a sequencer target.");
    browserMixDial->parameterId.clear();
    browserMixDial->label.parameterIdProvider = {};
    browserMixDial->label.setInterceptsMouseClicks(false, false);
    browserMixDial->label.setTooltip("MIX | Adjust Mix while the preset browser is open.");
    browserMixDial->slider.setTextBoxStyle(
        juce::Slider::NoTextBox, false, 0, 0);
    browserMixDial->slider.setCentreDetent(50.0, 2.0);
    browserMixDial->slider.setDoubleClickReturnValue(true, 50.0);
    browserMixDial->slider.setTooltip(
        "MIX | Adjust Mix while the preset browser is open.");
    browserMixDial->slider.setVisible(false);
    browserMixDial->label.setVisible(false);

    const auto configureCombo = [this](SteppedComboBox& box,
                                        juce::Label& label,
                                        const juce::String& title,
                                        const juce::String& tooltip)
    {
        box.setJustificationType(juce::Justification::centred);
        box.setColour(juce::ComboBox::backgroundColourId,
                      juce::Colour(0xff19151f));
        box.setColour(juce::ComboBox::textColourId, knobValueColour);
        box.setColour(juce::ComboBox::outlineColourId, knobTrackColour);
        box.setColour(juce::ComboBox::arrowColourId, knobIndicatorColour);
        box.setScrollWheelEnabled(true);
        box.setName(title);
        box.setTooltip(tooltip);
        label.setTooltip(tooltip);
        content.addAndMakeVisible(box);
        label.setText(title.toUpperCase(), juce::dontSendNotification);
        label.setJustificationType(juce::Justification::centred);
        label.setColour(juce::Label::textColourId, knobLabelColour);
        label.setFont(juce::Font(juce::FontOptions(
            10.5f, juce::Font::bold)));
        content.addAndMakeVisible(label);
    };

    configureCombo(ampCabinetBox, ampCabinetLabel, "CAB",
        "CAB | Amp-only cabinet coloration. D.I. bypasses the cabinet stage.");
    for (const auto& name : { "D.I.", "Brit", "1x8", "1x12", "2x10",
                              "2x12", "4x10", "4x12", "1x15 Bass",
                              "4x10 Bass", "Radio", "1x10 Vintage",
                              "2x15 Bass", "8x10 Bass", "4x12 Modern",
                              "1x6 Portable" })
        ampCabinetBox.addItem(name, ampCabinetBox.getNumItems() + 1);
    ampCabinetAttachment = std::make_unique<
        juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
            processor.parameters, "ampCabinet", ampCabinetBox);
    // distModeBox remains a hidden ComboBoxAttachment bridge to the shared
    // host parameter. Two compact radio LEDs are the visible selector.
    distModeBox.addItem("STEREO", 1);
    distModeBox.addItem("MONO", 2);
    content.addChildComponent(distModeBox);
    distModeAttachment = std::make_unique<
        juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
            processor.parameters, "distMode", distModeBox);

    const juce::String modeHelp =
        "STEREO/MONO | Stereo preserves independent inputs; Mono sums left and right before any distortion engine. Some engines can still produce stereo output.";
    for (auto* button : { &distStereoButton, &distMonoButton })
    {
        button->setClickingTogglesState(false);
        button->getProperties().set("radioLed", true);
        button->getProperties().set("activeColour", linkLedColour.toString());
        button->setTooltip(modeHelp);
        content.addAndMakeVisible(*button);
    }
    // APVTS -> hidden ComboBox -> radio display, including host automation
    // and restored presets. Radio click -> ComboBox -> APVTS notification.
    const auto syncDistMode = [this]
    {
        const bool mono = distModeBox.getSelectedId() == 2;
        distMonoButton.setToggleState(mono, juce::dontSendNotification);
        distStereoButton.setToggleState(! mono, juce::dontSendNotification);
    };
    distModeBox.onChange = syncDistMode;
    distStereoButton.onClick = [this]
    {
        distModeBox.setSelectedId(1, juce::sendNotificationSync);
    };
    distMonoButton.onClick = [this]
    {
        distModeBox.setSelectedId(2, juce::sendNotificationSync);
    };
    syncDistMode();

    ampCabinetBox.setVisible(false);
    ampCabinetLabel.setVisible(false);
    distStereoButton.setVisible(false);
    distMonoButton.setVisible(false);

    // View selection and DSP enable remain independent. Each EQ/ENV
    // selector contains a separately clickable power LED on its left.
    for (auto* button : { &eqViewButton, &envViewButton })
    {
        button->setClickingTogglesState(false);
        button->getProperties().set("filterSelectorTextButton", true);
        button->getProperties().set("filterSelectorThemeRed", true);
        button->getProperties().set("hasPowerLed", true);
        content.addAndMakeVisible(*button);
    }
    eqViewButton.setTooltip("EQ VIEW | Click EQ to show its seven-band controls; click the inset LED to enable or bypass EQ.");
    envViewButton.setTooltip("ENV VIEW | Click ENV to show the envelope follower; click the inset LED to enable or bypass ENV.");
    eqViewButton.onClick = [this] { envViewActive = false; updateEQEnvView(); };
    envViewButton.onClick = [this] { envViewActive = true; updateEQEnvView(); };
    envSensitivityDial = addDial("envSensitivity", "SENS",
        "Input sensitivity (-24 to +24 dB) for envelope detection.");
    envAttackDial = addDial("envAttack", "ATTACK",
        "Envelope rise response, 1 to 200 ms.");
    envReleaseDial = addDial("envRelease", "RELEASE",
        "Envelope fall response, 20 to 1500 ms.");
    envDepthDial = addDial("envDepth", "DEPTH",
        "Positive or inverted envelope modulation, -100% to +100%.");
    envMeter = std::make_unique<EnvMeter>(processor);
    envMeter->setTooltip("ENV METER | Tracked level of the incoming pre-distortion audio.");
    content.addAndMakeVisible(*envMeter);
    envAssignmentLabel.setJustificationType(juce::Justification::centredRight);
    envAssignmentLabel.setFont(juce::Font(juce::FontOptions(9.0f)));
    envAssignmentLabel.setColour(juce::Label::textColourId, knobLabelColour);
    envAssignmentLabel.setTooltip(
        "ENV ASSIGNMENTS | Right-click any knob or EQ point to assign modulation; up to 8 targets.");
    content.addAndMakeVisible(envAssignmentLabel);

    // Global EQ. A shared on/off LED and PRE/EMPH/POST selector apply to
    // every distortion type. The graph owns per-point host gestures.
    eqOnAttachment = std::make_unique<
        juce::AudioProcessorValueTreeState::ButtonAttachment>(
            processor.parameters, "eqEnabled", eqOnButton);
    eqModeBox.addItem("PRE", 1);
    eqModeBox.addItem("EMPH", 2);
    eqModeBox.addItem("POST", 3);
    eqModeBox.setJustificationType(juce::Justification::centred);
    eqModeBox.setColour(juce::ComboBox::backgroundColourId,
                         juce::Colour(0xff19151f));
    eqModeBox.setColour(juce::ComboBox::textColourId, knobValueColour);
    eqModeBox.setColour(juce::ComboBox::outlineColourId, knobTrackColour);
    eqModeBox.setColour(juce::ComboBox::arrowColourId, knobIndicatorColour);
    eqModeBox.setTooltip(
        "EQ MODE | PRE drives distortion; EMPH compensates after distortion; POST shapes the output.");
    content.addAndMakeVisible(eqModeBox);
    eqModeAttachment = std::make_unique<
        juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
            processor.parameters, "eqMode", eqModeBox);
    // RESET is momentary; its idle artwork matches the filter selectors.
    eqResetButton.setName("Reset");
    eqResetButton.getProperties().set("filterSelectorTextButton", true);
    eqResetButton.setColour(juce::ToggleButton::textColourId, knobValueColour);
    eqResetButton.setClickingTogglesState(false);
    eqResetButton.setTooltip("EQ RESET | Reset all seven bands: frequency, gain, Q and shape.");
    eqResetButton.onClick = [this] { resetGraphicEQ(); };
    content.addAndMakeVisible(eqResetButton);
    eqGraph = std::make_unique<EQGraph>(processor);
    content.addAndMakeVisible(*eqGraph);

    // Alphabetical display order, with item IDs tied to the original
    // distortion parameter indexes (+1). Saved preset values and the DSP
    // dispatch remain unchanged even though the menu is sorted.
    distortionTypeBox.addItem("Amp", 7);
    distortionTypeBox.addItem("Asym", 9);
    distortionTypeBox.addItem("Bumpy", 8);
    distortionTypeBox.addItem("Cascade", 23);
    distortionTypeBox.addItem("Codec", 21);
    distortionTypeBox.addItem("Comb", 12);
    distortionTypeBox.addItem("Crackle", 5);
    distortionTypeBox.addItem("Diode Clip", 19);
    distortionTypeBox.addItem("Fresh", 15);
    distortionTypeBox.addItem("Fuzz", 17);
    distortionTypeBox.addItem("Hard Clip", 16);
    distortionTypeBox.addItem("Hump", 18);
    distortionTypeBox.addItem("Lightning", 6);
    distortionTypeBox.addItem("Lo-Fi", 20);
    distortionTypeBox.addItem("Pixelated", 11);
    distortionTypeBox.addItem("Razor", 10);
    distortionTypeBox.addItem("Rectify", 4);
    distortionTypeBox.addItem("Samurai", 14);
    distortionTypeBox.addItem("Saturation", 2);
    distortionTypeBox.addItem("Soft Clip", 1);
    distortionTypeBox.addItem("Stressed", 13);
    distortionTypeBox.addItem("Tube", 3);
    distortionTypeBox.addItem("Vocal", 22);
    configureCombo(distortionTypeBox, distortionTypeLabel, "Dist Type",
        "Choose a distortion alphabetically (Hump keeps its original MECH and TUBE).");
    if (auto* parameter = processor.parameters.getParameter("distortionType"))
    {
        // Derive the last dropdown ID from the host parameter rather than
        // hard-coding it: a new distortion type must remain selectable.
        const int maximumItemId = juce::roundToInt(
            parameter->convertFrom0to1(1.0f)) + 1;
        jassert(maximumItemId == distortionTypeBox.getNumItems());
        jassert(maximumItemId == static_cast<int>(distortionSummaries.size()));
        distortionTypeAttachment = std::make_unique<juce::ParameterAttachment>(
            *parameter,
            [this, maximumItemId](float value)
            {
                // Host parameter indices start at 0; dropdown IDs start at 1.
                distortionTypeBox.setSelectedId(
                    juce::jlimit(1, maximumItemId, juce::roundToInt(value) + 1),
                    juce::dontSendNotification);
                updateDistortionControls();
            }, nullptr);
    }
    distortionTypeBox.onChange = [this]
    {
        const int type = distortionTypeBox.getSelectedId() - 1;
        if (! juce::isPositiveAndBelow(type,
                static_cast<int>(distortionSummaries.size())))
            return;
        setParameterActualValue(
            processor.parameters.getParameter("distortionType"),
            static_cast<float>(type));
        updateDistortionControls();
        information.setText(
            "DIST TYPE: " + distortionTypeBox.getText() + " | "
                + distortionSummaries[static_cast<size_t>(type)] + "\n"
                + distortionDetails[static_cast<size_t>(type)],
            juce::dontSendNotification);
    };
    if (distortionTypeAttachment != nullptr)
        distortionTypeAttachment->sendInitialUpdate();

    const auto setupEffectButton = [this](
        juce::ToggleButton& button, const juce::String& name,
        const juce::String& tooltip, const juce::Colour colour)
    {
        button.setName(name);
        button.getProperties().set("standardTextButton", true);
        button.getProperties().set("activeColour", colour.toString());
        button.setTooltip(tooltip);
        button.setColour(juce::ToggleButton::textColourId, knobValueColour);
        button.setClickingTogglesState(true);
        content.addAndMakeVisible(button);
    };
    // Three independent amber power LEDs; reuse the existing sequencer
    // LED drawing code, not the rectangular text-button styling.
    const auto setupPowerLed = [this](juce::ToggleButton& button,
                                      const juce::String& name,
                                      const juce::String& tooltip)
    {
        button.setName(name);
        button.setClickingTogglesState(true);
        button.getProperties().set("statusLedOnly", true);
        button.getProperties().set("activeColour", linkLedColour.toString());
        button.setTooltip(tooltip);
        content.addAndMakeVisible(button);
    };
    setupPowerLed(eqOnButton, "Graphic EQ On",
        "EQ | Enable the seven-band graphic EQ for every distortion type.");
    setupPowerLed(distortionOnButton, "Distortion On",
        "DISTORTION | Enable or bypass the selected distortion model.");
    setupPowerLed(filterOnButton, "Filter On",
        "FILTER | Enable the shared LP / BP / HP resonant filter.");
    // An invisible choice bridge ensures host automation and preset recall
    // update the three compact selectable response icons immediately.
    filterShapeBox.addItem("LP", 1);
    filterShapeBox.addItem("BP", 2);
    filterShapeBox.addItem("HP", 3);
    filterShapeBox.onChange = [this] { updateFilterShapeIcons(); };
    filterShapeAttachment = std::make_unique<
        juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
            processor.parameters, "filterShape", filterShapeBox);
    const std::array<FilterShapeButton*, 3> filterButtons
        { &filterLowButton, &filterBandButton, &filterHighButton };
    const std::array<juce::String, 3> filterNames
        { "LP | Original resonant low-pass", "BP | Resonant band-pass",
          "HP | Resonant high-pass" };
    for (int i = 0; i < 3; ++i)
    {
        auto* button = filterButtons[static_cast<size_t>(i)];
        button->setClickingTogglesState(false);
        button->setTooltip(filterNames[static_cast<size_t>(i)]);
        button->onClick = [this, i]
        {
            filterShapeBox.setSelectedId(i + 1, juce::sendNotificationSync);
            updateFilterShapeIcons();
        };
        content.addAndMakeVisible(*button);
    }
    updateFilterShapeIcons();
    setupPowerLed(compressorOnButton, "Compressor On",
        "COMP | Engage the stereo-linked compressor.");
    setupEffectButton(phatButton, "Phat",
        "PHAT | Use direct peaks instead of the smooth envelope detector.",
        knobIndicatorColour);
    // PHAT uses the same selected yellow outline/fill as LP/BP/HP and EQ/ENV.
    phatButton.getProperties().set("standardTextButton", false);
    phatButton.getProperties().set("filterSelectorTextButton", true);
    distortionOnAttachment = std::make_unique<
        juce::AudioProcessorValueTreeState::ButtonAttachment>(
            processor.parameters, "distortionEnabled", distortionOnButton);
    filterOnAttachment = std::make_unique<
        juce::AudioProcessorValueTreeState::ButtonAttachment>(
            processor.parameters, "filterEnabled", filterOnButton);
    setupPowerLed(envOnButton, "Global Envelope On",
        "ENV | Enable audio-driven modulation for all distortion types.");
    envOnAttachment = std::make_unique<
        juce::AudioProcessorValueTreeState::ButtonAttachment>(
            processor.parameters, "envEnabled", envOnButton);
    compressorOnAttachment = std::make_unique<
        juce::AudioProcessorValueTreeState::ButtonAttachment>(
            processor.parameters, "compEnabled", compressorOnButton);
    phatAttachment = std::make_unique<
        juce::AudioProcessorValueTreeState::ButtonAttachment>(
            processor.parameters, "compPhat", phatButton);

    const auto trackLockedMix = [this](Dial& dial)
    {
        const auto existing = dial.slider.onValueChange;
        dial.slider.onValueChange = [this, &dial, existing]
        {
            if (existing != nullptr)
                existing();
            if (mixLockButton.getToggleState() && ! restoringLockedMix)
                lockedMixValue = dial.slider.getValue();
        };
    };
    trackLockedMix(*mixDial);
    trackLockedMix(*browserMixDial);
    mixLockButton.setClickingTogglesState(true);
    mixLockButton.setName("Mix Lock");
    mixLockButton.getProperties().set("mixLockIcon", true);
    mixLockButton.getProperties().set(
        "activeColour", knobIndicatorColour.toString());
    mixLockButton.setTooltip(
        "Lock the current Mix level so loading presets cannot change it");
    mixLockButton.onClick = [this]
    {
        if (mixLockButton.getToggleState() && mixDial != nullptr)
            lockedMixValue = mixDial->slider.getValue();
        information.setText(
            mixLockButton.getToggleState()
                ? "MIX LOCK: ON | Presets retain the current Mix level."
                : "MIX LOCK: OFF | Presets can load their saved Mix level.",
            juce::dontSendNotification);
    };
    content.addAndMakeVisible(mixLockButton);

    bypassButton.setName("Power");
    bypassButton.setTooltip("POWER | Bypass the Crusher effect processing.");
    bypassButton.getProperties().set("ledOnly", true);
    bypassButton.getProperties().set(
        "activeColour", powerLedColour.toString());
    content.addAndMakeVisible(bypassButton);
    if (auto* parameter = processor.parameters.getParameter("bypass"))
        bypassAttachment = std::make_unique<juce::ParameterAttachment>(
            *parameter,
            [this](float value)
            {
                bypassButton.setToggleState(
                    value < 0.5f, juce::dontSendNotification);
            });
    bypassButton.onClick = [this]
    {
        // The host parameter is Bypass, while the UI presents Power.
        if (auto* parameter = processor.parameters.getParameter("bypass"))
        {
            parameter->beginChangeGesture();
            parameter->setValueNotifyingHost(
                bypassButton.getToggleState() ? 0.0f : 1.0f);
            parameter->endChangeGesture();
        }
    };
    if (bypassAttachment != nullptr)
        bypassAttachment->sendInitialUpdate();

    presetBrowserButton.setName("Preset Browser");
    presetBrowserButton.setTooltip("Open or close the preset browser");
    presetBrowserButton.getProperties().set("listIcon", true);
    presetBrowserButton.getProperties().set(
        "activeColour", presetAccentColour.toString());
    presetBrowserButton.setClickingTogglesState(true);
    presetBrowserButton.onClick = [this]
    {
        setPresetBrowserOpen(presetBrowserButton.getToggleState());
    };
    content.addAndMakeVisible(presetBrowserButton);

    const auto configurePresetStepButton = [this](
        juce::ToggleButton& button, int direction,
        const juce::String& description)
    {
        button.setName(description);
        button.setTooltip(description);
        button.getProperties().set("presetArrowDirection", direction);
        button.setClickingTogglesState(false);
        content.addAndMakeVisible(button);
    };
    configurePresetStepButton(previousPresetButton, -1, "Previous preset");
    configurePresetStepButton(nextPresetButton, 1, "Next preset");
    previousPresetButton.onClick = [this] { stepPreset(-1); };
    nextPresetButton.onClick = [this] { stepPreset(1); };

    currentPresetLabel.setJustificationType(juce::Justification::centred);
    currentPresetLabel.setColour(juce::Label::backgroundColourId,
                                 juce::Colour(0xff14181d));
    currentPresetLabel.setColour(juce::Label::outlineColourId,
                                 knobTrackColour);
    currentPresetLabel.setColour(juce::Label::textColourId,
                                 knobValueColour);
    currentPresetLabel.setFont(juce::Font(
        juce::FontOptions(10.0f, juce::Font::bold)));
    currentPresetLabel.setMinimumHorizontalScale(0.68f);
    currentPresetLabel.setInterceptsMouseClicks(true, false);
    currentPresetLabel.setName("Current Preset");
    currentPresetLabel.onStep = [this](int direction) { stepPreset(direction); };
    currentPresetLabel.onShowMenu = [this] { showPresetQuickMenu(); };
    content.addAndMakeVisible(currentPresetLabel);

    const auto configureBrowserHeading = [this](
        juce::Label& label, const juce::String& text)
    {
        label.setText(text, juce::dontSendNotification);
        label.setJustificationType(juce::Justification::centredLeft);
        label.setColour(juce::Label::textColourId, knobLabelColour);
        label.setFont(juce::Font(juce::FontOptions(
            9.5f, juce::Font::bold)));
        content.addChildComponent(label);
    };
    configureBrowserHeading(presetFolderLabel, "FOLDERS");
    configureBrowserHeading(presetListLabel, "PRESETS");
    presetFolderTree.setRootItemVisible(false);
    presetFolderTree.setDefaultOpenness(false);
    presetFolderTree.setIndentSize(14);
    presetFolderTree.setColour(juce::TreeView::backgroundColourId,
                               juce::Colour(0xff11151a));
    presetFolderTree.setColour(juce::TreeView::linesColourId,
                               knobTrackColour);
    presetFolderTree.setColour(
        juce::TreeView::selectedItemBackgroundColourId,
        juce::Colours::transparentBlack);
    content.addChildComponent(presetFolderTree);
    presetList.setRowHeight(22);
    presetList.setColour(juce::ListBox::backgroundColourId,
                         juce::Colour(0xff11151a));
    presetList.setColour(juce::ListBox::outlineColourId, knobTrackColour);
    presetList.setOutlineThickness(1);
    presetList.onHorizontalStep = [this](int direction)
    {
        return handlePresetCursorKey(direction);
    };
    content.addChildComponent(presetList);

    const auto configureBrowserActionButton = [this](
        juce::ToggleButton& button, const juce::String& name,
        const juce::Identifier& icon)
    {
        button.setName(name);
        button.setTooltip(name);
        button.getProperties().set(icon, true);
        button.getProperties().set(
            "activeColour", presetAccentColour.toString());
        button.setClickingTogglesState(false);
        content.addChildComponent(button);
    };
    configureBrowserActionButton(
        presetSaveButton, "Save preset", juce::Identifier("saveIcon"));
    configureBrowserActionButton(
        presetSaveAsButton, "Save preset as", juce::Identifier("saveAsIcon"));
    configureBrowserActionButton(
        presetOpenFolderButton, "Open current preset folder",
        juce::Identifier("folderIcon"));
    presetSaveButton.onClick = [this] { saveCurrentPreset(); };
    presetSaveAsButton.onClick = [this] { showPresetSaveAs(); };
    presetOpenFolderButton.onClick = [this]
    {
        const auto root = CrusherAudioProcessor::getPresetsDirectory();
        auto folder = selectedPresetFolder;
        if (! folder.isDirectory()
            || (folder != root && ! folder.isAChildOf(root)))
            folder = root;
        if (! folder.startAsProcess())
            information.setText(
                "PRESET FOLDER | The current folder could not be opened.",
                juce::dontSendNotification);
    };

    randomButton.setClickingTogglesState(false);
    randomButton.setName("Randomise");
    randomButton.getProperties().set("randomIcon", true);
    randomButton.getProperties().set(
        "activeColour", knobIndicatorColour.toString());
    randomButton.setTooltip(
        "RANDOM | Vary Mech, Tube and Dist without changing the sequencer or filter/compressor switches.");
    randomButton.onClick = [this] { randomiseEffectParameters(); };
    content.addAndMakeVisible(randomButton);

    information.setText(
        "CRUSHER 0.1.15 | Comb controls and shared EQ.\nRight-click an EQ point to change its shape.",
        juce::dontSendNotification);
    information.setColour(juce::Label::textColourId,
                          juce::Colours::lightgrey);
    information.setJustificationType(juce::Justification::centredLeft);
    information.setFont(juce::FontOptions(11.5f));
    information.setMinimumHorizontalScale(0.82f);
    content.addAndMakeVisible(information);

    sequencerPanel = std::make_unique<SequencerPanel>(
        processor, lookAndFeel,
        [this] { return panelColour(1); },
        themeBackdrop,
        [this] {
            return themeImageOpacity(themeDial != nullptr
                ? themeDial->slider.getValue() : 50.0);
        },
        [this](const juce::String& helpText)
        {
            information.setText(formatControlHelp(helpText),
                                juce::dontSendNotification);
        });
    content.addAndMakeVisible(*sequencerPanel);
    sequencerPanel->setAnimate(storedSettings.sequencerAnimate);
    sequencerPanel->onAnimateChanged = [this]
    {
        updateAnimatedParameterDisplays();
        saveInterfaceSettings();
    };
    sequencerOpen = storedSettings.sequencerOpen;
    sequencerPanel->setVisible(sequencerOpen);

    sequencerButton.setClickingTogglesState(true);
    sequencerButton.setName("Sequencer");
    sequencerButton.setToggleState(sequencerOpen,
                                   juce::dontSendNotification);
    sequencerButton.getProperties().set("standardTextButton", true);
    sequencerButton.getProperties().set(
        "activeColour", controlAccentColour.toString());
    sequencerButton.setTooltip("Show or hide the sequencer");
    sequencerButton.onClick = [this]
    {
        setSequencerOpen(sequencerButton.getToggleState());
    };
    content.addAndMakeVisible(sequencerButton);

    updateSequencerTargetHighlights();
    updatePresetNavigationDisplay();
    updatePresetBrowserVisibility();
    setResizable(true, true);
    setResizeLimits(695, sequencerOpen ? 414 : 207,
                    2778, sequencerOpen ? 1590 : 828);
    if (auto* editorConstrainer = getConstrainer())
        editorConstrainer->setFixedAspectRatio(
            static_cast<double>(designWidth)
                / static_cast<double>(activeDesignHeight()));
    const float scale = static_cast<float>(storedSettings.scalePercent) * 0.01f;
    setSize(juce::roundToInt(static_cast<float>(designWidth) * scale),
            juce::roundToInt(static_cast<float>(activeDesignHeight()) * scale));
    observedEditorWidth = getWidth();
    observedThemeValue = themeDial->slider.getValue();
    saveInterfaceSettings();
    startTimerHz(30);
}

CrusherAudioProcessorEditor::~CrusherAudioProcessorEditor()
{
    stopTimer();
    saveInterfaceSettings();
    if (presetFolderNameWindow != nullptr)
    {
        presetFolderNameWindow->exitModalState(0);
        presetFolderNameWindow->setLookAndFeel(nullptr);
        presetFolderNameWindow.reset();
    }
    content.removeMouseListener(this);
    presetFolderTree.setRootItem(nullptr);
    presetFolderRoot.reset();
    setLookAndFeel(nullptr);
}

bool CrusherAudioProcessorEditor::keyPressed(const juce::KeyPress& key)
{
    if (key.getKeyCode() == juce::KeyPress::leftKey)
        return handlePresetCursorKey(-1);
    if (key.getKeyCode() == juce::KeyPress::rightKey)
        return handlePresetCursorKey(1);
    return juce::AudioProcessorEditor::keyPressed(key);
}

void CrusherAudioProcessorEditor::mouseDown(const juce::MouseEvent& event)
{
    const auto position = event.getEventRelativeTo(&content).position;
    if (event.mods.isLeftButtonDown()
        && juce::Rectangle<float>(22.0f, 5.0f, 280.0f, 43.0f)
               .contains(position))
    {
        showZoomMenu(content.localPointToGlobal(
            juce::Point<int>(22, 48)));
        return;
    }
    if (! presetBrowserOpen || ! event.mods.isPopupMenu())
        return;
    for (auto* component = event.eventComponent; component != nullptr;
         component = component->getParentComponent())
    {
        if (component != &presetList)
            continue;
        const auto relative = event.getEventRelativeTo(&presetList);
        const int row = presetList.getRowContainingPosition(
            juce::roundToInt(relative.position.x),
            juce::roundToInt(relative.position.y));
        if (row < 0)
            showPresetContextMenu({}, event.getScreenPosition());
        return;
    }
}

void CrusherAudioProcessorEditor::showZoomMenu(juce::Point<int> screenPosition)
{
    juce::PopupMenu menu;
    const int currentPercent = juce::roundToInt(
        100.0f * static_cast<float>(getWidth())
               / static_cast<float>(designWidth));
    const float menuScale = juce::jlimit(0.75f, 3.0f,
        static_cast<float>(getWidth()) / static_cast<float>(designWidth));
    zoomMenuLookAndFeel.setZoomPopupScale(menuScale);
    zoomMenuLookAndFeel.setPopupBorderColour(lookAndFeel.getPopupBorderColour());
    menu.setLookAndFeel(&zoomMenuLookAndFeel);
    for (const int percent : { 75, 100, 125, 150, 175, 200,
                               225, 250, 275, 300 })
        menu.addItem(percent, juce::String(percent) + "%", true,
                     currentPercent == percent);
    // The logo occupies local x=22..302, y=5..48. Anchor the popup's
    // upper-left edge to its bottom-left corner, at any supported zoom.
    menu.showMenuAsync(juce::PopupMenu::Options()
                           .withTargetScreenArea({ screenPosition.x,
                                                   screenPosition.y, 1, 1 })
                           .withMinimumWidth(juce::roundToInt(116.0f * menuScale))
                           .withStandardItemHeight(juce::roundToInt(23.0f * menuScale))
                           .withPreferredPopupDirection(
                               juce::PopupMenu::Options::PopupDirection::downwards),
        [safeThis = juce::Component::SafePointer<CrusherAudioProcessorEditor>(this)]
        (int selected)
        {
            if (safeThis == nullptr || selected < 75 || selected > 300)
                return;
            const float scale = static_cast<float>(selected) * 0.01f;
            safeThis->setSize(
                juce::roundToInt(static_cast<float>(designWidth) * scale),
                juce::roundToInt(static_cast<float>(
                    safeThis->activeDesignHeight()) * scale));
            safeThis->saveInterfaceSettings();
        });
}

void CrusherAudioProcessorEditor::mouseEnter(const juce::MouseEvent& event)
{
    const auto isWithin = [](juce::Component* component,
                             const juce::Component& owner)
    {
        for (auto* current = component; current != nullptr;
             current = current->getParentComponent())
            if (current == &owner)
                return true;
        return false;
    };
    for (const auto& dial : dials)
        if (isWithin(event.eventComponent, dial->slider)
            || isWithin(event.eventComponent, dial->label))
        {
            information.setText(
                formatControlHelp(dial->label.getText() + " | " + dial->help),
                juce::dontSendNotification);
            return;
        }
    if (sequencerPanel != nullptr
        && isWithin(event.eventComponent, *sequencerPanel))
        return;
    for (auto* component = event.eventComponent;
         component != nullptr && component != &content;
         component = component->getParentComponent())
        if (auto* tooltip = dynamic_cast<juce::TooltipClient*>(component))
            if (tooltip->getTooltip().isNotEmpty())
            {
                auto title = component->getName().trim();
                if (title.isEmpty())
                    title = "Crusher";
                information.setText(
                    formatControlHelp(tooltip->getTooltip().contains(" | ")
                        ? tooltip->getTooltip()
                        : title.toUpperCase() + " | " + tooltip->getTooltip()),
                    juce::dontSendNotification);
                return;
            }
}

CrusherAudioProcessorEditor::Dial* CrusherAudioProcessorEditor::addDial(
    const char* id, const char* caption, const char* description,
    bool attachToParameter)
{
    auto dial = std::make_unique<Dial>();
    dial->help = description;
    dial->parameterId = attachToParameter ? juce::String(id)
                                          : juce::String();
    dial->slider.setSliderStyle(
        juce::Slider::RotaryHorizontalVerticalDrag);
    dial->slider.setTextBoxStyle(
        juce::Slider::TextBoxBelow, false, 72, 18);
    dial->slider.setMouseDragSensitivity(80);
    dial->slider.setColour(
        juce::Slider::rotarySliderFillColourId, knobIndicatorColour);
    dial->slider.setColour(
        juce::Slider::textBoxTextColourId, knobValueColour);
    dial->slider.setColour(juce::Slider::textBoxBackgroundColourId,
                           juce::Colours::transparentBlack);
    dial->slider.setColour(juce::Slider::textBoxOutlineColourId,
                           juce::Colours::transparentBlack);
    dial->label.setText(caption, juce::dontSendNotification);
    auto* pointer = dial.get();
    dial->label.parameterIdProvider = [pointer]
    {
        return pointer->parameterId;
    };
    const auto contextAction = [this, pointer](juce::Point<int> screen)
    {
        showParameterEnvMenu(pointer->parameterId, screen);
    };
    dial->label.onContextMenu = contextAction;
    dial->slider.onContextMenu = contextAction;
    const juce::String sharedTooltip = juce::String(caption)
        + " | " + juce::String(description);
    dial->label.setTooltip(sharedTooltip);
    dial->slider.setTooltip(sharedTooltip);
    dial->label.setJustificationType(juce::Justification::centredTop);
    dial->label.setInterceptsMouseClicks(true, false);
    dial->label.setColour(juce::Label::textColourId, knobLabelColour);
    dial->label.setFont(juce::Font(juce::FontOptions(
        10.5f, juce::Font::bold)));
    content.addAndMakeVisible(dial->slider);
    content.addAndMakeVisible(dial->label);
    if (attachToParameter)
        dial->attachment = std::make_unique<SliderAttachment>(
            processor.parameters, id, dial->slider);
    if (attachToParameter)
        pointer->slider.onSavedStateReset = [this, pointer]
        {
            return restoreDialFromSavedState(*pointer);
        };
    pointer->slider.onValueChange = [this, pointer]
    {
        information.setText(
            formatControlHelp(pointer->label.getText() + " | " + pointer->help),
            juce::dontSendNotification);
    };
    dials.push_back(std::move(dial));
    return pointer;
}

bool CrusherAudioProcessorEditor::restoreDialFromSavedState(Dial& dial)
{
    if (dial.parameterId.isEmpty())
        return false;
    const auto saved = processor.getSavedParameterValue(dial.parameterId);
    if (! saved.has_value())
        return false;
    dial.slider.setValue(*saved, juce::sendNotificationSync);
    information.setText(
        "RESTORED: " + dial.label.getText() + " = "
            + dial.slider.getTextFromValue(dial.slider.getValue())
            + "\nReturned to the value stored in the current saved state.",
        juce::dontSendNotification);
    return true;
}

juce::Colour CrusherAudioProcessorEditor::filterAccent(int filterIndex) const
{
    return filterColourForId(filterIndex, 0);
}

juce::Colour CrusherAudioProcessorEditor::panelColour(int filterIndex) const
{
    juce::ignoreUnused(filterIndex);
    return themeSolidColour(themeDial != nullptr
        ? themeDial->slider.getValue() : 50.0);
}

void CrusherAudioProcessorEditor::updateContextSensitiveHelp()
{
    repaint();
}

void CrusherAudioProcessorEditor::updateSectionDimming()
{
    // Only the four effect sections dim. Keep their amber ON/OFF LEDs fully
    // visible and clickable, and leave MIX/OUT, browser and sequencer alone.
    // Dimming is visual: a disabled effect's settings remain editable.
    const int mask = (eqOnButton.getToggleState() ? 1 : 0)
                   | (distortionOnButton.getToggleState() ? 2 : 0)
                   | (filterOnButton.getToggleState() ? 4 : 0)
                   | (compressorOnButton.getToggleState() ? 8 : 0)
                   | (envOnButton.getToggleState() ? 16 : 0);
    if (mask == lastSectionDimmingMask)
        return;
    lastSectionDimmingMask = mask;
    constexpr float dimmed = 0.32f;
    const auto dim = [dimmed](juce::Component& control, bool on)
    {
        control.setAlpha(on ? 1.0f : dimmed);
    };
    const auto dimDial = [&dim](Dial* dial, bool on)
    {
        if (dial != nullptr)
        {
            dim(dial->slider, on);
            dim(dial->label, on);
        }
    };
    const bool eqOn = (mask & 1) != 0;
    dim(eqModeBox, eqOn);
    dim(eqResetButton, eqOn);
    if (eqGraph != nullptr) dim(*eqGraph, eqOn);
    const bool distOn = (mask & 2) != 0;
    dim(distortionTypeBox, distOn);
    dim(distortionTypeLabel, distOn);
    dim(ampCabinetBox, distOn);
    dim(ampCabinetLabel, distOn);
    dim(distStereoButton, distOn);
    dim(distMonoButton, distOn);
    dimDial(distOutputDial, distOn);
    for (auto* dial : { driveDial, mechDial, tubeDial,
                        humpBodyDial, humpBiasDial, humpFoldDial,
                        humpToneDial, ampSoftDial, ampTopDial, ampBiasDial, bumpyRippleDial, bumpyRateDial, bumpyContourDial,
                        bumpyBiasDial, bumpyToneDial, combTimeDial, combFeedbackDial, combDampDial,
                        combSpreadDial, combToneDial, crackleDensityDial, crackleBurstDial, crackleDecayDial,
                        crackleGritDial, crackleToneDial, diodeInputHPDial, diodeEnvelopeDial,
                        diodeShapeDial, diodeOutputHPDial, diodeOutputLPDial, codecModeDial, codecRateDial, codecBitsDial, codecDamageDial, codecToneDial, vocalCharacterDial, vocalBodyDial, vocalPresenceDial, vocalBiteDial, vocalSmoothDial, cascadeStagesDial, cascadeVariationDial, cascadeEmphasisDial, cascadeBiasDial, cascadePunchDial, loFiRateDial, loFiBitsDial, loFiClipDial, loFiRectifyDial, loFiNoiseDial, lightningSparkDial, lightningReactDial, lightningClipDial,
                        lightningEdgeDial, lightningToneDial, pixelatedRateDial, pixelatedBitsDial, pixelatedDitherDial,
                        pixelatedJitterDial, pixelatedToneDial, rectifyWaveDial, rectifyBiasDial, rectifyCurveDial,
                        rectifyFoldDial, rectifyToneDial, softClipThresholdDial, softClipKneeDial, softClipBiasDial,
                        softClipBodyDial, softClipToneDial, tubeBiasDial, tubeHeatDial, tubeSagDial,
                        tubeBodyDial, tubeToneDial, stressedPressDial, stressedFoldDial, stressedTensionDial,
                        stressedBiasDial, stressedToneDial, saturationCurveDial, saturationBiasDial, saturationWarmDial,
                        saturationBodyDial, saturationToneDial, samuraiEdgeDial, samuraiGateDial, samuraiBladeDial,
                        samuraiRingDial, samuraiToneDial, razorEdgeDial, razorFreqDial, razorRingDial,
                        razorBiteDial, razorToneDial, freshFrequencyDial, freshExciteDial, freshAirDial,
                        freshSaturationDial, freshToneDial, fuzzGateDial, fuzzBiasDial, fuzzClipDial,
                        fuzzBodyDial, fuzzToneDial, hardClipThresholdDial, hardClipBiasDial, hardClipKneeDial,
                        hardClipFoldDial, hardClipToneDial, asymBiasDial, asymSkewDial, asymShapeDial,
                        asymToneDial })
        dimDial(dial, distOn);
    const bool filterOn = (mask & 4) != 0;
    dimDial(filterCutoffDial, filterOn);
    dimDial(filterResonanceDial, filterOn);
    for (auto* icon : { &filterLowButton, &filterBandButton, &filterHighButton })
        dim(*icon, filterOn);
    const bool envOn = envOnButton.getToggleState();
    for (auto* dial : { envSensitivityDial, envAttackDial,
                        envReleaseDial, envDepthDial })
        dimDial(dial, envOn);
    dim(envAssignmentLabel, envOn);
    if (envMeter != nullptr) dim(*envMeter, envOn);
    const bool compOn = (mask & 8) != 0;
    dimDial(compAmountDial, compOn);
    dim(phatButton, compOn);
    // Section titles are painted in the editor itself; repaint them too.
    repaint();
}

void CrusherAudioProcessorEditor::updateFilterShapeIcons()
{
    const int selected = filterShapeBox.getSelectedId();
    std::array<FilterShapeButton*, 3> buttons
        { &filterLowButton, &filterBandButton, &filterHighButton };
    for (int i = 0; i < 3; ++i)
    {
        buttons[static_cast<size_t>(i)]->setToggleState(
            selected == i + 1, juce::dontSendNotification);
        buttons[static_cast<size_t>(i)]->repaint();
    }
}

void CrusherAudioProcessorEditor::updateEQEnvView()
{
    const bool eqVisible = ! presetBrowserOpen && ! envViewActive;
    const bool envVisible = ! presetBrowserOpen && envViewActive;
    eqViewButton.setVisible(! presetBrowserOpen);
    envViewButton.setVisible(! presetBrowserOpen);
    eqViewButton.setToggleState(! envViewActive, juce::dontSendNotification);
    envViewButton.setToggleState(envViewActive, juce::dontSendNotification);
    // Each independent APVTS-attached power LED sits over its own view
    // button; click on the LED toggles DSP, elsewhere selects the view.
    eqOnButton.setVisible(! presetBrowserOpen);
    eqModeBox.setVisible(eqVisible);
    eqResetButton.setVisible(eqVisible);
    if (eqGraph != nullptr) eqGraph->setVisible(eqVisible);
    envOnButton.setVisible(! presetBrowserOpen);
    // Keep both inset LEDs above the selector hit areas and artwork.
    if (! presetBrowserOpen)
    {
        eqOnButton.toFront(false);
        envOnButton.toFront(false);
    }
    if (envMeter != nullptr) envMeter->setVisible(envVisible);
    envAssignmentLabel.setVisible(envVisible);
    for (auto* dial : { envSensitivityDial, envAttackDial,
                        envReleaseDial, envDepthDial })
    {
        if (dial == nullptr) continue;
        dial->label.setVisible(envVisible);
        dial->slider.setVisible(envVisible);
    }
    repaint();
}

void CrusherAudioProcessorEditor::showParameterEnvMenu(
    const juce::String& parameterId, juce::Point<int> screenPosition)
{
    if (crusher::sequencer::targetChoiceForParameter(parameterId) <= 0)
        return;
    const bool assigned = processor.isEnvelopeTargetAssigned(parameterId);
    const bool available = processor.getEnvelopeAssignmentCount()
        < processor.getEnvelopeTargetLimit();
    juce::PopupMenu menu;
    menu.addItem(1, "Set Env Target", available || assigned, assigned);
    menu.addItem(2, "Remove Env Target", assigned);
    menu.addSeparator();
    menu.addItem(3, "Clear All Env Targets",
                 processor.getEnvelopeAssignmentCount() > 0);
    const bool loFiRateMenu = parameterId == "loFiRate"
        && distortionTypeBox.getSelectedId() == 20;
    if (loFiRateMenu)
    {
        // Keep the six-knob grid unchanged: the AA stages are available in
        // RATE's existing right-click menu and through host automation.
        juce::PopupMenu pre, post;
        const auto* preParam = processor.parameters.getRawParameterValue("loFiPreAA");
        const auto* postParam = processor.parameters.getRawParameterValue("loFiPostAA");
        const auto addLevels = [](juce::PopupMenu& submenu, int base,
                                  float current)
        {
            for (const int level : { 0, 25, 50, 75, 100 })
                submenu.addItem(base + level,
                    level == 0 ? "OFF" : juce::String(level) + "%",
                    true, juce::roundToInt(current / 25.0f) * 25 == level);
        };
        addLevels(pre, 1000, preParam != nullptr ? preParam->load() : 0.0f);
        addLevels(post, 2000, postParam != nullptr ? postParam->load() : 0.0f);
        menu.addSeparator();
        menu.addSubMenu("PRE Anti-Alias", pre);
        menu.addSubMenu("POST Anti-Alias", post);
        menu.addSeparator();
        const bool preAssigned = processor.isEnvelopeTargetAssigned("loFiPreAA");
        const bool postAssigned = processor.isEnvelopeTargetAssigned("loFiPostAA");
        const bool freeSlot = processor.getEnvelopeAssignmentCount()
            < processor.getEnvelopeTargetLimit();
        menu.addItem(4, "Set Env Target PRE AA", freeSlot || preAssigned, preAssigned);
        menu.addItem(5, "Set Env Target POST AA", freeSlot || postAssigned, postAssigned);
        menu.addItem(6, "Remove Env Target PRE AA", preAssigned);
        menu.addItem(7, "Remove Env Target POST AA", postAssigned);
    }
    juce::Component::SafePointer<CrusherAudioProcessorEditor> safe(this);
    menu.showMenuAsync(juce::PopupMenu::Options().withTargetScreenArea(
        juce::Rectangle<int>(screenPosition.x, screenPosition.y, 1, 1)),
        [safe, parameterId](int result)
        {
            if (safe == nullptr || result <= 0) return;
            if (result >= 1000 && result <= 1100)
                setParameterActualValue(
                    safe->processor.parameters.getParameter("loFiPreAA"),
                    static_cast<float>(result - 1000));
            else if (result >= 2000 && result <= 2100)
                setParameterActualValue(
                    safe->processor.parameters.getParameter("loFiPostAA"),
                    static_cast<float>(result - 2000));
            else if (result == 1) safe->processor.assignEnvelopeTarget(parameterId);
            else if (result == 2) safe->processor.clearEnvelopeTarget(parameterId);
            else if (result == 3) safe->processor.clearAllEnvelopeTargets();
            else if (result == 4) safe->processor.assignEnvelopeTarget("loFiPreAA");
            else if (result == 5) safe->processor.assignEnvelopeTarget("loFiPostAA");
            else if (result == 6) safe->processor.clearEnvelopeTarget("loFiPreAA");
            else if (result == 7) safe->processor.clearEnvelopeTarget("loFiPostAA");
            safe->updateSequencerTargetHighlights();
        });
}

void CrusherAudioProcessorEditor::resetGraphicEQ()
{
    for (int i = 0; i < crusher::eq::bandCount; ++i)
    {
        const juce::String prefix = "eq" + juce::String(i + 1);
        EQGraph::setWithGesture(processor.parameters.getParameter(prefix + "Freq"),
            crusher::eq::defaultFrequencies[static_cast<size_t>(i)]);
        EQGraph::setWithGesture(processor.parameters.getParameter(prefix + "Gain"), 0.0f);
        EQGraph::setWithGesture(processor.parameters.getParameter(prefix + "Q"), 1.0f);
        EQGraph::setWithGesture(processor.parameters.getParameter(prefix + "Shape"), 0.0f);
    }
    information.setText(
        "EQ RESET | All seven bands returned to neutral settings.",
        juce::dontSendNotification);
    if (eqGraph != nullptr) eqGraph->repaint();
}

void CrusherAudioProcessorEditor::updateDistortionControls()
{
    const int selected = distortionTypeBox.getSelectedId() - 1;
    if (selected == displayedDistortionType && ! presetBrowserOpen)
        return;
    displayedDistortionType = selected;
    const bool humpSelected = selected == 17;
    const bool ampSelected = selected == 6;
    const bool asymSelected = selected == 8;
    const bool bumpySelected = selected == 7;
    const bool combSelected = selected == 11;
    const bool crackleSelected = selected == 4;
    const bool diodeSelected = selected == 18;
    const bool codecSelected = selected == 20;
    const bool vocalSelected = selected == 21;
    const bool cascadeSelected = selected == 22;
    const bool loFiSelected = selected == 19;
    const bool lightningSelected = selected == 5;
    const bool pixelatedSelected = selected == 10;
    const bool rectifySelected = selected == 3;
    const bool softClipSelected = selected == 0;
    const bool tubeSelected = selected == 2;
    const bool stressedSelected = selected == 12;
    const bool saturationSelected = selected == 1;
    const bool samuraiSelected = selected == 13;
    const bool razorSelected = selected == 9;
    const bool freshSelected = selected == 14;
    const bool fuzzSelected = selected == 16;
    const bool hardClipSelected = selected == 15;
    const bool visible = ! presetBrowserOpen;
    if (driveDial != nullptr)
    {
        driveDial->label.setVisible(visible && ! humpSelected);
        driveDial->slider.setVisible(visible && ! humpSelected);
        const bool driveCaption = cascadeSelected || vocalSelected || codecSelected || loFiSelected || diodeSelected || ampSelected || asymSelected || bumpySelected
                                  || combSelected || crackleSelected || freshSelected
                                  || fuzzSelected || hardClipSelected || lightningSelected || pixelatedSelected || razorSelected || rectifySelected || samuraiSelected || saturationSelected || softClipSelected || stressedSelected || tubeSelected;
        driveDial->label.setText(driveCaption ? "DRIVE" : "DIST",
                                  juce::dontSendNotification);
        driveDial->help = cascadeSelected
            ? "Cascade drive: blend from clean to increasingly dense multi-stage saturation."
            : vocalSelected
            ? "Vocal drive: blend from clean to frequency-selective saturation for vocals and drums."
            : codecSelected
            ? "Codec drive: blend the digital codec degradation from dry to processed."
            : loFiSelected
            ? "Lo-Fi drive: blend the complete digital damage chain from dry to full effect."
            : diodeSelected
            ? "Diode Clip drive: range from subtle clipping to aggressive harmonics."
            : ampSelected
            ? "Amp input drive: 0-100%, controllable from the sequencer."
            : asymSelected
                ? "Asym drive: controls saturation and distortion intensity."
                : bumpySelected
                    ? "Bumpy drive: saturation intensity and ripple depth."
                    : combSelected
                        ? "Comb drive: increases resonance and drive intensity."
                        : crackleSelected
                            ? "Crackle drive: increases saturation and random event activity."
                            : freshSelected
                                ? "Fresh drive: increases high-frequency excitation and saturation."
                                : fuzzSelected
                                    ? "Fuzz drive: increases asymmetric high-gain clipping and sustain."
                                    : hardClipSelected
                                        ? "Hard Clip drive: pushes the input into the adjustable hard clipping rails."
                                    : lightningSelected
                                        ? "Lightning drive: controls hard clipping and electrical spark intensity."
                                    : pixelatedSelected
                                        ? "Pixelated drive: changes sample-hold depth, bit reduction and dither."
                                    : rectifySelected
                                        ? "Rectify drive: increases full-wave saturation and harmonic intensity."
                                    : razorSelected
                                        ? "Razor drive: increases pre-emphasis, saturation and ringing harmonics."
                                    : tubeSelected
                                        ? "Tube drive: increases asymmetric tube-style saturation and compression."
                                    : stressedSelected
                                        ? "Stressed drive: intensifies heavy compression and waveform folding."
                                    : softClipSelected
                                        ? "Soft Clip drive: increases tanh saturation and harmonic density."
                                    : saturationSelected
                                        ? "Saturation drive: increases the open arctangent saturation intensity."
                                    : samuraiSelected
                                        ? "Samurai drive: raises blade saturation, gating and metallic harmonics."
                                        : "Model-specific DIST amount for the selected distortion type.";
        const auto sharedHelp = juce::String((cascadeSelected || vocalSelected || codecSelected || loFiSelected || diodeSelected || ampSelected || asymSelected || bumpySelected || combSelected || crackleSelected || freshSelected || fuzzSelected || hardClipSelected || lightningSelected || pixelatedSelected || razorSelected || rectifySelected || samuraiSelected || saturationSelected || softClipSelected || stressedSelected || tubeSelected) ? "DRIVE" : "DIST")
                              + " | " + driveDial->help;
        driveDial->label.setTooltip(sharedHelp);
        driveDial->slider.setTooltip(sharedHelp);
    }
    for (auto* dial : { ampSoftDial, ampTopDial, ampBiasDial })
        if (dial != nullptr)
        {
            dial->label.setVisible(visible && ampSelected);
            dial->slider.setVisible(visible && ampSelected);
        }
    for (auto* dial : { bumpyRippleDial, bumpyRateDial, bumpyContourDial,
                        bumpyBiasDial, bumpyToneDial })
        if (dial != nullptr)
        {
            dial->label.setVisible(visible && bumpySelected);
            dial->slider.setVisible(visible && bumpySelected);
        }
    for (auto* dial : { combTimeDial, combFeedbackDial, combDampDial,
                        combSpreadDial, combToneDial })
        if (dial != nullptr)
        {
            dial->label.setVisible(visible && combSelected);
            dial->slider.setVisible(visible && combSelected);
        }
    for (auto* dial : { diodeInputHPDial, diodeEnvelopeDial, diodeShapeDial,
                        diodeOutputHPDial, diodeOutputLPDial })
        if (dial != nullptr)
        {
            dial->label.setVisible(visible && diodeSelected);
            dial->slider.setVisible(visible && diodeSelected);
        }
    for (auto* dial : { crackleDensityDial, crackleBurstDial, crackleDecayDial,
                        crackleGritDial, crackleToneDial })
        if (dial != nullptr)
        {
            dial->label.setVisible(visible && crackleSelected);
            dial->slider.setVisible(visible && crackleSelected);
        }
    for (auto* dial : { codecModeDial, codecRateDial, codecBitsDial,
                        codecDamageDial, codecToneDial })
        if (dial != nullptr)
        {
            dial->label.setVisible(visible && codecSelected);
            dial->slider.setVisible(visible && codecSelected);
        }
    for (auto* dial : { cascadeStagesDial, cascadeVariationDial, cascadeEmphasisDial,
                        cascadeBiasDial, cascadePunchDial })
        if (dial != nullptr)
        {
            dial->label.setVisible(visible && cascadeSelected);
            dial->slider.setVisible(visible && cascadeSelected);
        }
    for (auto* dial : { vocalCharacterDial, vocalBodyDial, vocalPresenceDial,
                        vocalBiteDial, vocalSmoothDial })
        if (dial != nullptr)
        {
            dial->label.setVisible(visible && vocalSelected);
            dial->slider.setVisible(visible && vocalSelected);
        }
    for (auto* dial : { loFiRateDial, loFiBitsDial, loFiClipDial,
                        loFiRectifyDial, loFiNoiseDial })
        if (dial != nullptr)
        {
            dial->label.setVisible(visible && loFiSelected);
            dial->slider.setVisible(visible && loFiSelected);
        }
    for (auto* dial : { pixelatedRateDial, pixelatedBitsDial, pixelatedDitherDial,
                        pixelatedJitterDial, pixelatedToneDial })
        if (dial != nullptr)
        {
            dial->label.setVisible(visible && pixelatedSelected);
            dial->slider.setVisible(visible && pixelatedSelected);
        }
    for (auto* dial : { rectifyWaveDial, rectifyBiasDial, rectifyCurveDial,
                        rectifyFoldDial, rectifyToneDial })
        if (dial != nullptr)
        {
            dial->label.setVisible(visible && rectifySelected);
            dial->slider.setVisible(visible && rectifySelected);
        }
    for (auto* dial : { softClipThresholdDial, softClipKneeDial, softClipBiasDial,
                        softClipBodyDial, softClipToneDial })
        if (dial != nullptr)
        {
            dial->label.setVisible(visible && softClipSelected);
            dial->slider.setVisible(visible && softClipSelected);
        }
    for (auto* dial : { tubeBiasDial, tubeHeatDial, tubeSagDial,
                        tubeBodyDial, tubeToneDial })
        if (dial != nullptr)
        {
            dial->label.setVisible(visible && tubeSelected);
            dial->slider.setVisible(visible && tubeSelected);
        }
    for (auto* dial : { stressedPressDial, stressedFoldDial, stressedTensionDial,
                        stressedBiasDial, stressedToneDial })
        if (dial != nullptr)
        {
            dial->label.setVisible(visible && stressedSelected);
            dial->slider.setVisible(visible && stressedSelected);
        }
    for (auto* dial : { saturationCurveDial, saturationBiasDial, saturationWarmDial,
                        saturationBodyDial, saturationToneDial })
        if (dial != nullptr)
        {
            dial->label.setVisible(visible && saturationSelected);
            dial->slider.setVisible(visible && saturationSelected);
        }
    for (auto* dial : { samuraiEdgeDial, samuraiGateDial, samuraiBladeDial,
                        samuraiRingDial, samuraiToneDial })
        if (dial != nullptr)
        {
            dial->label.setVisible(visible && samuraiSelected);
            dial->slider.setVisible(visible && samuraiSelected);
        }
    for (auto* dial : { razorEdgeDial, razorFreqDial, razorRingDial,
                        razorBiteDial, razorToneDial })
        if (dial != nullptr)
        {
            dial->label.setVisible(visible && razorSelected);
            dial->slider.setVisible(visible && razorSelected);
        }
    for (auto* dial : { lightningSparkDial, lightningReactDial, lightningClipDial,
                        lightningEdgeDial, lightningToneDial })
        if (dial != nullptr)
        {
            dial->label.setVisible(visible && lightningSelected);
            dial->slider.setVisible(visible && lightningSelected);
        }
    for (auto* dial : { freshFrequencyDial, freshExciteDial, freshAirDial,
                        freshSaturationDial, freshToneDial })
        if (dial != nullptr)
        {
            dial->label.setVisible(visible && freshSelected);
            dial->slider.setVisible(visible && freshSelected);
        }
    for (auto* dial : { fuzzGateDial, fuzzBiasDial, fuzzClipDial,
                        fuzzBodyDial, fuzzToneDial })
        if (dial != nullptr)
        {
            dial->label.setVisible(visible && fuzzSelected);
            dial->slider.setVisible(visible && fuzzSelected);
        }
    for (auto* dial : { hardClipThresholdDial, hardClipBiasDial, hardClipKneeDial,
                        hardClipFoldDial, hardClipToneDial })
        if (dial != nullptr)
        {
            dial->label.setVisible(visible && hardClipSelected);
            dial->slider.setVisible(visible && hardClipSelected);
        }
    for (auto* dial : { asymBiasDial, asymSkewDial, asymShapeDial,
                        asymToneDial })
        if (dial != nullptr)
        {
            dial->label.setVisible(visible && asymSelected);
            dial->slider.setVisible(visible && asymSelected);
        }
    ampCabinetLabel.setVisible(visible && ampSelected);
    ampCabinetBox.setVisible(visible && ampSelected);
    distStereoButton.setVisible(visible);
    distMonoButton.setVisible(visible);
    // The ComboBox is never visible: APVTS still uses it for saved IDs.
    distModeBox.setVisible(false);
    for (auto* dial : { mechDial, tubeDial, humpBodyDial,
                        humpBiasDial, humpFoldDial, humpToneDial })
        if (dial != nullptr)
        {
            dial->label.setVisible(visible && humpSelected);
            dial->slider.setVisible(visible && humpSelected);
        }
    // All expanded models use the same two contextual knob rows.
    resized();
}
void CrusherAudioProcessorEditor::setPresetBrowserOpen(bool shouldOpen)
{
    presetBrowserOpen = shouldOpen;
    presetBrowserButton.setToggleState(shouldOpen,
                                       juce::dontSendNotification);
    if (shouldOpen)
    {
        const auto root = CrusherAudioProcessor::getPresetsDirectory();
        root.createDirectory();
        if (! selectedPresetFolder.isDirectory()
            || (selectedPresetFolder != root
                && ! selectedPresetFolder.isAChildOf(root)))
            selectedPresetFolder = root;
        rebuildPresetFolderTree();
        information.setText(
            "PRESET BROWSER | Choose a folder, then click a preset to load it.\nSAVE overwrites the current preset; SAVE AS creates a new preset or copy.",
            juce::dontSendNotification);
    }
    updatePresetBrowserVisibility();
}

void CrusherAudioProcessorEditor::updatePresetBrowserVisibility()
{
    for (const auto& dial : dials)
    {
        const bool visible = dial.get() == themeDial
            ? ! presetBrowserOpen
            : (dial.get() == browserMixDial
                ? presetBrowserOpen : ! presetBrowserOpen);
        dial->slider.setVisible(visible);
        dial->label.setVisible(visible);
    }

    distortionTypeBox.setVisible(true);
    distortionTypeLabel.setVisible(true);
    distortionOnButton.setVisible(! presetBrowserOpen);
    filterOnButton.setVisible(! presetBrowserOpen);
    compressorOnButton.setVisible(! presetBrowserOpen);
    phatButton.setVisible(! presetBrowserOpen);
    eqOnButton.setVisible(! presetBrowserOpen);
    eqModeBox.setVisible(! presetBrowserOpen);
    eqResetButton.setVisible(! presetBrowserOpen);
    if (eqGraph != nullptr) eqGraph->setVisible(! presetBrowserOpen);
    for (auto* icon : { &filterLowButton, &filterBandButton, &filterHighButton })
        icon->setVisible(! presetBrowserOpen);
    updateEQEnvView();
    information.setVisible(! presetBrowserOpen);
    randomButton.setVisible(! presetBrowserOpen);
    mixLockButton.setVisible(! presetBrowserOpen);
    presetFolderLabel.setVisible(presetBrowserOpen);
    presetListLabel.setVisible(presetBrowserOpen);
    presetFolderTree.setVisible(presetBrowserOpen);
    presetList.setVisible(presetBrowserOpen);
    presetSaveButton.setVisible(presetBrowserOpen);
    presetSaveAsButton.setVisible(presetBrowserOpen);
    presetOpenFolderButton.setVisible(presetBrowserOpen);
    displayedDistortionType = -1;
    updateDistortionControls();
    updateSectionDimming();
    resized();
    repaint();
}

void CrusherAudioProcessorEditor::setSequencerOpen(bool shouldOpen)
{
    if (sequencerOpen == shouldOpen)
        return;

    const float scale = juce::jlimit(
        0.75f, 3.0f,
        static_cast<float>(getWidth()) / static_cast<float>(designWidth));
    sequencerOpen = shouldOpen;
    sequencerButton.setToggleState(shouldOpen, juce::dontSendNotification);
    if (sequencerPanel != nullptr)
        sequencerPanel->setVisible(shouldOpen);
    if (auto* editorConstrainer = getConstrainer())
        editorConstrainer->setFixedAspectRatio(
            static_cast<double>(designWidth)
                / static_cast<double>(activeDesignHeight()));
    setResizeLimits(695, shouldOpen ? 414 : 207,
                    2778, shouldOpen ? 1590 : 828);
    setSize(juce::roundToInt(static_cast<float>(designWidth) * scale),
            juce::roundToInt(static_cast<float>(activeDesignHeight()) * scale));
    saveInterfaceSettings();
    resized();
    repaint();
}

void CrusherAudioProcessorEditor::updateSequencerTargetHighlights()
{
    if (sequencerPanel == nullptr)
        return;

    const auto accent = sequencerPanel->getAccent();
    for (const auto& dial : dials)
    {
        const auto parameterId = dial->label.parameterIdProvider != nullptr
            ? dial->label.parameterIdProvider() : juce::String();
        const bool seqAssigned = sequencerPanel->isTargetAssigned(parameterId);
        const bool envAssigned = processor.isEnvelopeTargetAssigned(parameterId);
        dial->label.setTargetAppearance(seqAssigned || envAssigned,
            seqAssigned ? accent : juce::Colour(0xffd19aa7));
    }
    sequencerPanel->updateTargetHighlights();
}

void CrusherAudioProcessorEditor::updateAnimatedParameterDisplays()
{
    const bool animate = sequencerPanel != nullptr
                      && sequencerPanel->getAnimate();
    for (const auto& dial : dials)
    {
        const auto parameterId = dial->label.parameterIdProvider != nullptr
            ? dial->label.parameterIdProvider() : juce::String();
        const auto value = animate && parameterId.isNotEmpty()
            ? processor.getSequencerAnimatedNormalised(parameterId)
            : std::nullopt;
        auto& sliderProperties = dial->slider.getProperties();
        if (value.has_value())
        {
            const bool changed = ! sliderProperties.contains("animatedNormalised")
                || std::abs(static_cast<float>(
                       sliderProperties["animatedNormalised"]) - *value)
                       > 0.000001f;
            if (changed)
            {
                sliderProperties.set("animatedNormalised", *value);
                dial->slider.repaint();
            }
        }
        else if (sliderProperties.contains("animatedNormalised"))
        {
            sliderProperties.remove("animatedNormalised");
            dial->slider.repaint();
        }
    }
}

void CrusherAudioProcessorEditor::randomiseEffectParameters()
{
    juce::Random random;
    setParameterActualValue(
        processor.parameters.getParameter("distortionType"),
        static_cast<float>(random.nextInt(
            static_cast<int>(distortionSummaries.size()))));
    setParameterActualValue(
        processor.parameters.getParameter("drive"),
        10.0f + random.nextFloat() * 70.0f);
    setParameterActualValue(
        processor.parameters.getParameter("mech"),
        5.0f + random.nextFloat() * 55.0f);
    setParameterActualValue(
        processor.parameters.getParameter("tube"),
        5.0f + random.nextFloat() * 55.0f);
    setParameterActualValue(
        processor.parameters.getParameter("output"),
        -9.0f + random.nextFloat() * 9.0f);
    information.setText(
        "RANDOMISE | Distortion type, its amounts and Output changed.\nFilter, Comp, Mix, Power and Sequencer were preserved.",
        juce::dontSendNotification);
}

void CrusherAudioProcessorEditor::loadFavouritePresets()
{
    favouritePresetPaths.clear();
    const auto file = CrusherAudioProcessor::getSettingsDirectory()
        .getChildFile("Favourites.ini");
    if (! file.existsAsFile())
        return;

    juce::StringArray lines;
    lines.addLines(file.loadFileAsString());
    bool inFavourites = false;
    for (auto line : lines)
    {
        line = line.trim();
        if (line.startsWithChar('['))
        {
            inFavourites = line.equalsIgnoreCase("[Favourites]");
            continue;
        }
        if (! inFavourites || ! line.startsWithIgnoreCase("Preset="))
            continue;

        auto relativePath = line.substring(7).trim()
            .replaceCharacter('\\', '/');
        if (relativePath.isNotEmpty()
            && ! favouritePresetPaths.contains(relativePath, true))
            favouritePresetPaths.add(relativePath);
    }
}

void CrusherAudioProcessorEditor::saveFavouritePresets() const
{
    const auto settingsDirectory =
        CrusherAudioProcessor::getSettingsDirectory();
    settingsDirectory.createDirectory();
    juce::String contents("[Favourites]\r\n");
    for (const auto& relativePath : favouritePresetPaths)
        contents += "Preset=" + relativePath + "\r\n";
    settingsDirectory.getChildFile("Favourites.ini")
        .replaceWithText(contents, false, false, "\r\n");
}

bool CrusherAudioProcessorEditor::isPresetFavourite(
    const juce::File& file) const
{
    const auto root = CrusherAudioProcessor::getPresetsDirectory();
    if (! file.existsAsFile()
        || (file.getParentDirectory() != root
            && ! file.isAChildOf(root)))
        return false;

    const auto relativePath = file.getRelativePathFrom(root)
        .replaceCharacter('\\', '/');
    return favouritePresetPaths.contains(relativePath, true);
}

juce::Array<juce::File>
CrusherAudioProcessorEditor::favouritePresetFiles() const
{
    const auto root = CrusherAudioProcessor::getPresetsDirectory();
    juce::Array<juce::File> files;
    for (const auto& relativePath : favouritePresetPaths)
    {
        const auto file = root.getChildFile(relativePath);
        if (file.existsAsFile() && file.hasFileExtension(presetFileExtension)
            && (file.getParentDirectory() == root || file.isAChildOf(root)))
            files.addIfNotAlreadyThere(file);
    }
    std::sort(files.begin(), files.end(),
              [](const juce::File& first, const juce::File& second)
              {
                  const int nameOrder = first.getFileNameWithoutExtension()
                      .compareNatural(second.getFileNameWithoutExtension(),
                                      true);
                  return nameOrder != 0
                      ? nameOrder < 0
                      : first.getFullPathName().compareNatural(
                            second.getFullPathName(), true) < 0;
              });
    return files;
}

juce::Array<juce::File>
CrusherAudioProcessorEditor::navigationPresetFiles() const
{
    if (favouritesViewSelected)
        return favouritePresetFiles();

    const auto root = CrusherAudioProcessor::getPresetsDirectory();
    auto folder = currentPresetFile.existsAsFile()
        ? currentPresetFile.getParentDirectory()
        : selectedPresetFolder;
    if (! folder.isDirectory()
        || (folder != root && ! folder.isAChildOf(root)))
        folder = root;
    return presetFilesInFolder(folder);
}

void CrusherAudioProcessorEditor::togglePresetFavourite(
    const juce::File& file)
{
    const auto root = CrusherAudioProcessor::getPresetsDirectory();
    if (! file.existsAsFile()
        || (file.getParentDirectory() != root && ! file.isAChildOf(root)))
        return;

    const auto relativePath = file.getRelativePathFrom(root)
        .replaceCharacter('\\', '/');
    const int existing = favouritePresetPaths.indexOf(relativePath, true);
    if (existing >= 0)
        favouritePresetPaths.remove(existing);
    else
        favouritePresetPaths.add(relativePath);
    saveFavouritePresets();
    refreshPresetList();
}

void CrusherAudioProcessorEditor::copyPreset(const juce::File& file)
{
    if (! file.existsAsFile() || ! file.hasFileExtension(presetFileExtension))
        return;
    copiedPresetFile = file;
    information.setText(
        "PRESET COPIED: " + file.getFileNameWithoutExtension()
            + "\nRight-click a real preset folder or preset list and choose Paste preset here.",
        juce::dontSendNotification);
}

void CrusherAudioProcessorEditor::pastePresetIntoFolder(
    const juce::File& folder)
{
    const auto root = CrusherAudioProcessor::getPresetsDirectory();
    if (! copiedPresetFile.existsAsFile() || ! folder.isDirectory()
        || (folder != root && ! folder.isAChildOf(root)))
        return;

    const auto baseName = copiedPresetFile.getFileNameWithoutExtension();
    auto destination = folder.getChildFile(
        baseName + presetFileExtension);
    if (destination.exists())
    {
        destination = folder.getChildFile(
            baseName + " Copy" + presetFileExtension);
        int copyNumber = 2;
        while (destination.exists())
            destination = folder.getChildFile(
                baseName + " Copy " + juce::String(copyNumber++)
                    + presetFileExtension);
    }

    if (! copiedPresetFile.copyFileTo(destination))
    {
        information.setText(
            "PRESET PASTE FAILED | The preset file could not be copied.",
            juce::dontSendNotification);
        return;
    }

    selectedPresetFolder = folder;
    favouritesViewSelected = false;
    rebuildPresetFolderTree();
    information.setText(
        "PRESET PASTED: " + destination.getFileNameWithoutExtension()
            + "\nThe copy is independent of the original and can be edited or saved normally.",
        juce::dontSendNotification);
}

void CrusherAudioProcessorEditor::showPresetContextMenu(
    const juce::File& file, juce::Point<int> position)
{
    juce::PopupMenu menu;
    menu.setLookAndFeel(&lookAndFeel);
    if (file.existsAsFile())
    {
        menu.addItem(1, "Copy preset");
        menu.addSeparator();
    }
    menu.addItem(2, "Paste preset here",
                 copiedPresetFile.existsAsFile()
                    && ! favouritesViewSelected
                    && selectedPresetFolder.isDirectory());
    lookAndFeel.setPopupBorderColour(filterAccent(1));
    const auto safeThis = juce::Component::SafePointer<
        CrusherAudioProcessorEditor>(this);
    const auto destinationFolder = selectedPresetFolder;
    menu.showMenuAsync(
        juce::PopupMenu::Options()
            .withTargetScreenArea({ position.x, position.y, 1, 1 })
            .withMinimumWidth(150),
        [safeThis, file, destinationFolder](int result)
        {
            if (auto* editor = safeThis.getComponent())
            {
                if (result == 1)
                    editor->copyPreset(file);
                else if (result == 2)
                    editor->pastePresetIntoFolder(destinationFolder);
            }
        });
}

void CrusherAudioProcessorEditor::showCreatePresetFolderDialog(
    const juce::File& parentFolder)
{
    const auto root = CrusherAudioProcessor::getPresetsDirectory();
    if (! parentFolder.isDirectory()
        || (parentFolder != root && ! parentFolder.isAChildOf(root)))
        return;

    presetFolderNameWindow = std::make_unique<juce::AlertWindow>(
        "New Preset Folder",
        "Enter a name for the new folder inside "
            + (parentFolder == root ? juce::String("Presets")
                                    : parentFolder.getFileName()) + ".",
        juce::AlertWindow::NoIcon);
    presetFolderNameWindow->setLookAndFeel(&lookAndFeel);
    presetFolderNameWindow->addTextEditor(
        "folderName", {}, "Folder name:");
    presetFolderNameWindow->addButton(
        "Create", 1,
        juce::KeyPress(juce::KeyPress::returnKey, 0, 0));
    presetFolderNameWindow->addButton(
        "Cancel", 0,
        juce::KeyPress(juce::KeyPress::escapeKey, 0, 0));

    const auto safeThis = juce::Component::SafePointer<
        CrusherAudioProcessorEditor>(this);
    presetFolderNameWindow->enterModalState(
        true,
        juce::ModalCallbackFunction::create(
            [safeThis, parentFolder](int result)
            {
                auto* editor = safeThis.getComponent();
                if (editor == nullptr || editor->presetFolderNameWindow == nullptr)
                    return;

                const auto name = editor->presetFolderNameWindow
                    ->getTextEditorContents("folderName").trim();
                editor->presetFolderNameWindow->setLookAndFeel(nullptr);
                editor->presetFolderNameWindow.reset();
                if (result != 1)
                    return;

                if (name.isEmpty() || name == "." || name == ".."
                    || name.containsAnyOf("\\/:*?\"<>|")
                    || name.endsWithChar('.') || name.endsWithChar(' '))
                {
                    juce::AlertWindow::showMessageBoxAsync(
                        juce::AlertWindow::WarningIcon,
                        "Invalid Folder Name",
                        "Please use a normal folder name without \\ / : * ? \" < > | or a trailing dot.",
                        "OK", editor);
                    return;
                }

                const auto folder = parentFolder.getChildFile(name);
                if (folder.exists())
                {
                    juce::AlertWindow::showMessageBoxAsync(
                        juce::AlertWindow::WarningIcon,
                        "Folder Already Exists",
                        "A file or folder with that name already exists.",
                        "OK", editor);
                    return;
                }

                const auto createResult = folder.createDirectory();
                if (createResult.failed())
                {
                    juce::AlertWindow::showMessageBoxAsync(
                        juce::AlertWindow::WarningIcon,
                        "Folder Creation Failed",
                        createResult.getErrorMessage(), "OK", editor);
                    return;
                }

                editor->selectedPresetFolder = folder;
                editor->favouritesViewSelected = false;
                editor->rebuildPresetFolderTree();
            }),
        false);
}

void CrusherAudioProcessorEditor::confirmDeletePresetFolder(
    const juce::File& folder)
{
    const auto root = CrusherAudioProcessor::getPresetsDirectory();
    if (! folder.isDirectory() || folder == root
        || ! folder.isAChildOf(root))
        return;

    const bool hasContents = folder.getNumberOfChildFiles(
        juce::File::findFilesAndDirectories) > 0;
    const auto message = hasContents
        ? "Delete the folder '" + folder.getFileName()
            + "' and everything inside it?\n\nThis moves the folder, presets and subfolders to the system Recycle Bin or Trash."
        : "Delete the empty folder '" + folder.getFileName()
            + "'?\n\nThis moves it to the system Recycle Bin or Trash.";
    const auto safeThis = juce::Component::SafePointer<
        CrusherAudioProcessorEditor>(this);
    juce::AlertWindow::showOkCancelBox(
        juce::AlertWindow::WarningIcon,
        "Delete Preset Folder", message,
        "Delete", "Cancel", this,
        juce::ModalCallbackFunction::create(
            [safeThis, folder](int result)
            {
                auto* editor = safeThis.getComponent();
                if (editor == nullptr || result != 1)
                    return;

                const bool containedCurrentPreset =
                    editor->currentPresetFile.existsAsFile()
                    && editor->currentPresetFile.isAChildOf(folder);
                const bool containedCopiedPreset =
                    editor->copiedPresetFile.existsAsFile()
                    && editor->copiedPresetFile.isAChildOf(folder);
                if (! folder.moveToTrash())
                {
                    juce::AlertWindow::showMessageBoxAsync(
                        juce::AlertWindow::WarningIcon,
                        "Folder Deletion Failed",
                        "The preset folder could not be moved to the system Recycle Bin or Trash.",
                        "OK", editor);
                    return;
                }

                const auto rootDirectory =
                    CrusherAudioProcessor::getPresetsDirectory();
                bool favouritesChanged = false;
                for (int index = editor->favouritePresetPaths.size();
                     --index >= 0;)
                {
                    const auto favouriteFile = rootDirectory.getChildFile(
                        editor->favouritePresetPaths[index]);
                    if (favouriteFile.isAChildOf(folder))
                    {
                        editor->favouritePresetPaths.remove(index);
                        favouritesChanged = true;
                    }
                }
                if (favouritesChanged)
                    editor->saveFavouritePresets();
                if (containedCopiedPreset)
                    editor->copiedPresetFile = juce::File();
                if (containedCurrentPreset)
                {
                    editor->currentPresetFile = juce::File();
                    editor->processor.parameters.state.removeProperty(
                        "currentPresetPath", nullptr);
                }

                editor->selectedPresetFolder = rootDirectory;
                editor->favouritesViewSelected = false;
                editor->rebuildPresetFolderTree();
                editor->updatePresetNavigationDisplay();
            }));
}

void CrusherAudioProcessorEditor::showPresetFolderContextMenu(
    const juce::File& folder, bool favourites, juce::Point<int> position)
{
    juce::PopupMenu menu;
    menu.setLookAndFeel(&lookAndFeel);
    menu.addItem(1, "New folder...");
    menu.addItem(2, "Delete folder...",
                 ! favourites
                    && folder != CrusherAudioProcessor::getPresetsDirectory()
                    && folder.isDirectory());
    menu.addSeparator();
    menu.addItem(3, "Paste preset here",
                 ! favourites && copiedPresetFile.existsAsFile()
                    && folder.isDirectory());
    lookAndFeel.setPopupBorderColour(filterAccent(1));
    const auto safeThis = juce::Component::SafePointer<
        CrusherAudioProcessorEditor>(this);
    menu.showMenuAsync(
        juce::PopupMenu::Options()
            .withTargetScreenArea({ position.x, position.y, 1, 1 })
            .withMinimumWidth(150),
        [safeThis, folder, favourites](int result)
        {
            if (auto* editor = safeThis.getComponent())
            {
                if (result == 1)
                    editor->showCreatePresetFolderDialog(
                        favourites
                            ? CrusherAudioProcessor::getPresetsDirectory()
                            : folder);
                else if (result == 2)
                    editor->confirmDeletePresetFolder(folder);
                else if (result == 3)
                    editor->pastePresetIntoFolder(folder);
            }
        });
}

void CrusherAudioProcessorEditor::rebuildPresetFolderTree()
{
    const auto root = CrusherAudioProcessor::getPresetsDirectory();
    root.createDirectory();
    presetFolderTree.setRootItem(nullptr);
    presetFolderRoot.reset();
    presetFolderRoot = std::make_unique<FolderTreeItem>(
        root,
        [this](const juce::File& folder, bool favourites)
        {
            if (favourites)
                selectFavouritePresets();
            else
                selectPresetFolder(folder);
        },
        [this](const juce::File& folder, bool favourites,
               juce::Point<int> position)
        {
            showPresetFolderContextMenu(folder, favourites, position);
        });
    presetFolderTree.setRootItem(presetFolderRoot.get());
    presetFolderRoot->setOpen(true);

    auto target = selectedPresetFolder;
    if (! target.isDirectory()
        || (target != root && ! target.isAChildOf(root)))
        target = root;

    if (auto* item = presetFolderRoot->findItemForDirectory(
            target, favouritesViewSelected))
    {
        for (auto* parent = item->getParentItem(); parent != nullptr;
             parent = parent->getParentItem())
            parent->setOpen(true);
        item->setSelected(true, true, juce::sendNotification);
        presetFolderTree.scrollToKeepItemVisible(item);
    }
    else
    {
        selectedPresetFolder = root;
        presetFolderRoot->setSelected(true, true, juce::sendNotification);
    }
}

void CrusherAudioProcessorEditor::selectPresetFolder(
    const juce::File& folder)
{
    const auto root = CrusherAudioProcessor::getPresetsDirectory();
    if (! folder.isDirectory()
        || (folder != root && ! folder.isAChildOf(root)))
        return;

    favouritesViewSelected = false;
    selectedPresetFolder = folder;
    refreshPresetList();
}

void CrusherAudioProcessorEditor::selectFavouritePresets()
{
    favouritesViewSelected = true;
    refreshPresetList();
}

void CrusherAudioProcessorEditor::refreshPresetList()
{
    visiblePresetFiles = favouritesViewSelected
        ? favouritePresetFiles()
        : presetFilesInFolder(selectedPresetFolder);

    presetList.updateContent();
    presetList.deselectAllRows();
    if ((favouritesViewSelected && isPresetFavourite(currentPresetFile))
        || (! favouritesViewSelected
            && currentPresetFile.getParentDirectory() == selectedPresetFolder))
        for (int index = 0; index < visiblePresetFiles.size(); ++index)
            if (visiblePresetFiles[index] == currentPresetFile)
            {
                selectedPresetBrowserColumn = index % 2;
                presetList.selectRow(index / 2);
                break;
            }
    presetList.repaint();
    updatePresetNavigationDisplay();
}

void CrusherAudioProcessorEditor::updatePresetNavigationDisplay()
{
    const auto files = navigationPresetFiles();
    const bool hasPresets = ! files.isEmpty();
    previousPresetButton.setEnabled(hasPresets);
    nextPresetButton.setEnabled(hasPresets);

    if (currentPresetFile != juce::File())
    {
        currentPresetLabel.setText(
            currentPresetFile.getFileNameWithoutExtension(),
            juce::dontSendNotification);
        currentPresetLabel.setTooltip(currentPresetFile.getFullPathName());
    }
    else
    {
        currentPresetLabel.setText("NO PRESET", juce::dontSendNotification);
        currentPresetLabel.setTooltip({});
    }
}

bool CrusherAudioProcessorEditor::handlePresetCursorKey(int direction)
{
    auto& keyDown = direction < 0
        ? previousPresetKeyDown : nextPresetKeyDown;
    if (! keyDown)
        stepPreset(direction);
    keyDown = true;
    return true;
}

void CrusherAudioProcessorEditor::stepPreset(int direction)
{
    const auto files = navigationPresetFiles();
    if (files.isEmpty())
    {
        information.setText(
            "PRESET NAVIGATION | No presets are available in the selected folder.",
            juce::dontSendNotification);
        updatePresetNavigationDisplay();
        return;
    }

    int currentIndex = files.indexOf(currentPresetFile);
    if (currentIndex < 0)
        currentIndex = direction >= 0 ? -1 : 0;
    const int nextIndex = (currentIndex + (direction >= 0 ? 1 : -1)
                           + files.size()) % files.size();
    loadPresetFile(files[nextIndex]);
}

void CrusherAudioProcessorEditor::showPresetQuickMenu()
{
    const auto files = navigationPresetFiles();
    if (files.isEmpty())
    {
        information.setText(
            "PRESET MENU | No presets are available in the current folder.",
            juce::dontSendNotification);
        return;
    }

    const auto safeThis = juce::Component::SafePointer<
        CrusherAudioProcessorEditor>(this);
    auto quickList = std::make_unique<PresetPopupList>(
        files, currentPresetFile,
        [safeThis](const juce::File& file)
        {
            if (auto* editor = safeThis.getComponent())
                editor->loadPresetFile(file);
        });

    juce::PopupMenu menu;
    menu.setLookAndFeel(&lookAndFeel);
    menu.addCustomItem(1, std::move(quickList), nullptr, "Preset list");
    lookAndFeel.setPopupBorderColour(filterAccent(1));
    menu.showMenuAsync(
        juce::PopupMenu::Options()
            .withTargetComponent(&currentPresetLabel)
            .withTargetScreenArea(currentPresetLabel.getScreenBounds())
            .withPreferredPopupDirection(
                juce::PopupMenu::Options::PopupDirection::downwards)
            .withMinimumWidth(220)
            .withMaximumNumColumns(1));
}

int CrusherAudioProcessorEditor::getNumRows()
{
    return (visiblePresetFiles.size() + 1) / 2;
}

void CrusherAudioProcessorEditor::paintListBoxItem(
    int row, juce::Graphics& g, int width, int height, bool rowIsSelected)
{
    const int firstColumnWidth = width / 2;
    for (int column = 0; column < 2; ++column)
    {
        const int index = row * 2 + column;
        if (! juce::isPositiveAndBelow(index, visiblePresetFiles.size()))
            continue;

        const int cellX = column == 0 ? 0 : firstColumnWidth;
        const int cellWidth = column == 0
            ? firstColumnWidth : width - firstColumnWidth;
        const auto& file = visiblePresetFiles.getReference(index);
        const bool isSelected = rowIsSelected
                             && column == selectedPresetBrowserColumn;
        if (isSelected)
        {
            g.setColour(presetAccentColour.withAlpha(0.30f));
            g.fillRoundedRectangle(static_cast<float>(cellX + 2), 1.0f,
                                   static_cast<float>(cellWidth - 4),
                                   static_cast<float>(height - 2), 3.0f);
        }

        const bool isCurrent = file == currentPresetFile;
        const bool isFavourite = isPresetFavourite(file);
        g.setColour(isFavourite ? knobIndicatorColour
                                : knobLabelColour.darker(0.25f));
        g.fillPath(makeStarPath({ static_cast<float>(cellX + 7),
                                 static_cast<float>(height) * 0.5f - 5.0f,
                                 10.0f, 10.0f }));
        g.setColour(isCurrent ? knobIndicatorColour : knobValueColour);
        g.setFont(juce::Font(juce::FontOptions(
            11.0f, isCurrent ? juce::Font::bold : juce::Font::plain)));
        g.drawFittedText(file.getFileNameWithoutExtension(), cellX + 24, 0,
                         juce::jmax(0, cellWidth - 48), height,
                         juce::Justification::centredLeft, 1);
        if (isCurrent)
        {
            g.setColour(knobIndicatorColour);
            g.fillEllipse(static_cast<float>(cellX + cellWidth - 18),
                          static_cast<float>(height) * 0.5f - 3.0f,
                          6.0f, 6.0f);
        }
    }
}

void CrusherAudioProcessorEditor::listBoxItemClicked(
    int row, const juce::MouseEvent& event)
{
    const int eventWidth = event.eventComponent != nullptr
        ? event.eventComponent->getWidth() : presetList.getWidth();
    const int clickedColumn = event.position.x
        >= static_cast<float>(eventWidth) * 0.5f ? 1 : 0;
    const int clickedIndex = row * 2 + clickedColumn;
    if (! juce::isPositiveAndBelow(clickedIndex, visiblePresetFiles.size()))
    {
        if (event.mods.isPopupMenu())
            showPresetContextMenu({}, event.getScreenPosition());
        return;
    }
    selectedPresetBrowserColumn = clickedColumn;
    presetList.repaintRow(row);
    const int index = row * 2 + selectedPresetBrowserColumn;
    if (! juce::isPositiveAndBelow(index, visiblePresetFiles.size()))
        return;

    const int firstColumnWidth = eventWidth / 2;
    const int cellX = selectedPresetBrowserColumn == 0
        ? 0 : firstColumnWidth;
    const float positionInCell = event.position.x
        - static_cast<float>(cellX);
    const auto file = visiblePresetFiles[index];
    if (event.mods.isPopupMenu())
    {
        showPresetContextMenu(file, event.getScreenPosition());
        return;
    }
    if (positionInCell >= 3.0f && positionInCell <= 22.0f)
    {
        togglePresetFavourite(file);
        return;
    }
    loadPresetFile(file);
}

void CrusherAudioProcessorEditor::returnKeyPressed(int row)
{
    loadPresetAtBrowserCell(row, selectedPresetBrowserColumn);
}

void CrusherAudioProcessorEditor::selectedRowsChanged(int row)
{
    if (row < 0)
        return;

    if (! juce::isPositiveAndBelow(
            row * 2 + selectedPresetBrowserColumn,
            visiblePresetFiles.size()))
        selectedPresetBrowserColumn = 0;

    presetList.repaintRow(row);
}

void CrusherAudioProcessorEditor::loadPresetAtBrowserCell(
    int row, int column)
{
    const int index = row * 2 + juce::jlimit(0, 1, column);
    if (! juce::isPositiveAndBelow(index, visiblePresetFiles.size()))
        return;

    loadPresetFile(visiblePresetFiles[index]);
}

void CrusherAudioProcessorEditor::loadPresetFile(const juce::File& file)
{
    const bool preserveMix = mixLockButton.getToggleState();
    const double mixToPreserve = lockedMixValue;
    const juce::ScopedValueSetter<bool> mixRestoreGuard(
        restoringLockedMix, preserveMix);
    const auto result = processor.loadPresetFromFile(file);
    if (result.failed())
    {
        information.setText("PRESET LOAD FAILED | " + result.getErrorMessage(),
                            juce::dontSendNotification);
        return;
    }

    if (preserveMix)
    {
        setParameterActualValue(
            processor.parameters.getParameter("mix"),
            static_cast<float>(mixToPreserve));
        lockedMixValue = mixToPreserve;
    }

    currentPresetFile = file;
    selectedPresetFolder = file.getParentDirectory();
    if (presetBrowserOpen)
        refreshPresetList();
    else
        updatePresetNavigationDisplay();
    information.setText(
        "PRESET LOADED: " + file.getFileNameWithoutExtension()
            + "\nClick another preset to load it, or use the Save As... button to save a copy.",
        juce::dontSendNotification);
}

void CrusherAudioProcessorEditor::saveCurrentPreset()
{
    if (currentPresetFile == juce::File())
    {
        showPresetSaveAs();
        return;
    }

    const auto result = processor.savePresetToFile(currentPresetFile);
    if (result.failed())
    {
        information.setText("PRESET SAVE FAILED | " + result.getErrorMessage(),
                            juce::dontSendNotification);
        return;
    }

    refreshPresetList();
    updatePresetNavigationDisplay();
    information.setText(
        "PRESET SAVED: " + currentPresetFile.getFileNameWithoutExtension()
            + "\nThe current preset file has been overwritten with the latest settings.",
        juce::dontSendNotification);
}

void CrusherAudioProcessorEditor::showPresetSaveAs()
{
    const auto root = CrusherAudioProcessor::getPresetsDirectory();
    root.createDirectory();
    auto destinationFolder = selectedPresetFolder;
    if (! destinationFolder.isDirectory()
        || (destinationFolder != root
            && ! destinationFolder.isAChildOf(root)))
        destinationFolder = root;

    const auto suggestedName = currentPresetFile == juce::File()
        ? juce::String("New Preset")
        : currentPresetFile.getFileNameWithoutExtension();
    const auto suggestedFile = destinationFolder.getChildFile(
        suggestedName + presetFileExtension);
    presetFileChooser = std::make_unique<juce::FileChooser>(
        "Save Crusher Preset As", suggestedFile,
        "*" + juce::String(presetFileExtension), true, false, this);
    const auto safeThis = juce::Component::SafePointer<
        CrusherAudioProcessorEditor>(this);
    presetFileChooser->launchAsync(
        juce::FileBrowserComponent::saveMode
            | juce::FileBrowserComponent::canSelectFiles
            | juce::FileBrowserComponent::warnAboutOverwriting,
        [safeThis](const juce::FileChooser& chooser)
        {
            auto* editor = safeThis.getComponent();
            if (editor == nullptr)
                return;

            auto file = chooser.getResult();
            if (file == juce::File())
                return;
            if (! file.hasFileExtension(presetFileExtension))
                file = file.withFileExtension(presetFileExtension);

            const auto rootDirectory =
                CrusherAudioProcessor::getPresetsDirectory();
            const auto parentDirectory = file.getParentDirectory();
            if (parentDirectory != rootDirectory
                && ! parentDirectory.isAChildOf(rootDirectory))
            {
                editor->information.setText(
                    "PRESET SAVE FAILED | Presets must be saved inside the Crusher Presets folder.",
                    juce::dontSendNotification);
                return;
            }

            const auto result = editor->processor.savePresetToFile(file);
            if (result.failed())
            {
                editor->information.setText(
                    "PRESET SAVE FAILED | " + result.getErrorMessage(),
                    juce::dontSendNotification);
                return;
            }

            editor->currentPresetFile = file;
            editor->selectedPresetFolder = parentDirectory;
            editor->rebuildPresetFolderTree();
            editor->updatePresetNavigationDisplay();
            editor->information.setText(
                "PRESET SAVED AS: " + file.getFileNameWithoutExtension()
                    + "\nThe Save button now overwrites this file; use Save As... to save another copy.",
                juce::dontSendNotification);
        });
}

void CrusherAudioProcessorEditor::paint(juce::Graphics& g)
{
    const int designHeight = activeDesignHeight();
    g.fillAll(juce::Colour(0xff1b2026));
    const float scale = juce::jmin(
        static_cast<float>(getWidth()) / static_cast<float>(designWidth),
        static_cast<float>(getHeight()) / static_cast<float>(designHeight));
    const float offsetX = 0.5f * (static_cast<float>(getWidth())
        - static_cast<float>(designWidth) * scale);
    const float offsetY = 0.5f * (static_cast<float>(getHeight())
        - static_cast<float>(designHeight) * scale);
    g.saveState();
    g.addTransform(
        juce::AffineTransform::scale(scale).translated(offsetX, offsetY));

    const double themeValue = themeDial != nullptr
        ? themeDial->slider.getValue() : 50.0;
    g.setColour(themeSolidColour(themeValue));
    g.fillRoundedRectangle(8.0f, 52.0f,
                           static_cast<float>(designWidth - 16), 216.0f,
                           12.0f);
    // Same 926x530 artwork coordinates as the sequencer: paint only inside
    // the top surround, leaving all dark control panels and the gap opaque.
    const float imageOpacity = themeImageOpacity(themeValue);
    if (themeBackdrop.isValid() && imageOpacity > 0.0f)
    {
        g.saveState();
        juce::Path clip;
        clip.addRoundedRectangle(8.0f, 52.0f,
            static_cast<float>(designWidth - 16), 216.0f, 12.0f);
        g.reduceClipRegion(clip);
        g.setOpacity(imageOpacity);
        g.drawImage(themeBackdrop,
            juce::Rectangle<float>(8.0f, 52.0f, 910.0f, 470.0f),
            juce::RectanglePlacement::fillDestination);
        g.restoreState();
    }

    const auto drawControlPanel = [&g](float x, float y,
                                       float width, float height)
    {
        g.setColour(juce::Colour(0xff171b20));
        g.fillRoundedRectangle(x, y, width, height, 8.0f);
        g.setColour(knobTrackColour);
        g.drawRoundedRectangle(x, y, width, height, 8.0f, 1.0f);
    };

    if (presetBrowserOpen)
    {
        drawControlPanel(18.0f, 62.0f, 890.0f, 196.0f);
        g.setColour(knobTrackColour);
        g.drawLine(288.0f, 70.0f, 288.0f, 250.0f, 1.0f);
    }
    else
    {
        // Match the 6-pixel gap between the sequencer's top/bottom panels.
        // Keep the rightmost MIX/OUT panel fixed at its original position.
        drawControlPanel(18.0f, 62.0f, 264.0f, 170.0f); // Contextual DIST: 3x2 knob grid
        drawControlPanel(288.0f, 62.0f, 66.0f, 170.0f); // Shared OUTPUT / input MODE
        drawControlPanel(360.0f, 62.0f, 332.0f, 170.0f); // Seven-band graphic EQ
        drawControlPanel(698.0f, 62.0f, 66.0f, 170.0f); // Original filter
        drawControlPanel(770.0f, 62.0f, 66.0f, 170.0f); // Original comp
        drawControlPanel(842.0f, 62.0f, 66.0f, 170.0f); // Original mix/out
        g.setFont(juce::Font(juce::FontOptions(10.0f, juce::Font::bold)));
        // EQ and ENV labels/power LEDs are integrated in their view
        // buttons, so there are no extra painted labels in this header.
        // No DISTORTION heading: its power LED sits at the top-left
        // of the contextual distortion panel, matching the panel inset.
        // Neither Filter nor Comp needs a painted title: their labels
        // identify the controls, with matching amber power LEDs.
    }

    const juce::Rectangle<float> typeStrip(
        18.0f, 53.0f, static_cast<float>(designWidth - 36), 7.0f);
    g.setColour(themeDistortionRed);
    g.fillRoundedRectangle(typeStrip, 3.5f);

    if (logoDrawable != nullptr)
        logoDrawable->drawWithin(
            g, juce::Rectangle<float>(22.0f, 5.0f, 280.0f, 43.0f),
            juce::RectanglePlacement::xLeft
                | juce::RectanglePlacement::yMid
                | juce::RectanglePlacement::onlyReduceInSize,
            1.0f);
    g.restoreState();
}

void CrusherAudioProcessorEditor::resized()
{
    const int designHeight = activeDesignHeight();
    content.setTransform(juce::AffineTransform());
    content.setBounds(0, 0, designWidth, designHeight);
    const float scale = juce::jmin(
        static_cast<float>(getWidth()) / static_cast<float>(designWidth),
        static_cast<float>(getHeight()) / static_cast<float>(designHeight));
    const float offsetX = 0.5f * (static_cast<float>(getWidth())
        - static_cast<float>(designWidth) * scale);
    const float offsetY = 0.5f * (static_cast<float>(getHeight())
        - static_cast<float>(designHeight) * scale);
    content.setTransform(
        juce::AffineTransform::scale(scale).translated(offsetX, offsetY));

    constexpr int knobWidth = 60;
    const auto placeDial = [knobWidth](
        Dial* dial, int knobX, int labelY, int labelWidth = 70)
    {
        if (dial == nullptr)
            return;
        dial->label.setBounds(
            knobX - (labelWidth - knobWidth) / 2,
            labelY, labelWidth, 13);
        dial->slider.setBounds(knobX, labelY + 13, knobWidth, 57);
    };

    // All six-knob distortion types use the same three-column, two-row grid.
    // In models with DRIVE, TONE sits directly beneath DRIVE. Hump uses
    // MECH as its first control, so TONE sits directly beneath MECH.
    // Asym retains five knobs; its two lower controls are swapped so TONE
    // is below DRIVE. Amp has no TONE and uses the bottom row for CAB/BIAS.
    // Knob columns: 39, 120, 201 (81 px); label rows: 67, 154. DIST is x=18..282.
    const bool ampSelected = distortionTypeBox.getSelectedId() == 7;
    const bool asymSelected = distortionTypeBox.getSelectedId() == 9;
    const bool bumpySelected = distortionTypeBox.getSelectedId() == 8;
    const bool combSelected = distortionTypeBox.getSelectedId() == 12;
    const bool crackleSelected = distortionTypeBox.getSelectedId() == 5;
    const bool diodeSelected = distortionTypeBox.getSelectedId() == 19;
    const bool codecSelected = distortionTypeBox.getSelectedId() == 21;
    const bool vocalSelected = distortionTypeBox.getSelectedId() == 22;
    const bool cascadeSelected = distortionTypeBox.getSelectedId() == 23;
    const bool loFiSelected = distortionTypeBox.getSelectedId() == 20;
    const bool lightningSelected = distortionTypeBox.getSelectedId() == 6;
    const bool pixelatedSelected = distortionTypeBox.getSelectedId() == 11;
    const bool rectifySelected = distortionTypeBox.getSelectedId() == 4;
    const bool softClipSelected = distortionTypeBox.getSelectedId() == 1;
    const bool tubeSelected = distortionTypeBox.getSelectedId() == 3;
    const bool stressedSelected = distortionTypeBox.getSelectedId() == 13;
    const bool saturationSelected = distortionTypeBox.getSelectedId() == 2;
    const bool samuraiSelected = distortionTypeBox.getSelectedId() == 14;
    const bool razorSelected = distortionTypeBox.getSelectedId() == 10;
    const bool freshSelected = distortionTypeBox.getSelectedId() == 15;
    const bool fuzzSelected = distortionTypeBox.getSelectedId() == 17;
    const bool hardClipSelected = distortionTypeBox.getSelectedId() == 16;

    // Shared DRIVE must remain at the top-left for every model that uses it.
    placeDial(driveDial,
        (ampSelected || asymSelected || bumpySelected || combSelected
         || crackleSelected || diodeSelected || codecSelected || vocalSelected || cascadeSelected || loFiSelected || freshSelected || fuzzSelected || hardClipSelected
         || lightningSelected || pixelatedSelected || razorSelected
         || rectifySelected || samuraiSelected || saturationSelected
         || softClipSelected || stressedSelected || tubeSelected) ? 39 : 202,
        67);

    // Amp: DRIVE / SOFT CLIP / TOP BOOST above CAB / BIAS.
    placeDial(ampSoftDial, 120, 67);
    placeDial(ampTopDial, 201, 67, 90);
    placeDial(ampBiasDial, 201, 154);

    // Asym: keep three knobs on top and move TONE below DRIVE.
    placeDial(asymBiasDial, 120, 67);
    placeDial(asymSkewDial, 201, 67);
    placeDial(asymToneDial, 39, 154);
    placeDial(asymShapeDial, 120, 154);

    // Hump has MECH rather than DRIVE as its first knob.
    placeDial(mechDial, 39, 67);
    placeDial(tubeDial, 120, 67);
    placeDial(humpBodyDial, 201, 67);
    placeDial(humpToneDial, 39, 154);
    placeDial(humpBiasDial, 120, 154);
    placeDial(humpFoldDial, 201, 154);

    // All remaining six-knob types: three top, three bottom, TONE bottom-left.
    placeDial(bumpyRippleDial, 120, 67);
    placeDial(bumpyRateDial, 201, 67);
    placeDial(bumpyToneDial, 39, 154);
    placeDial(bumpyContourDial, 120, 154, 90);
    placeDial(bumpyBiasDial, 201, 154);

    placeDial(combTimeDial, 120, 67);
    placeDial(combFeedbackDial, 201, 67, 90);
    placeDial(combToneDial, 39, 154);
    placeDial(combDampDial, 120, 154);
    placeDial(combSpreadDial, 201, 154);

    // Diode Clip: DRIVE / DIODE / ENV above IN HP / OUT HP / OUT LP.
    placeDial(diodeShapeDial, 120, 67);
    placeDial(diodeEnvelopeDial, 201, 67);
    placeDial(diodeInputHPDial, 39, 154);
    placeDial(diodeOutputHPDial, 120, 154);
    placeDial(diodeOutputLPDial, 201, 154);

    placeDial(crackleDensityDial, 120, 67);
    placeDial(crackleBurstDial, 201, 67);
    placeDial(crackleToneDial, 39, 154);
    placeDial(crackleDecayDial, 120, 154);
    placeDial(crackleGritDial, 201, 154);

    // Codec: DRIVE / CODEC / RATE above TONE / BITS / DAMAGE.
    placeDial(codecModeDial, 120, 67);
    placeDial(codecRateDial, 201, 67);
    placeDial(codecToneDial, 39, 154);
    placeDial(codecBitsDial, 120, 154);
    placeDial(codecDamageDial, 201, 154);

    // Cascade: DRIVE / STAGES / VARIATION above EMPH / BIAS / PUNCH.
    placeDial(cascadeStagesDial, 120, 67);
    placeDial(cascadeVariationDial, 201, 67, 92);
    placeDial(cascadeEmphasisDial, 39, 154);
    placeDial(cascadeBiasDial, 120, 154);
    placeDial(cascadePunchDial, 201, 154);

    // Vocal: DRIVE / CHARACTER / PRESENCE above BODY / BITE / SMOOTH.
    placeDial(vocalCharacterDial, 120, 67, 95);
    placeDial(vocalPresenceDial, 201, 67, 90);
    placeDial(vocalBodyDial, 39, 154);
    placeDial(vocalBiteDial, 120, 154);
    placeDial(vocalSmoothDial, 201, 154);

    // Lo-Fi: DRIVE / RATE / BITS above CLIP / RECTIFY / NOISE.
    placeDial(loFiRateDial, 120, 67);
    placeDial(loFiBitsDial, 201, 67);
    placeDial(loFiClipDial, 39, 154);
    placeDial(loFiRectifyDial, 120, 154, 85);
    placeDial(loFiNoiseDial, 201, 154);

    placeDial(pixelatedRateDial, 120, 67);
    placeDial(pixelatedBitsDial, 201, 67);
    placeDial(pixelatedToneDial, 39, 154);
    placeDial(pixelatedDitherDial, 120, 154);
    placeDial(pixelatedJitterDial, 201, 154);

    placeDial(rectifyWaveDial, 120, 67);
    placeDial(rectifyBiasDial, 201, 67);
    placeDial(rectifyToneDial, 39, 154);
    placeDial(rectifyCurveDial, 120, 154);
    placeDial(rectifyFoldDial, 201, 154);

    placeDial(softClipThresholdDial, 120, 67);
    placeDial(softClipKneeDial, 201, 67);
    placeDial(softClipToneDial, 39, 154);
    placeDial(softClipBiasDial, 120, 154);
    placeDial(softClipBodyDial, 201, 154);

    placeDial(tubeBiasDial, 120, 67);
    placeDial(tubeHeatDial, 201, 67);
    placeDial(tubeToneDial, 39, 154);
    placeDial(tubeSagDial, 120, 154);
    placeDial(tubeBodyDial, 201, 154);

    placeDial(stressedPressDial, 120, 67);
    placeDial(stressedFoldDial, 201, 67);
    placeDial(stressedToneDial, 39, 154);
    placeDial(stressedTensionDial, 120, 154);
    placeDial(stressedBiasDial, 201, 154);

    placeDial(saturationCurveDial, 120, 67);
    placeDial(saturationBiasDial, 201, 67);
    placeDial(saturationToneDial, 39, 154);
    placeDial(saturationWarmDial, 120, 154);
    placeDial(saturationBodyDial, 201, 154);

    placeDial(samuraiEdgeDial, 120, 67);
    placeDial(samuraiGateDial, 201, 67);
    placeDial(samuraiToneDial, 39, 154);
    placeDial(samuraiBladeDial, 120, 154);
    placeDial(samuraiRingDial, 201, 154);

    placeDial(razorEdgeDial, 120, 67);
    placeDial(razorFreqDial, 201, 67);
    placeDial(razorToneDial, 39, 154);
    placeDial(razorRingDial, 120, 154);
    placeDial(razorBiteDial, 201, 154);

    placeDial(lightningSparkDial, 120, 67);
    placeDial(lightningReactDial, 201, 67);
    placeDial(lightningToneDial, 39, 154);
    placeDial(lightningClipDial, 120, 154);
    placeDial(lightningEdgeDial, 201, 154);

    placeDial(freshFrequencyDial, 120, 67);
    placeDial(freshExciteDial, 201, 67);
    placeDial(freshToneDial, 39, 154);
    placeDial(freshAirDial, 120, 154);
    placeDial(freshSaturationDial, 201, 154);

    placeDial(fuzzGateDial, 120, 67);
    placeDial(fuzzBiasDial, 201, 67);
    placeDial(fuzzToneDial, 39, 154);
    placeDial(fuzzClipDial, 120, 154);
    placeDial(fuzzBodyDial, 201, 154);

    placeDial(hardClipThresholdDial, 120, 67);
    placeDial(hardClipBiasDial, 201, 67);
    placeDial(hardClipToneDial, 39, 154);
    placeDial(hardClipKneeDial, 120, 154);
    placeDial(hardClipFoldDial, 201, 154);

    // Amp CAB: original 150 x 25 becomes 120 x 20 (same 6:1 ratio),
    // matching the sequencer dropdown height. CAB occupies the bottom-left,
    // while BIAS uses the bottom-right knob column.
    ampCabinetLabel.setBounds(50, 154, 120, 13);
    ampCabinetBox.setBounds(50, 177, 120, 20);
    // Compact, stacked global input-mode LEDs beneath shared OUTPUT.
    distStereoButton.setBounds(290, 161, 62, 20);
    distMonoButton.setBounds(290, 183, 62, 20);
    placeDial(distOutputDial, 291, 67);
    // Shared EQ/ENV header: clickable power LEDs are inset within the
    // respective 50x23 view buttons (2px left, 4px top). Each LED's
    // separate hit target toggles DSP; the surrounding button selects view.
    eqViewButton.setBounds(390, 67, 50, 23);
    envViewButton.setBounds(446, 67, 50, 23);
    eqOnButton.setBounds(392, 71, 14, 14);
    envOnButton.setBounds(448, 71, 14, 14);
    eqModeBox.setBounds(512, 69, 90, 20);
    eqResetButton.setBounds(620, 67, 50, 23);
    // Restore EQ graph height now that no secondary power LED sits below.
    if (eqGraph != nullptr) eqGraph->setBounds(370, 97, 312, 126);
    // Match Filter and Comp: 14x14 amber LED, 3 px inset from the
    // panel's right edge and 4 px below its top edge.
    distortionOnButton.setBounds(21, 66, 14, 14);

    // Keep the original Mix/Out panel (842,62,66,170), its knob coordinates
    // and Mix Lock position. Filter and Comp match its 66-pixel width.
    // Filter's two knobs align precisely with MIX and OUT in the original layout.
    // The filter algorithm is unchanged by this GUI-only adjustment.
    filterOnButton.setBounds(747, 66, 14, 14);
    placeDial(filterCutoffDial, 701, 67);
    placeDial(filterResonanceDial, 701, 154);
    filterLowButton.setBounds(703, 138, 17, 15);
    filterBandButton.setBounds(722, 138, 17, 15);
    filterHighButton.setBounds(741, 138, 17, 15);
    // The ENV view shares the central EQ panel, not the main window space.
    placeDial(envSensitivityDial, 375, 110);
    placeDial(envAttackDial, 451, 110);
    placeDial(envReleaseDial, 527, 110);
    placeDial(envDepthDial, 603, 110);
    if (envMeter != nullptr) envMeter->setBounds(371, 200, 210, 13);
    envAssignmentLabel.setBounds(586, 196, 91, 21);
    // COMP mirrors FILTER: matched LED and top-row knob. PHAT's centre
    // (y=186.5) shares the exact horizontal axis of the OUT knob circle.
    compressorOnButton.setBounds(819, 66, 14, 14);
    placeDial(compAmountDial, 773, 67);
    phatButton.setBounds(777, 174, 52, 25);
    placeDial(mixDial, 845, 67);
    placeDial(outputDial, 845, 154);

    versionLabel.setBounds(designWidth - 61, 2, 58, 12);
    distortionTypeLabel.setBounds(348, 17, 54, 20);
    distortionTypeBox.setBounds(402, 13, 106, 26);
    currentPresetLabel.setBounds(632, 13, 94, 26);
    previousPresetButton.setBounds(726, 13, 21, 26);
    nextPresetButton.setBounds(747, 13, 21, 26);
    presetBrowserButton.setBounds(774, 13, 26, 26);
    if (themeDial != nullptr)
    {
        themeDial->label.setBounds(designWidth - 118, 2, 58, 12);
        themeDial->slider.setBounds(designWidth - 104, 16, 30, 30);
    }
    if (browserMixDial != nullptr)
    {
        browserMixDial->label.setBounds(designWidth - 118, 2, 58, 12);
        browserMixDial->slider.setBounds(designWidth - 104, 16, 30, 30);
    }
    mixLockButton.setBounds(891, 66, 14, 14);
    bypassButton.setBounds(designWidth - 47, 16, 30, 30);

    presetFolderLabel.setBounds(28, 66, 252, 14);
    presetListLabel.setBounds(298, 66, 560, 14);
    presetFolderTree.setBounds(26, 82, 256, 168);
    presetList.setBounds(296, 82, 564, 168);
    presetSaveButton.setBounds(870, 85, 26, 26);
    presetSaveAsButton.setBounds(870, 130, 26, 26);
    presetOpenFolderButton.setBounds(870, 175, 26, 26);

    information.setBounds(25, 236, 670, 28);
    randomButton.setBounds(836, 236, 26, 26);
    sequencerButton.setBounds(presetBrowserOpen ? 865 : 872,
                              presetBrowserOpen ? 220 : 236,
                              36, 26);
    if (sequencerPanel != nullptr)
        sequencerPanel->setBounds(
            8, collapsedDesignHeight, designWidth - 16,
            expandedDesignHeight - collapsedDesignHeight - 8);

    if (isShowing() && ! presetBrowserOpen
        && getWidth() != observedEditorWidth)
    {
        const int scalePercent = juce::jlimit(
            75, 300, juce::roundToInt(
                100.0f * static_cast<float>(getWidth())
                    / static_cast<float>(designWidth)));
        information.setText(
            "GUI SIZE: " + juce::String(scalePercent) + "%\n"
                + juce::String(getWidth()) + " x "
                + juce::String(getHeight()) + " px",
            juce::dontSendNotification);
    }
}

void CrusherAudioProcessorEditor::timerCallback()
{
    const bool leftKeyDown = isShowing()
        && juce::KeyPress::isKeyCurrentlyDown(juce::KeyPress::leftKey);
    const bool rightKeyDown = isShowing()
        && juce::KeyPress::isKeyCurrentlyDown(juce::KeyPress::rightKey);
    if (leftKeyDown && ! previousPresetKeyDown)
        stepPreset(-1);
    if (rightKeyDown && ! nextPresetKeyDown)
        stepPreset(1);
    previousPresetKeyDown = leftKeyDown;
    nextPresetKeyDown = rightKeyDown;

    updateDistortionControls();
    updateSectionDimming();
    updateSequencerTargetHighlights();
    if (sequencerOpen && sequencerPanel != nullptr)
        sequencerPanel->timerTick();
    updateAnimatedParameterDisplays();
    if (envViewActive && envMeter != nullptr && envMeter->isShowing())
        envMeter->repaint();
    envAssignmentLabel.setText(
        juce::String(processor.getEnvelopeAssignmentCount()) + "/"
            + juce::String(processor.getEnvelopeTargetLimit()) + " TARGETS",
        juce::dontSendNotification);
    // Follow host automation/loaded state of EQ/ENV power without changing views.
    if (envViewActive)
    {
        const bool enabled = envOnButton.getToggleState();
        for (auto* dial : { envSensitivityDial, envAttackDial,
                            envReleaseDial, envDepthDial })
        {
            dial->label.setAlpha(enabled ? 1.0f : 0.32f);
            dial->slider.setAlpha(enabled ? 1.0f : 0.32f);
        }
        if (envMeter != nullptr) envMeter->setAlpha(enabled ? 1.0f : 0.32f);
        envAssignmentLabel.setAlpha(enabled ? 1.0f : 0.32f);
    }

    const auto processorPresetFile = processor.getCurrentPresetFile();
    if (processorPresetFile != currentPresetFile)
    {
        const auto root = CrusherAudioProcessor::getPresetsDirectory();
        const auto parent = processorPresetFile.getParentDirectory();
        if (processorPresetFile != juce::File()
            && (parent == root || parent.isAChildOf(root)))
        {
            currentPresetFile = processorPresetFile;
            selectedPresetFolder = parent;
        }
        else
            currentPresetFile = juce::File();
        if (presetBrowserOpen)
            rebuildPresetFolderTree();
        else
            updatePresetNavigationDisplay();
    }

    const int currentWidth = getWidth();
    const double currentTheme = themeDial != nullptr
        ? themeDial->slider.getValue() : 100.0;
    if (currentWidth != observedEditorWidth
        || std::abs(currentTheme - observedThemeValue) > 0.0001)
    {
        observedEditorWidth = currentWidth;
        observedThemeValue = currentTheme;
        stableSettingsTicks = 0;
        settingsDirty = true;
    }
    else if (settingsDirty && ++stableSettingsTicks >= 5)
    {
        saveInterfaceSettings();
        stableSettingsTicks = 0;
        settingsDirty = false;
    }
}

void CrusherAudioProcessorEditor::saveInterfaceSettings() const
{
    const auto directory = CrusherAudioProcessor::getSettingsDirectory();
    if (! directory.isDirectory() && directory.createDirectory().failed())
        return;
    const int scalePercent = juce::jlimit(
        75, 300, juce::roundToInt(100.0f * static_cast<float>(getWidth())
                                  / static_cast<float>(designWidth)));
    const double themePercent = themeDial != nullptr
        ? juce::jlimit(0.0, 100.0, themeDial->slider.getValue()) : 100.0;
    juce::String text;
    text << "; Crusher interface settings\r\n"
         << "[GUI]\r\n"
         << "ScalePercent=" << scalePercent << "\r\n"
         << "ThemePercent=" << juce::String(themePercent, 1) << "\r\n"
         << "SequencerOpen=" << (sequencerOpen ? 1 : 0) << "\r\n"
         << "SequencerAnimate="
         << (sequencerPanel != nullptr && sequencerPanel->getAnimate()
                 ? 1 : 0)
         << "\r\n";
    directory.getChildFile("Settings.ini").replaceWithText(text);
}



