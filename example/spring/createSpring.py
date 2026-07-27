# -*- coding: utf-8 -*-
from sam import *
from samConstants import *

# ===================== 从CSV文件创建接地弹簧（在指定Set内） =====================
def create_ground_springs_from_csv_in_set(model_name, part_name, set_name, csv_file_path, z1=-1.0, z2=1.0):
    """
    从CSV文件批量创建接地弹簧，在指定的Set内进行节点搜索
    
    参数:
    - model_name: 模型名称
    - part_name: 部件名称
    - set_name: 要在其中搜索节点的Set名称
    - csv_file_path: CSV文件路径
    - z1: 包围盒下界Z坐标
    - z2: 包围盒上界Z坐标
    """
    
    print("="*60)
    print("在指定Set内批量创建接地弹簧")
    print("="*60)
    print("Set名称: %s" % set_name)
    print("Z范围: %.2f 到 %.2f" % (z1, z2))
    
    try:
        # 获取模型、部件和Set
        model = mdb.models[model_name]
        part = model.parts[part_name]
        print("成功获取部件: %s" % part_name)
        
        # 获取指定的Set
        if set_name not in part.sets:
            print("错误: 找不到Set - %s" % set_name)
            return False
        
        target_set = part.sets[set_name]
        print("成功获取Set: %s" % set_name)
        
        # 获取Set中的节点
        set_nodes = target_set.nodes
        print("Set中包含 %d 个节点" % len(set_nodes))
        
    except KeyError as e:
        print("错误: 找不到模型、部件或Set - %s" % e)
        return False
    except Exception as e:
        print("错误: 获取Set失败 - %s" % e)
        return False
    
    # 读取CSV文件
    try:
        with open(csv_file_path, 'r') as f:
            lines = f.readlines()
        print("成功读取CSV文件: %s" % csv_file_path)
        print("读取到 %d 行数据" % len(lines))
    except Exception as e:
        print("读取CSV文件失败: %s" % e)
        return False
    
    # 为每个包围盒创建Set和弹簧
    spring_count = 0
    error_count = 0
    
    for i, line in enumerate(lines):
        # 跳过空行和注释行
        if not line.strip() or line.strip().startswith('#'):
            continue
        
        print("\n" + "="*50)
        print("处理第 %d 行" % (i+1))
        print("-"*50)
        
        # 解析数据
        try:
            parts = [p.strip() for p in line.split(',')]
            if len(parts) < 5:
                print("警告: 数据格式不正确，跳过")
                continue
            
            # 提取数据
            center_x = float(parts[0])
            center_y = float(parts[1])
            length_x = float(parts[2])
            length_y = float(parts[3])
            total_stiffness = float(parts[4])  # 总刚度
            
            print("包围盒定义:")
            print("  中点: (%.2f, %.2f)" % (center_x, center_y))
            print("  尺寸: %.2f × %.2f" % (length_x, length_y))
            print("  总刚度: %.2f" % total_stiffness)
            
            # 计算包围盒坐标
            x1 = center_x - length_x / 2.0
            y1 = center_y - length_y / 2.0
            x2 = center_x + length_x / 2.0
            y2 = center_y + length_y / 2.0
            
            # 确保x1 < x2, y1 < y2
            if x1 > x2:
                x1, x2 = x2, x1
            if y1 > y2:
                y1, y2 = y2, y1
            
        except ValueError as e:
            print("错误: 数据格式错误，跳过: %s" % e)
            error_count += 1
            continue
        except Exception as e:
            print("错误: 解析数据时出错: %s" % e)
            error_count += 1
            continue
        
        # 在指定Set内搜索节点
        try:
            # 使用SAM内置的getByBoundingBox方法
            nodes_in_box = part.nodes.getByBoundingBox(xMin=x1, yMin=y1, zMin=z1, 
                                                       xMax=x2, yMax=y2, zMax=z2)
            
            # 筛选出在目标Set中的节点
            filtered_nodes = []
            for node in nodes_in_box:
                # 检查节点是否在目标Set中
                if node in set_nodes:
                    filtered_nodes.append(node)
            
            node_count = len(filtered_nodes)
            print("在Set内找到节点数: %d" % node_count)
            
            if node_count == 0:
                print("警告: 该包围盒内没有找到节点，跳过")
                continue
            
        except Exception as e:
            print("获取节点失败: %s" % e)
            error_count += 1
            continue
        
        # 计算每个节点的弹簧刚度
        node_stiffness = total_stiffness / node_count
        print("每个节点的弹簧刚度: %.2f" % node_stiffness)
        
        # 创建Set名称：Set-spring-行数-弹簧刚度
        new_set_name = "Set-spring-%d-%.2f" % (i+1, node_stiffness)
        new_set_name = new_set_name.replace('.', '_')  # 替换点号为下划线
        
        try:
            # 如果Set已存在，先删除
            if new_set_name in part.sets:
                del part.sets[new_set_name]
                print("删除已存在的Set: %s" % new_set_name)
            
            # 收集节点标签
            node_labels = []
            for node in filtered_nodes:
                node_labels.append(node.label)
            
            # 使用节点标签创建节点序列
            node_sequence = part.nodes.sequenceFromLabels(labels=tuple(node_labels))
            
            # 创建Set
            part.Set(nodes=node_sequence, name=new_set_name)
            print("创建Set: %s" % new_set_name)
            
        except Exception as e:
            print("创建Set失败: %s" % e)
            error_count += 1
            continue
        
        # 创建弹簧
        spring_name = "Spring-%d-%.2f" % (i+1, node_stiffness)
        spring_name = spring_name.replace('.', '_')  # 替换点号为下划线
        
        try:
            # 确保Set存在
            if new_set_name not in part.sets:
                print("错误: Set不存在，无法创建弹簧")
                error_count += 1
                continue
                
            region_set = part.sets[new_set_name]
            
            
            # 创建弹簧
            part.engineeringFeatures.SpringDashpotToGround(
                name=spring_name,
                region=region_set,
                dof=3,  # Z方向
                springBehavior=ON,
                springStiffness=node_stiffness,  # 使用计算后的单个节点刚度
                dashpotBehavior=OFF,
                dashpotCoefficient=0.0
            )
            print("创建弹簧: %s" % spring_name)
            spring_count += 1
            
        except Exception as e:
            print("创建弹簧失败: %s" % e)
            error_count += 1
            continue
    
    print("\n" + "="*60)
    print("接地弹簧创建完成")
    print("="*60)
    print("总结:")
    print("  成功创建弹簧: %d 个" % spring_count)
    print("  发生错误: %d 个" % error_count)
    
    return spring_count > 0


# ===================== 主程序 =====================
if __name__ == "__main__":
    # 配置参数
    model_name = "180k-dry"          # 模型名称
    part_name = "NAS-180k-dry"       # 部件名称
    set_name = "Set-shell"                # 要在其中搜索的Set名称
    csv_file_path = "springNode.csv"  # CSV文件路径
    z1 = -10.0                        # 下界Z坐标
    z2 = 10.0                        # 上界Z坐标
    
    # 调用函数创建接地弹簧
    try:
        success = create_ground_springs_from_csv_in_set(
            model_name, part_name, set_name, csv_file_path, z1, z2
        )
        
        if not success:
            print("\n处理终止，有错误。")
        else:
            print("\n处理成功完成。")
            
    except Exception as e:
        print("\n脚本执行过程中发生致命错误:")
        print("错误类型: %s" % type(e).__name__)
        print("错误信息: %s" % str(e))
        print("脚本执行终止。")