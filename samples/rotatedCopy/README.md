# VK_QCOM_rotated_copy_commands Extension Sample

Sample to initialize and use the 'VK_QCOM_rotated_copy_commands' Vulkan extension.

Extension may/will need enabling on older Qualcomm Vulkan drivers.  Sample does nothing useful on non Qualcomm hardware.

![Screenshot](img/screenshot.PNG)

## Overview

This extension extends adds an optional rotation transform to copy commands vkCmdBlitImage2KHR, vkCmdCopyImageToBuffer2KHR and vkCmdCopyBufferToImage2KHR. When copying between two resources, where one resource contains rotated content and the other does not, a rotated copy may be desired.

## Building

### Dependencies

The following dependencies must be installed and the appropriate locations should be referenced in the `PATH` environment variable.

* Android SDK
* Andorid NDK
* Gradle
* CMake
* Android Studio

### Pre-Build

Compile the underlying shaders to .spv by running the batch file below:

```
01_CompileShaders.bat
```

Note: The sample assumes the existence of supporting assets under the **'Media'** folder. These assets are not currently distributed with the framework.
The framework team is working to build a centralized asset repository that should minimize these requirements in the near future.

### Build

Once the dependencies are installed and shaders compiled, building this sample .apk/.exe is as simple as running any of the batch files from the framework root directory, accordingly to your target system:

```
01_BuildAndroid.bat
02_BuildWindows.bat
```

### Deploy (android-only)

To deploy the media files and the .apk to a connected device, run the batch files below:

```
02_Install_APK.bat
```

If desired, you can keep track of any logging by running one of the logcat batch files (which you can find on the current directory).

## Android Studio

This sample can also be easily imported to Android Studio and be used within the Android Studio ecosystem including building, deploying, and native code debugging.

To do this, open Android Studio and go to `File->New->Import Project...` and select the `project\android` folder as the source for the import. This will load up the gradle configuration and once finalized, the sample can be used within Android Studio.


