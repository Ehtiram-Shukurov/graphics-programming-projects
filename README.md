# CSCI 5607 Graphics Projects

This repository contains a collection of my projects from CSCI 5607 (Computer Graphics), covering interactive OpenGL rendering, ray tracing, small engine systems, and a final non-photorealistic rendering (NPR) platformer.

Each project is organized in its own folder and includes its own README with more details, build instructions, and media.

## Projects

### 1. Interactive Square
A simple interactive OpenGL project focused on 2D transformations and user interaction.

**Highlights**
- Scaling
- Rotation
- Texture switching
- Brightness control
- Animation toggle

📁 Folder: [InteractiveSquare](./InteractiveSquare/)

---

### 2. Ray Tracer (Basic)
A foundational CPU ray tracer written in C++ that implements the core ray tracing pipeline.

**Highlights**
- Sphere intersections
- Phong shading
- Shadows
- Scene parsing from `.txt` files
- Image output

📁 Folder: [raytracer-basic](./raytracer-basic/)

---

### 3. Ray Tracer (Advanced)
An extended version of the basic ray tracer with additional geometry, recursive effects, anti-aliasing, and parallel rendering.

**Highlights**
- Reflections and refractions
- Planes, boxes, and triangles
- Spot lights
- Jittered supersampling
- OpenMP parallelization

📁 Folder: [raytracer-advanced](./raytracer-advanced/)

---

### 4. 3D Maze Engine
A small interactive 3D OpenGL project with level logic, movement, collision, and simple gameplay systems.

**Highlights**
- Grid-based level loading
- OBJ model loading
- Texture mapping
- Camera movement
- Collision handling
- Keys, gates, and progression logic

📁 Folder: [3d-maze-engine-opengl](./3d-maze-engine-opengl/)

---

### 5. Platform Ink: NPR Platformer
My final project for the course: a third-person 3D platformer with non-photorealistic rendering and a complete playable game loop.

**Highlights**
- Toon / cel shading
- Rim lighting
- Inverted-hull outlines
- Third-person camera
- Data-driven level loading
- Collectibles, hazards, and multi-level progression
- Lightweight platforming collision system

📁 Folder: [platform-ink-npr-platformer](./platform-ink-npr-platformer/)

---

## Skills Covered

Across these projects, I worked with:

- C++
- OpenGL
- GLSL shaders
- SDL3
- GLAD
- GLM
- CPU ray tracing
- OBJ / MTL parsing
- Texture mapping
- Collision logic
- Data-driven level systems
- Non-photorealistic rendering (NPR)

## Notes

- These projects were developed as coursework, but each one was cleaned and documented as an individual portfolio piece.
- Some projects build directly on earlier assignments, showing progression from foundational rendering systems to more complete interactive experiences.
- Build instructions and usage details are included in each project folder.

## Context

Built as part of **CSCI 5607: Computer Graphics**, these projects reflect my progress from core graphics fundamentals to more advanced rendering and gameplay systems.
