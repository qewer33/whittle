# whittle

**whittle** is an experimental homebrew low-poly 3D modelling app for the 3DS.

> [!IMPORTANT]
> **LOGO NEEDED**: If any artists wants to create a high quality 3D logo for whittle (preferablly something whittling/wood related, maybe a low-poly bird woodcarving?) using whittle, I will make it the official logo :D 

> [!NOTE]
> Also if you're a 3D artist and liked whittle, **please contact me on Discord (@qewer33)**, I need feedback from artists to streamline the UX and add missing features.

## Installation

Move the `.3dsx` file in the Releases section to the `3ds/` folder on your SD card. Then launch it in the 3DS from the Homebrew Launcher.

## Usage

**whittle**'s workflow is *optimized for low-poly modelling* and is rather similar to that of **picoCAD**'s. whittle also takes advantage of the dual screens of the 3DS, the *top screen is reserved exclusively for the model preview*, while the *bottom touchscreen houses all of the editing functionality and view options*, intended to be used with the 3DS stylus.

**whittle** has two main *workspaces* (3D & 2D), the 3D workspca is for working with the general geometry and the 3D appereance of the model while the 2D workbench is for working on the texture atlas and UV mapping. Each workspace has different *modes* to do different tasks.

## 3D Workspace

The **3D workspace** has the base tools for 3D modelling, geometry editing and painting/texturing the model.

Below are a list of the modes in the 3D workspace with explanations:
- **Object**: The **Object mode** allows you to edit *whole* objects and apply the base transformations. It provides 3 tools: **Move**, **Rotate** and **Scale**.
- **Edit**: The **Edit mode** allows you to edit the individual geometry of objects. It provides 3 tools: **Vertex**, **Edge** and **Face**.
- **Paint**: The **Paint mode** allows you to paint faces of objects in soid colors. It provides 2 tools: **Brush** and **Picker**.
- **Texture**: The **Texture mode** allows you to texture the faces of objects. It provides 2 tools: **Texture** and **Untexture**.

## 2D Workspace

The **2D workspace** has tools for editing the texture atlas of the model and UV mapping the textured geometry faces onto the texture atlas.

Below are a list of the modes in the 3D workspace with explanations:
- **Paint**: The **Paint mode** provides a simple pixelart editor that allows you to edit the global 128x128 texture atlas. It provides 3 tools: **Brush**, **Bucket** and **Picker**.
- **UV**: The **UV mode** allows you to edit the UV maps of textured faces.

## Development

Written in C++ using devkitpro.

### Development Scripts

Some useful Bash (Linux CLI) scripts for development.

- `build.sh`: Builds the program to the `build/` folder.
- `run.sh`: Builds the program and opens it in Azahar (3DS emulator).
- `sync.sh`: Sends the program to a given FTP host (`FTP_HOST=<host>`). Useful for syncing the program to the 3DS.
