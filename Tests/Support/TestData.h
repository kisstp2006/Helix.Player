#pragma once

#include <Helix/Engine/Asset/Asset.h>
#include <Helix/Engine/GameObject/GameObject.h>
#include <Helix/Engine/GameObject/Lifecycle.h>
#include <Helix/Engine/Material/Material.h>
#include <Helix/Engine/Mesh/Mesh.h>
#include <Helix/Engine/Rendering/Camera.h>
#include <Helix/Engine/Rendering/Light.h>
#include <Helix/Engine/Rendering/Renderer.h>
#include <Helix/Engine/Scene/SceneFile.h>
#include <Helix/Engine/Scene/SceneManager.h>
#include <Helix/Engine/Shader/Shader.h>
#include <Helix/Engine/Transform/Transform.h>
#include <Helix/Platform/IO/FileSystem.h>
#include <Helix/Player/Data/DataFolder.h>

#include <cmath>
#include <cstdio>
#include <cstring>

#include <PlayerTestShadersAssembly.h>
#include <PlayerTestShadersSymbols.h>

namespace helix::player::testing
{
    inline void RemoveTree(StringView path)
    {
        struct Names
        {
            Array<String> files, dirs;
        } n{Array<String>(), Array<String>()};
        platform::ListDirectory(
            path,
            [](const platform::DirectoryEntry& e, void* user) {
                Names& n = *static_cast<Names*>(user);
                (e.isDirectory ? n.dirs : n.files).Push(String(e.name));
            },
            &n);
        for (const String& f : n.files)
            platform::RemoveFile(platform::JoinPath(path, f));
        for (const String& d : n.dirs)
            RemoveTree(platform::JoinPath(path, d));
        platform::RemoveEmptyDirectory(path);
    }

    inline void CopyTree(StringView from, StringView to)
    {
        struct Names
        {
            Array<String> files, dirs;
        } n{Array<String>(), Array<String>()};
        platform::CreateDirectories(to);
        platform::ListDirectory(
            from,
            [](const platform::DirectoryEntry& e, void* user) {
                Names& n = *static_cast<Names*>(user);
                (e.isDirectory ? n.dirs : n.files).Push(String(e.name));
            },
            &n);
        Array<u8> bytes;
        for (const String& f : n.files)
            if (platform::ReadFile(platform::JoinPath(from, f), bytes))
                platform::WriteFile(platform::JoinPath(to, f), bytes);
        for (const String& d : n.dirs)
            CopyTree(platform::JoinPath(from, d), platform::JoinPath(to, d));
    }

    // The engine's objects of a test, by id (destroying a root frees its children, so pointers taken up front dangle).
    inline void DestroyEverything()
    {
        using namespace engine;
        Array<Object*> all;
        Array<EntityId> ids;
        FindObjectsOfTypeAll(ClassOf<Object>(), all);
        for (Object* o : all)
            if (o->Is<GameObject>() && (!static_cast<GameObject*>(o)->transform() || !static_cast<GameObject*>(o)->transform()->parent()))
                ids.Push(o->GetEntityId());
        for (EntityId id : ids)
            if (Object* o = EntityIdToObject(id))
                DestroyImmediate(o, true);
        FindObjectsOfTypeAll(ClassOf<Object>(), all);
        ids.Clear();
        for (Object* o : all)
            ids.Push(o->GetEntityId());
        for (EntityId id : ids)
            if (Object* o = EntityIdToObject(id))
                DestroyImmediate(o, true);
        while (PendingDestroyCount())
            ProcessDestroyQueue(), SetLifecycleTime(LifecycleTime() + 1e9);
    }

    // A unit cube with a normal per face (24 vertices).
    inline engine::Mesh* MakeCube()
    {
        using namespace engine;
        Mesh* m = NewObject<Mesh>();
        m->SetName("Cube");
        Array<Vector3> v;
        Array<i32> t;
        const Vector3 normals[] = {{1, 0, 0}, {-1, 0, 0}, {0, 1, 0}, {0, -1, 0}, {0, 0, 1}, {0, 0, -1}};
        for (const Vector3& n : normals)
        {
            // Two axes across the face, ordered so the triangles wind clockwise seen from outside.
            Vector3 a = std::fabs(n.y) > 0.5f ? Vector3(1, 0, 0) : Vector3(0, 1, 0);
            Vector3 b = Vector3::Cross(n, a);
            i32 base = i32(v.Size());
            v.Push((n - a - b) * 0.5f);
            v.Push((n - a + b) * 0.5f);
            v.Push((n + a + b) * 0.5f);
            v.Push((n + a - b) * 0.5f);
            const i32 quad[] = {0, 1, 2, 0, 2, 3};
            for (i32 q : quad)
                t.Push(base + q);
        }
        m->SetVertices(Span<const Vector3>(v.Data(), v.Size()));
        m->SetTriangles(Span<const i32>(t.Data(), t.Size()));
        m->RecalculateNormals();
        return m;
    }

    // A player data folder made by code: the test shaders, a cube mesh, two materials, a scene (camera, light, two
    // cubes) and the settings with the product and the build scene list.
    struct TestDataFolder
    {
        String root;
        Guid shadersGuid, meshGuid, redGuid, blueGuid, sceneGuid;
        static constexpr StringView kScenePath = "Scenes/Level.helix";

        TestDataFolder()
        {
            using namespace engine;
            root = platform::JoinPath(platform::TempDirectory(), Format("helix-player-{}", Guid::Generate().ToString()));
            platform::CreateDirectories(root);
            Write("Shaders/HelixPlayer.TestShaders.dll", Span<const u8>(kPlayerTestShadersAssembly, sizeof(kPlayerTestShadersAssembly)));
            Write("Shaders/HelixPlayer.TestShaders.pdb", Span<const u8>(kPlayerTestShadersSymbols, sizeof(kPlayerTestShadersSymbols)));
            shadersGuid = Guid::Generate();
            meshGuid = Guid::Generate();
            redGuid = Guid::Generate();
            blueGuid = Guid::Generate();
            sceneGuid = Guid::Generate();
            Array<AssetEntry> entries;
            Add(entries, shadersGuid, "Shaders/HelixPlayer.TestShaders.dll");
            String error;
            WriteAssetIndex(root, Span<const AssetEntry>(entries.Data(), entries.Size()), error);

            // The objects, with the shader loaded from the folder so references to it name its asset.
            PlayerDataSource source(root);
            AddAssetSource(&source);
            LoadAsset(shadersGuid);
            Shader* lit = Shader::Find("Tests/PlayerLit");
            Mesh* cube = MakeCube();
            RegisterAssetObject(meshGuid, MainAssetFileId(ClassId::Mesh), cube, true);
            Material* red = Material::Create(lit);
            red->SetName("Red");
            red->SetColor("color", Color(0.9f, 0.2f, 0.1f, 1));
            RegisterAssetObject(redGuid, MainAssetFileId(ClassId::Material), red, true);
            Material* blue = Material::Create(lit);
            blue->SetName("Blue");
            blue->SetColor("color", Color(0.1f, 0.3f, 0.9f, 1));
            RegisterAssetObject(blueGuid, MainAssetFileId(ClassId::Material), blue, true);
            WriteAsset("Meshes/Cube.asset", cube, true);
            WriteAsset("Materials/Red.mat", red, false);
            WriteAsset("Materials/Blue.mat", blue, false);
            Add(entries, meshGuid, "Meshes/Cube.asset");
            Add(entries, redGuid, "Materials/Red.mat");
            Add(entries, blueGuid, "Materials/Blue.mat");

            Scene scene = SceneManager::CreateScene("Level");
            Scene previous = SceneManager::GetActiveScene();
            SceneManager::SetActiveScene(scene);
            GameObject* camera = GameObject::Create("Main Camera");
            camera->SetTag("MainCamera");
            camera->transform()->SetPosition(Vector3(0, 2, -5));
            camera->transform()->LookAt(Vector3(0, 0, 0));
            Camera* c = camera->AddComponent<Camera>();
            c->SetFieldOfView(50);
            c->SetClearFlags(CameraClearFlags::SolidColor);
            c->SetBackgroundColor(Color(0.1f, 0.12f, 0.15f, 1));
            GameObject* sun = GameObject::Create("Directional Light");
            sun->transform()->SetEulerAngles(Vector3(50, -30, 0));
            Light* light = sun->AddComponent<Light>();
            light->SetType(LightType::Directional);
            light->SetIntensity(1);
            Thing("Red Cube", cube, red, Vector3(-0.8f, 0, 0), Vector3(0, 30, 0));
            Thing("Blue Cube", cube, blue, Vector3(0.9f, 0.2f, 0.5f), Vector3(20, -40, 10));
            String yaml;
            if (!WriteSceneYaml(scene, yaml, error))
                std::printf("    the test scene could not be written: %s\n", error.CStr());
            Write(kScenePath, Span<const u8>(reinterpret_cast<const u8*>(yaml.Data()), yaml.Size()));
            Add(entries, sceneGuid, kScenePath);
            SceneManager::SetActiveScene(previous);
            SceneManager::UnloadSceneAsync(scene);
            SceneManager::ProcessSceneLoads();

            WriteAssetIndex(root, Span<const AssetEntry>(entries.Data(), entries.Size()), error);
            const char* settings = R"(%YAML 1.1
%TAG !h! tag:helixengine,2026:
--- !h!129 &1
PlayerSettings:
  companyName: Helix Tests
  productName: Player Test
  bundleVersion: 1.0
  applicationIdentifier: org.helix.playertest
--- !h!141 &2
BuildSettings:
  scenes:
  - Scenes/Level.helix
)";
            Write(kSettingsFile, Span<const u8>(reinterpret_cast<const u8*>(settings), std::strlen(settings)));
            // Nothing of it stays loaded: the player reads it all back.
            DestroyEverything();
            ResetAssets();
            Shader::Globals().Clear();
        }

        ~TestDataFolder() { RemoveTree(root); }

        void Write(StringView path, Span<const u8> bytes)
        {
            String full = platform::JoinPath(root, path);
            platform::CreateDirectories(String(platform::GetDirectoryName(full)));
            platform::WriteFile(full, bytes);
        }

    private:
        static void Add(Array<engine::AssetEntry>& entries, const Guid& guid, StringView path)
        {
            engine::AssetEntry e;
            e.guid = guid;
            e.path = String(path);
            entries.Push(Move(e));
        }

        void WriteAsset(StringView path, engine::Object* object, bool binary)
        {
            Array<u8> bytes;
            String error;
            engine::Object* const objects[] = {object};
            engine::WriteAssetFile(Span<engine::Object* const>(objects, 1), bytes, error, binary);
            Write(path, Span<const u8>(bytes.Data(), bytes.Size()));
        }

        static void Thing(StringView name, engine::Mesh* mesh, engine::Material* material, Vector3 position, Vector3 euler)
        {
            using namespace engine;
            GameObject* g = GameObject::Create(name);
            g->transform()->SetPosition(position);
            g->transform()->SetEulerAngles(euler);
            g->AddComponent<MeshFilter>()->SetSharedMesh(mesh);
            g->AddComponent<MeshRenderer>()->SetSharedMaterial(material);
        }
    };
}
