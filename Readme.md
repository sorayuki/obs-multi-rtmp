# Screenshot

![screenshot](./screenshot.jpg)

# Build

1. Prepare environment
   1. Put official release OBS 25.0 into obs-bin directory. 
   2. Extract OBS source code of same version as binary to obs-src
   3. Download Qt that obs-bin uses. Which can be found in CI\install-qt-win.cmd

2. Configure
   Use cmake to configure this project. must use VS2017 or higher. 
   cmake's QTDIR variable is set to the path of QT in the same version as obs uses. 
   
   Set CMAKE_BUILD_TYPE to Release. 

   > Notice: debug version of this plugin only works with debug version of OBS, which means you must build OBS from source.

3. Compile


# FAQ

Q: Why it must be OBS 25.0.0 or higher?

A: OBS's rtmp output module is not thread-safe until 25.0.0. 

detail: 

https://github.com/obsproject/obs-studio/commit/2b131d212fc0e5a6cd095b502072ddbedc54ab52 


Q: Why it must start streaming in OBS main UI before the multiple output plugin works?

A: This plugin shares encoders with OBS, but OBS will not create them before first streaming.
