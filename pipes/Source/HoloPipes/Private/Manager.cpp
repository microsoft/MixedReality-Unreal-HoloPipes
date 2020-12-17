#include "pch.h"
#include "Manager.h"
#include "SavedGameLoader.h"
#include "SavedGameSaver.h"
#include "UserSavedGameSaver.h"
#include "UserSavedGameLoader.h"

IFACEMETHODIMP Manager::LoadGame(PCWSTR name,
                                 SavedGamePathType path, 
                                 _COM_Outptr_ ISavedGame** ppSavedGame)
{
    (*ppSavedGame) = nullptr;

    
    if (path == SavedGamePathType::UserSelected)
    {
        return winrt::make<UserSavedGameLoader>(name)->QueryInterface(ppSavedGame);
    }
    else
    {
        return winrt::make<SavedGameLoader>(path, name)->QueryInterface(ppSavedGame);
    }
}

// Starts the asynchronous process of saving the game
IFACEMETHODIMP Manager::SaveGame(PCWSTR name,
                                 SavedGamePathType path,
                                 _In_ const byte* pBytes,
                                 UINT32 count,
                                 _COM_Outptr_ ISavedGame** ppSavedGame)
{
    (*ppSavedGame) = nullptr;
    
    if (path == SavedGamePathType::UserSelected)
    {
        return winrt::make<UserSavedGameSaver>(name, pBytes, count)->QueryInterface(ppSavedGame);
    }
    else
    {
        return winrt::make<SavedGameSaver>(path, name, pBytes, count)->QueryInterface(ppSavedGame);
    }
}

winrt::Windows::Storage::StorageFolder ResolvePath(SavedGamePathType type, bool temp)
{
    switch (type)
    {
        case SavedGamePathType::Documents:
            return winrt::Windows::Storage::KnownFolders::DocumentsLibrary();

        case SavedGamePathType::LocalState:
            if (temp)
            {
                return winrt::Windows::Storage::ApplicationData::Current().TemporaryFolder();
            }
            else
            {
                return winrt::Windows::Storage::ApplicationData::Current().LocalFolder();
            }
    }

    return winrt::Windows::Storage::StorageFolder(nullptr);
}

HRESULT CreateSavedGameManager(_COM_Outptr_ ISavedGameManager** ppManager)
{
    return winrt::make<Manager>()->QueryInterface(ppManager);
}