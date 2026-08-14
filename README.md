# RayPalette

RayPalette は、制御した照明下でイラストの色がどのように変化するかを
検討するための軽量な CUDA レンダラーです。固定した球と床のシーンを、
ネイティブ GUI で対話的に調整できるようにします。

## 1. 前提条件

### 1.1. 共通

- CMake 3.24 以降
- C++17 コンパイラ
- clang-format 14 以降（ソース整形を行う場合）
- NVIDIA CUDA Toolkit 12.8 以降（GPU版をビルドする場合のみ）
- GPU版を使わない場合は、CUDA ToolkitとNVIDIA GPUは不要

### 1.2. Ubuntu 24.04（確認済み）

- GCC 13.3
- NVIDIA CUDA Toolkit 12.8.93
- 将来の GLFW/OpenGL GUI 用 X11・OpenGL 開発パッケージ

Ubuntu で GLFW/OpenGL GUI をビルドする場合は、X11 と OpenGL の開発パッケージを
インストールしてください。

```sh
sudo apt install libgl1-mesa-dev libxrandr-dev libxinerama-dev libxcursor-dev libxi-dev
```

### 1.3. Windows 11（対応予定）

- Visual Studio Community 2022 または Build Tools 2022
- 「C++ によるデスクトップ開発」ワークロード、MSVC v143、Windows SDK
- NVIDIA CUDA Toolkit 12.8 以降
- OpenGL / GLFW の開発環境: TBD（Windows 実機で未検証）

## 2. 構成とビルド

初期の bootstrap ターゲットはネットワーク依存を持ちません。

### 2.1. Ubuntu 24.04（確認済み）

CUDA版のDebugビルドは次のコマンドで構成、ビルド、テストを実行できます。

```sh
cmake --preset linux-debug
cmake --build --preset linux-debug
ctest --preset linux-debug
```

Release ビルドでは `linux-release` を使用します。

```sh
cmake --preset linux-release
cmake --build --preset linux-release
ctest --preset linux-release
```

Debug の bootstrap 実行ファイルは次のように起動します。

```sh
./build/linux-debug/raypalette
```

CUDA ToolkitやNVIDIA GPUがない環境では、CPU版を使用します。CPU版とCUDA版は
別のビルドディレクトリとバイナリになります。

```sh
cmake --preset linux-cpu-debug
cmake --build --preset linux-cpu-debug
ctest --preset linux-cpu-debug --output-on-failure
./build/linux-cpu-debug/raypalette
```

GUI をビルドする場合は、Ubuntu の X11/OpenGL 開発パッケージをインストールした
うえで、GUI 用 preset を使用します。初回の構成では FetchContent による依存ライブラリ
のダウンロードが発生します。

```sh
cmake --preset linux-gui-debug
cmake --build --preset linux-gui-debug
./build/linux-gui-debug/raypalette_gui
```

CPU版のGUIは次のpresetでビルドできます。CUDA Toolkitは不要です。

```sh
cmake --preset linux-cpu-gui-debug
cmake --build --preset linux-cpu-gui-debug
./build/linux-cpu-gui-debug/raypalette_gui
```

### 2.2. Windows 11（未検証）

Visual Studio 2022 の「Developer PowerShell for VS 2022」または同等の開発者用
コマンドプロンプトで、Debug ビルドを実行します。

```powershell
cmake --preset windows-debug
cmake --build --preset windows-debug
ctest --preset windows-debug
```

Release ビルドでは `windows-release` を使用します。

```powershell
cmake --preset windows-release
cmake --build --preset windows-release
ctest --preset windows-release
```

Visual Studio generator では構成別サブディレクトリが作られるため、Debug の
bootstrap 実行ファイルは次のパスになります。

```powershell
.\build\windows-debug\Debug\raypalette.exe
```

GUI は Windows 用 preset で構成・ビルドします。Visual Studio 2022 の Developer
PowerShell から実行してください。

```powershell
cmake --preset windows-gui-debug
cmake --build --preset windows-gui-debug
.\build\windows-gui-debug\Debug\raypalette_gui.exe
```

### 3. コード整形

### 3.1. Ubuntu 24.04（確認済み）

Ubuntuでは`clang-format`をインストールしてから、任意のビルドディレクトリで
整形または整形チェックを実行します。C++とCUDAのソースが対象です。

```sh
sudo apt install clang-format
cmake --build build/linux-cpu-debug --target format
cmake --build build/linux-cpu-debug --target format-check
```

`format`はファイルを直接更新し、`format-check`は差分がある場合に失敗します。
整形設定は[.clang-format](.clang-format)にあります。既存ファイルの一括整形は
意図しない大きな差分になり得るため、導入時には変更内容を確認してください。

## 4. テスト

テストは CTest に登録されています。構成とビルドを完了した後、次のコマンドで
すべてのテストを実行します。

### 4.1. Ubuntu 24.04（確認済み）

```sh
ctest --preset linux-debug --output-on-failure
```

Release ビルドでは `linux-release` を使用します。

```sh
ctest --preset linux-release --output-on-failure
```

特定のテスト群だけを実行する場合は、`-R` に正規表現を指定します。

```sh
ctest --preset linux-debug -R Vec3 --output-on-failure
```

現在は、ベクトル・色・極座標のGoogleTest unit testとCPU Renderer testを実行します。
CUDA版では加えて、共有数学ヘッダを`nvcc`で検査するCUDAコンパイルチェックと、
GPU Renderer testを実行します。CUDAコンパイルチェックはビルド時に実行され、
CTestのテスト一覧には含まれません。

### 4.2. Windows 11（未検証）

```powershell
ctest --preset windows-debug --output-on-failure
```

Release ビルドでは `windows-release` を使用します。

```powershell
ctest --preset windows-release --output-on-failure
```

## 5. 依存ライブラリ

Dear ImGui、GLFW、GoogleTest は、CMake の `FetchContent` を通じて
`cmake/Dependencies.cmake` に不変の上流コミット SHA で定義しています。
実装済みターゲットで `RAYPALETTE_FETCH_DEPENDENCIES=ON` を有効にした場合にのみ
ダウンロードされます。初回のダウンロードにはネットワーク接続が必要ですが、
以降は CMake がビルドツリー内に展開したキャッシュを再利用します。Ubuntu では、
先に上記の GUI 用パッケージをインストールしてから有効にしてください。

## 6. 実行結果

CPU版はCUDA版と同じレンダリング機能を提供しますが、一般に大幅に低速です。
CPU版でGUIを確認する場合は、解像度、サンプル数、最大バウンス数を小さくして
実行してください。

左の`RayPalette Controls`画面でシーンの設定を行うと、その結果は逐次右の`Preview`画面へと反映されます。
また`Preview`画面でマウスをクリックするとその下側の`Palette`タブにそのuv位置の色を追加します。
`Palette`に追加された各色のHEXコードを選択して`Ctrl+C`を行うことで貼り付けすることが可能です。
この`Palette`上の色はシーンの設定を変えるたびにリセットされます。
また光源によって固有色がHSV色空間上でどのように変化するか？を右の`HSV Space`画面で3Dプロットとして確認できます。

- <img src="./assets/screenshots/20260813_gui.png" alt="実行画面画像" title="実行画面" width="70%">