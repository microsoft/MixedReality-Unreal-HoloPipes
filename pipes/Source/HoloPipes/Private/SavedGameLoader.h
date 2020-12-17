#pragma once


struct SavedGameLoader : winrt::implements<SavedGameLoader, ISavedGame>
{

public:

    SavedGameLoader(SavedGamePathType type, PCWSTR name);
    virtual ~SavedGameLoader();

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
    void Run();

private:

    SavedGameState m_state = SavedGameState::Pending;
    std::vector<byte> m_bytes;
    std::wstring m_name;
    SavedGamePathType m_pathType;

    std::thread m_thread;
};
