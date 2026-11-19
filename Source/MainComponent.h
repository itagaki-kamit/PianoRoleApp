#pragma once

#include <JuceHeader.h>
#include <array>
#include <atomic>
#include <cmath>
#include <functional>
#include <memory>
#include <vector>

//==============================================================================
// 鍵盤とグリッドで共有する寸法・変換
// 鍵盤もグリッドも「1半音 = 14px の行」で統一し、同じ関数で Y→音程 変換する。
//==============================================================================
namespace PR
{
    constexpr float  noteHeight    = 14.0f;
    constexpr float  pixelsPerBeat = 80.0f;
    constexpr int    numNotes      = 128;
    constexpr int    totalBeats    = 32;
    constexpr double snapBeats     = 0.25;
    constexpr float  resizeMargin  = 8.0f;
    constexpr int    keyboardWidth = 80;
    constexpr int    gridHeightPx  = 1792;   // 14 * 128
    constexpr int    gridWidthPx   = 2560;   // 80 * 32

    inline int yToNote (float y)
    {
        const int row = (int) std::floor (y / noteHeight);
        return juce::jlimit (0, numNotes - 1, numNotes - 1 - row);
    }

    inline float noteToY (int note)
    {
        return (float) (numNotes - 1 - note) * noteHeight;
    }

    inline bool isBlackKey (int note)
    {
        const int pc = note % 12;
        return pc == 1 || pc == 3 || pc == 6 || pc == 8 || pc == 10;
    }
}

//==============================================================================
// 簡易サイン波シンセ
//==============================================================================
struct SineSound : public juce::SynthesiserSound
{
    SineSound() = default;
    bool appliesToNote (int) override    { return true; }
    bool appliesToChannel (int) override { return true; }
};

class SineVoice : public juce::SynthesiserVoice
{
public:
    SineVoice() = default;

    bool canPlaySound (juce::SynthesiserSound* s) override
    {
        return dynamic_cast<SineSound*> (s) != nullptr;
    }

    void startNote (int midiNote, float velocity, juce::SynthesiserSound*, int) override
    {
        phase = 0.0;
        level = (double) velocity * 0.25;
        delta = juce::MathConstants<double>::twoPi
                * juce::MidiMessage::getMidiNoteInHertz (midiNote) / getSampleRate();

        adsr.setSampleRate (getSampleRate());
        adsr.setParameters ({ 0.005f, 0.1f, 0.7f, 0.15f });
        adsr.noteOn();
    }

    void stopNote (float, bool allowTailOff) override
    {
        if (allowTailOff)
        {
            adsr.noteOff();
        }
        else
        {
            adsr.reset();
            clearCurrentNote();
        }
    }

    void pitchWheelMoved (int) override {}
    void controllerMoved (int, int) override {}

    void renderNextBlock (juce::AudioBuffer<float>& buffer, int startSample, int numSamples) override
    {
        if (! adsr.isActive())
            return;

        for (int i = 0; i < numSamples; ++i)
        {
            const float s = (float) (std::sin (phase) * level) * adsr.getNextSample();

            for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
                buffer.addSample (ch, startSample + i, s);

            phase += delta;
        }

        if (! adsr.isActive())
            clearCurrentNote();
    }

private:
    double phase = 0.0, delta = 0.0, level = 0.0;
    juce::ADSR adsr;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SineVoice)
};

//==============================================================================
struct PianoRollNote
{
    int    noteNumber  = 60;   // 0..127
    double startBeats  = 0.0;
    double lengthBeats = 0.5;
};

//==============================================================================
// 左側の鍵盤 (グリッドと同じ 14px 行で描画 → 位置と音程が必ず一致する)
//==============================================================================
class PianoKeyboardComponent : public juce::Component,
                               private juce::MidiKeyboardState::Listener
{
public:
    explicit PianoKeyboardComponent (juce::MidiKeyboardState& stateToUse);
    ~PianoKeyboardComponent() override;

    void paint (juce::Graphics& g) override;
    void mouseDown (const juce::MouseEvent& e) override;
    void mouseDrag (const juce::MouseEvent& e) override;
    void mouseUp (const juce::MouseEvent& e) override;

private:
    void handleNoteOn  (juce::MidiKeyboardState*, int midiChannel, int midiNoteNumber, float velocity) override;
    void handleNoteOff (juce::MidiKeyboardState*, int midiChannel, int midiNoteNumber, float velocity) override;

    void startNote (int note);
    void stopNote();
    void repaintNote (int note);

    juce::MidiKeyboardState& state;
    std::array<bool, 128> pressed {};
    int currentNote = -1;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PianoKeyboardComponent)
};

//==============================================================================
class NoteGridComponent : public juce::Component
{
public:
    NoteGridComponent();

    // ノート操作時の試聴用コールバック
    std::function<void (int note)> onNoteOn;
    std::function<void (int note)> onNoteOff;
    // ノート配列が変更されたとき (再生用データ更新)
    std::function<void()> onNotesChanged;

    const std::vector<PianoRollNote>& getNotes() const noexcept { return notes; }

    // 再生位置(拍)。負の値で非表示
    void setPlayheadBeats (double beats);

    void paint (juce::Graphics& g) override;
    void mouseMove (const juce::MouseEvent& e) override;
    void mouseDown (const juce::MouseEvent& e) override;
    void mouseDrag (const juce::MouseEvent& e) override;
    void mouseUp (const juce::MouseEvent& e) override;
    void mouseDoubleClick (const juce::MouseEvent& e) override;

private:
    enum class DragMode { none, move, resize };

    juce::Rectangle<float> getNoteRect (const PianoRollNote& n) const;
    int  findNoteAt (juce::Point<float> p) const;
    bool isOnRightEdge (const PianoRollNote& n, juce::Point<float> p) const;
    void setAuditionNote (int note);
    void notifyChanged();

    std::vector<PianoRollNote> notes;
    int      selectedNoteIndex = -1;
    int      draggedNoteIndex  = -1;
    int      auditionedNote    = -1;
    DragMode dragMode          = DragMode::none;
    double   dragOffsetBeats   = 0.0;
    double   playheadBeats     = -1.0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (NoteGridComponent)
};

//==============================================================================
class SyncViewport : public juce::Viewport
{
public:
    SyncViewport() = default;

    std::function<void (int x, int y)> onScrolled;

    void visibleAreaChanged (const juce::Rectangle<int>& newVisibleArea) override
    {
        juce::Viewport::visibleAreaChanged (newVisibleArea);
        if (onScrolled)
            onScrolled (newVisibleArea.getX(), newVisibleArea.getY());
    }

private:
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SyncViewport)
};

// 鍵盤側Viewportはホイールを受け取らず、グリッド側へ転送する (同期ズレ防止)
class KeyboardViewport : public juce::Viewport
{
public:
    KeyboardViewport() = default;

    std::function<void (const juce::MouseEvent&, const juce::MouseWheelDetails&)> onWheel;

    void mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& w) override
    {
        if (onWheel)
            onWheel (e, w);
    }

private:
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (KeyboardViewport)
};

//==============================================================================
class MainComponent : public juce::AudioAppComponent,
                      private juce::Timer
{
public:
    MainComponent();
    ~MainComponent() override;

    // Audio
    void prepareToPlay (int samplesPerBlockExpected, double sampleRate) override;
    void getNextAudioBlock (const juce::AudioSourceChannelInfo& info) override;
    void releaseResources() override {}

    // UI
    void paint (juce::Graphics& g) override;
    void resized() override;
    bool keyPressed (const juce::KeyPress& key) override;

private:
    void timerCallback() override;

    void updatePlaybackNotes();
    void startPlayback();
    void stopPlayback();
    void togglePlayback();
    void exportMidi();

    static constexpr int toolbarHeight = 44;
    static constexpr int keyboardExtraBottom = 32;  // グリッド側の横スクロールバー分の余白

    // 音
    juce::Synthesiser synth;
    juce::MidiMessageCollector midiCollector;
    juce::MidiKeyboardState keyboardState;

    // UI (中身 → Viewport の順に宣言: 破棄順の都合)
    PianoKeyboardComponent keyboard { keyboardState };
    NoteGridComponent      noteGrid;
    KeyboardViewport       keyboardViewport;
    SyncViewport           gridViewport;

    juce::TextButton playButton   { "Play" };
    juce::TextButton exportButton { "Export MIDI" };
    juce::Label      bpmLabel     { {}, "BPM" };
    juce::Slider     bpmSlider;

    // 再生 (UIスレッド <-> オーディオスレッド)
    std::atomic<bool>   playing { false };
    std::atomic<bool>   resetRequested { false };
    std::atomic<double> bpm { 120.0 };
    std::atomic<double> positionBeats { 0.0 };

    double playPosBeats     = 0.0;   // オーディオスレッド専用
    double currentSampleRate = 44100.0;

    juce::SpinLock notesLock;
    std::vector<PianoRollNote> playbackNotes;   // notesLock で保護
    double playbackEndBeats = 0.0;              // notesLock で保護

    bool uiWasPlaying = false;
    std::unique_ptr<juce::FileChooser> chooser;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MainComponent)
};
