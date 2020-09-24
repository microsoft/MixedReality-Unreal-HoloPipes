#pragma once

#define WIN32_LEAN_AND_MEAN             // Exclude rarely-used stuff from Windows headers
// Windows Header Files

#include <combaseapi.h>

#ifdef SAVEDGAMEMANAGER_EXPORTS
    #define SGM_API __declspec(dllexport)
#else
    #define SGM_API __declspec(dllimport)
#endif


enum class SavedGamePathType : UINT32
{
    // Path is the user's Document's Directory. This will fail
    // for UWP apps
    Documents = 0,

    // Path is the UWP's Local State Directory. This will fail
    // for non-UWP apps
    LocalState = 1,

    // Path is user selected
    UserSelected = 2
};

enum class SavedGameState : UINT32
{
    // The saved game isn't ready yet (it's waiting, likely on disk
    // or network activity)
    Pending = 0,

    // The user canceled the saved game operation
    Canceled = 1,

    // The saved game operation (loading or saving) is complete
    Complete = 2,

    // The saved game operation failed
    Failed = 3
};

DECLARE_INTERFACE_IID_(ISavedGame, IUnknown, "ED0C5450-563C-413B-ADD9-F6E0CEB809A6")
{
    // Retrieves the current state of the saved game
    STDMETHOD_(SavedGameState, GetState)() PURE;

    // Retrieves a pointer to the (immutable) contents of the saved game
    STDMETHOD(GetBytes)(_Outptr_ const byte** ppBytes, _Out_ UINT32* pCount) PURE;

    // Synchronously waits for a pending Saved Game operation to complete
    STDMETHOD_(void, WaitForCompletion)() PURE;
};

DECLARE_INTERFACE_IID_(ISavedGameManager, IUnknown, "60614B82-7BF9-4C1B-A77C-B3A7485F4B99")
{
    // Starts the asynchronous process of loading the game
    STDMETHOD(LoadGame)(PCWSTR name, SavedGamePathType path, _COM_Outptr_ ISavedGame** ppSavedGame) PURE;

    // Starts the asynchronous process of saving the game
    // When called with SavedGamePathType::UserSelected, the name parameter will be intereprted as follows:
    //      "<File Type Description>|<Suggested File Name>.<File Extension>"
    STDMETHOD(SaveGame)(PCWSTR name,
                        SavedGamePathType path, 
                        _In_ const byte* pBytes, 
                        UINT32 count,
                        _COM_Outptr_ ISavedGame** ppSavedGame) PURE;
};

SGM_API HRESULT CreateSavedGameManager(_COM_Outptr_ ISavedGameManager** ppManager);
