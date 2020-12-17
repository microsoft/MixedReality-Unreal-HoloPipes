#include "SavedGameSaver.h"

using namespace winrt::Windows::Storage;
using namespace winrt::Windows::Storage::Streams;
using namespace winrt::Windows::Foundation;

SavedGameSaver::SavedGameSaver(SavedGamePathType type,
                               PCWSTR name,
                               _In_ const byte* pBytes,
                               UINT32 count)
{
    m_bytes.assign(pBytes, pBytes + count);
    m_name = name;
    m_pathType = type;

    Start();
}

SavedGameSaver::~SavedGameSaver()
{
    WaitForCompletion();
}

IFACEMETHODIMP SavedGameSaver::GetBytes(_Outptr_ const byte** ppBytes, _Out_ UINT32* pCount)
{
    (*ppBytes) = m_bytes.data();
    (*pCount) = static_cast<UINT32>(m_bytes.size());
    return S_OK;
}

IFACEMETHODIMP_(void) SavedGameSaver::WaitForCompletion()
{
    if (m_thread.joinable())
    {
        m_thread.join();
    }
}

void SavedGameSaver::Start()
{
    // Unreal's game thread is an STA, which means any async action we start on that
    // thread will only execute in the STA. As such, we can't wait on the async action
    // to complete on that thread without causing a deadlock. To deal with that problem,
    // we launch a seperate MTA thread which in turn launches the async action and
    // waits for it to complete. If we need to ensure the action is completed, we can
    // simply join our MTA thread.
    std::thread thread([this]() { RunThread(); });
    m_thread = std::move(thread);
}

void SavedGameSaver::RunThread()
{
    winrt::init_apartment();
    Run();
}

void SavedGameSaver::Run()
{
    SavedGameState result = SavedGameState::Failed;

    try
    {
        std::wstring tempFileName = m_name + L".tmp";

        StorageFolder tempFolder = ResolvePath(m_pathType, true);

        StorageFile tempFile = tempFolder.CreateFileAsync(tempFileName, CreationCollisionOption::GenerateUniqueName).get();

        if (tempFile)
        {
            FileIO::WriteBytesAsync(tempFile, m_bytes).get();

            StorageFolder destFolder = ResolvePath(m_pathType, false);
            tempFile.MoveAsync(destFolder, m_name, NameCollisionOption::ReplaceExisting).get();
            result = SavedGameState::Complete;
        }
    }
    catch (...)
    {
    }

    m_state = result;
}


