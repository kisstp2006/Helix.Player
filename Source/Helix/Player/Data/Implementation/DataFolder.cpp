#include <Helix/Player/Data/DataFolder.h>

#include <Helix/Core/Logging/Log.h>
#include <Helix/Engine/Settings/Settings.h>
#include <Helix/Platform/IO/FileSystem.h>

namespace helix::player
{
    namespace
    {
        LogChannel DataLog{"Player"};
    }

    String DefaultDataFolder()
    {
#if HX_PLATFORM_WEB
        return String("/Data");
#elif HX_PLATFORM_ANDROID
        return String("Data");
#else
        String exe = platform::ExecutablePath();
        StringView name = platform::GetFileName(exe);
        StringView stem = name.Substr(0, name.Size() - platform::GetExtension(name).Size());
        return platform::JoinPath(platform::GetDirectoryName(exe), Format("{}_Data", stem));
#endif
    }

    bool PlayerSceneSource::Read(StringView path, Array<u8>& out, String& error)
    {
        if (engine::FindAssetAtPath(path))
            return assets_.Read(path, out, error);
        return files_.Read(path, out, error);
    }

    bool DataFolder::ReadFile(StringView path, Array<u8>& out)
    {
        if (reader_)
            return reader_->Read(root_, path, out);
        return platform::ReadFile(platform::JoinPath(root_, path), out);
    }

    bool DataFolder::Open(StringView root, engine::DataFileReader* reader, Array<String>& errors)
    {
        Close();
        root_ = String(root);
        reader_ = reader;
        // The scene source first: it makes the catalog list scene files, before the folder is listed.
        scenes_ = new PlayerSceneSource();
        engine::SceneManager::SetSceneSource(scenes_);
        Array<u8> bytes;
        if (ReadFile(engine::kAssetIndexFile, bytes))
        {
            source_ = new engine::PlayerDataSource(root_, reader_);
            engine::AddAssetSource(source_);
        }
        else
            HX_LOG_INFO(DataLog, "The data folder '{}' has no {}: no assets.", root_, engine::kAssetIndexFile);
        bool ok = true;
        bytes.Clear();
        if (ReadFile(kSettingsFile, bytes))
        {
            usize before = errors.Size();
            ok = engine::LoadSettingsFile(Span<const u8>(bytes.Data(), bytes.Size()), errors);
            for (usize i = before; i < errors.Size(); i++)
                HX_LOG_WARNING(DataLog, "{}: {}", kSettingsFile, errors[i]);
            if (!ok)
                HX_LOG_ERROR(DataLog, "The settings file of '{}' could not be read.", root_);
        }
        return ok;
    }

    void DataFolder::Close()
    {
        if (scenes_)
        {
            engine::SceneManager::SetSceneSource(nullptr);
            delete scenes_;
            scenes_ = nullptr;
        }
        if (source_)
        {
            engine::RemoveAssetSource(source_);
            delete source_;
            source_ = nullptr;
        }
        reader_ = nullptr;
        root_ = String();
    }
}
