#include "TutorialParser.h"
#include "Paths.h"

/*
Tutorial xml format is as follows:

<tutorials> (1) - Root node of the tutorial file. 
    Attributes:
        None
    Data:
        None
    Parent Nodes:
        None
    Child Nodes:
        <tutorial> (1-n)

<tutorial> (1-n) - One each for each level that is a tutorial
    Attributes:
        number - Specifies the level number of the tutorial
        size - A single dimension (height/width/depth) of the playable game cube (starts and ends exist outside of this dimension)
        generate - Whether the tutorial should generate the level (defaults to false)
        seed - Generate the tutorial level with the correct level's options, but using the seed to change its layout
    Data:
        None
    Parent Nodes:
        <tutorials>
    Child Nodes:
        <page> (1-n)
        <dialog> (0-1)

<page>
    Attributes:
        coordinate - The location of the tutorial dialog within the grid. Specified as: "x,y,z". If not specified, the dialog won't be anchored to the grid
        offset - The offset of the tutorial dialog, in centimeters, from the anchor point. Specified as: "x,y,z". Deafults to 0,0,0. (ignored if anchor isn't specified)
    Data: 
        None
    Parent Nodes:
        <tutorial>
    Child Nodes:
        <text> (1-n)
        <disabled> (0-1)
        <pipes> (0-1)
        <toolbox> (0-1)
        <arrows> (0-1)
        <goals> (0-1)

<text> (1) - The text for the tutorial
    Attributes:
        None
    Data: 
        For a single line of text, it can be specified directly within the node. For multiple lines of text, specified <line> nodes instead.
    Parent Nodes:
        <page>
    Child Nodes:
        <line> (0-n) (mutually exclusive with data)

<line> (0-n) - For for each space separated line in a screen of tutorial text
    Attributes:
        None
    Data:
        A single line of new line separated lines of text
    Parent Nodes:
        <text>
    Child Nodes:
        None

<disabled> (0-1) - The list of features disabled in this tutorial (if missing or empty, no features disabled
    Attributes:
        None
    Data: 
        None
    Parent Nodes:
        <page>
    Child Nodes:
        <feature> (0-n) 

<feature> (0-n) - A single feature being disabled on this page of the tutorial
    Attributes:
        None
    Data: 
        The name of a disabled feature. One of: drag, rotate or toolbox
    Parent Nodes:
        <disabled>
    Child Nodes:
        None

<pipes> (0-1) - The set of pipes placed on this page of the tutorial. Pages don't replace pipes, just add to them
    Attributes:
        None
    Data:
        None
    Parent Nodes:
        <page>
    Child Nodes:
        <pipe>

<toolbox> (0-1) - The set of pipes that are available in the toolbox. Pages don't replace pipes, just add to them
    Attributes:
        None
    Data:
        None
    Parent Nodes:
        <page>
    Child Nodes:
        <pipe>

<pipe> (0-n) - A single pipe placed in the level
    Attributes:
        type - The type of the pipe piece. One of: start, end, straight, corner or junction.
        coordinate - The location of the pipe piece within the grid. Specified as: "x,y,z" (ignored for toolbox pipes)
        class - The pipe class the piece belongs to, 0 (default) to 12. (ignored for toolbox pipes)
        fixed - Whether the pipe is fixed in place (ignored for toolbox pipes)
    Data:
        None
    Parent Nodes:
        <pipes>
        <toolbox>
        <goal>
    Child Nodes:
        <connection>

<connection> (0-n) - A single outbound connection on the pipe. (Ignored for toolbox pipes)
    Attributes:
        None
    Data:
        One of the following: left, right, front, back, top, bottom
    Parent Nodes:
        <pipe>
    Child Nodes:
        none

<arrows> (0-1) - The list of indicator arrows placed on this tutorial page
    Attributes:
        None
    Data: 
        None
    Parent Nodes:
        <page>
    Child Nodes:
        <arrow> (0-n) 

<arrow> (0-n) - A single indicator arrow placed within the grid
    Attributes:
        coordinate - The location of the arrow within the grid. Specified as: "x,y,z"
    Data: 
        None
    Parent Nodes:
        <arrows>
    Child Nodes:
        none

<goals> (0-1) The list of goals, any of which will dismiss the page
    Attributes:
        none
    Data: 
        None
    Parent Nodes:
        <page>
    Child Nodes:
        none

<goal> (0-n) A single goal which will dismiss the page
    Attributes:
        type - One of the following: next, dismiss, movegame, menu, grab, place, handle or grabfromtoolbox
            * Note - When specifying multiple mutually exclusive goals, the behavior is undefined
        mintime - The minimum number of seconds to be in the goal state before dismissing (currently
                  only supported for handle)
        mincount - The minimum number of times the goal state must be satisifed before dismissing 
                   (currently only supported for handle)
    Data:
        None
    Parent Nodes:
        <goals>
    Child Nodes:
        <pipe> (0-1) 1 for grab, place or handle; otherwise 0

Example:

    <?xml version="1.0"?>
    <tutorials>
      <tutorial level="1" size="1">
        <page coordinate="-1,0,1" offset="0,0,10">
          <text>
            <line>Welcome to HoloPipes!</line>
            <line>Say “Move Game” to move the game. Press or say "Next" to continue.</line>
          </text>
          <disabled>
            <feature>rotate</feature>
            <feature>toolbox</feature>
          </disabled>
          <pipes>
            <pipe type="start" class="1" coordinate="0,-1,0" fixed="true">
              <connection>right</connection>
            </pipe>
            <pipe type="end" class="1" coordinate="0,1,0" fixed = "true">
              <connection>left</connection>
            </pipe>
            <pipe type="straight" class="0" coordinate="-1,0,0">
              <connection>left</connection>
              <connection>right</connection>
            </pipe>
          </pipes>
          <goals>
            <goal type="next"/>
            <goal type="move"/>
          </goals>
        </page>
      </tutorial>
      <tutorial level="5">
        <page coordinate="0,0,1" offset="0,0,10">
          <text>There's a junction in your toolbox! That means one of the pipes will have two ends.</text>
        </page>
      </tutorial>
      <tutorial level="10">
        <page coordinate="0,0,2" offset="0,0,10">
          <text>These metal boxes appear to block pipes. You'll have to find a way around them.</text>
        </page>
      </tutorial>
      <tutorial level="15">
        <page coordinate="0,0,2" offset="0,0,15">
          <text>A piece of your pipe is already locked in place. You can't move it, so you'll have to build through it.</text>
        </page>
      </tutorial>
    </tutorials>

*/

void TutorialParser::Parse(std::vector<std::shared_ptr<TutorialLevel>>& tutorials)
{
    if (m_nodeValidationMap.empty())
    {
        // On first parse, initialize our node map
        m_nodeValidationMap[L"tutorials"] = { TutorialNode::Tutorials, TutorialNode::None };
        m_nodeValidationMap[L"tutorial"] = { TutorialNode::Tutorial, TutorialNode::Tutorials};
        m_nodeValidationMap[L"page"] = { TutorialNode::Page, TutorialNode::Tutorial };
        m_nodeValidationMap[L"text"] = { TutorialNode::Text, TutorialNode::Page };
        m_nodeValidationMap[L"line"] = { TutorialNode::Line, TutorialNode::Text };
        m_nodeValidationMap[L"disabled"] = { TutorialNode::Disabled, TutorialNode::Page };
        m_nodeValidationMap[L"feature"] = { TutorialNode::Feature, TutorialNode::Disabled };
        m_nodeValidationMap[L"pipes"] = { TutorialNode::Pipes, TutorialNode::Page };
        m_nodeValidationMap[L"toolbox"] = { TutorialNode::Toolbox, TutorialNode::Page };
        m_nodeValidationMap[L"arrows"] = { TutorialNode::Arrows, TutorialNode::Page };
        m_nodeValidationMap[L"arrow"] = { TutorialNode::Arrow, TutorialNode::Arrows };
        m_nodeValidationMap[L"goals"] = { TutorialNode::Goals, TutorialNode::Page };
        m_nodeValidationMap[L"goal"] = { TutorialNode::Goal, TutorialNode::Goals };
        m_nodeValidationMap[L"pipe"] = { TutorialNode::Pipe, TutorialNode::Pipes | TutorialNode::Toolbox | TutorialNode::Goal };
        m_nodeValidationMap[L"connection"] = { TutorialNode::Connection, TutorialNode::Pipe };
    }

    if (m_attributeValidationMap.empty())
    {
        m_attributeValidationMap[L"level"] = { TutorialAttribute::Level, TutorialNode::Tutorial };
        m_attributeValidationMap[L"size"] = { TutorialAttribute::Size, TutorialNode::Tutorial };
        m_attributeValidationMap[L"generate"] = { TutorialAttribute::Generate, TutorialNode::Tutorial };
        m_attributeValidationMap[L"seed"] = { TutorialAttribute::Seed, TutorialNode::Tutorial };
        m_attributeValidationMap[L"coordinate"] = { TutorialAttribute::Coordinate, TutorialNode::Page | TutorialNode::Arrow | TutorialNode::Pipe };
        m_attributeValidationMap[L"offset"] = { TutorialAttribute::Offset, TutorialNode::Page };
        m_attributeValidationMap[L"type"] = { TutorialAttribute::Type, TutorialNode::Goal | TutorialNode::Pipe };
        m_attributeValidationMap[L"class"] = { TutorialAttribute::Class, TutorialNode::Pipe };
        m_attributeValidationMap[L"fixed"] = { TutorialAttribute::Fixed, TutorialNode::Pipe };
        m_attributeValidationMap[L"mintime"] = { TutorialAttribute::MinTime, TutorialNode::Goal };
        m_attributeValidationMap[L"mincount"] = { TutorialAttribute::MinCount, TutorialNode::Goal };
    }

    m_tutorials.clear();
    m_currentTutorial = nullptr;
    m_currentPage = nullptr;
    m_currentGoal = nullptr;

    m_tempPipe = {};

    m_ancestorStack.clear();

    FString path = FPaths::ProjectContentDir();
    path += "XML/tutorials.xml";

    FText errorMessage;
    int32 errorLine;

    if (FFastXml::ParseXmlFile(this,
                               *path,
                               L"",
                               nullptr,
                               false,
                               false,
                               errorMessage,
                               errorLine))
    {
        m_tutorials.swap(tutorials);
    }
    else
    {
        UE_LOG(HoloPipesLog, Error, L"TutorialParser - Errors encountered while parsing line %d - \"%ls\"", errorLine, *(errorMessage.ToString()));
    }
}

TutorialNode TutorialParser::CurrentNode()
{
    if (m_ancestorStack.empty())
    {
        return TutorialNode::None;
    }
    else
    {
        return m_ancestorStack.back();
    }
}

bool TutorialParser::ProcessXmlDeclaration(const TCHAR* /*ElementData*/, int32 /*XmlFileLineNumber*/) 
{
    return true;
}

bool TutorialParser::ProcessElement(const TCHAR* ElementName, const TCHAR* ElementData, int32 XmlFileLineNumber)
{
    bool success = false;

    std::wstring name = ElementName;
    std::wstring data = ElementData ? ElementData : L"";

    TutorialNode currentNode = TutorialNode::None;

    const auto& it = m_nodeValidationMap.find(name);

    if (it != m_nodeValidationMap.end())
    {
        currentNode = it->second.Node;
        TutorialNode parentNode = CurrentNode();
        success = ((parentNode == it->second.ValidUnder) ||
            (parentNode != TutorialNode::None && ((parentNode & it->second.ValidUnder) == parentNode)));

        if (success)
        {
            m_ancestorStack.push_back(currentNode);
        }
    }

    if (success)
    {
        switch (currentNode)
        {
            case TutorialNode::Tutorial:
            {
                m_tutorials.push_back(std::make_shared<TutorialLevel>());
                m_currentTutorial = m_tutorials.back().get();
                break;
            }

            case TutorialNode::Page:
            {
                m_currentTutorial->Pages.push_back({});
                m_currentPage = &m_currentTutorial->Pages.back();
                break;
            }

            case TutorialNode::Text:
            {
                if (data.length() > 0)
                {
                    m_currentPage->Text = data;
                }
                else
                {
                    m_currentPage->Text = L"";
                }
                break;
            }

            case TutorialNode::Line:
            {
                if (data.length() > 0)
                {
                    if (m_currentPage->Text.length() > 0)
                    {
                        m_currentPage->Text.append(L"\n\n");
                    }
                    m_currentPage->Text.append(data);
                }

                break;
            }

            case TutorialNode::Feature:
            {
                if (data == L"drag")
                {
                    m_currentPage->Disabled |= TutorialDisabledFeatures::Drag;
                }
                else if (data == L"rotate")
                {
                    m_currentPage->Disabled |= TutorialDisabledFeatures::Rotate;
                }
                else if (data == L"toolbox")
                {
                    m_currentPage->Disabled |= TutorialDisabledFeatures::Toolbox;
                }
                else
                {
                    success = false;
                }
                break;
            }

            case TutorialNode::Arrow:
            {
                m_currentPage->Arrows.push_back({});
                m_currentArrow = &m_currentPage->Arrows.back();
                break;
            }

            case TutorialNode::Goal:
            {
                m_currentPage->Goals.push_back({});
                m_currentGoal = &m_currentPage->Goals.back();
                break;
            }

            case TutorialNode::Pipe:
            {
                m_tempPipe = {};
                break;
            }

            case TutorialNode::Connection:
            {
                if (data == L"right")
                {
                    m_tempPipe.Connections |= PipeDirections::Right;
                }
                else if (data == L"left")
                {
                    m_tempPipe.Connections |= PipeDirections::Left;
                }
                else if (data == L"front")
                {
                    m_tempPipe.Connections |= PipeDirections::Front;
                }
                else if (data == L"back")
                {
                    m_tempPipe.Connections |= PipeDirections::Back;
                }
                else if (data == L"top")
                {
                    m_tempPipe.Connections |= PipeDirections::Top;
                }
                else if (data == L"bottom")
                {
                    m_tempPipe.Connections |= PipeDirections::Bottom;
                }
                else
                {
                    success = false;
                }
                break;
            }
        }
    }

    return success;
}

bool TutorialParser::ProcessAttribute(const TCHAR* AttributeName, const TCHAR* AttributeValue)
{
    bool success = true;

    std::wstring name = AttributeName;
    std::wstring data = AttributeValue;

    TutorialNode currentNode = CurrentNode();
    TutorialAttribute currentAttribute = TutorialAttribute::None;

    // We don't process any attributes unless they're on one of our nodes
    bool process = (currentNode != TutorialNode::None);

    if (process)
    {
        const auto& it = m_attributeValidationMap.find(name);
        success = it != m_attributeValidationMap.end();

        if (success)
        {
            currentAttribute = it->second.Attribute;
            success = ((currentNode & it->second.ValidOn) == currentNode);
        }
    }

    if (process && success)
    {
        switch (currentAttribute)
        {
            case TutorialAttribute::Level:
            {
                m_currentTutorial->Level = std::stoi(data);
                success = (m_currentTutorial->Level > 0);
                break;
            }

            case TutorialAttribute::Size:
            {
                m_currentTutorial->GridSize = std::stoi(data);
                success = (m_currentTutorial->GridSize > 0);
                break;
            }

            case TutorialAttribute::Generate:
            {
                m_currentTutorial->Generate = ProcessBool(data);
                break;
            }

            case TutorialAttribute::Seed:
            {
                m_currentTutorial->Seed = std::stoi(data);
                success = m_currentTutorial->HasSeed = m_currentTutorial->Seed > 0;
                break;
            }

            case TutorialAttribute::Coordinate:
            {
                FPipeGridCoordinate coordinate = ProcessCoordinate(data);

                switch (currentNode)
                {
                    case TutorialNode::Page:
                        m_currentPage->Anchor.Valid = true;
                        m_currentPage->Anchor.Coordinate = coordinate;
                        break;

                    case TutorialNode::Arrow:
                        m_currentArrow->Type = TutorialArrowType::Coordinate;
                        m_currentArrow->Coordinate = coordinate;
                        break;

                    case TutorialNode::Pipe:
                        m_tempPipe.Coordinate = coordinate;
                        break;
                }

                break;
            }

            case TutorialAttribute::Offset:
            {
                m_currentPage->Anchor.Offset = ProcessVector(data);
                break;
            }

            case TutorialAttribute::Type:
            {
                if (currentNode == TutorialNode::Pipe)
                {
                    if (data == L"start")
                    {
                        m_tempPipe.Type = EPipeType::Start;
                    }
                    else if (data == L"end")
                    {
                        m_tempPipe.Type = EPipeType::End;
                    }
                    else if (data == L"straight")
                    {
                        m_tempPipe.Type = EPipeType::Straight;
                    }
                    else if (data == L"corner")
                    {
                        m_tempPipe.Type = EPipeType::Corner;
                    }
                    else if (data == L"junction")
                    {
                        m_tempPipe.Type = EPipeType::Junction;
                    }
                    else
                    {
                        success = false;
                    }
                }
                else if (currentNode == TutorialNode::Goal)
                {
                    if (data == L"next")
                    {
                        m_currentGoal->Type = TutorialGoalType::Next;
                    }
                    else if (data == L"dismiss")
                    {
                        m_currentGoal->Type = TutorialGoalType::Dismiss;
                    }
                    else if (data == L"move")
                    {
                        m_currentGoal->Type = TutorialGoalType::Move;
                    }
                    else if (data == L"menu")
                    {
                        m_currentGoal->Type = TutorialGoalType::Menu;
                    }
                    else if (data == L"grab")
                    {
                        m_currentGoal->Type = TutorialGoalType::Grab;
                    }
                    else if (data == L"place")
                    {
                        m_currentGoal->Type = TutorialGoalType::Place;
                    }
                    else if (data == L"handle")
                    {
                        m_currentGoal->Type = TutorialGoalType::Handle;
                    }
                    else if (data == L"grabfromtoolbox")
                    {
                        m_currentGoal->Type = TutorialGoalType::GrabFromToolbox;
                    }
                    else
                    {
                        success = false;
                    }
                }

                break;
            }

            case TutorialAttribute::Class:
            {
                m_tempPipe.PipeClass = std::stoi(data);
                success = m_tempPipe.PipeClass >= 0 && m_tempPipe.PipeClass < PipeClassCount;
                break;
            }

            case TutorialAttribute::Fixed:
            {
                m_tempPipe.Fixed = ProcessBool(data);
                break;
            }

            case TutorialAttribute::MinTime:
            {
                m_currentGoal->MinTime = std::stof(data);
                break;
            }

            case TutorialAttribute::MinCount:
            {
                m_currentGoal->MinCount = std::stoi(data);
                break;
            }
        }
    }


    return success;
}

FPipeGridCoordinate TutorialParser::ProcessCoordinate(std::wstring& data)
{
    size_t firstComma = data.find(L',');
    size_t lastComma = data.rfind(L',');

    FPipeGridCoordinate result = { std::stoi(data), 0, 0 };

    if (firstComma != std::string::npos)
    {
        result.Y = std::stoi(data.substr(firstComma + 1));
    }

    if (lastComma != firstComma && lastComma != std::string::npos)
    {
        result.Z = std::stoi(data.substr(lastComma + 1));
    }

    return result;
}

bool TutorialParser::ProcessBool(std::wstring& data)
{
    if (data.length() > 0)
    {
        switch (data.at(0))
        {
            case L'T':
            case L't':
            case 'Y':
            case 'y':
            case '1':
                return true;
        }
    }

    return false;
}

FVector TutorialParser::ProcessVector(std::wstring& data)
{
    size_t firstComma = data.find(L',');
    size_t lastComma = data.rfind(L',');

    FVector result = { std::stof(data), 0, 0 };

    if (firstComma != std::string::npos)
    {
        result.Y = std::stof(data.substr(firstComma + 1));
    }

    if (lastComma != firstComma && lastComma != std::string::npos)
    {
        result.Z = std::stof(data.substr(lastComma + 1));
    }

    return result;
}

bool TutorialParser::ProcessClose(const TCHAR* Element)
{
    bool success = false;

    std::wstring name = Element;

    TutorialNode closedNode = TutorialNode::None;

    const auto& it = m_nodeValidationMap.find(name);

    if (it != m_nodeValidationMap.end())
    {
        closedNode = it->second.Node;
        success = (closedNode == CurrentNode());

        if (success)
        {
            m_ancestorStack.pop_back();
        }
    }

    if (success)
    {
        switch (closedNode)
        {
            case TutorialNode::Tutorial:
            {
                if (m_currentTutorial->Pages.size() == 0 || m_currentTutorial->Level <= 0)
                {
                    m_tutorials.pop_back();
                }
                m_currentTutorial = nullptr;
                break;
            }

            case TutorialNode::Page:
            {
                if (m_currentPage->Text.size() == 0)
                {
                    m_currentTutorial->Pages.pop_back();
                }
                m_currentPage = nullptr;
                break;
            }

            case TutorialNode::Arrow:
            {
                m_currentArrow = nullptr;
                break;
            }

            case TutorialNode::Goal:
            {
                if (m_currentGoal->Type == TutorialGoalType::None)
                {
                    m_currentPage->Goals.pop_back();
                }
                m_currentGoal = nullptr;
                break;
            }

            case TutorialNode::Pipe:
            {
                switch (CurrentNode())
                {
                    case TutorialNode::Pipes:
                    {
                        if (m_tempPipe.Type != EPipeType::None && m_tempPipe.Connections != PipeDirections::None)
                        {
                            m_currentPage->Pipes.push_back(m_tempPipe);
                        }
                        break;
                    }

                    case TutorialNode::Toolbox:
                    {
                        if (m_tempPipe.Type != EPipeType::None)
                        {
                            m_currentPage->Toolbox.push_back(m_tempPipe.Type);
                        }
                        break;
                    }

                    case TutorialNode::Goal:
                    {
                        switch (m_currentGoal->Type)
                        {
                            case TutorialGoalType::Grab:
                            case TutorialGoalType::Handle:
                                m_currentGoal->Pipe = m_tempPipe;
                                break;

                            case TutorialGoalType::Place:
                                if (m_tempPipe.Type != EPipeType::None && m_tempPipe.Connections != PipeDirections::None)
                                {
                                    m_currentGoal->Pipe = m_tempPipe;
                                }
                                break;
                        }

                        break;
                    }
                }

                m_tempPipe = {};
                break;
            }
        }
    }

    return success;
}

bool TutorialParser::ProcessComment(const TCHAR* /*Comment*/)
{
    return true;
}