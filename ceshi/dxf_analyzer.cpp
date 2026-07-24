#include "libdxfrw.h"
#include "drw_interface.h"
#include <iostream>
#include <iomanip>
#include <fstream>
#include <string>
#include <vector>
#include <sstream>
#include <algorithm>
#include <cmath>

// ==================== 格式辅助函数 ====================
std::string trim(const std::string& str) {
    size_t first = str.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) return "";
    size_t last = str.find_last_not_of(" \t\r\n");
    return str.substr(first, last - first + 1);
}

std::string safeCoord(double val) {
    if (std::isnan(val) || std::isinf(val)) return "0.0";
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(4) << val;
    return oss.str();
}

// ==================== 数据收集器 ====================
struct DataCollector {
    // 统计信息
    struct EntityStats {
        int pointCount = 0;
        int lineCount = 0;
        int arcCount = 0;
        int circleCount = 0;
        int ellipseCount = 0;
        int lwPolylineCount = 0;
        int polylineCount = 0;
        int splineCount = 0;
        int textCount = 0;
        int mTextCount = 0;
        int insertCount = 0;
        int hatchCount = 0;
        int solidCount = 0;
        int traceCount = 0;
        int dimCount = 0;
        int leaderCount = 0;
        int rayCount = 0;
        int xlineCount = 0;
        int blockCount = 0;
        int viewportCount = 0;
        int imageCount = 0;
        int total = 0;

        void calcTotal() {
            total = pointCount + lineCount + arcCount + circleCount + ellipseCount
                  + lwPolylineCount + polylineCount + splineCount + textCount
                  + mTextCount + insertCount + hatchCount + solidCount + traceCount
                  + dimCount + leaderCount + rayCount + xlineCount + blockCount
                  + viewportCount + imageCount;
        }
    } stats;

    // 按类别存储详细数据
    std::vector<std::string> pointData;
    std::vector<std::string> lineData;
    std::vector<std::string> arcData;
    std::vector<std::string> circleData;
    std::vector<std::string> ellipseData;
    std::vector<std::string> lwPolylineData;
    std::vector<std::string> polylineData;
    std::vector<std::string> splineData;
    std::vector<std::string> textData;
    std::vector<std::string> mTextData;
    std::vector<std::string> insertData;
    std::vector<std::string> hatchData;
    std::vector<std::string> dimData;
    std::vector<std::string> leaderData;

    // 表数据
    std::vector<std::string> layerData;
    std::vector<std::string> linetypeData;
    std::vector<std::string> textstyleData;
    std::vector<std::string> blockData;
    std::vector<std::string> headerData;

    void writeTxt(const std::string& outputFile) {
        std::ofstream f(outputFile);
        if (!f.is_open()) {
            std::cerr << "ERROR: Cannot create output file: " << outputFile << std::endl;
            return;
        }

        stats.calcTotal();

        f << "==========================================" << std::endl;
        f << "  DXF File Analysis Report" << std::endl;
        f << "==========================================" << std::endl;
        f << std::endl;

        // ==== Overview ====
        f << "=== 1. OVERVIEW ===" << std::endl;
        f << "Total entities: " << stats.total << std::endl;
        f << std::endl;

        // ==== Entity Statistics ====
        f << "=== 2. ENTITY STATISTICS ===" << std::endl;
        f << "  Point        : " << stats.pointCount << std::endl;
        f << "  Line         : " << stats.lineCount << std::endl;
        f << "  Arc          : " << stats.arcCount << std::endl;
        f << "  Circle       : " << stats.circleCount << std::endl;
        f << "  Ellipse      : " << stats.ellipseCount << std::endl;
        f << "  LWPolyline   : " << stats.lwPolylineCount << std::endl;
        f << "  Polyline     : " << stats.polylineCount << std::endl;
        f << "  Spline       : " << stats.splineCount << std::endl;
        f << "  Text         : " << stats.textCount << std::endl;
        f << "  MText        : " << stats.mTextCount << std::endl;
        f << "  Insert(Block): " << stats.insertCount << std::endl;
        f << "  Hatch        : " << stats.hatchCount << std::endl;
        f << "  Solid        : " << stats.solidCount << std::endl;
        f << "  Dimension    : " << stats.dimCount << std::endl;
        f << "  Leader       : " << stats.leaderCount << std::endl;
        f << "  (others)     : " << (stats.total - stats.pointCount - stats.lineCount
            - stats.arcCount - stats.circleCount - stats.ellipseCount - stats.lwPolylineCount
            - stats.polylineCount - stats.splineCount - stats.textCount - stats.mTextCount
            - stats.insertCount - stats.hatchCount - stats.solidCount - stats.dimCount
            - stats.leaderCount) << std::endl;
        f << std::endl;

        // ==== Layers ====
        f << "=== 3. LAYERS ===" << std::endl;
        f << "Total: " << layerData.size() << std::endl;
        for (const auto& s : layerData) f << s << std::endl;
        f << std::endl;

        // ==== Linetypes ====
        f << "=== 4. LINETYPES ===" << std::endl;
        f << "Total: " << linetypeData.size() << std::endl;
        for (const auto& s : linetypeData) f << "  " << s << std::endl;
        f << std::endl;

        // ==== TextStyle ====
        f << "=== 5. TEXT STYLES ===" << std::endl;
        f << "Total: " << textstyleData.size() << std::endl;
        for (const auto& s : textstyleData) f << "  " << s << std::endl;
        f << std::endl;

        // ==== Blocks ====
        f << "=== 6. BLOCKS ===" << std::endl;
        f << "Total: " << blockData.size() << std::endl;
        for (const auto& s : blockData) f << "  " << s << std::endl;
        f << std::endl;

        // ==== Detailed Entity Data by Category ====
        f << "==========================================" << std::endl;
        f << "  DETAILED ENTITY DATA" << std::endl;
        f << "==========================================" << std::endl;
        f << std::endl;

        auto writeSection = [&f](const std::string& title, const std::vector<std::string>& data) {
            f << "--- " << title << " (count=" << data.size() << ") ---" << std::endl;
            for (const auto& s : data) f << s << std::endl;
            if (data.empty()) f << "  (none)" << std::endl;
            f << std::endl;
        };

        writeSection("POINTS", pointData);
        writeSection("LINES", lineData);
        writeSection("ARCS", arcData);
        writeSection("CIRCLES", circleData);
        writeSection("ELLIPSES", ellipseData);
        writeSection("LW POLYLINES", lwPolylineData);
        writeSection("POLYLINES", polylineData);
        writeSection("SPLINES", splineData);
        writeSection("TEXT", textData);
        writeSection("MTEXT", mTextData);
        writeSection("INSERTS (Block References)", insertData);
        writeSection("HATCHES", hatchData);
        writeSection("DIMENSIONS", dimData);
        writeSection("LEADERS", leaderData);

        f << "===================== END =====================" << std::endl;
        f.close();
        std::cout << "Output saved to: " << outputFile << std::endl;
        std::cout << "Total size: " << stats.total << " entities" << std::endl;
    }
};

struct LayerInfo {
    std::string name;
    int color;
    std::string linetype;
    bool isOff;
    bool isFrozen;
    bool isLocked;
};

// ==================== Analyzer Class ====================
class DxfAnalyzer : public DRW_Interface {
public:
    DataCollector collector;

    // -------- Table Callbacks --------
    void addHeader(const DRW_Header* data) override {
        std::ostringstream oss;
        oss << "  Header variables: " << data->vars.size();
        collector.headerData.push_back(oss.str());
        std::cout << "[Header] " << data->vars.size() << " vars" << std::endl;
    }

    void addLType(const DRW_LType& data) override {
        collector.linetypeData.push_back(data.name);
    }

    void addLayer(const DRW_Layer& data) override {
        std::ostringstream oss;
        oss << "  " << data.name
            << " (color=" << data.color
            << ", ltype=" << data.lineType << ")";
        if (data.flags & 0x01) oss << " [OFF]";
        if (data.flags & 0x02) oss << " [FROZEN]";
        if (data.flags & 0x04) oss << " [LOCKED]";
        collector.layerData.push_back(oss.str());
    }

    void addDimStyle(const DRW_Dimstyle& data) override {}
    void addVport(const DRW_Vport& data) override {}

    void addTextStyle(const DRW_Textstyle& data) override {
        collector.textstyleData.push_back(data.name);
    }

    void addAppId(const DRW_AppId& data) override {}

    // -------- Block Callbacks --------
    void addBlock(const DRW_Block& data) override {
        collector.blockData.push_back(data.name);
        collector.stats.blockCount++;
    }

    void setBlock(const int handle) override {}
    void endBlock() override {}

    // -------- Entity Callbacks --------
    void addPoint(const DRW_Point& data) override {
        collector.stats.pointCount++;
        std::ostringstream oss;
        oss << "  Point " << collector.stats.pointCount
            << ": layer=" << data.layer
            << " X=" << safeCoord(data.basePoint.x)
            << " Y=" << safeCoord(data.basePoint.y)
            << " Z=" << safeCoord(data.basePoint.z);
        collector.pointData.push_back(oss.str());
    }

    void addLine(const DRW_Line& data) override {
        collector.stats.lineCount++;
        int idx = collector.stats.lineCount;
        std::ostringstream oss;
        oss << "  Line " << idx
            << ": layer=" << data.layer
            << " start=(X=" << safeCoord(data.basePoint.x)
            << " Y=" << safeCoord(data.basePoint.y)
            << " Z=" << safeCoord(data.basePoint.z) << ")"
            << " end=(X=" << safeCoord(data.secPoint.x)
            << " Y=" << safeCoord(data.secPoint.y)
            << " Z=" << safeCoord(data.secPoint.z) << ")";
        collector.lineData.push_back(oss.str());
    }

    void addRay(const DRW_Ray& data) override {}
    void addXline(const DRW_Xline& data) override {}

    void addArc(const DRW_Arc& data) override {
        collector.stats.arcCount++;
        int idx = collector.stats.arcCount;
        std::ostringstream oss;
        oss << "  Arc " << idx
            << ": layer=" << data.layer
            << " center=(X=" << safeCoord(data.basePoint.x)
            << " Y=" << safeCoord(data.basePoint.y) << ")"
            << " radius=" << safeCoord(data.radious)
            << " startAngle_rad=" << safeCoord(data.staangle)
            << " endAngle_rad=" << safeCoord(data.endangle);
        collector.arcData.push_back(oss.str());
    }

    void addCircle(const DRW_Circle& data) override {
        collector.stats.circleCount++;
        int idx = collector.stats.circleCount;
        std::ostringstream oss;
        oss << "  Circle " << idx
            << ": layer=" << data.layer
            << " center=(X=" << safeCoord(data.basePoint.x)
            << " Y=" << safeCoord(data.basePoint.y) << ")"
            << " radius=" << safeCoord(data.radious);
        collector.circleData.push_back(oss.str());
    }

    void addEllipse(const DRW_Ellipse& data) override {
        collector.stats.ellipseCount++;
        int idx = collector.stats.ellipseCount;
        std::ostringstream oss;
        oss << "  Ellipse " << idx
            << ": layer=" << data.layer
            << " center=(X=" << safeCoord(data.basePoint.x)
            << " Y=" << safeCoord(data.basePoint.y) << ")"
            << " majorAxisEnd=(X=" << safeCoord(data.secPoint.x)
            << " Y=" << safeCoord(data.secPoint.y) << ")"
            << " ratio=" << safeCoord(data.ratio);
        collector.ellipseData.push_back(oss.str());
    }

    void addLWPolyline(const DRW_LWPolyline& data) override {
        collector.stats.lwPolylineCount++;
        int idx = collector.stats.lwPolylineCount;
        std::ostringstream oss;
        oss << "  LWPolyline " << idx
            << ": layer=" << data.layer
            << " vertices=" << data.vertlist.size()
            << " flags=" << data.flags;
        // Add vertex details
        for (size_t vi = 0; vi < data.vertlist.size(); vi++) {
            auto v = data.vertlist[vi];
            oss << "  |  V" << (vi+1) << ": (X=" << safeCoord(v->x)
                << " Y=" << safeCoord(v->y) << ")";
            if (v->bulge != 0.0)
                oss << " bulge=" << safeCoord(v->bulge);
        }
        collector.lwPolylineData.push_back(oss.str());
    }

    void addPolyline(const DRW_Polyline& data) override {
        collector.stats.polylineCount++;
        int idx = collector.stats.polylineCount;
        std::ostringstream oss;
        oss << "  Polyline " << idx
            << ": layer=" << data.layer
            << " vertices=" << data.vertlist.size();
        collector.polylineData.push_back(oss.str());
    }

    void addSpline(const DRW_Spline* data) override {
        if (!data) return;
        collector.stats.splineCount++;
        int idx = collector.stats.splineCount;
        std::ostringstream oss;
        oss << "  Spline " << idx
            << ": layer=" << data->layer
            << " degree=" << data->degree
            << " nControlPts=" << data->controllist.size()
            << " nFitPts=" << data->fitlist.size();
        collector.splineData.push_back(oss.str());
    }

    void addKnot(const DRW_Entity& data) override {}

    void addInsert(const DRW_Insert& data) override {
        collector.stats.insertCount++;
        int idx = collector.stats.insertCount;
        std::ostringstream oss;
        oss << "  Insert " << idx
            << ": layer=" << data.layer
            << " blockName=" << data.name
            << " pos=(X=" << safeCoord(data.basePoint.x)
            << " Y=" << safeCoord(data.basePoint.y) << ")"
            << " scale=(X=" << safeCoord(data.xscale)
            << " Y=" << safeCoord(data.yscale)
            << " Z=" << safeCoord(data.zscale) << ")"
            << " rotation_rad=" << safeCoord(data.angle);
        collector.insertData.push_back(oss.str());
    }

    void addTrace(const DRW_Trace& data) override {}
    void add3dFace(const DRW_3Dface& data) override {}
    void addSolid(const DRW_Solid& data) override { collector.stats.solidCount++; }

    void addMText(const DRW_MText& data) override {
        collector.stats.mTextCount++;
        int idx = collector.stats.mTextCount;
        std::string txt = data.text;
        // Replace newlines for readability in text file
        size_t pos = 0;
        while ((pos = txt.find('\n', pos)) != std::string::npos) {
            txt.replace(pos, 1, "\\n");
            pos += 2;
        }
        std::ostringstream oss;
        oss << "  MText " << idx
            << ": layer=" << data.layer
            << " pos=(X=" << safeCoord(data.basePoint.x)
            << " Y=" << safeCoord(data.basePoint.y) << ")"
            << " height=" << safeCoord(data.height)
            << " style=" << data.style
            << " text=[" << txt << "]";
        collector.mTextData.push_back(oss.str());
    }

    void addText(const DRW_Text& data) override {
        collector.stats.textCount++;
        int idx = collector.stats.textCount;
        std::ostringstream oss;
        oss << "  Text " << idx
            << ": layer=" << data.layer
            << " pos=(X=" << safeCoord(data.basePoint.x)
            << " Y=" << safeCoord(data.basePoint.y) << ")"
            << " height=" << safeCoord(data.height)
            << " rotation_deg=" << safeCoord(data.angle)
            << " style=" << data.style
            << " text=[" << trim(data.text) << "]";
        collector.textData.push_back(oss.str());
    }

    void addDimAlign(const DRW_DimAligned *data) override {
        collector.stats.dimCount++;
        if (!data) return;
        std::ostringstream oss;
        oss << "  DimAligned: layer=" << data->layer
            << " type=" << data->type;
        collector.dimData.push_back(oss.str());
    }

    void addDimLinear(const DRW_DimLinear *data) override {
        collector.stats.dimCount++;
        if (!data) return;
        std::ostringstream oss;
        oss << "  DimLinear: layer=" << data->layer
            << " type=" << data->type;
        collector.dimData.push_back(oss.str());
    }

    void addDimRadial(const DRW_DimRadial *data) override {
        collector.stats.dimCount++;
    }
    void addDimDiametric(const DRW_DimDiametric *data) override {
        collector.stats.dimCount++;
    }
    void addDimAngular(const DRW_DimAngular *data) override {
        collector.stats.dimCount++;
    }
    void addDimAngular3P(const DRW_DimAngular3p *data) override {
        collector.stats.dimCount++;
    }
    void addDimOrdinate(const DRW_DimOrdinate *data) override {
        collector.stats.dimCount++;
    }

    void addLeader(const DRW_Leader *data) override {
        collector.stats.leaderCount++;
    }

    void addHatch(const DRW_Hatch *data) override {
        if (!data) return;
        collector.stats.hatchCount++;
        int idx = collector.stats.hatchCount;
        std::ostringstream oss;
        oss << "  Hatch " << idx
            << ": layer=" << data->layer
            << " pattern=" << data->name
            << " solid=" << data->solid
            << " loops=" << data->loopsnum;
        collector.hatchData.push_back(oss.str());
    }

    void addViewport(const DRW_Viewport& data) override {}
    void addImage(const DRW_Image *data) override {}

    void linkImage(const DRW_ImageDef *data) override {}
    void addComment(const char* comment) override {}
    void addPlotSettings(const DRW_PlotSettings *data) override {}

    // -------- Write callbacks (unused) --------
    void writeHeader(DRW_Header& data) override {}
    void writeBlocks() override {}
    void writeBlockRecords() override {}
    void writeEntities() override {}
    void writeLTypes() override {}
    void writeLayers() override {}
    void writeTextstyles() override {}
    void writeVports() override {}
    void writeDimstyles() override {}
    void writeObjects() override {}
    void writeAppId() override {}
};

// ==================== Main ====================
int main(int argc, char* argv[]) {
    std::string dxfFile;

    if (argc < 2) {
        std::cerr << "\nUsage: dxf_analyzer.exe <dxf_file>" << std::endl;
        std::cerr << "  Output: result/<dxf_filename>.txt (analysis report)\n" << std::endl;
        return 1;
    }

    dxfFile = argv[1];

    DxfAnalyzer analyzer;

    std::cout << "Reading: " << dxfFile << " ..." << std::endl;

    dxfRW dxf(dxfFile.c_str());
    bool success = dxf.read(&analyzer, false);

    if (!success) {
        std::cerr << "\nERROR: Failed to read DXF file!" << std::endl;
        return 1;
    }

    std::cout << "Read complete! Total entities: " << analyzer.collector.stats.total << std::endl;

    // Generate output filename in the result/ directory
    std::string outFile;
    {
        // Extract just the filename without path
        std::string baseName = dxfFile;
        size_t sepPos = baseName.find_last_of("/\\");
        if (sepPos != std::string::npos) {
            baseName = baseName.substr(sepPos + 1);
        }
        // Replace .dxf with .txt
        size_t dotPos = baseName.rfind('.');
        if (dotPos != std::string::npos) {
            baseName = baseName.substr(0, dotPos) + ".txt";
        } else {
            baseName += ".txt";
        }
        outFile = "result/" + baseName;
    }

    analyzer.collector.writeTxt(outFile);
    std::cout << "\nAnalysis complete! File saved to: " << outFile << std::endl;

    return 0;
}
