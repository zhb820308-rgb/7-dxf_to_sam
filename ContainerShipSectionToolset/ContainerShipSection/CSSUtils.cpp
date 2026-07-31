#include "CSSUtils.h"

#include "CSSFragment.h"
#include "CSSPytModule.h"
#include <Python.h>
#include <iniPythonModuleRegistrar.h>

#include <omuAtom.h>

static int sam_css_cnt = 0;
static CSSPytModule* css_pm_ptr = nullptr;
static CSSFragment* sam_css_fragment_ptr = nullptr;

void SAMCSSInitialize(int& count)
{
    if(!sam_css_cnt++)
    {
        ++count;
        css_pm_ptr = new CSSPytModule;
        sam_css_fragment_ptr = new CSSFragment;

        omuAtom::CreateAtom("kefKLine");
    }
}

void SAMCSSFinalize(int& count)
{
    if(!--sam_css_cnt)
    {
        --count;
        if(css_pm_ptr)
        {
            delete css_pm_ptr;
            css_pm_ptr = nullptr;
        }
        if(sam_css_fragment_ptr)
        {
            delete sam_css_fragment_ptr;
            sam_css_fragment_ptr = nullptr;
        }
    }
}

// 函数名与模块注册名一致
PyMODINIT_FUNC initContainerShipSection()
{
    iniPythonModuleRegistrar::Instance().Register(SAMCSSInitialize, SAMCSSFinalize);
}
