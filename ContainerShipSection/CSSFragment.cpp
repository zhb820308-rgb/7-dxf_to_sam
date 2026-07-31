#include "CSSFragment.h"

#include "SectionDraftHelper.h"

#include <QString>
#include <SAMMeshEditWork.h>
#include <pytInterpreterRole.h>
#include <cmdKCommandDeliveryRole.h>
#include <sesKSessionState.h>
#include <cowListInt.h>
#include <cowListFloat.h>
#include <cowListDouble.h>
#include <cowMapInt2Int.h>
#include <ptoKUtils.h>
#include <ptsKMeshFactory.h>
#include <utiCoordCont3D.h>

#include <bmeElementData.h>
#include <bmeNodeData.h>

#include <bmeElementClass.h>
#include <bmeMesh.h>
#include <bmgUtils.h>
#include <mesUtils.h>
#include <omeMesh.h>
#include <ptoKPart.h>
#include <shpShape.h>

#include <g3dVector.h>
#include <gdyEditor.h>
#include <gdyScene.h>
#include <kefKLine.h>

#include <g3dBoundingVolume.h>
#include <g3dCamera.h>
#include <ptsKScene.h>
#include <ptsKSceneManager.h>

#include <QtMath>
#include <basBasis.h>
#include <gslVector.h>
#include <cmdCWIP.h>

#include <QDebug>

static omuInterfaceObj::methodTable Css_fragment_methods[] = {
    {       "draftOuterHull",       (omuInterfaceObj::methodFunc)(&CSSFragment::draftOuterHull) },
    {          "draftCamber",          (omuInterfaceObj::methodFunc)(&CSSFragment::draftCamber) },
    {       "draftInnerHull",       (omuInterfaceObj::methodFunc)(&CSSFragment::draftInnerHull) },
    {          "draftGrider",          (omuInterfaceObj::methodFunc)(&CSSFragment::draftGrider) },
    {        "draftStringer",        (omuInterfaceObj::methodFunc)(&CSSFragment::draftStringer) },
    {    "draftHatchCoaming",    (omuInterfaceObj::methodFunc)(&CSSFragment::draftHatchCoaming) },
    {        "rollbackDraft",        (omuInterfaceObj::methodFunc)(&CSSFragment::rollbackDraft) },
    {           "clearDraft",           (omuInterfaceObj::methodFunc)(&CSSFragment::clearDraft) },
    { "draftToNodesAndBeams", (omuInterfaceObj::methodFunc)(&CSSFragment::draftToNodesAndBeams) },
    {        "mirrorSection",        (omuInterfaceObj::methodFunc)(&CSSFragment::mirrorSection) },
    {       "extrudeSection",       (omuInterfaceObj::methodFunc)(&CSSFragment::extrudeSection) },
    {        "refineSection",        (omuInterfaceObj::methodFunc)(&CSSFragment::refineSection) },
    {         "addStiffener",         (omuInterfaceObj::methodFunc)(&CSSFragment::addStiffener) },
    {         "fitDraftView",         (omuInterfaceObj::methodFunc)(&CSSFragment::fitDraftView) },
	{         "createContour",        (omuInterfaceObj::methodFunc)(&CSSFragment::createContour)},
	{         "clearContour",         (omuInterfaceObj::methodFunc)(&CSSFragment::clearContour) },
    {                      0,                                                                 0 }
};

static omuInterfaceObj::memberTable Css_fragment_members[] = {
    { 0, 0, 0 }
};

CSSFragment::CSSFragment(): ptsKPartFragment()
{
    // 注册派生类
    omuInterfaceObj::DescribeType("CSSFragment", Css_fragment_methods, Css_fragment_members);

    helper_ptr = new SectionDraftHelper();
}

CSSFragment::~CSSFragment()
{
    delete helper_ptr;
    helper_ptr = nullptr;
}

omuPrimitive* CSSFragment::Copy() const
{
    return new CSSFragment(*this);
}

omuPrimitive* CSSFragment::draftOuterHull(omuArguments& args)
{
    cowList<g3dVector> nodes = helper_ptr->connectAtOuterHull(args);

    DrawLine(nodes, 0, false, "purple");

    return nullptr;
}

omuPrimitive* CSSFragment::draftCamber(omuArguments& args)
{
    cowList<g3dVector> nodes = helper_ptr->connectAtCamber(args);

    DrawLine(nodes, 1, true, "orange");

    return nullptr;
}

omuPrimitive* CSSFragment::draftInnerHull(omuArguments& args)
{
    cowList<g3dVector> nodes = helper_ptr->connectAtInnerHull(args);

    DrawLine(nodes, 2, false, "purple");

    return nullptr;
}

omuPrimitive* CSSFragment::draftGrider(omuArguments& args)
{
    cowList<g3dVector> nodes = helper_ptr->connectAtGrider(args);
    cowList<g3dVector> nodes_for_outer_hull = helper_ptr->connectAtPreciseZOuterHull();

    DrawLine(nodes_for_outer_hull, 0, false, "purple");
    DrawLine(nodes, 3, true, "purple");

    return nullptr;
}

omuPrimitive* CSSFragment::draftStringer(omuArguments& args)
{
    cowList<g3dVector> nodes = helper_ptr->connectAtStringer(args);
    cowList<g3dVector> nodes_for_outer_hull = helper_ptr->connectAtPreciseYZOuterHull();

    DrawLine(nodes_for_outer_hull, 0, false, "purple");
    DrawLine(nodes, 4, true, "purple");

    return nullptr;
}

omuPrimitive* CSSFragment::draftHatchCoaming(omuArguments& args)
{
    cowList<g3dVector> nodes = helper_ptr->connectAtHatchCoaming(args);

    DrawLine(nodes, 5, false, "purple");

    return nullptr;
}

omuPrimitive* CSSFragment::rollbackDraft(omuArguments& args)
{
    int id;

    args.Begin();
    args.Get(id);
    args.End();

    ptsKSceneManager* scn_manager_ptr = ptsKSceneManager::TheInstance();
    ptsKScene* scn_ptr = scn_manager_ptr->GetNthScene(scn_manager_ptr->GetCurrentViewport());
    gdyEditor* editor_ptr = scn_ptr->Editor();
    kefKLine* line_editer_ptr = static_cast<kefKLine*>(editor_ptr->GetGeomEditorByType("kefKLine"));
    if(!line_editer_ptr)
    {
        editor_ptr->AddGeomEditor(new kefKLine);
        line_editer_ptr = static_cast<kefKLine*>(editor_ptr->GetGeomEditorByType("kefKLine"));
    }

    line_editer_ptr->deleteOneObject(id);
    scn_ptr->ExposeVP();

    return nullptr;
}

omuPrimitive* CSSFragment::clearDraft(omuArguments&)
{
    ptsKSceneManager* scn_manager_ptr = ptsKSceneManager::TheInstance();
    ptsKScene* scn_ptr = scn_manager_ptr->GetNthScene(scn_manager_ptr->GetCurrentViewport());
    gdyEditor* editor_ptr = scn_ptr->Editor();
    kefKLine* line_editer_ptr = static_cast<kefKLine*>(editor_ptr->GetGeomEditorByType("kefKLine"));
    if(!line_editer_ptr)
    {
        editor_ptr->AddGeomEditor(new kefKLine);
        line_editer_ptr = static_cast<kefKLine*>(editor_ptr->GetGeomEditorByType("kefKLine"));
    }

    line_editer_ptr->deleteAllObjects();
    scn_ptr->ExposeVP();

    return nullptr;
}

omuPrimitive* CSSFragment::draftToNodesAndBeams(omuArguments&)
{
    cowList<g3dVector> nodes;
    cowList<QPair<int, int>> connections;

    helper_ptr->calcNodesAndBeams(nodes, connections);
    clearDraft(omuArguments());
    createNodesAndBeams(nodes, connections);

    QString model_name = cmdKCommandDeliveryRole::Instance().CurrentModel();
    QString part_name = ConstGetPart()->GetName();
    cmd_target = model_name + ".parts['" + part_name + "']";
    pytInterpreterRole::Instance().RunCommand(
        cmd_target + ".mergeNodes(state=0, tolerance=0.005, keepHighLabel=False)", false);

    return nullptr;
}

omuPrimitive* CSSFragment::mirrorSection(omuArguments&)
{
    pytInterpreterRole::Instance().RunCommand(
        cmd_target + ".mirrorElement(" + cmd_target + ".elements, "
        "isCoordinatePlane=True, principalPlane=XZPLANE, offset=0.0)", false);
    pytInterpreterRole::Instance().RunCommand(
        cmd_target + ".mergeNodes(state=0, tolerance=0.005, keepHighLabel=False)", false);
    pytInterpreterRole::Instance().RunCommand("session.viewports['" +
        sesKSessionState::Instance()->GetCurrentName() +
        "'].view.fitView()", false);
    return nullptr;
}

omuPrimitive* CSSFragment::extrudeSection(omuArguments& args)
{
    double extrude_direction = 0.0;
    int extrude_cnt = 0;

    args.Begin();
    args.Get(extrude_direction, "extrude_direction");
    args.Get(extrude_cnt, "extrude_cnt");
    args.End();

    helper_ptr->saveExtrudeOptions(extrude_direction, extrude_cnt);

    pytInterpreterRole::Instance().RunCommand(
        QString("%1.extrude(region=regionToolset.Region("
            "face1Elements=%1.elements), type=LINETOSHELL, "
            "distance=(%2, 0.0, 0.0), copyNumbers=%3, deleteBeam=True)"
        )
            .arg(cmd_target).arg(extrude_direction).arg(extrude_cnt),
        false);
    pytInterpreterRole::Instance().RunCommand(
        cmd_target + ".mergeNodes(state=0, tolerance=0.005, keepHighLabel=False)", false);
    pytInterpreterRole::Instance().RunCommand(
        cmd_target + ".mergeElements(elements=" + cmd_target + ".elements)", false);
    pytInterpreterRole::Instance().RunCommand(
        "session.viewports['" +
        sesKSessionState::Instance()->GetCurrentName() + "'].view.fitView()", false);

    return nullptr;
}

omuPrimitive *CSSFragment::refineSection(omuArguments&)
{
    pytInterpreterRole::Instance().RunCommand(
        cmd_target + ".meshRefine(region=regionToolset.Region("
        "elements=" + cmd_target + ".elements),"
        "objectType=ELEMENTREFINE, refineType=DIMENSION, "
        "refinementSize=500.0)", false);
    pytInterpreterRole::Instance().RunCommand("session.viewports['" +
        sesKSessionState::Instance()->GetCurrentName() +
        "'].view.fitView()", false);

    return nullptr;
}

omuPrimitive* CSSFragment::addStiffener(omuArguments& args)
{
    bool need_mirror_section = true;
    QString profile_name;
    QString material_name;
    args.Begin();
    args.Get(need_mirror_section, "need_mirror_section");
    args.Get(profile_name, "profile_name");
    args.Get(material_name, "material_name");
    args.End();

    QString model_name = sesKSessionState::Instance()->GetModelName();
    QString part_name = sesKSessionState::Instance()->GetPartName();

    double interval = 1000.0;
    double angle = 0.0;

    double length = helper_ptr->getStiffenerLen();
    cowList<g3dVector> positions = helper_ptr->getStiffenerPos(need_mirror_section);

    for(int i = 0; i < positions.Length(); ++i)
    {
        double target_x = positions[i].GetX();
        double target_y = positions[i].GetY();
        double target_z = positions[i].GetZ();

        QString command = QString(
            "%1.BaseParametricStiffenedPlate("
            "XBarList='0,%2', YBarList='0,%3,%4', insertPoint='%5,%6,%7', "
            "startVector='%8,%9,%10', endVector='%11,%12,%13', angle=%14, "
            "includePlate=0, addBeamsOnTheSides=0, "
            "tableXNum=2, tableYNum=3, "
            "modelName='%15', partName='%16', "
            "thickness=0, "
            "beamXOrientation='0,1,0', beamYOrientation='-1,0,0', "
            "profileX='%17', profileY='%17', materialName='%18')"
        )
            .arg(cmd_target)
            .arg(length).arg(interval).arg(2 * interval)
            .arg(target_x).arg(target_y - interval).arg(target_z)
            .arg(0).arg(0).arg(0)
            .arg(0).arg(0).arg(0)
            .arg(angle)
            .arg(model_name).arg(part_name)
            .arg(profile_name).arg(material_name);

        // qDebug() << "command: " << command;

        pytInterpreterRole::Instance().RunCommand(command, false);
    }
    pytInterpreterRole::Instance().RunCommand(
        cmd_target + ".clearFreeNodes()", false);
    pytInterpreterRole::Instance().RunCommand(
        cmd_target + ".mergeNodes(state=0, tolerance=0.005, keepHighLabel=False)", false);
    pytInterpreterRole::Instance().RunCommand(
        cmd_target + ".mergeElements(elements=" + cmd_target + ".elements)", false);

    return nullptr;
}

omuPrimitive* CSSFragment::fitDraftView(omuArguments& args)
{
    QString step;
    args.Begin();
    args.Get(step, "step");
    args.End();

    double bounding_box_width = 0.0, bounding_box_height = 0.0;
    helper_ptr->draftBoundingBox(step, bounding_box_width, bounding_box_height);

    // qDebug() << "bounding_box_width: " << bounding_box_width
    //          << ", bounding_box_height: " << bounding_box_height;

    double half_diagonal =
        qSqrt(bounding_box_width * bounding_box_width + bounding_box_height * bounding_box_height) / 2;

    QString command = QString(
        "session.viewports['" + sesKSessionState::Instance()->GetCurrentName() + "'].view.setValues("
        "nearPlane=%1, farPlane=%2, "
        "width=%3, height=%4, "
        "cameraPosition=(%5, %6, %7), cameraUpVector=(%8, %9, %10), cameraTarget=(%11, %12, %13), "
        "viewOffsetX=0, viewOffsetY=0)"
    )
        .arg(8 * half_diagonal).arg(10 * half_diagonal)
        .arg(bounding_box_width).arg(bounding_box_height)
        .arg(-9 * half_diagonal).arg(bounding_box_width / 2).arg(bounding_box_height / 2)
        .arg(0).arg(0).arg(1)
        .arg(0).arg(bounding_box_width / 2).arg(bounding_box_height / 2);

    // qDebug() << "command: " << command;

    // cmdGCommandDeliveryRole::Instance().SendCommand(command);
    pytInterpreterRole::Instance().RunCommand(command, false);

    return nullptr;
}

void CSSFragment::createNodesAndBeams(const cowList<g3dVector>& nodes,
    const cowList<QPair<int, int>>& connections)
{
    utiCoordCont3D node_container = utiCoordCont3D();
    cowListInt node_labels;
    bmeElementClass** elem_class_ptr2 = nullptr;
    int prev_node_cnt = 0, elem_class_cnt = 1;
    int node_next_label = 1, elem_next_label = 1;

    ftrFeatureList* feature_list_ptr = GetPart()->GetFeatureList();
    const bmeMesh* current_mesh_ptr = feature_list_ptr->ConstGetMesh(bdoDefaultInstId);
    if(!current_mesh_ptr)
    {
        elem_class_ptr2 = (bmeElementClass**)malloc(sizeof(bmeElementClass*) * 1);
    }
    else
    {
        const bmeNodeData& prev_node_data = current_mesh_ptr->NodeData();
        const bmeElementData& prev_elem_data = current_mesh_ptr->ElementData();

        prev_node_cnt = prev_node_data.NumNodes();
        elem_class_cnt = prev_elem_data.NumClasses() + 1;
        node_next_label = prev_node_data.NextLabel();
        elem_next_label = prev_elem_data.GetNextAvailableLabel();

        // 添加原有节点
        node_container.Append(prev_node_data.CoordContainer());
        prev_node_data.GetUserNodeLabels(node_labels);
        // 添加原有图元
        elem_class_ptr2 = (bmeElementClass**)malloc(sizeof(bmeElementClass*) * elem_class_cnt);
        for(int i = 0; i < elem_class_cnt - 1; ++i)
        {
            elem_class_ptr2[i] = bmeElementClass::ConstructObject(prev_elem_data.GetClass(i));
        }
    }

    // 添加新节点
    for(int i = 0; i < nodes.Length(); ++i)
    {
        node_container.Append(nodes[i].GetX(), nodes[i].GetY(), nodes[i].GetZ());
        node_labels.Append(node_next_label++);
    }
    // 创建新元素
    int* connections_ptr = (int*)malloc(sizeof(int) * connections.Length() * 2);
    int* new_elem_labels = (int*)malloc(sizeof(int) * connections.Length());
    for(int i = 0; i < connections.Length(); ++i)
    {
        connections_ptr[2 * i] = prev_node_cnt + connections[i].first;
        connections_ptr[2 * i + 1] = prev_node_cnt + connections[i].second;
        new_elem_labels[i] = elem_next_label++;
    }
    // 添加新元素
    elem_class_ptr2[elem_class_cnt - 1] = bmeElementClass::
        ConstructObject(connections.Length(), "B31", connections_ptr);
    elem_class_ptr2[elem_class_cnt - 1]->SetUserElementLabel(new_elem_labels);

    // 重新生成网格
    bmeMesh* new_mesh_ptr = new omeMesh(bdoDefaultInstId,
        node_container.NumCoord(), node_container,
        elem_class_cnt, elem_class_ptr2);
    new_mesh_ptr->GetNodeData().SetUserNodeLabels(node_labels);
    mesSetMesh(*feature_list_ptr, bdoDefaultInstId, new_mesh_ptr);
}

void CSSFragment::DrawLine(
    const cowList<g3dVector>& nodes, int id, bool is_single_line, const QString& color)
{
    // qDebug() << "CSSFragment::DrawLine() called";

    ptsKSceneManager* scn_manager_ptr = ptsKSceneManager::TheInstance();
    ptsKScene* scn_ptr = scn_manager_ptr->GetNthScene(scn_manager_ptr->GetCurrentViewport());
    gdyEditor* editor_ptr = scn_ptr->Editor();
    kefKLine* line_editer_ptr = static_cast<kefKLine*>(editor_ptr->GetGeomEditorByType("kefKLine"));
    if(!line_editer_ptr)
    {
        editor_ptr->AddGeomEditor(new kefKLine);
        line_editer_ptr = static_cast<kefKLine*>(editor_ptr->GetGeomEditorByType("kefKLine"));
    }

    line_editer_ptr->createOneObject(id, nodes, color, is_single_line);
    scn_ptr->ExposeVP();
}

omuPrimitive* CSSFragment::createContour(omuArguments&args)
{
	cowListDouble ValueList;

	args.Begin();
	args.Optional();//后面的参数可以缺省
	args.Get(ValueList, "Value");
	args.End();

	cowListInt ElemId;
	ElemId.Append(1);
	ElemId.Append(2);
	ElemId.Append(3);
	ElemId.Append(4);
	ElemId.Append(5);


	//当单元是参数时，错误提示信息，此信息打印在SAM message窗口
	if (!ElemId.Length())
	{
		cmdCWIP::Instance().Warning(QString("Elements error."));
		return 0;
	}

	//一个单元对应一个数值
	cowListFloat elementValue;

	for (int i = 0; i < ElemId.Length(); i++)
	{
		//测试用，value的值等于单元Id   ValueList[i]
		float tempValue = ElemId[i];

		elementValue.Append(tempValue);
	}
	ftrFeatureList* fl = GetPart()->GetFeatureList();
	SAMMeshEditWork work(fl);
	//work.CreateFringe(ElemId, elementValue);

	return 0;
}

omuPrimitive* CSSFragment::clearContour(omuArguments&)
{
	ftrFeatureList* fl = GetPart()->GetFeatureList();
	SAMMeshEditWork work(fl);
	//work.clearContour();

	return 0;
}
