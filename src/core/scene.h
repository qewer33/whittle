#pragma once

#include <vector>
#include <string>
#include "mesh.h"

// top-level editing mode (bottom-left popup)
enum class EditMode
{
    Object, // arrange whole objects (move/rotate/scale switch)
    Edit,   // sub-object mesh editing (vertex/edge/face switch)
    Paint,
    Texture,
};

// Object-mode transform tool (segmented switch)
enum class TransformTool
{
    Move,
    Rotate,
    Scale,
};

// Edit-mode sub-object level (segmented switch)
enum class SubLevel
{
    Vertex,
    Edge,
    Face,
};

struct VertRef
{
    int obj, vert;
};

struct FaceRef
{
    int obj, face;
};

struct EdgeRef
{
    int obj, v0, v1; // an unordered vertex pair
};

// one saved project on the SD card, as shown in the browser
struct ProjectInfo
{
    std::string path; // full sdmc path to the .whittle file
    std::string name; // display name from the file header
    long mtime;       // last-modified time, for sorting
};

// the document: objects, selection, undo/redo history, and SD save/load. no
// input or drawing, the Editor drives it.
struct Scene
{
    static constexpr int kTexSize = 128; // shared texture is kTexSize square

    Scene(); // fills the texture with the default checkerboard

    std::vector<Mesh> objects;
    int activeObject = 0;

    std::vector<VertRef> selectedVerts; // edit/vertex selection
    std::vector<EdgeRef> selectedEdges; // edit/edge selection
    std::vector<FaceRef> selectedFaces; // edit/face selection
    std::vector<int> selectedObjects;   // object mode selection

    // shared paintable texture, 0xRRGGBBAA. textureDirty asks main to re-upload.
    std::vector<u32> texture;
    bool textureDirty = true;

    // spawn a primitive at origin (0=cube,1=sphere,2=pyramid,3=cylinder,
    // 4=plane), returns its index or -1. clears selection, snapshots.
    int addShape(int kind);
    void deleteSelectedObjects();
    void deleteSelectedVerts();
    void deleteSelectedEdges();  // drop faces adjacent to the selected edges
    void deleteSelectedFaces();  // drop the selected faces
    void extrudeSelectedFaces();   // pull selected faces out, walling the border
    void subdivideSelectedFaces(); // refine the whole touched object; keep selected children
    void splitSelectedEdges();     // insert a vertex on each selected edge
    void textureAllFaces();      // mark every face textured (atlas layout done separately)
    void untextureAllFaces();    // revert every face to a flat color

    void snapshot(); // call before a mutation to make it undoable
    void undo();
    void redo();
    bool hasUndo() const { return !undoStack.empty(); }
    bool hasRedo() const { return !redoStack.empty(); }

    void clearSelection();
    bool isObjectSelected(int obj) const;
    bool isVertSelected(int obj, int vert) const;
    bool isEdgeSelected(int obj, int a, int b) const; // order-independent
    bool isFaceSelected(int obj, int face) const;
    Vec3 selectionCentroid() const;

    // current project identity. empty path = untitled (never saved).
    std::string projectName;
    std::string projectPath;
    bool dirty = false; // unsaved changes since the last save/load

    bool save();                          // overwrite projectPath, false if untitled
    bool saveAs(const std::string& name); // slug a new file under the projects dir
    bool load(const std::string& path);   // load a specific project file
    bool loadNewest();                    // load the most-recently-saved project
    std::vector<ProjectInfo> listProjects() const;
    // read a project's geometry only, without touching this scene (browser preview)
    bool peekMeshes(const std::string& path, std::vector<Mesh>& out) const;
    bool deleteProject(const std::string& path); // remove the file
    void newProject();                           // reset to a fresh untitled scene

private:
    bool writeTo(const char* path);  // serialize to a file
    bool readFrom(const char* path); // deserialize from a file
    static constexpr int kMaxUndo = 32;
    struct State
    {
        std::vector<Mesh> objects;
        std::vector<u32> texture;
    };
    std::vector<State> undoStack;
    std::vector<State> redoStack;
    void clampActive();
};
