# Snapdragon™ Game Super Resolution 2

![Screenshot](img/screenshot.png)

## Overview

This sample demonstrates how to use [Snapdragon™ Game Super Resolution 2](https://github.com/SnapdragonStudios/snapdragon-gsr).

Snapdragon™ Game Super Resolution 2 (Snapdragon™ GSR 2 or SGSR 2) was developed by Qualcomm Snapdragon Game Studios. It's a temporal upscaling solution optimized for Adreno devices. It comes with 3 different variants (compute 3-pass is the one being demonstrated on this sample).

## Building

### Dependencies

The following dependencies must be installed and the appropriate locations should be referenced in the `PATH` environment variable.

* Android SDK
* Andorid NDK
* Gradle
* CMake
* Android Studio

Build the tools program, which is used to conver textures, meshes and compile shaders.

```
03_BuildTools.bat
```

### Build

Once the dependencies are installed, building this sample .apk/.exe is as simple as running any of the batch files from the framework root directory, accordingly to your target system:

```
01_BuildAndroid.bat
02_BuildWindows.bat
```

## Android Studio

This sample can also be easily imported to Android Studio and be used within the Android Studio ecosystem including building, deploying, and native code debugging.

To do this, open Android Studio and go to `File->New->Import Project...` and select the `project\android` folder as the source for the import. This will load up the gradle configuration and once finalized, the sample can be used within Android Studio.
