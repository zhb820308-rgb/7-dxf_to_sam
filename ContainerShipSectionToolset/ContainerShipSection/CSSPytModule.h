#ifndef CSSPYTMODULE_H
#define CSSPYTMODULE_H

#include <pyoModule.h>

class omuArguments;

class CSSPytModule: public pyoModule
{
public:
    CSSPytModule();
    virtual ~CSSPytModule();

    virtual void DefineConstants() override;

private:
    CSSPytModule(const CSSPytModule&);
    CSSPytModule& operator=(const CSSPytModule&);
};

#endif // CSSPYTMODULE_H
