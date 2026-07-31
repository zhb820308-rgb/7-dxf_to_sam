#include <kefKLine.h>
#include <kefKSymbolFactory.h>

#include <sesKSessionState.h>

#include <gslMatrix.h>

#include <kefCEngineeringFeatures.h>

#include <gdrRenderer.h>

#include <dgpKScene.h>
#include <g3dMarker.h>
#include <g3dPoint.h>
#include <gdyDisplayRep.h>
#include <gdyEditor.h>
#include <gdyScene.h>

#include <smgSceneManagerRole.h>

#include <ftrEvaluatedRep.h>
#include <ftrFeature.h>
#include <ftrPrimaryObject.h>

#include <nexDynamicCast.h>

kefKLine::kefKLine():
    gdyNonPageGeomEditor(),
    showEOs(false),
    staleSymbolListMap(false),
    timeStamp(0.0),
    type_atom("kefKLine"),
    _geomVertexXYZs(cowList<cowList<g3dVector>>()),
    _colors(cowListString()),
    _segID(cowListInt()),
    _maxSegID(0)
{
    // do nothing
}

kefKLine::~kefKLine()
{
    // do nothing
}

gdyGeomEditor* kefKLine::Copy() const
{
    return new kefKLine(*this);
}

void kefKLine::Load(uint, const gdyDisplayRep& r, g3dSphere* extent_out)
{
    timeStamp = 0.0;
}

void kefKLine::Unload()
{
    timeStamp = 0.0;
}

void kefKLine::Draft(gdrRenderer& r) const
{
    Draw(r, false);
}

void kefKLine::Paint(gdrRenderer& r) const
{
    Draw(r, true);
}

void kefKLine::ASelect(gdrASelector& as) const
{
    // do nothing
}

void kefKLine::Highlight(gdrRenderer& r) const
{
    // todo
}

void kefKLine::SetHighlightedInertia(const QString& springName)
{
    if(!springName.isEmpty())
    {
        highlightedInertias.Append(springName);
    }
}

void kefKLine::HighlightInertia(const QString& springName)
{
    // todo
}

void kefKLine::UnhighlightAll()
{
    // todo
}

void kefKLine::UpdateDisplayOptions(const sesCDisplayOptions& options)
{ }

void kefKLine::setObject(const cowList<cowList<g3dVector>>& vertexXYZ, const cowListString& color)
{
    if(vertexXYZ.Length() != color.Length())
    {
        return;
    }

    for(int i = 0; i < vertexXYZ.Length(); i++)
    {
        _geomVertexXYZs.Append(vertexXYZ[i]);
        _colors.Append(color[i]);
        _segID.Append(_maxSegID++);
    }
}

void kefKLine::addObject(const cowList<cowList<g3dVector>>& vertexXYZ, const cowListString& color)
{
    if(vertexXYZ.Length() != color.Length())
    {
        return;
    }

    for(int i = 0; i < vertexXYZ.Length(); i++)
    {
        _geomVertexXYZs.Append(vertexXYZ[i]);
        _colors.Append(color[i]);
        _segID.Append(_maxSegID++);
    }
}

void kefKLine::createOneObject(int& SegID, const cowList<g3dVector>& vertexXYZ, const QString& color, bool isLine)
{
    int objectIndex = -1;
    if(!_segID.IsEmpty())
    {
        objectIndex = _segID.FindMember(SegID);
    }

    // modify a object
    if(objectIndex >= 0)
    {
        _geomVertexXYZs.Get(objectIndex) = vertexXYZ;
        _colors.Get(objectIndex) = color;
        if(isLine)
        {
            _isLine.Get(objectIndex) = 1;
        }
        else
        {
            _isLine.Get(objectIndex) = 0;
        }
    }
    // create a new object
    else
    {
        _geomVertexXYZs.Append(vertexXYZ);
        _colors.Append(color);
        _maxSegID++;
        if(isLine)
        {
            _isLine.Append(1);
        }
        else
        {
            _isLine.Append(0);
        }
        _segID.Append(SegID);
    }
}

void kefKLine::deleteOneObject(const int& SegID)
{
    int objectIndex = -1;
    objectIndex = _segID.FindMember(SegID);
    if(objectIndex < 0)
    {
        return;
    }
    _geomVertexXYZs.RemoveIndex(objectIndex);
    _colors.RemoveIndex(objectIndex);
    _segID.RemoveIndex(objectIndex);
    _isLine.RemoveIndex(objectIndex);
}

void kefKLine::deleteAllObjects()
{
    _geomVertexXYZs.Clear();
    _colors.Clear();
    _segID.Clear();
    _isLine.Clear();
}

void kefKLine::RebuildSymbolListMap()
{ }

void kefKLine::Draw(gdrRenderer& drafter, bool paintMode) const
{
    drafter.DepthTest(false);

    g3dPoint pnt;
    g3dVector normal(1.0f, 0.0f, 1.0f);
    drafter.Normal(normal);

    for(int i = 0; i < _geomVertexXYZs.Length(); i++)
    {
        if(_isLine[i] == 1)
        {
            drafter.Begin(g3d_Lines);
        }
        else if(_isLine[i] == 0)
        {
            drafter.Begin(g3d_LineStrip);
        }
        if(_colors[i] == "purple")
        {
            g3dColor sColor(0.529f, 0.0f, 1.0f, 1.0f);
            drafter.Color(sColor);
        }
        else if(_colors[i] == "orange")
        {
            g3dColor sColor(0.85f, 0.34f, 0.0f, 1.0);
            drafter.Color(sColor);
        }

        for(int j = 0; j < _geomVertexXYZs[i].Length(); j++)
        {
            pnt.Set(_geomVertexXYZs[i][j].GetX(), _geomVertexXYZs[i][j].GetY(),
                    _geomVertexXYZs[i][j].GetZ(), 1.0f);
            drafter.Vertex(pnt);
        }

        drafter.End();
    }
}

void kefKLine::CreateSymbols(bool repopulate, const ftrPrimaryObject* po, const QString& instanceName, const gslMatrix& transform)
{ }

