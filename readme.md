<div align="center">
<pre>
 ███████████     █████████   ██████████   █████  █████████ 
▒▒███▒▒▒▒▒███   ███▒▒▒▒▒███ ▒▒███▒▒▒▒███ ▒▒███  ███▒▒▒▒▒███
 ▒███    ▒███  ▒███    ▒███  ▒███   ▒▒███ ▒███ ▒███    ▒▒▒ 
 ▒██████████   ▒███████████  ▒███    ▒███ ▒███ ▒▒█████████ 
 ▒███▒▒▒▒▒███  ▒███▒▒▒▒▒███  ▒███    ▒███ ▒███  ▒▒▒▒▒▒▒▒███
 ▒███    ▒███  ▒███    ▒███  ▒███    ███  ▒███  ███    ▒███
 █████   █████ █████   █████ ██████████   █████▒▒█████████ 
▒▒▒▒▒   ▒▒▒▒▒ ▒▒▒▒▒   ▒▒▒▒▒ ▒▒▒▒▒▒▒▒▒▒   ▒▒▒▒▒  ▒▒▒▒▒▒▒▒▒
</pre>
</div>

<div align="center">

# Radis Engine

A modern, cross-platform game engine featuring **Vulkan 1.4** and **OpenGL 4.6** backends with real-time raytracing, PBR rendering, and a full-featured editor.

![C++](https://img.shields.io/badge/C%2B%2B-23-blue?style=flat-square&logo=cplusplus)
![Vulkan](https://img.shields.io/badge/Vulkan-1.4-red?style=flat-square&logo=vulkan)
![OpenGL](https://img.shields.io/badge/OpenGL-4.6-green?style=flat-square&logo=opengl)
![Platform](https://img.shields.io/badge/Platform-Windows-lightgrey?style=flat-square&logo=windows)
![License](https://img.shields.io/badge/License-MIT-yellow?style=flat-square)

</div>

---

## ✨ Features

### 🎨 Rendering
- **Multiple Render Paths** — Forward, Deferred, and Hardware Raytracing modes
- **Physically Based Rendering (PBR)** — GGX/Schlick-GGX BRDF with metallic-roughness workflow
- **Real-time Raytracing** — Vulkan RTX acceleration structures with reflections, shadows, and a cost heatmap visualization
- **G-Buffer Pipeline** — Albedo, normals, PBR properties, and emissive render targets
- **HDR Pipeline** — 16-bit floating-point scene buffer with tonemapping
- **Render Graph** — Automatic resource transitions and barrier management with dynamic rendering (VK_KHR_dynamic_rendering)
- **Hot-swappable Backends** — Switch between Vulkan and OpenGL at runtime (Ctrl+R)

### 🎮 Engine Architecture  
- **Entity Component System (ECS)** — Built on [EnTT](https://github.com/skypjack/entt) for high-performance entity management
- **Resource System** — Type-safe resource management with automatic lifetime handling
- **System Pipeline** — Ordered Init → FrameStart → Update → FrameEnd → Exit lifecycle
- **Scene Serialization** — JSON-based scene format with reflection-driven component serialization
- **Profiler** — Built-in hierarchical CPU profiler with per-frame snapshots and aggregated statistics

### 🖼️ Asset Pipeline
- **Model Loading** — Support for glTF, FBX, OBJ, and other formats via Assimp
- **Unified Mesh System** — All meshes batched into a single vertex/index buffer for efficient instanced rendering
- **Texture Formats** — KTX2 (GPU-compressed), PNG, JPG, and embedded textures
- **Multi-threaded Loading** — Parallel texture loading with queued GPU upload
- **Skeletal Animation** — Bone hierarchies with VQS (Vector-Quaternion-Scale) transforms uploaded to GPU

### 🔧 Editor
- **Dockable UI** — ImGui-based editor with scene hierarchy, inspector, asset browser, and texture viewer
- **Gizmos** — Translation, rotation, and scale manipulation via ImGuizmo
- **Live Tweaking** — Modify materials, lights, and transforms in real-time
- **Multi-select** — Select and duplicate multiple entities (Ctrl+D)
- **Debug Visualization** — Wireframe mode, editor grid, and debug draw primitives

### 🌐 Networking (Experimental)
- **ENet Integration** — Client-server architecture for multiplayer prototyping
- **Real-time Chat** — In-editor chat window with player presence

### ⚙️ Physics (Experimental)
- **Soft Body Simulation** — Mass-spring system with RK4 integration
- **Debug Visualization** — Strain-colored springs and velocity-colored particles

---

## 🖥️ System Requirements

| Component | Minimum | Recommended |
|-----------|---------|-------------|
| **OS** | Windows 10 | Windows 11 |
| **CPU** | Any x64 processor | Multi-core recommended |
| **GPU** | Vulkan 1.2 / OpenGL 4.6 | RTX 2060+ for raytracing |
| **RAM** | 8 GB | 16 GB |

---

## 🚀 Getting Started

### Prerequisites
- [Visual Studio 2026](https://visualstudio.microsoft.com/) with C++23 support
- Windows SDK 10.0+

### Building

1. Clone the repository
2. Open Radis.sln in Visual Studio, 
3. Build and run


---

## 🎛️ Camera Controls

| Action | Input |
|--------|-------|
| **Look Around** | Right Mouse + Drag |
| **Move** | W / A / S / D / E / Q |

---

## 🔌 Dependencies

| Library | Purpose |
|---------|---------|
| [Vulkan SDK](https://vulkan.lunarg.com/) | Graphics API |
| [Volk](https://github.com/zeux/volk) | Vulkan function loader |
| [VMA](https://github.com/GPUOpen-LibrariesAndSDKs/VulkanMemoryAllocator) | Vulkan memory allocation |
| [GLFW](https://www.glfw.org/) | Window & input |
| [GLEW](https://glew.sourceforge.net/) | OpenGL extension loading |
| [GLM](https://github.com/g-truc/glm) | Math library |
| [EnTT](https://github.com/skypjack/entt) | ECS framework |
| [Assimp](https://github.com/assimp/assimp) | Model importing |
| [stb_image](https://github.com/nothings/stb) | Image loading |
| [Dear ImGui](https://github.com/ocornut/imgui) | Editor UI |
| [ImGuizmo](https://github.com/CedricGuillemet/ImGuizmo) | 3D gizmos |
| [nlohmann/json](https://github.com/nlohmann/json) | JSON parsing |
| [ENet](http://enet.bespin.org/) | Networking |
| [reflect-cpp](https://github.com/getml/reflect-cpp) | Compile-time reflection |

---

## 📸 Showcase

> *Screenshots and videos coming soon!*

---

## 📄 License

This project is licensed under the **MIT License** — see the [LICENSE](LICENSE) file for details.

---

<div align="center">
  <sub>Built with ❤️ by <a href="https://github.com/AdiPrk">Aditya Prakash</a></sub>
</div>