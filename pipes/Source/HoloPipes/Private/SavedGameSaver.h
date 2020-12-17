#pragma once

struct SavedGameSaver : winrt::implements<SavedGameSaver, ISavedGame>
{

public:

    SavedGameSaver(SavedGamePathType type,
                   PCWSTR name,
                   _In_ const byte* pBytes,
                   UINT32 count);

    virtual ~SavedGameSaver();

    // Retrieves the current state of the saved game
    IFACEMETHODIMP_(SavedGameState) GetState() override
    {
        return m_state;
    }

    // Retrieves a pointer to the (immutable) contents of the saved game
    IFACEMETHOD(GetBytes)(_Outptr_ const byte** ppBytes, _Out_ UINT32* pCount) override;

    IFACEMETHOD_(void, WaitForCompletion)() override;

    void Start();
    void RunThread();
    winrt::Windows::Foundation::IAsyncAction Run();

private:

    SavedGameState m_state = SavedGameState::Pending;
    std::vector<byte> m_bytes;
    std::wstring m_name;
    SavedGamePathType m_pathType;

    std::thread m_thread;
};
