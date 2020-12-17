#include "pch.h"
#include "SavedGameLoader.h"

using namespace winrt::Windows::Storage;
using namespace winrt::Windows::Storage::Streams;
using namespace winrt::Windows::Foundation;

SavedGameLoader::SavedGameLoader(SavedGamePathType type, PCWSTR name)
{
    m_name = name;
    m_pathType = type;

    Start();
}

SavedGameLoader::~SavedGameLoader()
{
    WaitForCompletion();
}

IFACEMETHODIMP SavedGameLoader::GetBytes(_Outptr_ const byte** ppBytes, _Out_ UINT32* pCount)
{
    (*ppBytes) = nullptr;
    (*pCount) = 0;
    HRESULT hr = E_UNEXPECTED;

    if (m_state == SavedGameState::Complete)
    {
        (*ppBytes) = m_bytes.data();
        (*pCount) = static_cast<UINT32>(m_bytes.size());
        hr = S_OK;
    }

    return hr;
}

IFACEMETHODIMP_(void) SavedGameLoader::WaitForCompletion()
{
    if (m_thread.joinable())
    {
        m_thread.join();
    }
}

void SavedGameLoader::Start()
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

void SavedGameLoader::RunThread()
{
    winrt::init_apartment();
    Run();
}

void SavedGameLoader::Run()
{
    SavedGameState result = SavedGameState::Failed;

    try
    {
        StorageFolder folder = ResolvePath(m_pathType, false);
        StorageFile file = folder.GetFileAsync(m_name).get();

        if (file)
        {
            IBuffer buffer = FileIO::ReadBufferAsync(file).get();
            if (buffer)
            {
                m_bytes.assign(buffer.data(), buffer.data() + buffer.Length());
                result = SavedGameState::Complete;
            }
        }
    }
    catch (...)
    {
    }

    m_state = result;
}