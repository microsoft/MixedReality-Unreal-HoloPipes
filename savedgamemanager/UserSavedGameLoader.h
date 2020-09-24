#pragma once

struct UserSavedGameLoader : winrt::implements<UserSavedGameLoader, ISavedGame>
{

public:

    UserSavedGameLoader(PCWSTR name);
    virtual ~UserSavedGameLoader();

    // Retrieves the current state of the saved game
    IFACEMETHODIMP_(SavedGameState) GetState() override
    {
        return m_state;
    }

    // Retrieves a pointer to the (immutable) contents of the saved game
    IFACEMETHOD(GetBytes)(_Outptr_ const byte** ppBytes, _Out_ UINT32* pCount) override;

    IFACEMETHOD_(void, WaitForCompletion)() override;

    winrt::Windows::Foundation::IAsyncAction Start();

    void RunThread();
    winrt::Windows::Foundation::IAsyncAction Run();

private:

    SavedGameState m_state = SavedGameState::Pending;
    std::vector<byte> m_bytes;
    std::wstring m_name;

    winrt::Windows::Storage::StorageFile m_file = winrt::Windows::Storage::StorageFile(nullptr);

    std::thread m_thread;
};
