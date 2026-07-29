/* -*- mode: c++ -*- */
#ifndef kefKLine_h
#define kefKLine_h

//
// Begin local includes
//
#include <gdyNonPageGeomEditor.h>
#include <g3dVector.h>
#include <cowListInt.h>

#include <array>
#include <cowList.T>
#include <omiBtree.T>

class kefKLine : public gdyNonPageGeomEditor
{
public:
    // Constructor, destructor
    kefKLine();
    virtual ~kefKLine();

    virtual gdyGeomEditor* Copy() const;

    // Type access
    const omuAtom& Type() const { return type_atom; }

    // Methods called from gdyEditor
    virtual void Load(const gdyDisplayRep* r, g3dSphere*);
    virtual void Unload();
    virtual void Draft(gdrRenderer&) const;
    virtual void Paint(gdrRenderer&) const;
    virtual void ASelect(gdrASelector&) const;
    virtual void Highlight(gdrRenderer&) const;

    // Find the index of an object by its segment ID; returns -1 if not found
    int findObject(int segID) const;

    // Create a new object or modify an existing one.
    // segID: [in/out] — if found in existing objects, the entry is updated;
    //         otherwise a new object is created and segID receives the new ID.
    void createOneObject(int& segID, const cowList<g3dVector>& vertexXYZ = cowList<g3dVector>());

private:
    void Draw(gdrRenderer& drafter, bool paintMode) const;

    double timeStamp;
    omuAtom type_atom;

    cowList<cowList<g3dVector>> _geomVertexXYZs;
    cowListInt _segID;
    int _maxSegID;

};

#endif /* kefKLine_h */