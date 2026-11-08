<p align="center"><img src="docs/icon.png" width="160" alt="Cloudfin icon"></p>

# Cloudfin

A dreamy, floating multi-effect in a Frutiger Aero look, after a recurring dream of an endless white waterslide with blue light shining through it.

Seven effects run one after another, in any order you like: drag the chips under the title to rearrange them. Every knob can drift on its own random voltage, and a feedback loop wraps the whole chain. A hydrophone recording of moving water can play into it or over it. Switch on gravity and every control falls and piles up at the bottom of the window.

- macOS: Audio Unit, VST3 and a standalone app
- Windows: VST3 and a standalone app
- Linux: VST3, LV2 and a standalone app

![Cloudfin](docs/screenshot.jpg)

**[How to use it: the full help page](docs/HELP.md)**

## Download

Get the latest version from the [Releases page](https://github.com/femifleming/Cloudfin/releases/latest):

| System | File |
| --- | --- |
| macOS 11 or later (Apple silicon and Intel) | [Cloudfin-macOS.zip](https://github.com/femifleming/Cloudfin/releases/latest/download/Cloudfin-macOS.zip) |
| Windows 10 or later (64-bit) | [Cloudfin-Windows.zip](https://github.com/femifleming/Cloudfin/releases/latest/download/Cloudfin-Windows.zip) |
| Linux (x86-64) | [Cloudfin-Linux.tar.gz](https://github.com/femifleming/Cloudfin/releases/latest/download/Cloudfin-Linux.tar.gz) |

## Install

### macOS with Homebrew

```sh
brew install --cask femifleming/cloudfin/cloudfin
```

This puts the app in `/Applications` and the Audio Unit and VST3 in `~/Library/Audio/Plug-Ins`. Update with `brew upgrade --cask cloudfin`, remove with `brew uninstall --cask cloudfin`.

### macOS by hand

1. Unzip `Cloudfin-macOS.zip`.
2. Copy `Cloudfin.component` to `~/Library/Audio/Plug-Ins/Components` and `Cloudfin.vst3` to `~/Library/Audio/Plug-Ins/VST3`. Put the app anywhere you like, such as `/Applications`.
3. The builds aren't notarised by Apple, so macOS may refuse to open them the first time. Open Terminal and run:
   ```sh
   xattr -dr com.apple.quarantine ~/Library/Audio/Plug-Ins/Components/Cloudfin.component ~/Library/Audio/Plug-Ins/VST3/Cloudfin.vst3 /Applications/Cloudfin.app
   ```
4. In your DAW, rescan plug-ins. In Ableton Live: Settings > Plug-ins, turn on *Use Audio Units v2* and/or *Use VST3 Plug-in System Folders*, then press Rescan. It appears under **FreakiePie**.

### Windows

1. Unzip `Cloudfin-Windows.zip`.
2. Copy `Cloudfin.vst3` to `C:\Program Files\Common Files\VST3`.
3. Rescan plug-ins in your DAW. The standalone `Cloudfin.exe` runs from anywhere. Windows SmartScreen may warn you the first time because the build isn't signed: choose *More info* > *Run anyway*.

### Linux

1. Unpack: `tar -xzf Cloudfin-Linux.tar.gz`
2. Copy the plug-ins into your user folders:
   ```sh
   mkdir -p ~/.vst3 ~/.lv2
   cp -r Cloudfin/Cloudfin.vst3 ~/.vst3/
   cp -r Cloudfin/Cloudfin.lv2 ~/.lv2/
   ```
3. Rescan in your DAW (most Linux DAWs look in these folders). The standalone app runs with `./Cloudfin` and uses ALSA or JACK.

## Build it yourself

You need CMake 3.22 or later and a C++20 compiler (Xcode on macOS, Visual Studio 2022 on Windows, GCC or Clang on Linux).

```sh
git clone --recursive https://github.com/femifleming/Cloudfin.git
cd Cloudfin
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release --target Cloudfin_All
```

On macOS and Windows the build also copies the plug-ins into your user plug-in folders (turn that off with `-DCLOUDFIN_COPY_AFTER_BUILD=OFF`). On Linux, install the JUCE dependencies first:

```sh
sudo apt-get install libasound2-dev libjack-jackd2-dev libfreetype-dev libfontconfig1-dev libx11-dev libxcomposite-dev libxcursor-dev libxext-dev libxinerama-dev libxrandr-dev libxrender-dev libgl1-mesa-dev
```

The tests (`-DCLOUDFIN_BUILD_TESTS=ON`) check the sound and drive the whole interface headlessly, saving screenshots as they go.

## Credits and notes

- The hydrophone recording is original to this project.
- Built with [JUCE](https://juce.com).

## License

Released under the GNU Affero General Public License v3 (see [LICENSE](LICENSE)), as JUCE's free licence requires.
