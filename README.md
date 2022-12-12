# Vulkan Code Sample Framework

The Adreno™ GPU Vulkan Code Sample Framework is a lightweight collection of C++ classes and sample projects to demonstrate Vulkan rendering features on the Qualcomm Snapdragon Adreno™ GPU.

Both Android and Microsoft Windows build targets are supported (some samples may require/target Qualcomm Adreno™ specific Vulkan extensions).

## Prerequisites

### Windows

- Visual Studio (tested against 2019)
- CMake (tested against 3.18+) https://cmake.org/download/
- Git https://git-scm.com/downloads
- Vulkan SDK (1.3.224 or later) https://vulkan.lunarg.com/

### Android

- Android Studio (install NDK and SDK)
- CMake (> 3.10) https://cmake.org/download/
- Ninja https://github.com/ninja-build/ninja/releases (or install Cmake from Android Studio and add the cmake bin folder to your PATH - AFTER the CMake path from the step above)
- Java JDK https://www.oracle.com/java/technologies/javase-downloads.html

## Getting the vkSampleFramework project

The framework uses submodules for both external code dependencies and to get the shared assets for the sample projects (vkSampleFrameworkAssets).

`git clone --recursive https://github.com/quic/adreno-gpu-vulkan-code-sample-framework`

Will clone the vkSampleFramework and its submodules.

Subsequent pulls can be done with

`git pull --recurse-submodules`

## Build Setup

### Windows

Ensure CMake is in the Windows PATH

### Android

Create *project/android/local.properties* with the location of our cmake install eg:
<pre><code>cmake.dir=C\:\\Tools\\cmake\\3.10.2.4988404
</code></pre>

If necessary, also include on *local.properties* the ndk and sdk locations eg:

<pre><code>ndk.dir=C\:\\Users\\yourname\\AppData\\Local\\Android\\Sdk\\ndk\\21.0.6113669
sdk.dir=C\:\\Users\\yourname\\AppData\\Local\\Android\\Sdk
</code></pre>

Create *project/android/gradle.properties* with the location of our jdk and build heap space parameter, eg:
<pre><code>org.gradle.java.home=C\:\\Program Files\\Java\\jdk-11
org.gradle.jvmargs=-Xms512M -Xmx4G
</code></pre>

Note: Setting up the jdk location does not affect the Java version used to launch the Gradle client VM, so make sure your *JAVA_HOME* is set to a valid java version
(this is specially true if you receive errors in the format: *Unsupported class file major version XX)*

## Building

In the root folder there is a batch file for building and packaging on each platform

`01_BuildAndroid.bat`

`02_BuildWindows.bat`

Android uses Gradle and CMake to build an Android .apk for each sample.

Windows uses CMake to build a Visual Studio solution that is then built using Visual Studio (and can then be opened in the Visual Studio IDE).  An .exe is output for each sample.

The Windows Visual Studio file is written to `project\windows\solution\vkSampleFramework.sln`.  Once created for the first time it can be opened and used in Visual Studio 2019 (VS2019 has full support for editing CMakeLists.txt).  When using VS2019 to compile and run, be sure to point the debugger to the correct 'Working Directory' for each sample.

Note: Depending on your sample you might need to perform certain steps before running these batch files, see the readme on the desired sample for more information about this.

## Running

See the [Samples](samples) folder for instructions on building assets and running individual samples. 

## Directory Structure

- [/framework](framework)
Contains the sample framework code.
- [/framework/external](framework/external)
Contains external projects used by the framework (mostly as git submodules)
- [/project](project)
Platform specific top level build files and build tools.
- [/samples](samples)
Samples that use the framework.

## License
Adreno™ GPU Vulkan Code Sample Framework is licensed under the BSD 3-clause “New” or “Revised” License. Check out the [LICENSE](LICENSE) for more details.