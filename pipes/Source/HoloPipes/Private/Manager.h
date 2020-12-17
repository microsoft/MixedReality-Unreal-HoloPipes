#pragma once

struct Manager : winrt::implements<Manager, ISavedGameManager>
{
   // Starts the asynchronous process of loading the game
    IFACEMETHOD(LoadGame)(PCWSTR name, SavedGamePathType path, _COM_Outptr_ ISavedGame** ppSavedGame) override;

    // Starts the asynchronous process of saving the game
    IFACEMETHOD(SaveGame)(PCWSTR name,
                          SavedGamePathType path,
                          _In_ const byte* pBytes,
                          UINT32 count,
                          _COM_Outptr_ ISavedGame** ppSavedGame);
};
