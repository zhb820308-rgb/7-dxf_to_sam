#include "CSSPytModule.h"

static omuInterfaceObj::methodTable Css_pym_methods[] = {
    { 0, 0 }
};

CSSPytModule::CSSPytModule():
    pyoModule("ContainerShipSection", Css_pym_methods, pyoModule::NO_IMPORT)
{
    // 第一个参数与项目输出的库名称相同
}

CSSPytModule::~CSSPytModule()
{ }

void CSSPytModule::DefineConstants()
{ }
