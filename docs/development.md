# 開発者向けガイド

[READMEに戻る](../README.md)

## 1. このガイドについて

このガイドでは、raypaletteをソースコードからビルド、テスト、整形する方法と、
Windows向け正式配布版を作成する方法を説明します。

通常の利用方法は、GitHub Releasesから配布版をダウンロードすることです。
配布版の使い方は[README](../README.md)を参照してください。

## 2. 開発環境の準備

### 2.1 共通の前提条件

次のソフトウェアが必要です。

- Git
- CMake 3.24以降
- C++17に対応したコンパイラ
- FetchContentで依存ライブラリを取得できるネットワーク環境
- clang-format 14以降（コード整形を行う場合）
- NVIDIA CUDA Toolkit 12.8以降（GPU版をビルドする場合）

Dear ImGui、GLFW、GoogleTestは、CMakeの`FetchContent`を通じて必要な構成で取得します。
初回のconfigureにはネットワーク接続が必要です。依存ライブラリの取得先と固定コミットは
`cmake/Dependencies.cmake`で管理しています。

### 2.2 Ubuntuの追加要件

確認済みの環境はUbuntu 24.04とGCC 13.3です。GUIをビルドする場合は、X11/OpenGLの
開発パッケージをインストールしてください。

```sh
sudo apt install \
	build-essential \
	cmake \
	libgl1-mesa-dev \
	libxrandr-dev \
	libxinerama-dev \
	libxcursor-dev \
	libxi-dev \
	clang-format
```

GPU版をビルドする場合は、NVIDIA CUDA Toolkit 12.8以降も必要です。

### 2.3 Windowsの追加要件

確認済みの環境はWindows 11とVisual Studio 2026です。次のコンポーネントを含む
Visual Studio CommunityまたはBuild Toolsを使用してください。

- 「C++によるデスクトップ開発」ワークロード
- MSVC
- Windows SDK
- clang-format（Visual StudioのLLVM toolsまたはLLVM公式パッケージ）

GPU版をビルドする場合は、NVIDIA CUDA Toolkit 12.8以降も必要です。
CMakeコマンドはVisual StudioのDeveloper PowerShellまたはDeveloper Command Promptから
実行してください。Windows標準のOpenGLとWin32 DLLを使用します。

## 3. CMake presetの選び方

プリセット名は、対象OS、CPU/GPU、ビルド構成の組み合わせで決まります。

| 用途 | CPU | GPU |
|---|---|---|
| Debug | `linux-cpu-debug` / `windows-cpu-debug` | `linux-debug` / `windows-debug` |
| Release | `linux-cpu-release` / `windows-cpu-release` | `linux-release` / `windows-release` |

GPU版のpresetはCUDA Toolkitが必要です。CPU版はCUDA ToolkitやNVIDIA GPUなしで
ビルドできます。Debug版は開発・デバッグ向け、Release版は性能確認や配布向けです。

すべての開発用presetでは`BUILD_TESTING=ON`が有効です。配布用ビルドでは、後述の
正式配布スクリプトが`BUILD_TESTING=OFF`を指定します。

## 4. 構成・ビルド・実行

基本的な流れは、configure、build、test、runの順です。初回のconfigureでは依存ライブラリ
がダウンロードされます。

### 4.1 Ubuntu

CPU版GUIをビルドして実行する場合:

```sh
cmake --preset linux-cpu-debug
cmake --build --preset linux-cpu-debug
ctest --preset linux-cpu-debug --output-on-failure
./build/linux-cpu-debug/raypalette
```

GPU版をビルドする場合は、preset名を`linux-debug`に置き換えます。

```sh
cmake --preset linux-debug
cmake --build --preset linux-debug
ctest --preset linux-debug --output-on-failure
./build/linux-debug/raypalette
```

### 4.2 Windows

CPU版GUIをビルドして実行する場合:

```powershell
cmake --preset windows-cpu-debug
cmake --build --preset windows-cpu-debug
ctest --preset windows-cpu-debug --output-on-failure
.\build\windows-cpu-debug\Debug\raypalette.exe
```

GPU版をビルドする場合は、preset名を`windows-debug`に置き換えます。

```powershell
cmake --preset windows-debug
cmake --build --preset windows-debug
ctest --preset windows-debug --output-on-failure
.\build\windows-debug\Debug\raypalette.exe
```

Visual Studio generatorでは、実行ファイルは`build/<preset>/Debug`または
`build/<preset>/Release`に生成されます。

## 5. テスト

開発用presetでは、テストターゲットを含めて構成されます。ビルド後にCTestを実行します。

```sh
ctest --preset linux-cpu-debug --output-on-failure
```

Windowsでは、使用したpresetに合わせて`linux`を`windows`へ置き換えます。
特定のテストだけを実行する場合は、`-R`にテスト名の正規表現を指定します。

```sh
ctest --preset linux-cpu-debug -R Vec3 --output-on-failure
```

CPU版では、ベクトル・色・極座標などのunit testとCPU Renderer testを実行します。
GPU版では、これらに加えて共有数学ヘッダーのCUDAコンパイルチェックとGPU Renderer testを
実行します。CUDAコンパイルチェックはビルド時に実行され、CTestのテスト一覧には含まれません。

CUDA対応GPUがない環境では、CUDA実行テストはskipされます。GPUテストを実行する場合は、
CUDA Toolkitと対応するNVIDIA GPU、ドライバを準備してください。

## 6. コード整形

`format`は対象ファイルを直接整形し、`format-check`は差分がある場合に失敗します。
対象はC++、CUDA、ヘッダーファイルです。整形設定は`.clang-format`で管理しています。

```sh
cmake --build build/linux-cpu-debug --target format
cmake --build build/linux-cpu-debug --target format-check
```

Windowsでは、使用したビルドディレクトリを指定します。

```powershell
cmake --build build/windows-cpu-debug --target format
cmake --build build/windows-cpu-debug --target format-check
```

## 7. Windows正式配布版の作成

正式配布版は、Windows上でGPU版を作成する`scripts/package_windows.sh`を使用します。
このスクリプトは`BUILD_TESTING=OFF`で構成し、CTestを実行せずに`raypalette.exe`を作成します。
CUDA Toolkitと対応するGPUが必要です。

Git Bashまたは同等のbash環境から実行します。

```sh
bash scripts/package_windows.sh
```

生成物は次の3ファイルです。

```text
dist/windows-release/raypalette.exe
dist/windows-release/THIRD_PARTY_NOTICES.txt
dist/windows-release/SHA256SUMS.txt
```

`SHA256SUMS.txt`には、同じスクリプトで生成した`raypalette.exe`のSHA256値だけが記録されます。
GitHub ReleaseのAssetsには、上記3ファイルを登録してください。

## 8. 依存ライブラリ

| ライブラリ | 用途 | 取得方法 |
|---|---|---|
| Dear ImGui | GUI | CMake FetchContent |
| GLFW | ウィンドウ、入力、OpenGL連携 | CMake FetchContent |
| GoogleTest | unit test | CMake FetchContent |

Dear ImGuiとGLFWはGUI実行ファイルへ組み込まれます。GoogleTestはテスト専用で、配布用
ビルドでは取得・構成されません。第三者ライセンスの詳細は`THIRD_PARTY_NOTICES.txt`を
参照してください。

GPU版で使用するCUDA ToolkitとNVIDIAドライバは、raypaletteのMIT Licenseの対象外です。
それぞれの利用条件と配布条件を確認してください。

## 9. トラブルシューティング

### CUDA Toolkitが見つからない

GPU版のpresetではCUDA Toolkitが必要です。CPU版のpresetを使用する場合は、
`RAYPALETTE_ENABLE_CUDA=OFF`のpresetを選択してください。

### 依存ライブラリの取得に失敗する

初回のconfigureでは、FetchContentがDear ImGui、GLFW、GoogleTestを取得します。
ネットワーク接続とGitHubへのアクセスを確認してください。

### GUI用のライブラリが見つからない

Ubuntuでは、2.2節のX11/OpenGL開発パッケージをインストールしてください。
Windowsでは、Visual Studioの「C++によるデスクトップ開発」、MSVC、Windows SDKを
確認してください。

### GPUが利用できない

NVIDIAドライバ、CUDA Toolkit、対応GPUを確認してください。開発時の動作確認には、
CPU版のpresetを使用できます。
