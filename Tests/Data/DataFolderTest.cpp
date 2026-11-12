#include <Support/TestData.h>

#include <Helix/Engine/Application/Application.h>
#include <Helix/Engine/Scene/SceneManager.h>
#include <Helix/Player/Data/DataFolder.h>
#include <Helix/Testing/Test.h>

#include <cstdio>
#include <cstdlib>

using namespace helix;
using namespace helix::player;
using namespace helix::player::testing;

namespace
{
    // Reads through the folder's files and counts them, as the APK reader of Android reads its assets.
    struct CountingReader final : engine::DataFileReader
    {
        u32 reads = 0;
        bool Read(StringView root, StringView path, Array<u8>& out) override
        {
            reads++;
            return platform::ReadFile(platform::JoinPath(root, path), out);
        }
    };
}

HX_TEST(DataFolder, DefaultIsNextToTheExecutable)
{
    String folder = DefaultDataFolder();
#if HX_PLATFORM_WINDOWS || HX_PLATFORM_LINUX
    HX_CHECK(folder.View().EndsWith("helix_player_tests_Data"));
    HX_CHECK(platform::GetDirectoryName(folder) == platform::GetDirectoryName(platform::ExecutablePath()));
#else
    HX_CHECK(!folder.Empty());
#endif
}

HX_TEST(DataFolder, IndexSettingsAndScenes)
{
    TestDataFolder test;
    // A copy of the folder to run the player on by hand (HELIX_PLAYER_EXPORT_TEST_DATA=<folder>).
    if (const char* exportTo = std::getenv("HELIX_PLAYER_EXPORT_TEST_DATA"))
        CopyTree(test.root, exportTo);
    CountingReader reader;
    DataFolder data;
    Array<String> errors;
    HX_REQUIRE(data.Open(test.root, &reader, errors));
    HX_CHECK(errors.Empty() && data.HasIndex());
    HX_CHECK(engine::Application::productName() == StringView("Player Test") && engine::Application::companyName() == StringView("Helix Tests"));
    HX_CHECK(engine::SceneManager::sceneCountInBuildSettings() == 1);
    HX_CHECK(engine::FindAssetAtPath("Scenes/Level.helix") && engine::FindAssetAtPath("Materials/Red.mat"));
    std::printf("    %u reads\n", reader.reads);
    HX_CHECK(reader.reads == 3);
    // The scene by its id: its cubes come with their mesh and materials.
    engine::AsyncOperation op = engine::SceneManager::LoadSceneAsync(engine::LoadableSceneId{test.sceneGuid}, {engine::LoadSceneMode::Additive});
    HX_REQUIRE(op.IsValid());
    for (int i = 0; i < 10000 && !op.isDone(); i++)
    {
        engine::ProcessAssetLoads();
        engine::SceneManager::ProcessSceneLoads();
    }
    HX_REQUIRE(op.isDone());
    engine::Scene level = engine::SceneManager::GetSceneByLoadableSceneId(engine::LoadableSceneId{test.sceneGuid});
    HX_REQUIRE(level.isLoaded());
    HX_CHECK(level.rootCount() == 4 && level.buildIndex() == 0);
    engine::MeshRenderer* r = engine::FindAnyObjectByType<engine::MeshRenderer>();
    HX_REQUIRE(r && r->sharedMaterial() && r->sharedMaterial()->shader());
    HX_CHECK(r->sharedMaterial()->shader()->Name() == StringView("Tests/PlayerLit"));
    HX_CHECK(reader.reads > 3);
    engine::SceneManager::UnloadSceneAsync(level);
    engine::SceneManager::ProcessSceneLoads();
    DestroyEverything();
    data.Close();
    engine::ResetAssets();
    engine::SceneManager::SetBuildScenes(Span<const StringView>());
    engine::Application::SetProductInfo("DefaultCompany", "Product", "0.1", "com.DefaultCompany.Product");
}

HX_TEST(DataFolder, TheTestCubeFacesOutward)
{
    // Normals from the winding point away from the center: the faces are clockwise seen from outside.
    engine::Mesh* cube = MakeCube();
    Array<Vector3> vertices, normals;
    for (const Vector3& v : cube->vertices())
        vertices.Push(v);
    for (const Vector3& n : cube->normals())
        normals.Push(n);
    HX_REQUIRE(vertices.Size() == 24 && normals.Size() == 24);
    bool outward = true;
    for (usize i = 0; i < vertices.Size(); i++)
        outward = outward && Vector3::Dot(vertices[i], normals[i]) > 0.4f;
    HX_CHECK(outward);
    DestroyEverything();
}
