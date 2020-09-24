#pragma once

#include "PPipe.h"
#include "XmlParser/Public/FastXml.h"
#include "tutorialtypes.h"
#include <vector>
#include <map>
#include <memory>

enum class TutorialNode
{
    None        = 0x0000, 
    Tutorials   = 0x0001, // ValidUnder: None
    Tutorial    = 0x0002, // ValidUnder: Tutorials
    Page        = 0x0004, // ValidUnder: Tutorial
    Text        = 0x0008, // ValidUnder: Page
    Line        = 0x0010, // ValidUnder: Text
    Disabled    = 0x0020, // ValidUnder: Page
    Feature     = 0x0040, // ValidUnder: Disabled
    Pipes       = 0x0080, // ValidUnder: Page
    Toolbox     = 0x0100, // ValidUnder: Page
    Arrows      = 0x0200, // ValidUnder: Page
    Arrow       = 0x0400, // ValidUnder: Arrows
    Goals       = 0x0800, // ValidUnder: Page
    Goal        = 0x1000, // ValidUnder: Goals
    Pipe        = 0x2000, // ValidUnder: Pipes | Toolbox | Goal
    Connection  = 0x4000, // ValidUnder: Pipe
};

DEFINE_ENUM_FLAG_OPERATORS(TutorialNode)

struct TutorialNodeData
{
    TutorialNode Node;
    TutorialNode ValidUnder;
};

enum class TutorialAttribute
{
    None,
    Level,      // ValidOn: Tutorial
    Size,       // ValidOn: Tutorial
    Generate,   // ValidOn: Tutorial
    Seed,       // ValidOn: Tutorial
    Coordinate, // ValidOn: Page | Arrow | Pipe
    Offset,     // ValidOn: Page
    Type,       // ValidOn: Goal | Pipe
    Class,      // ValidOn: Pipe
    Fixed,      // ValidOn: Pipe
    MinTime,    // ValidOn: Goal
    MinCount,   // ValidOn: Goal
};

struct TutorialAttributeData
{
    TutorialAttribute Attribute;
    TutorialNode ValidOn;
};

class TutorialParser : private IFastXmlCallback
{

public:

    virtual ~TutorialParser() {}

    void Parse(std::vector<std::shared_ptr<TutorialLevel>>& tutorials);

private:

    virtual bool ProcessXmlDeclaration(const TCHAR* ElementData, int32 XmlFileLineNumber) override;
    virtual bool ProcessElement(const TCHAR* ElementName, const TCHAR* ElementData, int32 XmlFileLineNumber) override;
    virtual bool ProcessAttribute(const TCHAR* AttributeName, const TCHAR* AttributeValue) override;
    virtual bool ProcessClose(const TCHAR* Element) override;
    virtual bool ProcessComment(const TCHAR* Comment) override;

    FPipeGridCoordinate ProcessCoordinate(std::wstring& data);
    FVector ProcessVector(std::wstring& data);
    bool ProcessBool(std::wstring& data);
    TutorialNode CurrentNode();

    std::vector<std::shared_ptr<TutorialLevel>> m_tutorials;

    TutorialLevel* m_currentTutorial;
    TutorialPage* m_currentPage;
    TutorialGoal* m_currentGoal;
    TutorialArrow* m_currentArrow;

    TutorialPipe m_tempPipe;

    std::vector<TutorialNode> m_ancestorStack;

    std::map<std::wstring, TutorialNodeData> m_nodeValidationMap;
    std::map<std::wstring, TutorialAttributeData> m_attributeValidationMap;
};