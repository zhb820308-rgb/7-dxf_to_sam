#include "SectionDraftHelper.h"

#include "GeomCalcUtils.h"
#include <cowList.T>

cowList<g3dVector> SectionDraftHelper::connectAtOuterHull(omuArguments& args)
{
    args.Begin();
    args.Get(half_breadth, "half_breadth");
    args.Get(depth, "depth");
    args.Get(bilge_radius, "bilge_radius");
    args.Get(deck_corner_radius, "deck_corner_radius");
    args.End();

    cowList<g3dVector> nodes;
    nodes.Append(g3dVector(0.0, half_breadth, depth));
    nodes.Append(g3dVector(0.0, half_breadth, bilge_radius));
    nodes.Concatenate(calcArcInnerPoints(half_breadth, bilge_radius));
    nodes.Append(g3dVector(0.0, half_breadth - bilge_radius, 0.0));
    nodes.Append(g3dVector(0.0, 0.0, 0.0));

    return nodes;
}

cowList<g3dVector> SectionDraftHelper::connectAtCamber(omuArguments& args)
{
    args.Begin();
    args.Get(camber_height, "camber_height");
    args.Get(deck_distance, "deck_distance");
    args.End();

    cowList<g3dVector> nodes;
    nodes.Append(g3dVector(0.0, half_breadth, depth));
    nodes.Append(g3dVector(0.0, deck_distance, depth + camber_height));

    return nodes;
}

cowList<g3dVector> SectionDraftHelper::connectAtInnerHull(omuArguments& args)
{
    args.Begin();
    args.Get(inner_hull_height, "inner_hull_height");
    args.Get(inner_hull_distance, "inner_hull_distance");
    args.End();

    cowList<g3dVector> nodes;
    nodes.Append(g3dVector(0.0, inner_hull_distance, depth));
    nodes.Append(g3dVector(0.0, inner_hull_distance, inner_hull_height));
    nodes.Append(g3dVector(0.0, 0.0, inner_hull_height));

    return nodes;
}

cowList<g3dVector> SectionDraftHelper::connectAtGrider(omuArguments& args)
{
    args.Begin();
    args.Get(grider_distances, "grider_distances");
    args.End();

    cowList<g3dVector> nodes;
    for(int i = 0; i < grider_distances.Length() - 1; ++i)
    {
        nodes.Append(g3dVector(0.0, grider_distances[i], 0.0));
        nodes.Append(g3dVector(0.0, grider_distances[i], inner_hull_height));
    }

    double special_z = calcSpecialZ(half_breadth, bilge_radius, inner_hull_distance);
    nodes.Append(g3dVector(0.0, inner_hull_distance, special_z));
    nodes.Append(g3dVector(0.0, inner_hull_distance, inner_hull_height));

    return nodes;
}

cowList<g3dVector> SectionDraftHelper::connectAtStringer(omuArguments& args)
{
    args.Begin();
    args.Get(stringer_distances, "stringer_distances");
    args.End();

    cowList<g3dVector> nodes;
    double special_y = calcSpecialY(half_breadth, bilge_radius, inner_hull_height);
    nodes.Append(g3dVector(0.0, special_y, inner_hull_height));
    nodes.Append(g3dVector(0.0, inner_hull_distance, inner_hull_height));

    for(int i = 1; i < stringer_distances.Length(); ++i)
    {
        nodes.Append(g3dVector(0.0, half_breadth, stringer_distances[i]));
        nodes.Append(g3dVector(0.0, inner_hull_distance, stringer_distances[i]));
    }

    return nodes;
}

cowList<g3dVector> SectionDraftHelper::connectAtHatchCoaming(omuArguments& args)
{
    args.Begin();
    args.Get(hatch_coaming_height, "hatch_coaming_height");
    args.Get(hatch_coaming_width, "hatch_coaming_width");
    args.End();

    cowList<g3dVector> nodes;
    nodes.Append(g3dVector(0.0, inner_hull_distance, depth));
    nodes.Append(g3dVector(0.0, inner_hull_distance, depth + hatch_coaming_height));
    nodes.Append(g3dVector(0.0, inner_hull_distance + hatch_coaming_width, depth + hatch_coaming_height));

    return nodes;
}

cowList<g3dVector> SectionDraftHelper::connectAtPreciseZOuterHull() const
{
    cowList<g3dVector> nodes;
    nodes.Append(g3dVector(0.0, half_breadth, depth));
    nodes.Append(g3dVector(0.0, half_breadth, bilge_radius));
    nodes.Concatenate(calcArcInnerPoints(half_breadth, bilge_radius));
    nodes.Append(g3dVector(0.0, half_breadth - bilge_radius, 0.0));
    nodes.Append(g3dVector(0.0, 0.0, 0.0));

    int special_z_idx = -1;
    for(int i = 0; i < nodes.Length() - 1; ++i)
    {
        if(nodes[i].GetY() >= inner_hull_distance && nodes[i + 1].GetY() <= inner_hull_distance)
        {
            special_z_idx = i + 1;
            break;
        }
    }
    double special_z = calcSpecialZ(half_breadth, bilge_radius, inner_hull_distance);
    nodes.InsertIndex(special_z_idx, g3dVector(0.0, inner_hull_distance, special_z));
    return nodes;
}

cowList<g3dVector> SectionDraftHelper::connectAtPreciseYZOuterHull() const
{
    cowList<g3dVector> nodes;
    nodes.Append(g3dVector(0.0, half_breadth, depth));
    nodes.Append(g3dVector(0.0, half_breadth, bilge_radius));
    nodes.Concatenate(calcArcInnerPoints(half_breadth, bilge_radius));
    nodes.Append(g3dVector(0.0, half_breadth - bilge_radius, 0.0));
    nodes.Append(g3dVector(0.0, 0.0, 0.0));

    int special_y_idx = -1, special_z_idx = -1;
    for(int i = 0; i < nodes.Length() - 1; ++i)
    {
        if(nodes[i].GetZ() >= inner_hull_height && nodes[i + 1].GetZ() <= inner_hull_height)
        {
            special_y_idx = i + 1;
        }
        if(nodes[i].GetY() >= inner_hull_distance && nodes[i + 1].GetY() <= inner_hull_distance)
        {
            special_z_idx = i + 1;
            break;
        }
    }
    double special_y = calcSpecialY(half_breadth, bilge_radius, inner_hull_height);
    nodes.InsertIndex(special_y_idx, g3dVector(0.0, special_y, inner_hull_height));
    double special_z = calcSpecialZ(half_breadth, bilge_radius, inner_hull_distance);
    nodes.InsertIndex(++special_z_idx, g3dVector(0.0, inner_hull_distance, special_z));

    return nodes;
}

void SectionDraftHelper::calcNodesAndBeams(
    cowList<g3dVector>& nodes, cowList<QPair<int, int>>& connections) const
{
    const int arc_segmentation = 3;
    const int griders_cnt = grider_distances.Length();
    const int stringers_cnt = stringer_distances.Length();
    // 外壳节点
    nodes.Append(g3dVector(0, half_breadth, depth));
    for(int i = stringers_cnt - 2; i >= 1; --i)
    {
        nodes.Append(g3dVector(0, half_breadth, stringer_distances[i]));
    }
    // 舭部节点
    nodes.Append(g3dVector(0, half_breadth, bilge_radius));
    nodes.Concatenate(calcArcInnerPoints(half_breadth, bilge_radius, arc_segmentation));
    nodes.Append(g3dVector(0, half_breadth - bilge_radius, 0));
    // 外底节点
    for(int i = griders_cnt - 2; i >= 0; --i)
    {
        nodes.Append(g3dVector(0, grider_distances[i], 0));
    }
    // 特殊两点
    int special_y_idx = -1, special_z_idx = -1;
    for(int i = stringers_cnt - 2; i <= stringers_cnt + arc_segmentation - 1; ++i)
    {
        if(nodes[i].GetZ() >= inner_hull_height && nodes[i + 1].GetZ() <= inner_hull_height)
        {
            special_y_idx = i + 1;
        }
        if(nodes[i].GetY() >= inner_hull_distance && nodes[i + 1].GetY() <= inner_hull_distance)
        {
            special_z_idx = i + 1;
            break;
        }
    }
    double special_y = calcSpecialY(half_breadth, bilge_radius, inner_hull_height);
    nodes.InsertIndex(special_y_idx, g3dVector(0.0, special_y, inner_hull_height));
    double special_z = calcSpecialZ(half_breadth, bilge_radius, inner_hull_distance);
    nodes.InsertIndex(++special_z_idx, g3dVector(0.0, inner_hull_distance, special_z));
    // 内底节点
    for(int i = 0; i <= griders_cnt - 2; ++i)
    {
        nodes.Append(g3dVector(0, grider_distances[i], inner_hull_height));
    }
    // 内壳节点
    for(int i = 0; i < stringers_cnt - 1; ++i)
    {
        nodes.Append(g3dVector(0, inner_hull_distance, stringer_distances[i]));
    }
    nodes.Append(g3dVector(0, inner_hull_distance, depth));

    const int nodes_cnt = nodes.Length();
    // 主结构连接
    for(int i = 0; i <= nodes_cnt - 2; ++i)
    {
        connections.Append(QPair<int, int>(i, i + 1));
    }
    connections.Append(QPair<int, int>(nodes_cnt - 1, 0));
    // 双壳纵桁
    for(int i = 1; i <= stringers_cnt - 2; ++i)
    {
        connections.Append(QPair<int, int>(i, nodes_cnt - 1 - i));
    }
    // 双层底纵桁
    for(int i = 0; i <= griders_cnt - 3; ++i)
    {
        connections.Append(QPair<int, int>(
            stringers_cnt + arc_segmentation + 2 + i, nodes_cnt - stringers_cnt - 1 - i));
    }
    // 特殊两点连接
    for(int i = stringers_cnt - 2; i <= stringers_cnt + arc_segmentation + 2; ++i)
    {
        if(qFuzzyCompare(static_cast<double>(nodes[i].GetZ()), inner_hull_height))
        {
            connections.Append(QPair<int, int>(i, nodes_cnt - stringers_cnt));
        }
        else if(qFuzzyCompare(static_cast<double>(nodes[i].GetY()), inner_hull_distance))
        {
            connections.Append(QPair<int, int>(i, nodes_cnt - stringers_cnt));
            break;
        }
    }

    // 舱口围
    nodes.Append(g3dVector(0, inner_hull_distance, depth + hatch_coaming_height));
    nodes.Append(g3dVector(0, inner_hull_distance + hatch_coaming_width, depth + hatch_coaming_height));
    const int new_nodes_cnt = nodes.Length();
    connections.Append(QPair<int, int>(new_nodes_cnt - 3, new_nodes_cnt - 2));
    connections.Append(QPair<int, int>(new_nodes_cnt - 2, new_nodes_cnt - 1));
}

void SectionDraftHelper::draftBoundingBox(
    const QString& step, double& bounding_box_width, double& bounding_box_height) const
{
    if(step == "hatch_coaming")
    {
        bounding_box_width = qMax(half_breadth, 0.0);
        bounding_box_height = qMax(depth, 0.0) + qMax(hatch_coaming_height, 0.0);
        return;
    }
    else
    {
        bounding_box_width = qMax(half_breadth, 0.0);
        bounding_box_height = qMax(depth, 0.0);
        return;
    }
}

void SectionDraftHelper::saveExtrudeOptions(double direction, int count)
{
    extrude_direction = direction;
    extrude_cnt = count;
}

double SectionDraftHelper::getStiffenerLen() const
{
    return qAbs(extrude_direction) * extrude_cnt;
}

cowList<g3dVector> SectionDraftHelper::getStiffenerPos(bool need_mirror_section) const
{
    const double epsilon = 500;
    cowList<g3dVector> positions;

    double stiffener_x = 0.0;
    if(extrude_direction < 0)
    {
        stiffener_x = extrude_direction * extrude_cnt;
    }

    for(int i = 1; i < grider_distances.Length() - 1; ++i)
    {
        double a = grider_distances[i - 1];
        double b = grider_distances[i];
        double delta = b - a;
        double abs_delta = qAbs(delta);

        if(abs_delta >= epsilon)
        {
            int k_max = qFloor(abs_delta / epsilon);
            double step = delta / k_max;
            for(int k = 1; k < k_max; ++k)
            {
                positions.Append(g3dVector(stiffener_x, a + k * step, 0));
            }
        }
    }

    if(need_mirror_section)
    {
        int original_length = positions.Length();
        for(int i = 0; i < original_length; ++i)
        {
            const g3dVector& pos = positions[i];
            positions.Append(g3dVector(pos.GetX(), -pos.GetY(), pos.GetZ()));
        }
    }

    return positions;
}
