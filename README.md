# PianoRollApp

JUCEフレームワークとC++で開発された、スタンドアロン動作のピアノロール型MIDIシーケンサーアプリケーションです。
内蔵のサイン波シンセサイザーによるリアルタイム試聴・シーケンス再生と、標準MIDIファイル（`.mid`）への書き出しに対応しています。

## 主な機能

* **ピアノロール・ノート編集 (`NoteGridComponent`)**
  * **グリッド仕様**: 全128音階（MIDIノート 0〜127）× 32拍（8小節）、16分音符（0.25拍）単位のスナップに対応。
  * **直感的なマウス操作**:
    * **空白クリック**: 新規ノートを追加（初期長 0.5拍）
    * **ドラッグ**: ノートの位置（音程・タイミング）を移動
    * **ノート右端ドラッグ**: ノートの長さをリサイズ
    * **ダブルクリック**: ノートを削除
  * **試聴機能（オーディション）**: ノートの追加や上下移動に合わせてリアルタイムに発音。
* **同期スクロール鍵盤 (`PianoKeyboardComponent`)**
  * グリッドと完全に高さを一致（1半音 = 14px）させた自前描画の縦型ピアノ鍵盤。
  * 鍵盤のクリック・ドラッグによる直接演奏や、再生・編集中のキーハイライト表示に対応。
* **内蔵シンセサイザー＆再生エンジン (`MainComponent` / `SineVoice`)**
  * **16音ポリフォニック・サイン波シンセ**: ADSRエンベロープ付きの簡易サイン波音源を内蔵し、外部音源なしで単体動作。
  * **トランスポート操作**: `Play / Stop` ボタンまたは `Space` キーによる再生・停止、BPM調整（30〜300 BPM）、プレイヘッドの自動横スクロール追従。
* **SMF（MIDIファイル）エクスポート**
  * 作成したノートデータと設定BPMを標準MIDIファイル（Format 0、分解能 480 TPQN）として出力。
  * デフォルト保存先: `~/Documents/PianoRollMIDI/PianoRoll_YYYYMMDD_HHMMSS.mid`

## システム構成・アーキテクチャ

| コンポーネント / クラス | 役割・概要 |
| :--- | :--- |
| **`pianorollApplication`** (`Main.cpp`) | アプリケーションのエントリーポイントおよびメインウィンドウ（`DocumentWindow`）の管理。 |
| **`MainComponent`** (`MainComponent.h/.cpp`) | オーディオ処理（`AudioAppComponent`）、UIレイアウト、再生状態（UIスレッド⇔オーディオスレッド間の `std::atomic` / `SpinLock` 同期）、MIDIファイル書き出しを統括。 |
| **`NoteGridComponent`** | ノート配列（`std::vector<PianoRollNote>`）の保持、ピアノロールグリッド・ノート・プレイヘッドの描画、マウス編集イベントを処理。 |
| **`PianoKeyboardComponent`** | `MidiKeyboardState::Listener` を継承し、鍵盤の描画・押下状態の反映・マウス演奏を処理。 |
| **`SyncViewport` / `KeyboardViewport`** | グリッドと鍵盤の垂直スクロール位置を同期し、ホイール操作のズレを防止するカスタムビューポート。 |
| **`SineVoice` / `SineSound`** | `juce::Synthesiser` 上で動作する16ボイスのサイン波音源クラス。 |

## ビルド環境・手順

* **フレームワーク**: JUCE
* **開発環境**: macOS / Xcode（`Builds/MacOSX` 内の `.xcodeproj` を使用）

```bash
# リポジトリのクローン
git clone https://github.com
cd PianoRollApp

# Xcodeプロジェクトを開いてビルド・実行
open Builds/MacOSX/*.xcodeproj
```

