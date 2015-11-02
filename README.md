
# Houdini Engine for 3dsMax

## Binaries 
[here](https://github.com/K240/HoudiniEngineFor3dsMax/wiki/Houdini-Engine-for-3dsMax-Binary)

## Requirements
- Visual Studio 2012(vc110)
- 3dsMax 2013/2014/2015/2016
- Houdini 15.0.244.16

## How to Build
Project files of Visual Studio is located in the build folder. To build it is necessary to set the environment variable, please refer to the following.

### Environment Variables
#### HOUDINI
##### HOUDINI_ROOT

    HOUDINI_ROOT=[your houdini install path]
    example:
    HOUDINI_ROOT=C:\Program Files\Side Effects Software\Houdini 15.0.244.16

#### 3DSMAX
##### ADSK_3DSMAX_SDK_2013
##### ADSK_3DSMAX_SDK_2014
##### ADSK_3DSMAX_SDK_2015
##### ADSK_3DSMAX_SDK_2016
    ADSK_3DSMAX_SDK_2016=[your maxsdk path]
    example:
    ADSK_3DSMAX_SDK_2016=D:\Program Files\Autodesk\3ds Max 2016 SDK\maxsdk

#### PATH
    PATH = C:\Program Files\Side Effects Software\Houdini xx.y.zzz\bin;%PATH%
    or 
    PATH = %HOUDINI_ROOT%\bin;%PATH%

## Acknowledgement
Throughout this project I learned a lot from the following:
 
- [HoudiniEngine for Maya](https://github.com/sideeffects/HoudiniEngineForMaya)
  - [License](https://github.com/sideeffects/HoudiniEngineForMaya/blob/Houdini15.0/LICENSE.txt)
- [Exocortex Crate](https://github.com/Exocortex/ExocortexCrate)
  - [License](https://github.com/Exocortex/ExocortexCrate/blob/master/LICENSE.txt)
- Maxsdk samples
 
The Houdini Engine team has helped me solving the following problems, which led to the development of HAPI2.0.
 
1. DLL collisions between 3dsMax (2015 and 2016) and Houdini Engine
2. Conflicting TBB threads between 3dsMax (2013 and 2014) and Houdini Engine