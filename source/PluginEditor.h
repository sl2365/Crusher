#pragma once

#include "PluginProcessor.h"
#include <cmath>
#include <functional>

class CrusherAudioProcessorEditor final : public juce::AudioProcessorEditor,
                                       public juce::DragAndDropContainer,
                                       private juce::Timer,
                                       private juce::ListBoxModel
{
public:
    explicit CrusherAudioProcessorEditor(CrusherAudioProcessor&);
    ~CrusherAudioProcessorEditor() override;
    void paint(juce::Graphics&) override;
    void resized() override;
    void mouseDown(const juce::MouseEvent&) override;
    void mouseEnter(const juce::MouseEvent&) override;
    bool keyPressed(const juce::KeyPress&) override;

private:
    using SliderAttachment =
        juce::AudioProcessorValueTreeState::SliderAttachment;

    class DetentedSlider final : public juce::Slider
    {
    public:
        std::function<bool()> onSavedStateReset;
        std::function<void(juce::Point<int>)> onContextMenu;
        void mouseDown(const juce::MouseEvent& event) override
        {
            if (event.mods.isPopupMenu() && onContextMenu != nullptr)
            {
                onContextMenu(event.getScreenPosition());
                return;
            }
            juce::Slider::mouseDown(event);
        }
        void setCentreDetent(double value, double radius)
        {
            centreDetent = value;
            detentRadius = juce::jmax(0.0, radius);
            centreDetentEnabled = true;
            getProperties().set("hasCentreDetent", true);
            getProperties().set("centreDetentValue", value);
        }
        double snapValue(double attemptedValue, DragMode dragMode) override
        {
            if (centreDetentEnabled && dragMode != notDragging
                && std::abs(attemptedValue - centreDetent) <= detentRadius)
                return centreDetent;
            return attemptedValue;
        }
        void mouseDoubleClick(const juce::MouseEvent& event) override
        {
            if (onSavedStateReset != nullptr && onSavedStateReset())
                return;
            juce::Slider::mouseDoubleClick(event);
        }
    private:
        double centreDetent = 0.0;
        double detentRadius = 0.0;
        bool centreDetentEnabled = false;
    };

    class SteppedComboBox final : public juce::ComboBox
    {
    public:
        void mouseWheelMove(const juce::MouseEvent&,
                            const juce::MouseWheelDetails& wheel) override
        {
            if (! isEnabled() || getNumItems() <= 0
                || std::abs(wheel.deltaY) < 0.0001f)
                return;
            const int direction = wheel.deltaY > 0.0f ? -1 : 1;
            const auto now = juce::Time::getMillisecondCounter();
            if (direction == lastWheelDirection
                && now - lastWheelTimeMs < 100u)
                return;
            lastWheelDirection = direction;
            lastWheelTimeMs = now;
            const int current = juce::jmax(0, getSelectedItemIndex());
            const int next = juce::jlimit(
                0, getNumItems() - 1, current + direction);
            if (next != getSelectedItemIndex())
                setSelectedItemIndex(next, juce::sendNotificationSync);
        }
    private:
        juce::uint32 lastWheelTimeMs = 0;
        int lastWheelDirection = 0;
    };

    class PresetDisplayLabel final : public juce::Label
    {
    public:
        std::function<void(int)> onStep;
        std::function<void()> onShowMenu;
        void paint(juce::Graphics&) override;
        void mouseDown(const juce::MouseEvent&) override;
        void mouseWheelMove(const juce::MouseEvent&,
                            const juce::MouseWheelDetails&) override;
    private:
        juce::uint32 lastWheelTimeMs = 0;
        int lastWheelDirection = 0;
    };

    class PresetListBox final : public juce::ListBox
    {
    public:
        using juce::ListBox::ListBox;
        std::function<bool(int)> onHorizontalStep;
        bool keyPressed(const juce::KeyPress& key) override
        {
            int direction = 0;
            if (key.getKeyCode() == juce::KeyPress::leftKey)
                direction = -1;
            else if (key.getKeyCode() == juce::KeyPress::rightKey)
                direction = 1;
            if (direction != 0 && onHorizontalStep != nullptr
                && onHorizontalStep(direction))
                return true;
            return juce::ListBox::keyPressed(key);
        }
    };

    class ParameterDragLabel final : public juce::Label
    {
    public:
        std::function<juce::String()> parameterIdProvider;
        std::function<void(juce::Point<int>)> onContextMenu;
        void mouseDown(const juce::MouseEvent& event) override
        {
            if (event.mods.isPopupMenu() && onContextMenu != nullptr)
            {
                onContextMenu(event.getScreenPosition());
                return;
            }
            juce::Label::mouseDown(event);
        }
        bool dragHandleOnly = false;
        bool dragHandleRight = false;
        void setTargetAppearance(bool assigned, juce::Colour accent);
        void paint(juce::Graphics&) override;
        void mouseDrag(const juce::MouseEvent&) override;
    private:
        bool targetAssigned = false;
        juce::Colour targetAccent { 0xff34d6c6 };
    };

    struct Dial
    {
        DetentedSlider slider;
        ParameterDragLabel label;
        std::unique_ptr<SliderAttachment> attachment;
        juce::String help;
        juce::String parameterId;
    };

    class SequencerPanel;
    class EQGraph;
    class EnvMeter;
    class FilterShapeButton final : public juce::Button
    {
    public:
        explicit FilterShapeButton(int mode) : juce::Button("Filter shape"), mode(mode) {}
        void paintButton(juce::Graphics& graphics, bool highlighted, bool down) override;
    private:
        int mode = 0;
    };
    class FolderTreeItem;
    class LookAndFeel final : public juce::LookAndFeel_V4
    {
    public:
        explicit LookAndFeel(bool compactPopupFont = false);
        juce::Label* createSliderTextBox(juce::Slider&) override;
        juce::Font getPopupMenuFont() override;
        void setZoomPopupScale(float scale) noexcept { zoomPopupScale = scale; }
        juce::Font getComboBoxFont(juce::ComboBox&) override;
        void drawRotarySlider(juce::Graphics&, int, int, int, int,
                              float, float, float, juce::Slider&) override;
        void drawLinearSlider(juce::Graphics&, int, int, int, int,
                              float, float, float,
                              juce::Slider::SliderStyle,
                              juce::Slider&) override;
        void drawToggleButton(juce::Graphics&, juce::ToggleButton&,
                              bool, bool) override;
        void drawPopupMenuItem(juce::Graphics&,
                               const juce::Rectangle<int>&,
                               bool, bool, bool, bool, bool,
                               const juce::String&, const juce::String&,
                               const juce::Drawable*,
                               const juce::Colour*) override;
        void drawPopupMenuBackground(juce::Graphics&, int, int) override;
        juce::PopupMenu::Options getOptionsForComboBoxPopupMenu(
            juce::ComboBox&, juce::Label&) override;
        void setPopupBorderColour(juce::Colour colour)
        {
            popupBorderColour = colour;
        }
        juce::Colour getPopupBorderColour() const noexcept
        {
            return popupBorderColour;
        }
    private:
        juce::Colour popupBorderColour { 0xff1479b8 };
        bool useCompactPopupFont = false;
        float zoomPopupScale = 1.0f;
    };

    CrusherAudioProcessor& processor;
    LookAndFeel lookAndFeel;
    LookAndFeel zoomMenuLookAndFeel;
    juce::Component content;
    std::vector<std::unique_ptr<Dial>> dials;
    Dial* driveDial = nullptr;
    Dial* mechDial = nullptr;
    Dial* tubeDial = nullptr;
    Dial* humpBodyDial = nullptr;
    Dial* humpBiasDial = nullptr;
    Dial* humpFoldDial = nullptr;
    Dial* humpToneDial = nullptr;
    Dial* ampSoftDial = nullptr;
    Dial* ampTopDial = nullptr;
    Dial* ampBiasDial = nullptr;
    Dial* bumpyRippleDial = nullptr;
    Dial* bumpyRateDial = nullptr;
    Dial* bumpyContourDial = nullptr;
    Dial* bumpyBiasDial = nullptr;
    Dial* bumpyToneDial = nullptr;
    Dial* combTimeDial = nullptr;
    Dial* combFeedbackDial = nullptr;
    Dial* combDampDial = nullptr;
    Dial* combSpreadDial = nullptr;
    Dial* combToneDial = nullptr;
    Dial* crackleDensityDial = nullptr;
    Dial* crackleBurstDial = nullptr;
    Dial* crackleDecayDial = nullptr;
    Dial* crackleGritDial = nullptr;
    Dial* crackleToneDial = nullptr;
    Dial* diodeInputHPDial = nullptr;
    Dial* diodeEnvelopeDial = nullptr;
    Dial* diodeShapeDial = nullptr;
    Dial* diodeOutputHPDial = nullptr;
    Dial* diodeOutputLPDial = nullptr;
    Dial* codecModeDial = nullptr;
    Dial* codecRateDial = nullptr;
    Dial* codecBitsDial = nullptr;
    Dial* codecDamageDial = nullptr;
    Dial* codecToneDial = nullptr;
    Dial* vocalCharacterDial = nullptr;
    Dial* vocalBodyDial = nullptr;
    Dial* vocalPresenceDial = nullptr;
    Dial* vocalBiteDial = nullptr;
    Dial* vocalSmoothDial = nullptr;
    Dial* loFiRateDial = nullptr;
    Dial* loFiBitsDial = nullptr;
    Dial* loFiClipDial = nullptr;
    Dial* loFiRectifyDial = nullptr;
    Dial* loFiNoiseDial = nullptr;
    Dial* lightningSparkDial = nullptr;
    Dial* lightningReactDial = nullptr;
    Dial* lightningClipDial = nullptr;
    Dial* lightningEdgeDial = nullptr;
    Dial* lightningToneDial = nullptr;
    Dial* pixelatedRateDial = nullptr;
    Dial* pixelatedBitsDial = nullptr;
    Dial* pixelatedDitherDial = nullptr;
    Dial* pixelatedJitterDial = nullptr;
    Dial* pixelatedToneDial = nullptr;
    Dial* rectifyWaveDial = nullptr;
    Dial* rectifyBiasDial = nullptr;
    Dial* rectifyCurveDial = nullptr;
    Dial* rectifyFoldDial = nullptr;
    Dial* rectifyToneDial = nullptr;
    Dial* softClipThresholdDial = nullptr;
    Dial* softClipKneeDial = nullptr;
    Dial* softClipBiasDial = nullptr;
    Dial* softClipBodyDial = nullptr;
    Dial* softClipToneDial = nullptr;
    Dial* tubeBiasDial = nullptr;
    Dial* tubeHeatDial = nullptr;
    Dial* tubeSagDial = nullptr;
    Dial* tubeBodyDial = nullptr;
    Dial* tubeToneDial = nullptr;
    Dial* stressedPressDial = nullptr;
    Dial* stressedFoldDial = nullptr;
    Dial* stressedTensionDial = nullptr;
    Dial* stressedBiasDial = nullptr;
    Dial* stressedToneDial = nullptr;
    Dial* saturationCurveDial = nullptr;
    Dial* saturationBiasDial = nullptr;
    Dial* saturationWarmDial = nullptr;
    Dial* saturationBodyDial = nullptr;
    Dial* saturationToneDial = nullptr;
    Dial* samuraiEdgeDial = nullptr;
    Dial* samuraiGateDial = nullptr;
    Dial* samuraiBladeDial = nullptr;
    Dial* samuraiRingDial = nullptr;
    Dial* samuraiToneDial = nullptr;
    Dial* razorEdgeDial = nullptr;
    Dial* razorFreqDial = nullptr;
    Dial* razorRingDial = nullptr;
    Dial* razorBiteDial = nullptr;
    Dial* razorToneDial = nullptr;
    Dial* freshFrequencyDial = nullptr;
    Dial* freshExciteDial = nullptr;
    Dial* freshAirDial = nullptr;
    Dial* freshSaturationDial = nullptr;
    Dial* freshToneDial = nullptr;
    Dial* fuzzGateDial = nullptr;
    Dial* fuzzBiasDial = nullptr;
    Dial* fuzzClipDial = nullptr;
    Dial* fuzzBodyDial = nullptr;
    Dial* fuzzToneDial = nullptr;
    Dial* hardClipThresholdDial = nullptr;
    Dial* hardClipBiasDial = nullptr;
    Dial* hardClipKneeDial = nullptr;
    Dial* hardClipFoldDial = nullptr;
    Dial* hardClipToneDial = nullptr;
    Dial* asymBiasDial = nullptr;
    Dial* asymSkewDial = nullptr;
    Dial* asymShapeDial = nullptr;
    Dial* asymToneDial = nullptr;
    Dial* filterCutoffDial = nullptr;
    Dial* filterResonanceDial = nullptr;
    Dial* envSensitivityDial = nullptr;
    Dial* envAttackDial = nullptr;
    Dial* envReleaseDial = nullptr;
    Dial* envDepthDial = nullptr;
    Dial* compAmountDial = nullptr;
    Dial* mixDial = nullptr;
    Dial* distOutputDial = nullptr;
    Dial* outputDial = nullptr;
    Dial* themeDial = nullptr;
    Dial* browserMixDial = nullptr;
    std::unique_ptr<juce::Drawable> logoDrawable;
    juce::Image themeBackdrop;
    juce::Label versionLabel;
    SteppedComboBox distortionTypeBox;
    SteppedComboBox ampCabinetBox;
    SteppedComboBox eqModeBox;
    juce::ToggleButton eqOnButton;
    juce::ToggleButton eqResetButton { "RESET" };
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment>
        eqOnAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment>
        eqModeAttachment;
    std::unique_ptr<EQGraph> eqGraph;
    std::unique_ptr<EnvMeter> envMeter;
    juce::ToggleButton envOnButton;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> envOnAttachment;
    juce::Label envAssignmentLabel;
    juce::ToggleButton eqViewButton { "EQ" };
    juce::ToggleButton envViewButton { "ENV" };
    bool envViewActive = false;
    SteppedComboBox filterShapeBox; // APVTS bridge; icons are the visible selectors
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment>
        filterShapeAttachment;
    FilterShapeButton filterLowButton { 0 };
    FilterShapeButton filterBandButton { 1 };
    FilterShapeButton filterHighButton { 2 };
    SteppedComboBox distModeBox; // shared input-mode parameter bridge
    juce::Label ampCabinetLabel;
    juce::ToggleButton distStereoButton { "STEREO" };
    juce::ToggleButton distMonoButton { "MONO" };
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment>
        ampCabinetAttachment, distModeAttachment;
    juce::Label distortionTypeLabel;
    // The sorted menu maps its item IDs to the stable original parameter indices.
    std::unique_ptr<juce::ParameterAttachment> distortionTypeAttachment;
    juce::ToggleButton distortionOnButton;
    juce::ToggleButton filterOnButton;
    juce::ToggleButton compressorOnButton;
    juce::ToggleButton phatButton { "PHAT" };
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment>
        distortionOnAttachment, filterOnAttachment, compressorOnAttachment,
        phatAttachment;
    juce::ToggleButton bypassButton;
    std::unique_ptr<juce::ParameterAttachment> bypassAttachment;
    juce::ToggleButton mixLockButton;
    juce::ToggleButton presetBrowserButton;
    juce::ToggleButton previousPresetButton;
    juce::ToggleButton nextPresetButton;
    PresetDisplayLabel currentPresetLabel;
    juce::TreeView presetFolderTree;
    PresetListBox presetList { "Preset list", this };
    juce::Label presetFolderLabel;
    juce::Label presetListLabel;
    juce::ToggleButton presetSaveButton;
    juce::ToggleButton presetSaveAsButton;
    juce::ToggleButton presetOpenFolderButton;
    juce::ToggleButton randomButton { "!!!" };
    juce::ToggleButton sequencerButton { "SEQ" };
    std::unique_ptr<SequencerPanel> sequencerPanel;
    std::unique_ptr<FolderTreeItem> presetFolderRoot;
    std::unique_ptr<juce::FileChooser> presetFileChooser;
    std::unique_ptr<juce::AlertWindow> presetFolderNameWindow;
    juce::Array<juce::File> visiblePresetFiles;
    juce::File selectedPresetFolder;
    juce::File currentPresetFile;
    juce::File copiedPresetFile;
    juce::StringArray favouritePresetPaths;
    juce::Label information;

    static constexpr int designWidth = 926;
    static constexpr int collapsedDesignHeight = 276;
    static constexpr int expandedDesignHeight = 530;
    int activeDesignHeight() const noexcept
    {
        return sequencerOpen ? expandedDesignHeight : collapsedDesignHeight;
    }

    Dial* addDial(const char*, const char*, const char*,
                  bool attachToParameter = true);
    bool restoreDialFromSavedState(Dial&);
    void updateContextSensitiveHelp();
    void updateDistortionControls();
    void updateSectionDimming();
    void resetGraphicEQ();
    void updateEQEnvView();
    void updateFilterShapeIcons();
    void showParameterEnvMenu(const juce::String&, juce::Point<int>);
    juce::Colour filterAccent(int filterIndex) const;
    juce::Colour panelColour(int filterIndex) const;
    void setPresetBrowserOpen(bool);
    void updatePresetBrowserVisibility();
    void rebuildPresetFolderTree();
    void selectPresetFolder(const juce::File&);
    void selectFavouritePresets();
    void refreshPresetList();
    void updatePresetNavigationDisplay();
    void setSequencerOpen(bool);
    void showZoomMenu(juce::Point<int> screenPosition);
    void updateSequencerTargetHighlights();
    void updateAnimatedParameterDisplays();
    void randomiseEffectParameters();
    bool handlePresetCursorKey(int direction);
    void stepPreset(int direction);
    void showPresetQuickMenu();
    void loadPresetFile(const juce::File&);
    void loadPresetAtBrowserCell(int row, int column);
    void showPresetContextMenu(const juce::File&, juce::Point<int>);
    void showPresetFolderContextMenu(const juce::File&, bool,
                                     juce::Point<int>);
    void copyPreset(const juce::File&);
    void pastePresetIntoFolder(const juce::File&);
    void showCreatePresetFolderDialog(const juce::File&);
    void confirmDeletePresetFolder(const juce::File&);
    juce::Array<juce::File> favouritePresetFiles() const;
    juce::Array<juce::File> navigationPresetFiles() const;
    bool isPresetFavourite(const juce::File&) const;
    void togglePresetFavourite(const juce::File&);
    void loadFavouritePresets();
    void saveFavouritePresets() const;
    void saveCurrentPreset();
    void showPresetSaveAs();
    int getNumRows() override;
    void paintListBoxItem(int, juce::Graphics&, int, int, bool) override;
    void listBoxItemClicked(int, const juce::MouseEvent&) override;
    void returnKeyPressed(int) override;
    void selectedRowsChanged(int) override;
    void timerCallback() override;
    void saveInterfaceSettings() const;

    int observedEditorWidth = designWidth;
    double observedThemeValue = 100.0;
    int stableSettingsTicks = 0;
    bool settingsDirty = false;
    bool restoringLockedMix = false;
    double lockedMixValue = 35.0;
    bool presetBrowserOpen = false;
    int displayedDistortionType = -1;
    int lastSectionDimmingMask = -1;
    bool favouritesViewSelected = false;
    bool sequencerOpen = false;
    bool previousPresetKeyDown = false;
    bool nextPresetKeyDown = false;
    int selectedPresetBrowserColumn = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(CrusherAudioProcessorEditor)
};
