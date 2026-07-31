#ifndef KEFKLINE_H
#define KEFKLINE_H

#include <cowListInt.h>
#include <cowListString.h>
#include <gdyNonPageGeomEditor.h>
#include <gruSymbolListMap.h>
#include <sesCDisplayOptions.h>

#include <array>
#include <cowList.T>
#include <omiBtree.T>

using namespace std;

class ftrPrimaryObject;
class gslMatrix;

class kefKLine: public gdyNonPageGeomEditor
{
public:
    kefKLine();
    virtual ~kefKLine();

    virtual gdyGeomEditor* Copy() const;

    // Type access
    const omuAtom& Type() const
    {
        return type_atom;
    }

    // Methods called from gdyEditor
    virtual void Load(uint, const gdyDisplayRep& r, g3dSphere* extent_out);
    virtual void Unload();
    virtual void Draft(gdrRenderer& r) const;
    virtual void Paint(gdrRenderer& r) const;
    virtual void ASelect(gdrASelector& as) const;
    virtual void Highlight(gdrRenderer&) const;

    void SetHighlightedInertia(const QString&);
    void HighlightInertia(const QString& inertiaName);
    void UnhighlightAll();
    void UpdateDisplayOptions(const sesCDisplayOptions&);
    void setObject(const cowList<cowList<g3dVector>>& vertexXYZ, const cowListString& color);
    void addObject(const cowList<cowList<g3dVector>>& vertexXYZ, const cowListString& color);

    // create a new object or modify a object, SegID is input/output
    void createOneObject(int& SegID, const cowList<g3dVector>& vertexXYZ = cowList<g3dVector>(), const QString& color = "", bool isLine = 0);
    void deleteOneObject(const int& SegID);

    int GetmaxSegID()
    {
        return _maxSegID;
    };

    void deleteAllObjects();

private:
    void RebuildSymbolListMap();

    void ClearSymbolListMap()
    {
        symbolListMap.Clear();
        MarkStale();
    }

    void MarkStale()
    {
        staleSymbolListMap = true;
    }

    void MarkFresh()
    {
        staleSymbolListMap = false;
    }

    bool IsStale()
    {
        return staleSymbolListMap;
    }

    void SetTimeStamp(double ts)
    {
        timeStamp = ts;
    }

    void Draw(gdrRenderer& drafter, bool paintMode) const;
    void CreateSymbols(bool repopulate, const ftrPrimaryObject* po, const QString& instanceName, const gslMatrix& transform);

    bool showEOs;                      // 40
    bool staleSymbolListMap;           // 128
    cowListString highlightedInertias; // 48

    gruSymbolListMap symbolListMap; // 80
    double timeStamp;               // 136, 17
    omuAtom type_atom;              // 144

    cowList<cowList<g3dVector>> _geomVertexXYZs;
    cowListString _colors;
    cowListInt _segID;
    cowListInt _isLine;
    int _maxSegID;

    // display options data
    sesCDisplayOptions localDisplayOptions; // 152
};

#endif // KEFKLINE_H
