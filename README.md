# GLFW with DiligentEngine
Sample project showcase [GLFW](https://github.com/glfw/glfw) (window) with [DiligentEngine](https://github.com/DiligentGraphics/DiligentEngine) (rendering).
In this sample, D3D12 is used on Windows and OpenGL on MacOS. The render device type can be changed by `m_DeviceType` variable in `main.cpp`.

### Build
1. `git submodule update --init --recursive`
2. Configure the project: `cmake -B build`
3. Build the project: `cmake --build build`

### License
* GLFW: [LICENSE](https://www.glfw.org/license.html)
* DiligentCore: [LICENSE](https://github.com/DiligentGraphics/DiligentCore#license)
* DiligentSamples: [LICENSE](https://github.com/DiligentGraphics/DiligentSamples#license)
