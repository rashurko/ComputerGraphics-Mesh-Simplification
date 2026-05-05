# Computer Graphics – Mesh Simplification

Project repository for **Mesh Simplification (Group 6)** in Computer Graphics.

---

## Controls

| Action | Input |
|--------|--------|
| Load model | Right-click (context menu) |
| Move (X/Y axes) | `W`, `A`, `S`, `D` |
| Move (Z axis) | Mouse scroll |
| Rotate model | Hold left mouse button + move mouse |

---

## Dependencies

The project uses the following libraries:

- **GLAD** – OpenGL function loader  
- **GLFW (v3.3)** – Window and input handling  
- **OpenGL (v4.5)** – Rendering  
- **GLM** – Mathematics library  
  https://github.com/g-truc/glm  
- **stb_image** – Image loading  
  https://github.com/nothings/stb  
- **OBJ Loader** – `.obj` file parsing  
  https://github.com/Bly7/OBJ-Loader  
- **ImGui** – Graphical user interface  
  https://github.com/ocornut/imgui  
- **Portable File Dialogs** – File selection dialogs  
  https://github.com/samhocevar/portable-file-dialogs  

---

## Requirements
- C++ compiler (g++)
- OpenGL 4.5
- GLFW and GLAD

---

## Build Instructions


```bash
cd build (has to be empty)
cmake ..
make
```


## Running the application
```bash
./main
```
The application will be available in the source directory.

