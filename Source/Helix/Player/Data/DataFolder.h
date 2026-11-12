#pragma once

#include <Helix/Engine/Asset/Asset.h>
#include <Helix/Engine/Scene/SceneManager.h>

// The player's data folder: the asset index (every file, scenes included), the settings file (PlayerSettings,
// TimeManager, TagManager, BuildSettings) and the files. Scenes are read through the asset catalog first and as files
// on disk otherwise, so -scene can name a scene outside the data folder.
namespace helix::player
{
    inline constexpr StringView kSettingsFile = "settings.asset";

    // <executable folder>/<executable name without extension>_Data; "/Data" on the Web; "Data" (the APK's assets) on
    // Android.
    String DefaultDataFolder();

    // Catalog first, then files (relative to the working directory).
    class PlayerSceneSource final : public engine::SceneSource
    {
    public:
        bool Read(StringView path, Array<u8>& out, String& error) override;

    private:
        engine::AssetSceneSource assets_;
        engine::FileSceneSource files_;
    };

    class DataFolder
    {
    public:
        DataFolder() = default;
        ~DataFolder() { Close(); }
        DataFolder(const DataFolder&) = delete;
        DataFolder& operator=(const DataFolder&) = delete;

        // Adds the folder's assets to the catalog, applies its settings and makes the SceneManager read scenes through
        // it. A folder without an index has no assets (a scene file can still run); false only for a settings file
        // that does not read.
        bool Open(StringView root, engine::DataFileReader* reader, Array<String>& errors);
        void Close();
        StringView Root() const { return root_; }
        bool HasIndex() const { return source_ != nullptr; }

    private:
        bool ReadFile(StringView path, Array<u8>& out);

        String root_;
        engine::DataFileReader* reader_ = nullptr;
        engine::PlayerDataSource* source_ = nullptr;
        PlayerSceneSource* scenes_ = nullptr;
    };
}
