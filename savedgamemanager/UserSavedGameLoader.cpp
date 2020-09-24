#include "pch.h"
#include "UserSavedGameLoader.h"

using namespace winrt::Windows::Storage;
using namespace winrt::Windows::Storage::Streams;
using namespace winrt::Windows::Foundation;

UserSavedGameLoader::UserSavedGameLoader(PCWSTR name)
{
    m_name = name;
    Start();
}

UserSavedGameLoader::~UserSavedGameLoader()
{
    WaitForCompletion();
}

IFACEMETHODIMP UserSavedGameLoader::GetBytes(_Outptr_ const byte** ppBytes, _Out_ UINT32* pCount)
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

IFACEMETHODIMP_(void) UserSavedGameLoader::WaitForCompletion()
{
    if (m_thread.joinable())
    {
        m_thread.join();
    }
}

IAsyncAction UserSavedGameLoader::Start()
{
    bool startSucceeded = false;

    try
    {
        // Unfortunately, the FileOpenPicker must be invoked on the UI thread, so we can only push
        // writing of the bytes to a background thread.
        // Luckily, we won't corrupt state if the gane terminates or goes into the background
        // while picking a file path, so it's an acceptable trade off
        std::wstring description;
        std::wstring name;
        std::wstring extension;

        size_t pipeIndex = m_name.find(L'|');
        size_t dotIndex = m_name.find(L'.');

        if (dotIndex != std::wstring::npos &&
            dotIndex > 0 &&
            dotIndex < (m_name.length() - 1) &&
            (pipeIndex == std::wstring::npos || (pipeIndex + 1) < dotIndex))
        {
            if (pipeIndex != std::wstring::npos)
            {
                description = m_name.substr(0, pipeIndex);
                name = m_name.substr(pipeIndex + 1, dotIndex - pipeIndex - 1);
                extension = m_name.substr(dotIndex);
            }
            else
            {
                name = m_name.substr(0, dotIndex);
                extension = m_name.substr(dotIndex);
                description = m_name.substr(dotIndex);
            }

            auto picker = winrt::Windows::Storage::Pickers::FileOpenPicker();
            picker.ViewMode(winrt::Windows::Storage::Pickers::PickerViewMode::List);
            picker.FileTypeFilter().Clear();
            picker.FileTypeFilter().Append(extension);

            m_file = co_await picker.PickSingleFileAsync();
            
            if (m_file)
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
            else
            {
                m_state = SavedGameState::Canceled;
            }

            startSucceeded = true;
        }
    }
    catch (...)
    {

    }

    if (!startSucceeded)
    {
        m_state = SavedGameState::Failed;
    }
}

void UserSavedGameLoader::RunThread()
{
    winrt::init_apartment();
    Run().get();
}

IAsyncAction UserSavedGameLoader::Run()
{
    SavedGameState result = SavedGameState::Failed;

    try
    {
        IBuffer buffer = co_await FileIO::ReadBufferAsync(m_file);
        if (buffer)
        {
            m_bytes.assign(buffer.data(), buffer.data() + buffer.Length());
            result = SavedGameState::Complete;
        }
    }
    catch (...)
    {
    }

    m_state = result;
}