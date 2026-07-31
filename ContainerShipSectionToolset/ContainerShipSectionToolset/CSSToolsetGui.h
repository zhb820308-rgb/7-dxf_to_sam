#ifndef CSSTOOLSETGUI_H
#define CSSTOOLSETGUI_H

#include <SAMToolsetGui.h>
#include <omiSingleton.T>
#include <omiSingleton.h>

class CSSToolsetGui: public SAMToolsetGui, public omiSingleton<CSSToolsetGui>
{
    Q_OBJECT

public:
    CSSToolsetGui();
    ~CSSToolsetGui();

protected:
    virtual void activate() override;
    virtual void deactivate() override;

    virtual void createMenuItems();
    virtual void createToolboxItems();

private:
    CSSToolsetGui(const CSSToolsetGui&);
    CSSToolsetGui& operator=(const CSSToolsetGui&);

private slots:
    void showCSSUi();
};

#endif // CSSTOOLSETGUI_H
