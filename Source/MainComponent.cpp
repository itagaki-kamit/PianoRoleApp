#include "MainComponent.h"
#include <algorithm>
#include <cmath>

static juce::String jp (const char* utf8) { return juce::String::fromUTF8 (utf8); }

//==============================================================================
// PianoKeyboardComponent
//==============================================================================
PianoKeyboardComponent::PianoKeyboardComponent (juce::MidiKeyboardState& s)
    : state (s)
{
    state.addListener (this);
    setSize (PR::keyboardWidth, PR::gridHeightPx + 32);
}

PianoKeyboardComponent::~PianoKeyboardComponent()
{
    state.removeListener (this);
}

void PianoKeyboardComponent::repaintNote (int note)
{
    repaint (0, (int) PR::noteToY (note), getWidth(), (int) PR::noteHeight);
}

void PianoKeyboardComponent::handleNoteOn (juce::MidiKeyboardState*, int, int midiNoteNumber, float)
{
    if (juce::isPositiveAndBelow (midiNoteNumber, 128))
    {
        pressed[(size_t) midiNoteNumber] = true;
        repaintNote (midiNoteNumber);
    }
}

void PianoKeyboardComponent::handleNoteOff (juce::MidiKeyboardState*, int, int midiNoteNumber, float)
{
    if (juce::isPositiveAndBelow (midiNoteNumber, 128))
    {
        pressed[(size_t) midiNoteNumber] = false;
        repaintNote (midiNoteNumber);
    }
}

void PianoKeyboardComponent::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff121212));

    const auto clip = g.getClipBounds().toFloat();
    const int firstRow = juce::jmax (0, (int) std::floor (clip.getY() / PR::noteHeight));
    const int lastRow  = juce::jmin (PR::numNotes - 1, (int) std::floor (clip.getBottom() / PR::noteHeight));

    const float w = (float) getWidth();
    const float blackKeyWidth = 52.0f;

    for (int row = firstRow; row <= lastRow; ++row)
    {
        const int note = PR::numNotes - 1 - row;
        const float y = (float) row * PR::noteHeight;
        const bool black = PR::isBlackKey (note);
        const bool down  = pressed[(size_t) note];

        // 白鍵(行全体)
        g.setColour (down && ! black ? juce::Colour (0xffffb066) : juce::Colour (0xfff2f2f2));
        g.fillRect (0.0f, y, w, PR::noteHeight);

        // 行の下側の境界線 (C の下側は濃い線)
        g.setColour (note % 12 == 0 ? juce::Colour (0xff555555) : juce::Colour (0xffbdbdbd));
        g.fillRect (0.0f, y + PR::noteHeight - 1.0f, w, 1.0f);

        // 黒鍵
        if (black)
        {
            g.setColour (down ? juce::Colour (0xffd9781f) : juce::Colour (0xff202020));
            g.fillRect (0.0f, y + 1.0f, blackKeyWidth, PR::noteHeight - 2.0f);
        }

        // C の音名ラベル (JUCE の表記: 60 = C3)
        if (note % 12 == 0)
        {
            g.setColour (juce::Colour (0xff444444));
            g.setFont (juce::FontOptions (10.0f));
            g.drawText (juce::MidiMessage::getMidiNoteName (note, true, true, 3),
                        juce::Rectangle<float> (blackKeyWidth, y, w - blackKeyWidth - 4.0f, PR::noteHeight),
                        juce::Justification::centredRight, false);
        }
    }

    g.setColour (juce::Colour (0xff000000));
    g.fillRect (w - 1.0f, 0.0f, 1.0f, (float) getHeight());
}

void PianoKeyboardComponent::startNote (int note)
{
    if (note == currentNote)
        return;

    stopNote();
    currentNote = note;
    state.noteOn (1, currentNote, 0.8f);
}

void PianoKeyboardComponent::stopNote()
{
    if (currentNote >= 0)
    {
        state.noteOff (1, currentNote, 0.0f);
        currentNote = -1;
    }
}

void PianoKeyboardComponent::mouseDown (const juce::MouseEvent& e)
{
    if (e.position.y < (float) PR::gridHeightPx)
        startNote (PR::yToNote (e.position.y));
}

void PianoKeyboardComponent::mouseDrag (const juce::MouseEvent& e)
{
    if (e.position.y >= 0.0f && e.position.y < (float) PR::gridHeightPx)
        startNote (PR::yToNote (e.position.y));
}

void PianoKeyboardComponent::mouseUp (const juce::MouseEvent&)
{
    stopNote();
}

//==============================================================================
// NoteGridComponent
//==============================================================================
NoteGridComponent::NoteGridComponent()
{
    setSize (PR::gridWidthPx, PR::gridHeightPx);

    // サンプルノート: C3-E3-G3 のアルペジオ + 和音 (60 = C3)
    notes.push_back ({ 60, 0.0, 1.0 });
    notes.push_back ({ 64, 1.0, 1.0 });
    notes.push_back ({ 67, 2.0, 1.0 });
    notes.push_back ({ 60, 4.0, 2.0 });
    notes.push_back ({ 64, 4.0, 2.0 });
    notes.push_back ({ 67, 4.0, 2.0 });
}

juce::Rectangle<float> NoteGridComponent::getNoteRect (const PianoRollNote& n) const
{
    return { (float) n.startBeats * PR::pixelsPerBeat,
             PR::noteToY (n.noteNumber),
             (float) n.lengthBeats * PR::pixelsPerBeat,
             PR::noteHeight };
}

int NoteGridComponent::findNoteAt (juce::Point<float> p) const
{
    for (int i = (int) notes.size() - 1; i >= 0; --i)   // 後から追加したノートを優先
        if (getNoteRect (notes[(size_t) i]).contains (p))
            return i;

    return -1;
}

bool NoteGridComponent::isOnRightEdge (const PianoRollNote& n, juce::Point<float> p) const
{
    return p.x >= getNoteRect (n).getRight() - PR::resizeMargin;
}

void NoteGridComponent::setAuditionNote (int note)
{
    if (note == auditionedNote)
        return;

    if (auditionedNote >= 0 && onNoteOff)
        onNoteOff (auditionedNote);

    auditionedNote = note;

    if (auditionedNote >= 0 && onNoteOn)
        onNoteOn (auditionedNote);
}

void NoteGridComponent::notifyChanged()
{
    if (onNotesChanged)
        onNotesChanged();
}

void NoteGridComponent::setPlayheadBeats (double beats)
{
    if (beats == playheadBeats)
        return;

    auto repaintAt = [this] (double b)
    {
        if (b >= 0.0)
            repaint ((int) (b * PR::pixelsPerBeat) - 3, 0, 7, getHeight());
    };

    repaintAt (playheadBeats);
    playheadBeats = beats;
    repaintAt (playheadBeats);
}

void NoteGridComponent::paint (juce::Graphics& g)
{
    const auto clip = g.getClipBounds().toFloat();
    const float w = (float) getWidth();
    const float h = (float) getHeight();

    // --- 行の背景 ---
    const int firstRow = juce::jmax (0, (int) std::floor (clip.getY() / PR::noteHeight));
    const int lastRow  = juce::jmin (PR::numNotes - 1, (int) std::floor (clip.getBottom() / PR::noteHeight));

    for (int row = firstRow; row <= lastRow; ++row)
    {
        const int note = PR::numNotes - 1 - row;
        const float y = (float) row * PR::noteHeight;

        g.setColour (juce::Colour (PR::isBlackKey (note) ? 0xff161616 : 0xff1e1e1e));
        g.fillRect (0.0f, y, w, PR::noteHeight);

        // 行の下側の境界線 (C の下側は明るい白線)
        if (note % 12 == 0)
            g.setColour (juce::Colours::white.withAlpha (0.25f));
        else
            g.setColour (juce::Colours::black.withAlpha (0.5f));

        g.drawHorizontalLine ((int) (y + PR::noteHeight - 1.0f), 0.0f, w);
    }

    // --- 縦線 (16分 / 1拍 / 1小節) ---
    const float stepPx = PR::pixelsPerBeat * (float) PR::snapBeats;
    const int firstStep = juce::jmax (0, (int) std::floor (clip.getX() / stepPx));
    const int lastStep  = juce::jmin ((int) (PR::totalBeats / PR::snapBeats), (int) std::ceil (clip.getRight() / stepPx));

    for (int step = firstStep; step <= lastStep; ++step)
    {
        const float x = (float) step * stepPx;

        if (step % 16 == 0)      g.setColour (juce::Colours::white.withAlpha (0.30f)); // 4拍
        else if (step % 4 == 0)  g.setColour (juce::Colours::white.withAlpha (0.14f)); // 1拍
        else                     g.setColour (juce::Colours::white.withAlpha (0.05f)); // 16分

        g.drawVerticalLine ((int) x, 0.0f, h);
    }

    // --- ノート ---
    for (int i = 0; i < (int) notes.size(); ++i)
    {
        const auto r = getNoteRect (notes[(size_t) i]).reduced (0.5f, 1.0f);
        const bool selected = (i == selectedNoteIndex);

        g.setColour (selected ? juce::Colour (0xffff9a3c) : juce::Colour (0xff3d7be0));
        g.fillRoundedRectangle (r, 3.0f);

        g.setColour ((selected ? juce::Colour (0xffffc88a) : juce::Colour (0xff8db6f5)).withAlpha (0.9f));
        g.drawRoundedRectangle (r, 3.0f, 1.0f);
    }

    // --- プレイヘッド ---
    if (playheadBeats >= 0.0)
    {
        g.setColour (juce::Colour (0xffff4d4d));
        g.fillRect ((float) (playheadBeats * PR::pixelsPerBeat) - 1.0f, 0.0f, 2.0f, h);
    }
}

void NoteGridComponent::mouseMove (const juce::MouseEvent& e)
{
    const auto p = e.position;
    const int idx = findNoteAt (p);

    if (idx >= 0 && isOnRightEdge (notes[(size_t) idx], p))
        setMouseCursor (juce::MouseCursor::LeftRightResizeCursor);
    else
        setMouseCursor (juce::MouseCursor::NormalCursor);
}

void NoteGridComponent::mouseDown (const juce::MouseEvent& e)
{
    const auto p = e.position;
    const double beat = (double) p.x / (double) PR::pixelsPerBeat;
    const int idx = findNoteAt (p);

    if (idx >= 0)
    {
        selectedNoteIndex = idx;
        draggedNoteIndex  = idx;
        dragOffsetBeats   = beat - notes[(size_t) idx].startBeats;
        dragMode = isOnRightEdge (notes[(size_t) idx], p) ? DragMode::resize : DragMode::move;
    }
    else
    {
        PianoRollNote n;
        n.noteNumber  = PR::yToNote (p.y);
        n.startBeats  = std::floor (beat / PR::snapBeats) * PR::snapBeats;
        n.startBeats  = juce::jlimit (0.0, (double) PR::totalBeats - 0.5, n.startBeats);
        n.lengthBeats = 0.5;
        notes.push_back (n);

        selectedNoteIndex = (int) notes.size() - 1;
        draggedNoteIndex  = selectedNoteIndex;
        dragOffsetBeats   = beat - n.startBeats;
        dragMode          = DragMode::move;
        notifyChanged();
    }

    if (draggedNoteIndex >= 0 && draggedNoteIndex < (int) notes.size())
        setAuditionNote (notes[(size_t) draggedNoteIndex].noteNumber);

    repaint();
}

void NoteGridComponent::mouseDrag (const juce::MouseEvent& e)
{
    if (draggedNoteIndex < 0 || draggedNoteIndex >= (int) notes.size())
        return;

    auto& n = notes[(size_t) draggedNoteIndex];
    const auto p = e.position;
    const double beat = (double) p.x / (double) PR::pixelsPerBeat;

    if (dragMode == DragMode::move)
    {
        double newStart = std::round ((beat - dragOffsetBeats) / PR::snapBeats) * PR::snapBeats;
        newStart = juce::jlimit (0.0, juce::jmax (0.0, (double) PR::totalBeats - n.lengthBeats), newStart);
        n.startBeats = newStart;
        n.noteNumber = PR::yToNote (p.y);
        setAuditionNote (n.noteNumber);
    }
    else if (dragMode == DragMode::resize)
    {
        const double snappedEnd = std::round (beat / PR::snapBeats) * PR::snapBeats;
        double newLength = juce::jmax (PR::snapBeats, snappedEnd - n.startBeats);
        newLength = juce::jmin (newLength, (double) PR::totalBeats - n.startBeats);
        n.lengthBeats = juce::jmax (PR::snapBeats, newLength);
    }

    notifyChanged();
    repaint();
}

void NoteGridComponent::mouseUp (const juce::MouseEvent&)
{
    setAuditionNote (-1);
    draggedNoteIndex = -1;
    dragMode = DragMode::none;
    notifyChanged();
}

void NoteGridComponent::mouseDoubleClick (const juce::MouseEvent& e)
{
    const int idx = findNoteAt (e.position);

    if (idx >= 0 && idx < (int) notes.size())
    {
        notes.erase (notes.begin() + idx);
        selectedNoteIndex = -1;
        draggedNoteIndex  = -1;
        dragMode = DragMode::none;
        setAuditionNote (-1);
        setMouseCursor (juce::MouseCursor::NormalCursor);
        notifyChanged();
        repaint();
    }
}

//==============================================================================
// MainComponent
//==============================================================================
MainComponent::MainComponent()
{
    // --- ツールバー ---
    playButton.setMouseClickGrabsKeyboardFocus (false);
    playButton.onClick = [this] { togglePlayback(); };
    addAndMakeVisible (playButton);

    bpmLabel.setJustificationType (juce::Justification::centredRight);
    addAndMakeVisible (bpmLabel);

    bpmSlider.setSliderStyle (juce::Slider::LinearHorizontal);
    bpmSlider.setTextBoxStyle (juce::Slider::TextBoxLeft, false, 56, 24);
    bpmSlider.setRange (30.0, 300.0, 1.0);
    bpmSlider.setValue (120.0, juce::dontSendNotification);
    bpmSlider.onValueChange = [this] { bpm.store (bpmSlider.getValue()); };
    addAndMakeVisible (bpmSlider);

    exportButton.setMouseClickGrabsKeyboardFocus (false);
    exportButton.onClick = [this] { exportMidi(); };
    addAndMakeVisible (exportButton);

    // --- 鍵盤 (グリッドと同じ行高で描画する自前コンポーネント) ---
    keyboardViewport.setViewedComponent (&keyboard, false);
    keyboardViewport.setScrollBarsShown (false, false);
    keyboardViewport.onWheel = [this] (const juce::MouseEvent& e, const juce::MouseWheelDetails& w)
    {
        gridViewport.mouseWheelMove (e.getEventRelativeTo (&gridViewport), w);
    };
    addAndMakeVisible (keyboardViewport);

    // --- グリッド ---
    gridViewport.setViewedComponent (&noteGrid, false);
    gridViewport.setScrollBarThickness (12);
    gridViewport.setScrollBarsShown (true, true);
    gridViewport.onScrolled = [this] (int /*x*/, int y)
    {
        keyboardViewport.setViewPosition (0, y);
    };
    addAndMakeVisible (gridViewport);

    // --- シンセ ---
    for (int i = 0; i < 16; ++i)
        synth.addVoice (new SineVoice());
    synth.addSound (new SineSound());

    // 鍵盤クリック / グリッド操作 → keyboardState → midiCollector → synth
    keyboardState.addListener (&midiCollector);

    noteGrid.onNoteOn  = [this] (int n) { keyboardState.noteOn  (1, n, 0.8f); };
    noteGrid.onNoteOff = [this] (int n) { keyboardState.noteOff (1, n, 0.0f); };
    noteGrid.onNotesChanged = [this] { updatePlaybackNotes(); };

    updatePlaybackNotes();

    setWantsKeyboardFocus (true);
    setSize (1000, 640);

    // 初期スクロール: noteNumber 72 付近が上端
    const int initialY = (PR::numNotes - 1 - 72) * (int) PR::noteHeight;
    gridViewport.setViewPosition (0, initialY);
    keyboardViewport.setViewPosition (0, gridViewport.getViewPositionY());

    startTimerHz (30);

    // 入力なし・出力2ch
    setAudioChannels (0, 2);
}

MainComponent::~MainComponent()
{
    stopTimer();
    keyboardState.removeListener (&midiCollector);
    shutdownAudio();
}

//==============================================================================
void MainComponent::prepareToPlay (int /*samplesPerBlockExpected*/, double sampleRate)
{
    currentSampleRate = sampleRate;
    synth.setCurrentPlaybackSampleRate (sampleRate);
    midiCollector.reset (sampleRate);
}

void MainComponent::getNextAudioBlock (const juce::AudioSourceChannelInfo& info)
{
    info.clearActiveBufferRegion();

    // 鍵盤・グリッド試聴からの MIDI
    juce::MidiBuffer midi;
    midiCollector.removeNextBlockOfMessages (midi, info.numSamples);

    // 停止/頭出し要求
    if (resetRequested.exchange (false))
    {
        synth.allNotesOff (0, true);
        playPosBeats = 0.0;
        positionBeats.store (0.0);
    }

    bool finished = false;

    if (playing.load())
    {
        const double beatsPerSample = bpm.load() / 60.0 / currentSampleRate;
        const double blockStart = playPosBeats;
        const double blockEnd   = blockStart + (double) info.numSamples * beatsPerSample;

        auto sampleOffset = [&] (double beat)
        {
            return juce::jlimit (0, juce::jmax (0, info.numSamples - 1),
                                 (int) std::floor ((beat - blockStart) / beatsPerSample));
        };

        {
            const juce::SpinLock::ScopedLockType sl (notesLock);

            // 同じ時刻では note-off を先に入れる (連続する同音程ノート対策)
            for (const auto& n : playbackNotes)
            {
                const double off = n.startBeats + n.lengthBeats;
                if (off >= blockStart && off < blockEnd)
                    midi.addEvent (juce::MidiMessage::noteOff (1, n.noteNumber), sampleOffset (off));
            }

            for (const auto& n : playbackNotes)
            {
                if (n.startBeats >= blockStart && n.startBeats < blockEnd)
                    midi.addEvent (juce::MidiMessage::noteOn (1, n.noteNumber, (juce::uint8) 100),
                                   sampleOffset (n.startBeats));
            }

            finished = (blockEnd >= playbackEndBeats);
        }

        playPosBeats = blockEnd;
        positionBeats.store (blockEnd);
    }

    synth.renderNextBlock (*info.buffer, midi, info.startSample, info.numSamples);

    if (finished)
    {
        playing.store (false);
        synth.allNotesOff (0, true);
        playPosBeats = 0.0;
        positionBeats.store (0.0);
    }
}

//==============================================================================
void MainComponent::updatePlaybackNotes()
{
    std::vector<PianoRollNote> copy = noteGrid.getNotes();

    double end = 0.0;
    for (const auto& n : copy)
        end = juce::jmax (end, n.startBeats + n.lengthBeats);

    const juce::SpinLock::ScopedLockType sl (notesLock);
    playbackNotes.swap (copy);
    playbackEndBeats = end;
}

void MainComponent::startPlayback()
{
    updatePlaybackNotes();
    bpm.store (bpmSlider.getValue());
    resetRequested.store (true);   // 先頭から
    playing.store (true);
}

void MainComponent::stopPlayback()
{
    playing.store (false);
    resetRequested.store (true);   // 発音中のノートを止めて先頭へ
}

void MainComponent::togglePlayback()
{
    if (playing.load())
        stopPlayback();
    else
        startPlayback();
}

void MainComponent::timerCallback()
{
    const bool isPlaying = playing.load();

    if (isPlaying)
    {
        const double pos = positionBeats.load();
        noteGrid.setPlayheadBeats (pos);

        // プレイヘッドが画面外に出たら横スクロールで追従
        const int x = (int) (pos * PR::pixelsPerBeat);
        const int viewX = gridViewport.getViewPositionX();
        const int viewW = gridViewport.getViewWidth();

        if (x < viewX || x > viewX + viewW - 40)
            gridViewport.setViewPosition (juce::jmax (0, x - 40), gridViewport.getViewPositionY());
    }

    if (isPlaying != uiWasPlaying)
    {
        uiWasPlaying = isPlaying;
        playButton.setButtonText (isPlaying ? "Stop" : "Play");

        if (! isPlaying)
            noteGrid.setPlayheadBeats (-1.0);
    }
}

bool MainComponent::keyPressed (const juce::KeyPress& key)
{
    if (key == juce::KeyPress::spaceKey)
    {
        togglePlayback();
        return true;
    }

    return false;
}

//==============================================================================
void MainComponent::exportMidi()
{
    const auto notesCopy = noteGrid.getNotes();
    const double tempo = bpmSlider.getValue();

    if (notesCopy.empty())
    {
        juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::InfoIcon,
                                                jp ("MIDI書き出し"),
                                                jp ("ノートがありません。"));
        return;
    }

    // 保存先の既定: ~/Documents/PianoRollMIDI/PianoRoll_日時.mid
    auto dir = juce::File::getSpecialLocation (juce::File::userDocumentsDirectory).getChildFile ("PianoRollMIDI");
    dir.createDirectory();

    auto defaultFile = dir.getNonexistentChildFile ("PianoRoll_" + juce::Time::getCurrentTime().formatted ("%Y%m%d_%H%M%S"),
                                                    ".mid");

    chooser = std::make_unique<juce::FileChooser> (jp ("MIDIファイルとして保存"), defaultFile, "*.mid");

    chooser->launchAsync (juce::FileBrowserComponent::saveMode
                          | juce::FileBrowserComponent::canSelectFiles
                          | juce::FileBrowserComponent::warnAboutOverwriting,
                          [notesCopy, tempo] (const juce::FileChooser& fc)
    {
        auto file = fc.getResult();

        if (file == juce::File())
            return;   // キャンセル

        file = file.withFileExtension ("mid");

        constexpr int ticksPerQuarter = 480;

        juce::MidiMessageSequence seq;

        auto addMeta = [&] (juce::MidiMessage m)
        {
            m.setTimeStamp (0.0);
            seq.addEvent (m);
        };

        addMeta (juce::MidiMessage::tempoMetaEvent ((int) std::round (60000000.0 / tempo)));
        addMeta (juce::MidiMessage::timeSignatureMetaEvent (4, 4));

        // note-off を先に追加 → 同時刻では note-off が note-on より前に並ぶ
        for (const auto& n : notesCopy)
        {
            auto m = juce::MidiMessage::noteOff (1, n.noteNumber);
            m.setTimeStamp (std::round ((n.startBeats + n.lengthBeats) * ticksPerQuarter));
            seq.addEvent (m);
        }

        for (const auto& n : notesCopy)
        {
            auto m = juce::MidiMessage::noteOn (1, n.noteNumber, (juce::uint8) 100);
            m.setTimeStamp (std::round (n.startBeats * ticksPerQuarter));
            seq.addEvent (m);
        }

        seq.updateMatchedPairs();

        juce::MidiFile midiFile;
        midiFile.setTicksPerQuarterNote (ticksPerQuarter);
        midiFile.addTrack (seq);

        bool ok = false;

        file.deleteFile();   // FileOutputStream は追記になるため、既存ファイルは先に削除

        {
            juce::FileOutputStream stream (file);

            if (stream.openedOk())
            {
                ok = midiFile.writeTo (stream);
                stream.flush();
            }
        }

        juce::AlertWindow::showMessageBoxAsync (ok ? juce::MessageBoxIconType::InfoIcon
                                                   : juce::MessageBoxIconType::WarningIcon,
                                                jp ("MIDI書き出し"),
                                                ok ? jp ("保存しました:\n") + file.getFullPathName()
                                                   : jp ("保存に失敗しました:\n") + file.getFullPathName());
    });
}

//==============================================================================
void MainComponent::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff121212));
}

void MainComponent::resized()
{
    auto area = getLocalBounds();

    auto bar = area.removeFromTop (toolbarHeight).reduced (8, 8);
    playButton.setBounds (bar.removeFromLeft (80));
    bar.removeFromLeft (12);
    bpmLabel.setBounds (bar.removeFromLeft (40));
    bpmSlider.setBounds (bar.removeFromLeft (240));
    exportButton.setBounds (bar.removeFromRight (120));

    keyboardViewport.setBounds (area.removeFromLeft (PR::keyboardWidth));
    gridViewport.setBounds (area);

    keyboardViewport.setViewPosition (0, gridViewport.getViewPositionY());
}
