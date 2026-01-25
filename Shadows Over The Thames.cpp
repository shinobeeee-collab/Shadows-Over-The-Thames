// Shadows Over The Thames - Изометрическая игра с 3D NPC (ВЕРСИЯ С ПОДДЕРЖКОЙ MTL)
#include <windows.h>
#include <d3d11.h>
#include <d3dcompiler.h>
#include <DirectXMath.h>
#include <wincodec.h>
#include <vector>
#include <fstream>
#include <sstream>
#include <map>
#include <memory>
#include <string>
#include <cmath>
#include <algorithm>
#include <filesystem>
#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>

// Добавьте этот дефайн для аннотаций SAL
#define _In_
#define _In_opt_
#define _In_z_
#define _Out_
#define _Out_opt_
#define _Inout_
#define _Inout_opt_

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "d3dcompiler.lib")
#pragma comment(lib, "windowscodecs.lib")

using namespace DirectX;
// ==================== КОНСТАНТЫ И МАКРОСЫ ====================
const int SCREEN_WIDTH = 1280;
const int SCREEN_HEIGHT = 720;
const float CAMERA_DISTANCE = 15.0f;
const float CAMERA_HEIGHT = 10.0f;

// Отладочный вывод
#define DEBUG_LOG(msg) OutputDebugStringA((std::string("[DEBUG] ") + msg + "\n").c_str())
#define DEBUG_LOG_W(msg) OutputDebugStringW((std::wstring(L"[DEBUG] ") + msg + L"\n").c_str())
#define DEBUG_ERROR(msg) OutputDebugStringA((std::string("[ERROR] ") + msg + "\n").c_str())
#define DEBUG_WARNING(msg) OutputDebugStringA((std::string("[WARNING] ") + msg + "\n").c_str())
#define DEBUG_SUCCESS(msg) OutputDebugStringA((std::string("[SUCCESS] ") + msg + "\n").c_str())
// ==================== СИСТЕМА АНИМАЦИИ ====================

// Структура для вершин с анимацией (скиннинг)
struct AnimatedVertex {
    XMFLOAT3 position;
    XMFLOAT3 normal;
    XMFLOAT2 texcoord;
    XMFLOAT3 color;
    BYTE boneIndices[4];      // Индексы костей (максимум 4)
    float boneWeights[4];     // Веса костей

    AnimatedVertex() : position(0, 0, 0), normal(0, 1, 0), texcoord(0, 0), color(1, 1, 1) {
        memset(boneIndices, 0, sizeof(boneIndices));
        memset(boneWeights, 0, sizeof(boneWeights));
    }
};

// Простая система анимации (без скелета, трансформация всей модели)
class SimpleAnimator {
private:
    XMFLOAT3 startPosition;
    XMFLOAT3 startRotation;
    XMFLOAT3 startScale;

    float animationTime = 0.0f;
    float walkCycleTime = 1.0f;  // Время полного цикла ходьбы
    bool isWalking = false;
    bool isLooping = true;

    // Амплитуды анимации
    float walkHeightAmplitude = 0.2f;   // Высота шага
    float walkSwayAmplitude = 0.1f;     // Раскачивание в стороны
    float walkBobAmplitude = 0.05f;     // Покачивание вверх-вниз

public:
    void StartWalking() {
        isWalking = true;
        animationTime = 0.0f;
    }

    void StopWalking() {
        isWalking = false;
    }

    void Update(float deltaTime, XMFLOAT3& position, XMFLOAT3& rotation, XMFLOAT3& scale) {
        if (!isWalking) return;

        animationTime += deltaTime;
        if (animationTime > walkCycleTime && isLooping) {
            animationTime -= walkCycleTime;
        }

        // Простая синусоидальная анимация ходьбы
        float t = animationTime / walkCycleTime * XM_2PI;

        // Покачивание вверх-вниз (шаги)
        float bob = sinf(t * 2.0f) * walkBobAmplitude;

        // Раскачивание из стороны в сторону
        float sway = sinf(t) * walkSwayAmplitude;

        // Движение ног (визуальный эффект - поднимаем модель немного)
        float stepHeight = (sinf(t) + 1.0f) * 0.5f * walkHeightAmplitude;

        // Применяем к позиции
        position.y = startPosition.y + bob + stepHeight;
        position.x = startPosition.x + sway;

        // Легкий наклон при ходьбе
        rotation.z = sway * 5.0f;  // Наклон в сторону
        rotation.x = sinf(t * 2.0f) * 0.1f;  // Легкое покачивание вперед-назад
    }

    void SetStartTransform(const XMFLOAT3& pos, const XMFLOAT3& rot, const XMFLOAT3& scl) {
        startPosition = pos;
        startRotation = rot;
        startScale = scl;
    }

    void SetWalkCycleTime(float time) { walkCycleTime = time; }
    void SetAmplitudes(float height, float sway, float bob) {
        walkHeightAmplitude = height;
        walkSwayAmplitude = sway;
        walkBobAmplitude = bob;
    }

    bool IsWalking() const { return isWalking; }
    float GetAnimationTime() const { return animationTime; }
};
// ==================== ПОМОЩНИКИ ДЛЯ РАБОТЫ С ФАЙЛАМИ ====================
class FileSystemHelper {
public:
    static std::wstring GetExecutableDirectory() {
        wchar_t buffer[MAX_PATH];
        GetModuleFileNameW(nullptr, buffer, MAX_PATH);
        std::wstring exePath = buffer;
        size_t pos = exePath.find_last_of(L"\\/");
        return (pos != std::wstring::npos) ? exePath.substr(0, pos + 1) : L".\\";
    }

    // Добавляем этот метод
    static std::wstring FindFile(const std::wstring& filename) {
        // Попробуем несколько мест
        std::vector<std::wstring> searchPaths = {
            filename,  // Прямой путь
            GetExecutableDirectory() + filename,  // Рядом с EXE
            GetExecutableDirectory() + L"..\\" + filename,  // На уровень выше
            GetExecutableDirectory() + L"..\\..\\" + filename,  // На два уровня выше
        };

        // Также пробуем разные расширения
        std::vector<std::wstring> extensions = { L"", L".png", L".bmp", L".obj", L".jpg", L".jpeg", L".mtl", L".fbx" };

        for (const auto& path : searchPaths) {
            for (const auto& ext : extensions) {
                std::wstring fullPath = path + ext;
                if (FileExists(fullPath)) {
                    DEBUG_LOG_W(L"Найден файл: " + fullPath);
                    return fullPath;
                }
            }
        }

        DEBUG_LOG_W(L"Файл не найден: " + filename);
        return L"";
    }

    static std::wstring FindImageFile(const std::wstring& baseName) {
        std::vector<std::wstring> imageExtensions = {
            L".jpg", L".jpeg", L".png", L".bmp", L".dds", L".tga", L".gif"
        };

        std::vector<std::wstring> searchPaths = {
            baseName,  // Прямой путь
            GetExecutableDirectory() + baseName,
            GetExecutableDirectory() + L"textures\\" + baseName,
            GetExecutableDirectory() + L"assets\\" + baseName,
            GetExecutableDirectory() + L"images\\" + baseName,
        };

        // Также пробуем различные регистры
        std::wstring lowerName = baseName;
        std::transform(lowerName.begin(), lowerName.end(), lowerName.begin(), ::tolower);
        std::wstring upperName = baseName;
        std::transform(upperName.begin(), upperName.end(), upperName.begin(), ::toupper);

        searchPaths.push_back(GetExecutableDirectory() + lowerName);
        searchPaths.push_back(GetExecutableDirectory() + upperName);

        for (const auto& path : searchPaths) {
            for (const auto& ext : imageExtensions) {
                std::wstring fullPath = path + ext;
                if (FileExists(fullPath)) {
                    DEBUG_LOG_W(L"Найден файл изображения: " + fullPath);
                    return fullPath;
                }
            }
        }

        DEBUG_LOG_W(L"Файл изображения не найден: " + baseName);
        return L"";
    }

    static bool FileExists(const std::wstring& path) {
        DWORD attrs = GetFileAttributesW(path.c_str());
        return (attrs != INVALID_FILE_ATTRIBUTES && !(attrs & FILE_ATTRIBUTE_DIRECTORY));
    }

    static void ListFilesInDirectory(const std::wstring& directory) {
        DEBUG_LOG_W(L"Содержимое директории: " + directory);

        WIN32_FIND_DATAW findData;
        HANDLE hFind = FindFirstFileW((directory + L"*").c_str(), &findData);

        if (hFind != INVALID_HANDLE_VALUE) {
            do {
                if (!(findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) {
                    DEBUG_LOG_W(L"  " + std::wstring(findData.cFileName));
                }
            } while (FindNextFileW(hFind, &findData));
            FindClose(hFind);
        }
    }
};

// ==================== СТРУКТУРЫ ДАННЫХ ====================
struct Vertex {
    XMFLOAT3 position;
    XMFLOAT3 normal;
    XMFLOAT2 texcoord;
    XMFLOAT3 color;

    Vertex() : position(0, 0, 0), normal(0, 1, 0), texcoord(0, 0), color(1, 1, 1) {}
    Vertex(float px, float py, float pz, float nx, float ny, float nz, float u, float v, float r, float g, float b)
        : position(px, py, pz), normal(nx, ny, nz), texcoord(u, v), color(r, g, b) {
    }
};

struct Texture2D {
    ID3D11Texture2D* texture = nullptr;
    ID3D11ShaderResourceView* srv = nullptr;
    ID3D11SamplerState* samplerState = nullptr;
    std::wstring filename;
    int width = 0;
    int height = 0;

    bool LoadFromFile(ID3D11Device* device, const wchar_t* filename);
    bool CreateDebugTexture(ID3D11Device* device, const wchar_t* name);
    bool CreateColorTexture(ID3D11Device* device, const wchar_t* name, float r, float g, float b);
    void Cleanup();
};

struct Mesh {
    std::vector<Vertex> vertices;
    std::vector<uint32_t> indices;
    std::string materialName;  // Изменено с textureName на materialName
    int textureIndex = -1;
};

// Структура для материала из MTL
struct Material {
    std::string name;
    XMFLOAT3 ambient;
    XMFLOAT3 diffuse;
    XMFLOAT3 specular;
    float shininess;
    float alpha;
    std::string textureFilename;

    Material() : ambient(0.2f, 0.2f, 0.2f), diffuse(0.8f, 0.8f, 0.8f),
        specular(1.0f, 1.0f, 1.0f), shininess(20.0f), alpha(1.0f) {
    }
};

// ==================== КЛАСС ЗАГРУЗКИ OBJ И MTL ====================
class OBJLoader {
public:
    static bool Load(const std::wstring& filename, std::vector<Mesh>& meshes, std::map<std::string, Material>& materials) {
        DEBUG_LOG_W(L"Загрузка OBJ файла: " + filename);

        std::ifstream file(filename);
        if (!file.is_open()) {
            DEBUG_LOG_W(L"Не удалось открыть файл: " + filename);

            // Создаем простую кубическую модель как запасной вариант
            CreateSimpleCubeModel(meshes);
            return true;
        }

        std::vector<XMFLOAT3> positions;
        std::vector<XMFLOAT3> normals;
        std::vector<XMFLOAT2> texcoords;
        std::vector<std::string> mtlFiles;

        Mesh currentMesh;
        std::string currentMaterial;
        std::string currentMtlFile;

        std::string line;
        int lineNum = 0;
        while (std::getline(file, line)) {
            lineNum++;
            std::istringstream iss(line);
            std::string prefix;
            iss >> prefix;

            if (prefix == "v") { // Вершина
                XMFLOAT3 pos;
                iss >> pos.x >> pos.y >> pos.z;
                positions.push_back(pos);
            }
            else if (prefix == "vn") { // Нормаль
                XMFLOAT3 norm;
                iss >> norm.x >> norm.y >> norm.z;
                normals.push_back(norm);
            }
            else if (prefix == "vt") { // Текстурные координаты
                XMFLOAT2 tex;
                iss >> tex.x >> tex.y;
                tex.y = 1.0f - tex.y; // Flip Y для DirectX
                texcoords.push_back(tex);
            }
            else if (prefix == "f") { // Грань (поддержка треугольников, квадов и n-угольников)
                std::vector<std::string> faceVerts;
                std::string vert;

                // Считываем ВСЕ вершины грани
                while (iss >> vert) {
                    faceVerts.push_back(vert);
                }

                // Минимум 3 вершины — иначе это не грань
                if (faceVerts.size() < 3)
                    continue;

                // Fan triangulation:
                // (0, i, i+1)
                for (size_t i = 1; i + 1 < faceVerts.size(); ++i) {
                    ProcessFace(
                        faceVerts[0],
                        positions,
                        normals,
                        texcoords,
                        currentMesh,
                        currentMaterial,
                        materials
                    );

                    ProcessFace(
                        faceVerts[i],
                        positions,
                        normals,
                        texcoords,
                        currentMesh,
                        currentMaterial,
                        materials
                    );

                    ProcessFace(
                        faceVerts[i + 1],
                        positions,
                        normals,
                        texcoords,
                        currentMesh,
                        currentMaterial,
                        materials
                    );
                }
            }

            else if (prefix == "usemtl") { // Материал
                if (!currentMesh.vertices.empty()) {
                    currentMesh.materialName = currentMaterial;
                    meshes.push_back(currentMesh);
                    currentMesh = Mesh();
                }
                iss >> currentMaterial;

                char buffer[256];
                sprintf_s(buffer, "Строка %d: Используется материал: %s", lineNum, currentMaterial.c_str());
                DEBUG_LOG(buffer);
            }
            else if (prefix == "mtllib") { // Файл материалов
                std::string mtlFile;
                iss >> mtlFile;
                mtlFiles.push_back(mtlFile);

                // Получаем путь к MTL файлу
                std::wstring objPath = filename;
                size_t lastSlash = objPath.find_last_of(L"\\/");
                std::wstring basePath = (lastSlash != std::wstring::npos) ?
                    objPath.substr(0, lastSlash + 1) : L"";

                std::wstring mtlPath = basePath + std::wstring(mtlFile.begin(), mtlFile.end());

                // Загружаем материалы из MTL файла
                if (LoadMTL(mtlPath, materials)) {
                    DEBUG_LOG("MTL файл успешно загружен");
                }
                else {
                    DEBUG_WARNING("Не удалось загрузить MTL файл");
                }
            }
        }

        // Добавляем последний меш
        if (!currentMesh.vertices.empty()) {
            currentMesh.materialName = currentMaterial;
            meshes.push_back(currentMesh);
        }

        file.close();

        if (meshes.empty()) {
            CreateSimpleCubeModel(meshes);
        }

        char buffer[256];
        sprintf_s(buffer, "OBJ загружен: %zu мешей, %zu вершин в первом меше, %zu материалов",
            meshes.size(), meshes.empty() ? (size_t)0 : meshes[0].vertices.size(),
            materials.size());
        DEBUG_LOG(buffer);

        return !meshes.empty();
    }

    static bool LoadMTL(const std::wstring& filename, std::map<std::string, Material>& materials) {
        DEBUG_LOG_W(L"Загрузка MTL файла: " + filename);

        std::ifstream file(filename);
        if (!file.is_open()) {
            DEBUG_LOG_W(L"Не удалось открыть MTL файл: " + filename);
            return false;
        }

        Material currentMaterial;
        std::string line;

        while (std::getline(file, line)) {
            std::istringstream iss(line);
            std::string prefix;
            iss >> prefix;

            if (prefix == "newmtl") { // Новый материал
                if (!currentMaterial.name.empty()) {
                    materials[currentMaterial.name] = currentMaterial;
                }
                iss >> currentMaterial.name;
                currentMaterial = Material(); // Сброс значений по умолчанию
                currentMaterial.name = currentMaterial.name;

                DEBUG_LOG("Найден материал: " + currentMaterial.name);
            }
            else if (prefix == "Ka") { // Ambient color
                iss >> currentMaterial.ambient.x >> currentMaterial.ambient.y >> currentMaterial.ambient.z;
            }
            else if (prefix == "Kd") { // Diffuse color
                iss >> currentMaterial.diffuse.x >> currentMaterial.diffuse.y >> currentMaterial.diffuse.z;

                char buffer[256];
                sprintf_s(buffer, "  Цвет материала %s: (%.3f, %.3f, %.3f)",
                    currentMaterial.name.c_str(),
                    currentMaterial.diffuse.x,
                    currentMaterial.diffuse.y,
                    currentMaterial.diffuse.z);
                DEBUG_LOG(buffer);
            }
            else if (prefix == "Ks") { // Specular color
                iss >> currentMaterial.specular.x >> currentMaterial.specular.y >> currentMaterial.specular.z;
            }
            else if (prefix == "Ns") { // Shininess
                iss >> currentMaterial.shininess;
            }
            else if (prefix == "d" || prefix == "Tr") { // Alpha (transparency)
                iss >> currentMaterial.alpha;
            }
            else if (prefix == "map_Kd") { // Texture
                iss >> currentMaterial.textureFilename;
            }
        }

        // Добавляем последний материал
        if (!currentMaterial.name.empty()) {
            materials[currentMaterial.name] = currentMaterial;
        }

        file.close();

        char buffer[256];
        sprintf_s(buffer, "Загружено материалов: %zu", materials.size());
        DEBUG_LOG(buffer);

        return !materials.empty();
    }

private:
    static void ProcessFace(const std::string& faceStr,
        const std::vector<XMFLOAT3>& positions,
        const std::vector<XMFLOAT3>& normals,
        const std::vector<XMFLOAT2>& texcoords,
        Mesh& mesh,
        const std::string& currentMaterial,
        const std::map<std::string, Material>& materials) {

        std::istringstream fss(faceStr);
        std::string token;
        int indices[3] = { -1, -1, -1 };
        int idx = 0;

        while (std::getline(fss, token, '/') && idx < 3) {
            if (!token.empty()) {
                indices[idx] = std::stoi(token) - 1; // OBJ индексы начинаются с 1
            }
            idx++;
        }

        Vertex vertex;

        // Позиция
        if (indices[0] >= 0 && indices[0] < (int)positions.size()) {
            vertex.position = positions[indices[0]];
        }
        else {
            vertex.position = XMFLOAT3(0, 0, 0);
        }

        // Текстурные координаты
        if (indices[1] >= 0 && indices[1] < (int)texcoords.size()) {
            vertex.texcoord = texcoords[indices[1]];
        }
        else {
            vertex.texcoord = XMFLOAT2(0, 0);
        }

        // Нормаль
        if (indices[2] >= 0 && indices[2] < (int)normals.size()) {
            vertex.normal = normals[indices[2]];
        }
        else {
            // Вычисляем нормаль по умолчанию
            vertex.normal = XMFLOAT3(0, 1, 0);
        }

        // Цвет из материала
        if (!currentMaterial.empty() && materials.find(currentMaterial) != materials.end()) {
            const Material& mat = materials.at(currentMaterial);
            vertex.color = mat.diffuse; // Используем диффузный цвет

            // Отладочный вывод для первых вершин
            static int debugVertexCount = 0;
            if (debugVertexCount < 10) {
                char buffer[256];
                sprintf_s(buffer, "Вершина %d: материал '%s', цвет (%.3f, %.3f, %.3f)",
                    debugVertexCount, currentMaterial.c_str(),
                    vertex.color.x, vertex.color.y, vertex.color.z);
                DEBUG_LOG(buffer);
                debugVertexCount++;
            }
        }
        else {
            // Дефолтный цвет (белый)
            vertex.color = XMFLOAT3(1, 1, 1);
        }

        mesh.vertices.push_back(vertex);
        mesh.indices.push_back((uint32_t)mesh.indices.size());
    }

    static void CreateSimpleCubeModel(std::vector<Mesh>& meshes) {
        DEBUG_LOG("Создание простой кубической модели...");

        Mesh cubeMesh;

        // Вершины куба (8 вершин)
        Vertex vertices[] = {
            // Передняя грань
            Vertex(-1.0f, -1.0f, -1.0f, 0, 0, -1, 0, 1, 1, 0, 0),
            Vertex(1.0f, -1.0f, -1.0f, 0, 0, -1, 1, 1, 0, 1, 0),
            Vertex(1.0f,  1.0f, -1.0f, 0, 0, -1, 1, 0, 0, 0, 1),
            Vertex(-1.0f,  1.0f, -1.0f, 0, 0, -1, 0, 0, 1, 1, 0),

            // Задняя грань
            Vertex(-1.0f, -1.0f,  1.0f, 0, 0, 1, 1, 1, 1, 0, 0),
            Vertex(1.0f, -1.0f,  1.0f, 0, 0, 1, 0, 1, 0, 1, 0),
            Vertex(1.0f,  1.0f,  1.0f, 0, 0, 1, 0, 0, 0, 0, 1),
            Vertex(-1.0f,  1.0f,  1.0f, 0, 0, 1, 1, 0, 1, 1, 0),
        };

        // Индексы куба (12 треугольников)
        uint32_t indices[] = {
            // Передняя грань
            0, 1, 2, 2, 3, 0,
            // Задняя грань
            4, 5, 6, 6, 7, 4,
            // Верхняя грань
            3, 2, 6, 6, 7, 3,
            // Нижняя грань
            0, 1, 5, 5, 4, 0,
            // Левая грань
            0, 3, 7, 7, 4, 0,
            // Правая грань
            1, 2, 6, 6, 5, 1
        };

        cubeMesh.vertices.assign(vertices, vertices + 8);
        cubeMesh.indices.assign(indices, indices + 36);
        cubeMesh.materialName = "default";

        meshes.push_back(cubeMesh);
        DEBUG_LOG("Простая кубическая модель создана");
    }
};

// ==================== ТЕКСТУРНЫЙ МЕНЕДЖЕР ====================
class TextureManager {
private:
    std::map<std::wstring, Texture2D> textures;
    ID3D11Device* device = nullptr;

public:
    void Initialize(ID3D11Device* dev) {
        device = dev;
    }

    int LoadTexture(const std::wstring& filename) {
        // Ищем файл
        std::wstring foundPath = FileSystemHelper::FindFile(filename);
        if (foundPath.empty()) {
            DEBUG_WARNING("Не удалось найти текстуру, создаю дебажную");
            return CreateDebugTexture(filename);
        }

        // Проверяем, не загружена ли уже эта текстура
        auto it = textures.find(foundPath);
        if (it != textures.end()) {
            return (int)std::distance(textures.begin(), it);
        }

        // Загружаем новую текстуру
        Texture2D texture;
        if (texture.LoadFromFile(device, foundPath.c_str())) {
            textures[foundPath] = texture;
            DEBUG_LOG_W(L"Текстура загружена: " + foundPath);
            return (int)textures.size() - 1;
        }
        else {
            return CreateDebugTexture(filename);
        }
    }

    int CreateDebugTexture(const std::wstring& name) {
        Texture2D texture;
        if (texture.CreateDebugTexture(device, name.c_str())) {
            std::wstring debugName = L"[DEBUG]" + name;
            textures[debugName] = texture;
            DEBUG_LOG_W(L"Создана дебажная текстура: " + name);
            return (int)textures.size() - 1;
        }
        return -1;
    }

    int CreateColorTexture(const std::wstring& name, float r, float g, float b) {
        Texture2D texture;
        if (texture.CreateColorTexture(device, name.c_str(), r, g, b)) {
            std::wstring colorName = L"[COLOR]" + name;
            textures[colorName] = texture;

            char buffer[256];
            sprintf_s(buffer, "Создана цветная текстура %s: (%.3f, %.3f, %.3f)",
                std::string(name.begin(), name.end()).c_str(), r, g, b);
            DEBUG_LOG(buffer);

            return (int)textures.size() - 1;
        }
        return -1;
    }

    Texture2D* GetTexture(int index) {
        if (index < 0 || index >= (int)textures.size()) return nullptr;

        auto it = textures.begin();
        std::advance(it, index);
        return &it->second;
    }

    void Cleanup() {
        for (auto& pair : textures) {
            pair.second.Cleanup();
        }
        textures.clear();
    }
};

// ==================== ЗАГРУЗКА ТЕКСТУР ====================
bool Texture2D::LoadFromFile(ID3D11Device* device, const wchar_t* filename) {
    this->filename = filename;

    // Проверяем существование файла
    if (!FileSystemHelper::FileExists(filename)) {
        DEBUG_LOG_W(L"Файл не существует: " + std::wstring(filename));
        return false;
    }

    DEBUG_LOG_W(L"Загрузка текстуры: " + std::wstring(filename));

    // Инициализируем COM для WIC
    HRESULT hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    if (FAILED(hr) && hr != RPC_E_CHANGED_MODE) {
        DEBUG_ERROR("Ошибка инициализации COM");
        return false;
    }

    IWICImagingFactory* wicFactory = nullptr;
    hr = CoCreateInstance(CLSID_WICImagingFactory, nullptr,
        CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&wicFactory));
    if (FAILED(hr)) {
        DEBUG_ERROR("Ошибка создания WIC фабрики");
        CoUninitialize();
        return false;
    }

    IWICBitmapDecoder* decoder = nullptr;
    hr = wicFactory->CreateDecoderFromFilename(filename, nullptr,
        GENERIC_READ,
        WICDecodeMetadataCacheOnLoad,
        &decoder);
    if (FAILED(hr)) {
        DEBUG_ERROR("Ошибка создания декодера");
        wicFactory->Release();
        CoUninitialize();
        return false;
    }

    IWICBitmapFrameDecode* frame = nullptr;
    hr = decoder->GetFrame(0, &frame);
    if (FAILED(hr)) {
        DEBUG_ERROR("Ошибка получения фрейма");
        decoder->Release();
        wicFactory->Release();
        CoUninitialize();
        return false;
    }

    // Получаем размеры
    UINT w, h;
    frame->GetSize(&w, &h);
    width = (int)w;
    height = (int)h;

    // Конвертируем в RGBA
    IWICFormatConverter* converter = nullptr;
    hr = wicFactory->CreateFormatConverter(&converter);
    if (FAILED(hr)) {
        DEBUG_ERROR("Ошибка создания конвертера");
        frame->Release();
        decoder->Release();
        wicFactory->Release();
        CoUninitialize();
        return false;
    }

    hr = converter->Initialize(frame, GUID_WICPixelFormat32bppRGBA,
        WICBitmapDitherTypeNone, nullptr, 0.0,
        WICBitmapPaletteTypeCustom);
    if (FAILED(hr)) {
        DEBUG_ERROR("Ошибка инициализации конвертера");
        converter->Release();
        frame->Release();
        decoder->Release();
        wicFactory->Release();
        CoUninitialize();
        return false;
    }

    // Копируем пиксели
    std::vector<BYTE> pixels(width * height * 4);
    hr = converter->CopyPixels(nullptr, width * 4,
        (UINT)pixels.size(), pixels.data());
    if (FAILED(hr)) {
        DEBUG_ERROR("Ошибка копирования пикселей");
        converter->Release();
        frame->Release();
        decoder->Release();
        wicFactory->Release();
        CoUninitialize();
        return false;
    }

    // Создаем текстуру DirectX
    D3D11_TEXTURE2D_DESC texDesc = {};
    texDesc.Width = width;
    texDesc.Height = height;
    texDesc.MipLevels = 1;
    texDesc.ArraySize = 1;
    texDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    texDesc.SampleDesc.Count = 1;
    texDesc.SampleDesc.Quality = 0;
    texDesc.Usage = D3D11_USAGE_DEFAULT;
    texDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
    texDesc.CPUAccessFlags = 0;

    D3D11_SUBRESOURCE_DATA initData = {};
    initData.pSysMem = pixels.data();
    initData.SysMemPitch = width * 4;
    initData.SysMemSlicePitch = 0;

    hr = device->CreateTexture2D(&texDesc, &initData, &texture);
    if (FAILED(hr)) {
        DEBUG_ERROR("Ошибка создания текстуры DirectX");
        converter->Release();
        frame->Release();
        decoder->Release();
        wicFactory->Release();
        CoUninitialize();
        return false;
    }

    // Создаем шейдерный ресурс
    D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
    srvDesc.Format = texDesc.Format;
    srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
    srvDesc.Texture2D.MostDetailedMip = 0;
    srvDesc.Texture2D.MipLevels = 1;

    hr = device->CreateShaderResourceView(texture, &srvDesc, &srv);
    if (FAILED(hr)) {
        DEBUG_ERROR("Ошибка создания SRV");
        texture->Release();
        converter->Release();
        frame->Release();
        decoder->Release();
        wicFactory->Release();
        CoUninitialize();
        return false;
    }

    // Создаем сэмплер
    D3D11_SAMPLER_DESC sampDesc = {};
    sampDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
    sampDesc.AddressU = D3D11_TEXTURE_ADDRESS_WRAP;
    sampDesc.AddressV = D3D11_TEXTURE_ADDRESS_WRAP;
    sampDesc.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
    sampDesc.ComparisonFunc = D3D11_COMPARISON_NEVER;
    sampDesc.MinLOD = 0;
    sampDesc.MaxLOD = D3D11_FLOAT32_MAX;

    hr = device->CreateSamplerState(&sampDesc, &samplerState);
    if (FAILED(hr)) {
        DEBUG_ERROR("Ошибка создания сэмплера");
        srv->Release();
        texture->Release();
        converter->Release();
        frame->Release();
        decoder->Release();
        wicFactory->Release();
        CoUninitialize();
        return false;
    }

    // Освобождаем ресурсы
    converter->Release();
    frame->Release();
    decoder->Release();
    wicFactory->Release();
    CoUninitialize();

    char buffer[256];
    sprintf_s(buffer, "Текстура загружена успешно: %dx%d", width, height);
    DEBUG_SUCCESS(buffer);

    return true;
}

bool Texture2D::CreateDebugTexture(ID3D11Device* device, const wchar_t* name) {
    this->filename = L"[DEBUG]" + std::wstring(name);
    width = 256;
    height = 256;

    DEBUG_LOG_W(L"Создание дебажной текстуры: " + std::wstring(name));

    // Создаем пиксели для дебажной текстуры (шахматный паттерн)
    std::vector<BYTE> pixels(width * height * 4);

    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            int idx = (y * width + x) * 4;
            bool isChecker = ((x / 32) + (y / 32)) % 2 == 0;

            if (isChecker) {
                pixels[idx] = 255;     // Blue
                pixels[idx + 1] = 200; // Green
                pixels[idx + 2] = 150; // Red
            }
            else {
                pixels[idx] = 100;     // Blue
                pixels[idx + 1] = 150; // Green
                pixels[idx + 2] = 200; // Red
            }
            pixels[idx + 3] = 255; // Alpha
        }
    }

    // Создаем текстуру DirectX
    D3D11_TEXTURE2D_DESC texDesc = {};
    texDesc.Width = width;
    texDesc.Height = height;
    texDesc.MipLevels = 1;
    texDesc.ArraySize = 1;
    texDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    texDesc.SampleDesc.Count = 1;
    texDesc.SampleDesc.Quality = 0;
    texDesc.Usage = D3D11_USAGE_DEFAULT;
    texDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
    texDesc.CPUAccessFlags = 0;

    D3D11_SUBRESOURCE_DATA initData = {};
    initData.pSysMem = pixels.data();
    initData.SysMemPitch = width * 4;
    initData.SysMemSlicePitch = 0;

    HRESULT hr = device->CreateTexture2D(&texDesc, &initData, &texture);
    if (FAILED(hr)) {
        DEBUG_ERROR("Ошибка создания дебажной текстуры");
        return false;
    }

    // Создаем шейдерный ресурс
    D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
    srvDesc.Format = texDesc.Format;
    srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
    srvDesc.Texture2D.MostDetailedMip = 0;
    srvDesc.Texture2D.MipLevels = 1;

    hr = device->CreateShaderResourceView(texture, &srvDesc, &srv);
    if (FAILED(hr)) {
        DEBUG_ERROR("Ошибка создания SRV для дебажной текстуры");
        texture->Release();
        return false;
    }

    // Создаем сэмплер
    D3D11_SAMPLER_DESC sampDesc = {};
    sampDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
    sampDesc.AddressU = D3D11_TEXTURE_ADDRESS_WRAP;
    sampDesc.AddressV = D3D11_TEXTURE_ADDRESS_WRAP;
    sampDesc.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
    sampDesc.ComparisonFunc = D3D11_COMPARISON_NEVER;
    sampDesc.MinLOD = 0;
    sampDesc.MaxLOD = D3D11_FLOAT32_MAX;

    hr = device->CreateSamplerState(&sampDesc, &samplerState);
    if (FAILED(hr)) {
        DEBUG_ERROR("Ошибка создания сэмплера для дебажной текстуры");
        srv->Release();
        texture->Release();
        return false;
    }

    DEBUG_SUCCESS("Дебажная текстура создана");
    return true;
}

bool Texture2D::CreateColorTexture(ID3D11Device* device, const wchar_t* name, float r, float g, float b) {
    this->filename = L"[COLOR]" + std::wstring(name);
    width = 1;
    height = 1;

    DEBUG_LOG_W(L"Создание цветной текстуры: " + std::wstring(name));

    // Создаем пиксели для цветной текстуры (1x1 пиксель)
    std::vector<BYTE> pixels(4);
    pixels[0] = (BYTE)(b * 255);  // Blue
    pixels[1] = (BYTE)(g * 255);  // Green
    pixels[2] = (BYTE)(r * 255);  // Red
    pixels[3] = 255;              // Alpha

    // Создаем текстуру DirectX
    D3D11_TEXTURE2D_DESC texDesc = {};
    texDesc.Width = 1;
    texDesc.Height = 1;
    texDesc.MipLevels = 1;
    texDesc.ArraySize = 1;
    texDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    texDesc.SampleDesc.Count = 1;
    texDesc.SampleDesc.Quality = 0;
    texDesc.Usage = D3D11_USAGE_DEFAULT;
    texDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
    texDesc.CPUAccessFlags = 0;

    D3D11_SUBRESOURCE_DATA initData = {};
    initData.pSysMem = pixels.data();
    initData.SysMemPitch = 4;
    initData.SysMemSlicePitch = 0;

    HRESULT hr = device->CreateTexture2D(&texDesc, &initData, &texture);
    if (FAILED(hr)) {
        DEBUG_ERROR("Ошибка создания цветной текстуры");
        return false;
    }

    // Создаем шейдерный ресурс
    D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
    srvDesc.Format = texDesc.Format;
    srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
    srvDesc.Texture2D.MostDetailedMip = 0;
    srvDesc.Texture2D.MipLevels = 1;

    hr = device->CreateShaderResourceView(texture, &srvDesc, &srv);
    if (FAILED(hr)) {
        DEBUG_ERROR("Ошибка создания SRV для цветной текстуры");
        texture->Release();
        return false;
    }

    // Создаем сэмплер
    D3D11_SAMPLER_DESC sampDesc = {};
    sampDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
    sampDesc.AddressU = D3D11_TEXTURE_ADDRESS_WRAP;
    sampDesc.AddressV = D3D11_TEXTURE_ADDRESS_WRAP;
    sampDesc.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
    sampDesc.ComparisonFunc = D3D11_COMPARISON_NEVER;
    sampDesc.MinLOD = 0;
    sampDesc.MaxLOD = D3D11_FLOAT32_MAX;

    hr = device->CreateSamplerState(&sampDesc, &samplerState);
    if (FAILED(hr)) {
        DEBUG_ERROR("Ошибка создания сэмплера для цветной текстуры");
        srv->Release();
        texture->Release();
        return false;
    }

    DEBUG_SUCCESS("Цветная текстура создана");
    return true;
}

void Texture2D::Cleanup() {
    if (samplerState) samplerState->Release();
    if (srv) srv->Release();
    if (texture) texture->Release();
}

// ==================== 3D МОДЕЛЬ ====================
class Model3D
{
private:
    struct ModelMesh {
        ID3D11Buffer* vertexBuffer = nullptr;
        ID3D11Buffer* indexBuffer = nullptr;
        int textureIndex = -1;
        int indexCount = 0;
        int vertexCount = 0;
        std::string materialName;
    };

    std::vector<ModelMesh> meshes;
    XMFLOAT3 position = { 0, 0, 0 };
    XMFLOAT3 rotation = { 0, 0, 0 };
    XMFLOAT3 scale = { 1, 1, 1 };
    bool isVisible = true;
    bool hasError = false;

public:
    bool LoadFromOBJ(ID3D11Device* device, TextureManager& texManager,
        const std::wstring& objFile,
        const std::vector<std::wstring>& textureFiles) {
        DEBUG_LOG("Начало загрузки 3D модели...");

        // Ищем OBJ файл
        std::wstring foundObjPath = FileSystemHelper::FindFile(objFile + L".obj");
        if (foundObjPath.empty()) {
            // Пробуем без расширения
            foundObjPath = FileSystemHelper::FindFile(objFile);
        }

        if (foundObjPath.empty()) {
            DEBUG_WARNING("OBJ файл не найден, создаю простую модель");
            DEBUG_WARNING("Искали файл: " + std::string(objFile.begin(), objFile.end()) + ".obj");
            CreateSimpleHumanModel(device, texManager);
            hasError = true;
            return true;
        }

        DEBUG_LOG_W(L"OBJ найден: " + foundObjPath);

        // Загружаем OBJ с материалами
        std::vector<Mesh> loadedMeshes;
        std::map<std::string, Material> materials;

        if (!OBJLoader::Load(foundObjPath, loadedMeshes, materials)) {
            DEBUG_WARNING("Ошибка загрузки OBJ файла, создаю простую модель");
            CreateSimpleHumanModel(device, texManager);
            hasError = true;
            return true;
        }

        // Создаем текстуры для материалов
        std::map<std::string, int> materialTextures;
        for (const auto& matPair : materials) {
            const Material& mat = matPair.second;

            // Создаем цветную текстуру на основе диффузного цвета
            std::wstring matName(mat.name.begin(), mat.name.end());
            int texIndex = texManager.CreateColorTexture(matName,
                mat.diffuse.x,
                mat.diffuse.y,
                mat.diffuse.z);

            if (texIndex >= 0) {
                materialTextures[mat.name] = texIndex;

                char buffer[256];
                sprintf_s(buffer, "Создана текстура для материала %s: цвет (%.3f, %.3f, %.3f), индекс %d",
                    mat.name.c_str(), mat.diffuse.x, mat.diffuse.y, mat.diffuse.z, texIndex);
                DEBUG_LOG(buffer);
            }
        }

        // Если нет материалов, создаем дефолтную текстуру
        if (materialTextures.empty()) {
            int defaultTex = texManager.CreateDebugTexture(L"default_material");
            materialTextures["default"] = defaultTex;
            DEBUG_LOG("Создана дефолтная текстура");
        }

        // Создаем DirectX меши
        for (size_t i = 0; i < loadedMeshes.size(); i++) {
            ModelMesh dxMesh;
            dxMesh.materialName = loadedMeshes[i].materialName;

            char buffer[256];
            sprintf_s(buffer, "Создание меша %zu: %d вершин, %d индексов, материал: %s",
                i, loadedMeshes[i].vertices.size(), loadedMeshes[i].indices.size(),
                loadedMeshes[i].materialName.c_str());
            DEBUG_LOG(buffer);

            // Создаем вершинный буфер
            D3D11_BUFFER_DESC vbd = {};
            vbd.Usage = D3D11_USAGE_DEFAULT;
            vbd.ByteWidth = (UINT)(sizeof(Vertex) * loadedMeshes[i].vertices.size());
            vbd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
            vbd.CPUAccessFlags = 0;

            D3D11_SUBRESOURCE_DATA vinit = {};
            vinit.pSysMem = loadedMeshes[i].vertices.data();
            vinit.SysMemPitch = 0;
            vinit.SysMemSlicePitch = 0;

            HRESULT hr = device->CreateBuffer(&vbd, &vinit, &dxMesh.vertexBuffer);
            if (FAILED(hr)) {
                DEBUG_ERROR("Ошибка создания вершинного буфера");
                return false;
            }

            // Создаем индексный буфер
            D3D11_BUFFER_DESC ibd = {};
            ibd.Usage = D3D11_USAGE_DEFAULT;
            ibd.ByteWidth = (UINT)(sizeof(uint32_t) * loadedMeshes[i].indices.size());
            ibd.BindFlags = D3D11_BIND_INDEX_BUFFER;
            ibd.CPUAccessFlags = 0;

            D3D11_SUBRESOURCE_DATA iinit = {};
            iinit.pSysMem = loadedMeshes[i].indices.data();
            iinit.SysMemPitch = 0;
            iinit.SysMemSlicePitch = 0;

            hr = device->CreateBuffer(&ibd, &iinit, &dxMesh.indexBuffer);
            if (FAILED(hr)) {
                DEBUG_ERROR("Ошибка создания индексного буфера");
                dxMesh.vertexBuffer->Release();
                return false;
            }

            dxMesh.indexCount = (int)loadedMeshes[i].indices.size();
            dxMesh.vertexCount = (int)loadedMeshes[i].vertices.size();

            // Назначаем текстуру на основе материала
            if (!dxMesh.materialName.empty() && materialTextures.find(dxMesh.materialName) != materialTextures.end()) {
                dxMesh.textureIndex = materialTextures[dxMesh.materialName];
                sprintf_s(buffer, "Меш %zu: текстура %d для материала '%s'",
                    i, dxMesh.textureIndex, dxMesh.materialName.c_str());
                DEBUG_LOG(buffer);
            }
            else if (!materialTextures.empty()) {
                // Берем первую доступную текстуру
                dxMesh.textureIndex = materialTextures.begin()->second;
                sprintf_s(buffer, "Меш %zu: дефолтная текстура %d", i, dxMesh.textureIndex);
                DEBUG_LOG(buffer);
            }
            else {
                dxMesh.textureIndex = texManager.CreateDebugTexture(L"fallback");
                sprintf_s(buffer, "Меш %zu: резервная текстура %d", i, dxMesh.textureIndex);
                DEBUG_LOG(buffer);
            }

            meshes.push_back(dxMesh);
        }

        char buffer[256];
        sprintf_s(buffer, "Модель загружена: %zu мешей, %zu материалов",
            meshes.size(), materials.size());
        DEBUG_SUCCESS(buffer);

        // Выводим информацию о цветах для отладки
        for (const auto& matPair : materials) {
            const Material& mat = matPair.second;
            sprintf_s(buffer, "Материал '%s': цвет (%.3f, %.3f, %.3f)",
                mat.name.c_str(), mat.diffuse.x, mat.diffuse.y, mat.diffuse.z);
            DEBUG_LOG(buffer);
        }

        return true;
    }

    void CreateSimpleHumanModel(ID3D11Device* device, TextureManager& texManager) {
        DEBUG_LOG("Создание простой человеческой модели...");

        // Создаем один меш для простой модели
        ModelMesh humanMesh;

        // Создаем простую человеческую фигуру (торс + голова)
        std::vector<Vertex> vertices;
        std::vector<uint32_t> indices;

        // Торс (куб)
        CreateBox(vertices, indices, 0, 0, 0, 1.0f, 2.0f, 0.5f, XMFLOAT3(0.8f, 0.6f, 0.4f));

        // Голова (куб)
        CreateBox(vertices, indices, 0, 1.5f, 0, 0.8f, 0.8f, 0.8f, XMFLOAT3(0.9f, 0.8f, 0.7f));

        // Руки (2 куба)
        CreateBox(vertices, indices, -1.2f, 0.5f, 0, 0.3f, 1.5f, 0.3f, XMFLOAT3(0.7f, 0.5f, 0.3f));
        CreateBox(vertices, indices, 1.2f, 0.5f, 0, 0.3f, 1.5f, 0.3f, XMFLOAT3(0.7f, 0.5f, 0.3f));

        // Ноги (2 куба)
        CreateBox(vertices, indices, -0.4f, -1.5f, 0, 0.4f, 1.5f, 0.4f, XMFLOAT3(0.3f, 0.2f, 0.1f));
        CreateBox(vertices, indices, 0.4f, -1.5f, 0, 0.4f, 1.5f, 0.4f, XMFLOAT3(0.3f, 0.2f, 0.1f));

        // Создаем вершинный буфер
        D3D11_BUFFER_DESC vbd = {};
        vbd.Usage = D3D11_USAGE_DEFAULT;
        vbd.ByteWidth = (UINT)(sizeof(Vertex) * vertices.size());
        vbd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
        vbd.CPUAccessFlags = 0;

        D3D11_SUBRESOURCE_DATA vinit = {};
        vinit.pSysMem = vertices.data();

        HRESULT hr = device->CreateBuffer(&vbd, &vinit, &humanMesh.vertexBuffer);
        if (FAILED(hr)) {
            DEBUG_ERROR("Ошибка создания вершинного буфера для простой модели");
            return;
        }

        // Создаем индексный буфер
        D3D11_BUFFER_DESC ibd = {};
        ibd.Usage = D3D11_USAGE_DEFAULT;
        ibd.ByteWidth = (UINT)(sizeof(uint32_t) * indices.size());
        ibd.BindFlags = D3D11_BIND_INDEX_BUFFER;
        ibd.CPUAccessFlags = 0;

        D3D11_SUBRESOURCE_DATA iinit = {};
        iinit.pSysMem = indices.data();

        hr = device->CreateBuffer(&ibd, &iinit, &humanMesh.indexBuffer);
        if (FAILED(hr)) {
            DEBUG_ERROR("Ошибка создания индексного буфера для простой модели");
            humanMesh.vertexBuffer->Release();
            return;
        }

        humanMesh.indexCount = (int)indices.size();
        humanMesh.vertexCount = (int)vertices.size();
        humanMesh.textureIndex = texManager.CreateDebugTexture(L"human");
        humanMesh.materialName = "human_material";

        meshes.push_back(humanMesh);

        char buffer[256];
        sprintf_s(buffer, "Простая модель создана: %d вершин, %d индексов",
            humanMesh.vertexCount, humanMesh.indexCount);
        DEBUG_SUCCESS(buffer);
    }

    void CreateBox(std::vector<Vertex>& vertices, std::vector<uint32_t>& indices,
        float x, float y, float z,
        float width, float height, float depth,
        const XMFLOAT3& color) {
        float hw = width / 2;
        float hh = height / 2;
        float hd = depth / 2;

        int startIndex = (int)vertices.size();

        // Передняя грань
        vertices.push_back(Vertex(x - hw, y - hh, z - hd, 0, 0, -1, 0, 1, color.x, color.y, color.z));
        vertices.push_back(Vertex(x + hw, y - hh, z - hd, 0, 0, -1, 1, 1, color.x, color.y, color.z));
        vertices.push_back(Vertex(x + hw, y + hh, z - hd, 0, 0, -1, 1, 0, color.x, color.y, color.z));
        vertices.push_back(Vertex(x - hw, y + hh, z - hd, 0, 0, -1, 0, 0, color.x, color.y, color.z));

        // Задняя грань
        vertices.push_back(Vertex(x - hw, y - hh, z + hd, 0, 0, 1, 1, 1, color.x, color.y, color.z));
        vertices.push_back(Vertex(x + hw, y - hh, z + hd, 0, 0, 1, 0, 1, color.x, color.y, color.z));
        vertices.push_back(Vertex(x + hw, y + hh, z + hd, 0, 0, 1, 0, 0, color.x, color.y, color.z));
        vertices.push_back(Vertex(x - hw, y + hh, z + hd, 0, 0, 1, 1, 0, color.x, color.y, color.z));

        // Левая грань
        vertices.push_back(Vertex(x - hw, y - hh, z + hd, -1, 0, 0, 0, 1, color.x * 0.8f, color.y * 0.8f, color.z * 0.8f));
        vertices.push_back(Vertex(x - hw, y - hh, z - hd, -1, 0, 0, 1, 1, color.x * 0.8f, color.y * 0.8f, color.z * 0.8f));
        vertices.push_back(Vertex(x - hw, y + hh, z - hd, -1, 0, 0, 1, 0, color.x * 0.8f, color.y * 0.8f, color.z * 0.8f));
        vertices.push_back(Vertex(x - hw, y + hh, z + hd, -1, 0, 0, 0, 0, color.x * 0.8f, color.y * 0.8f, color.z * 0.8f));

        // Правая грань
        vertices.push_back(Vertex(x + hw, y - hh, z - hd, 1, 0, 0, 0, 1, color.x * 0.8f, color.y * 0.8f, color.z * 0.8f));
        vertices.push_back(Vertex(x + hw, y - hh, z + hd, 1, 0, 0, 1, 1, color.x * 0.8f, color.y * 0.8f, color.z * 0.8f));
        vertices.push_back(Vertex(x + hw, y + hh, z + hd, 1, 0, 0, 1, 0, color.x * 0.8f, color.y * 0.8f, color.z * 0.8f));
        vertices.push_back(Vertex(x + hw, y + hh, z - hd, 1, 0, 0, 0, 0, color.x * 0.8f, color.y * 0.8f, color.z * 0.8f));

        // Верхняя грань
        vertices.push_back(Vertex(x - hw, y + hh, z - hd, 0, 1, 0, 0, 1, color.x * 0.9f, color.y * 0.9f, color.z * 0.9f));
        vertices.push_back(Vertex(x + hw, y + hh, z - hd, 0, 1, 0, 1, 1, color.x * 0.9f, color.y * 0.9f, color.z * 0.9f));
        vertices.push_back(Vertex(x + hw, y + hh, z + hd, 0, 1, 0, 1, 0, color.x * 0.9f, color.y * 0.9f, color.z * 0.9f));
        vertices.push_back(Vertex(x - hw, y + hh, z + hd, 0, 1, 0, 0, 0, color.x * 0.9f, color.y * 0.9f, color.z * 0.9f));

        // Нижняя грань
        vertices.push_back(Vertex(x - hw, y - hh, z + hd, 0, -1, 0, 0, 1, color.x * 0.6f, color.y * 0.6f, color.z * 0.6f));
        vertices.push_back(Vertex(x + hw, y - hh, z + hd, 0, -1, 0, 1, 1, color.x * 0.6f, color.y * 0.6f, color.z * 0.6f));
        vertices.push_back(Vertex(x + hw, y - hh, z - hd, 0, -1, 0, 1, 0, color.x * 0.6f, color.y * 0.6f, color.z * 0.6f));
        vertices.push_back(Vertex(x - hw, y - hh, z - hd, 0, -1, 0, 0, 0, color.x * 0.6f, color.y * 0.6f, color.z * 0.6f));

        // Индексы для 6 граней (2 треугольника на грань)
        for (int face = 0; face < 6; face++) {
            int base = startIndex + face * 4;
            indices.push_back(base);
            indices.push_back(base + 1);
            indices.push_back(base + 2);
            indices.push_back(base);
            indices.push_back(base + 2);
            indices.push_back(base + 3);
        }
    }

    void Render(ID3D11DeviceContext* context, TextureManager& texManager) {
        if (!isVisible) return;

        // Отладочная информация для первого кадра
        static bool firstRender = true;
        if (firstRender) {
            char buffer[256];
            sprintf_s(buffer, "Рендеринг модели: %zu мешей", meshes.size());
            DEBUG_LOG(buffer);
            firstRender = false;
        }

        for (size_t i = 0; i < meshes.size(); i++) {
            const auto& mesh = meshes[i];
            if (!mesh.vertexBuffer || !mesh.indexBuffer) continue;

            // Устанавливаем вершинный и индексный буферы
            UINT stride = sizeof(Vertex);
            UINT offset = 0;
            context->IASetVertexBuffers(0, 1, &mesh.vertexBuffer, &stride, &offset);
            context->IASetIndexBuffer(mesh.indexBuffer, DXGI_FORMAT_R32_UINT, 0);
            context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

            // Устанавливаем текстуру, если есть
            if (mesh.textureIndex >= 0) {
                Texture2D* texture = texManager.GetTexture(mesh.textureIndex);
                if (texture && texture->srv && texture->samplerState) {
                    context->PSSetShaderResources(0, 1, &texture->srv);
                    context->PSSetSamplers(0, 1, &texture->samplerState);
                }
            }

            // Отрисовываем
            context->DrawIndexed(mesh.indexCount, 0, 0);
        }
    }

    void SetPosition(float x, float y, float z) {
        position = { x, y, z };
    }

    void SetRotation(float x, float y, float z) {
        rotation = { x, y, z };
    }

    // Добавляем метод для получения текущего поворота
    XMFLOAT3 GetRotation() const { return rotation; }

    void SetScale(float x, float y, float z) {
        scale = { x, y, z };
    }

    XMMATRIX GetWorldMatrix() const {
        return XMMatrixScaling(scale.x, scale.y, scale.z)
            * XMMatrixRotationRollPitchYaw(rotation.x, rotation.y, rotation.z)
            * XMMatrixTranslation(position.x, position.y, position.z);
    }

    XMFLOAT3 GetPosition() const { return position; }

    void Move(float dx, float dy, float dz) {
        position.x += dx;
        position.y += dy;
        position.z += dz;
    }

    void Cleanup() {
        for (auto& mesh : meshes) {
            if (mesh.indexBuffer) mesh.indexBuffer->Release();
            if (mesh.vertexBuffer) mesh.vertexBuffer->Release();
        }
        meshes.clear();
    }
};
class AnimatedModel3D : public Model3D {
private:
    SimpleAnimator animator;
    bool hasAnimation = false;

public:
    void SetAnimationEnabled(bool enabled) {
        if (enabled && !animator.IsWalking()) {
            animator.StartWalking();
        }
        else if (!enabled && animator.IsWalking()) {
            animator.StopWalking();
        }
        hasAnimation = enabled;
    }

    void UpdateAnimation(float deltaTime) {
        if (!hasAnimation) return;

        XMFLOAT3 pos = GetPosition();
        XMFLOAT3 rot = GetRotation();
        XMFLOAT3 scale = XMFLOAT3(1, 1, 1); // Масштаб не меняем

        animator.Update(deltaTime, pos, rot, scale);

        SetPosition(pos.x, pos.y, pos.z);
        SetRotation(rot.x, rot.y, rot.z);
    }

    void InitializeAnimation(const XMFLOAT3& startPos) {
        animator.SetStartTransform(startPos, XMFLOAT3(0, 0, 0), XMFLOAT3(1, 1, 1));
        animator.SetWalkCycleTime(0.8f);  // Быстрая ходьба
        animator.SetAmplitudes(0.15f, 0.08f, 0.05f);
    }

    bool IsAnimating() const { return animator.IsWalking(); }
};

// ==================== ШЕЙДЕРЫ ====================
class ShaderManager {
private:
    ID3D11VertexShader* vertexShader = nullptr;
    ID3D11PixelShader* pixelShader = nullptr;
    ID3D11InputLayout* inputLayout = nullptr;
    ID3D11Buffer* constantBuffer = nullptr;
    ID3D11RasterizerState* rasterizerState = nullptr;

public:
    bool Initialize(ID3D11Device* device) {
        DEBUG_LOG("Инициализация шейдеров...");

        // Вершинный шейдер
        const char* vsCode = R"(
            cbuffer MatrixBuffer {
                float4x4 world;
                float4x4 view;
                float4x4 proj;
                float3 lightDir;
                float padding;
            };
            
            struct VS_IN {
                float3 pos : POSITION;
                float3 normal : NORMAL;
                float2 tex : TEXCOORD;
                float3 color : COLOR;
            };
            
            struct VS_OUT {
                float4 pos : SV_POSITION;
                float2 tex : TEXCOORD0;
                float3 color : COLOR;
                float3 normal : NORMAL;
            };
            
            VS_OUT main(VS_IN input) {
                VS_OUT output;
                output.pos = mul(float4(input.pos, 1.0), world);
                output.pos = mul(output.pos, view);
                output.pos = mul(output.pos, proj);
                output.tex = input.tex;
                output.color = input.color;
                output.normal = mul(input.normal, (float3x3)world);
                return output;
            }
        )";

        // Пиксельный шейдер
        const char* psCode = R"(
            Texture2D tex : register(t0);
            SamplerState sam : register(s0);
            
            struct PS_IN {
                float4 pos : SV_POSITION;
                float2 tex : TEXCOORD0;
                float3 color : COLOR;
                float3 normal : NORMAL;
            };
            
            float4 main(PS_IN input) : SV_TARGET {
                float4 textureColor = tex.Sample(sam, input.tex);
                
                if (textureColor.a < 0.1) discard;
                
                float3 lightDir = normalize(float3(1.0, 1.0, 0.5));
                float diff = max(dot(normalize(input.normal), lightDir), 0.2);
                float3 diffuse = diff * float3(1.0, 1.0, 1.0);
                
                // Смешиваем цвет текстуры с цветом вершины
                return textureColor * float4(input.color * diffuse, 1.0);
            }
        )";

        // Компилируем шейдеры
        ID3DBlob* vsBlob = nullptr;
        ID3DBlob* psBlob = nullptr;
        ID3DBlob* errorBlob = nullptr;

        HRESULT hr = D3DCompile(vsCode, strlen(vsCode), nullptr, nullptr, nullptr,
            "main", "vs_5_0", D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION, 0, &vsBlob, &errorBlob);

        if (FAILED(hr)) {
            if (errorBlob) {
                DEBUG_ERROR((char*)errorBlob->GetBufferPointer());
                errorBlob->Release();
            }
            DEBUG_ERROR("Ошибка компиляции вершинного шейдера");
            return false;
        }

        hr = D3DCompile(psCode, strlen(psCode), nullptr, nullptr, nullptr,
            "main", "ps_5_0", D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION, 0, &psBlob, &errorBlob);

        if (FAILED(hr)) {
            if (errorBlob) {
                DEBUG_ERROR((char*)errorBlob->GetBufferPointer());
                errorBlob->Release();
            }
            DEBUG_ERROR("Ошибка компиляции пиксельного шейдера");
            vsBlob->Release();
            return false;
        }

        // Создаем шейдеры
        hr = device->CreateVertexShader(vsBlob->GetBufferPointer(),
            vsBlob->GetBufferSize(),
            nullptr, &vertexShader);
        if (FAILED(hr)) {
            DEBUG_ERROR("Ошибка создания вершинного шейдера");
            vsBlob->Release();
            psBlob->Release();
            return false;
        }

        hr = device->CreatePixelShader(psBlob->GetBufferPointer(),
            psBlob->GetBufferSize(),
            nullptr, &pixelShader);
        if (FAILED(hr)) {
            DEBUG_ERROR("Ошибка создания пиксельного шейдера");
            vsBlob->Release();
            psBlob->Release();
            return false;
        }

        // Создаем input layout
        D3D11_INPUT_ELEMENT_DESC layout[] = {
            {"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0},
            {"NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0},
            {"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 24, D3D11_INPUT_PER_VERTEX_DATA, 0},
            {"COLOR", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 32, D3D11_INPUT_PER_VERTEX_DATA, 0}
        };

        hr = device->CreateInputLayout(layout, 4,
            vsBlob->GetBufferPointer(),
            vsBlob->GetBufferSize(),
            &inputLayout);

        vsBlob->Release();
        psBlob->Release();

        if (FAILED(hr)) {
            DEBUG_ERROR("Ошибка создания input layout");
            return false;
        }

        // Создаем константный буфер
        D3D11_BUFFER_DESC cbDesc = {};
        cbDesc.ByteWidth = sizeof(XMMATRIX) * 3 + sizeof(XMFLOAT4);
        cbDesc.Usage = D3D11_USAGE_DYNAMIC;
        cbDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
        cbDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

        hr = device->CreateBuffer(&cbDesc, nullptr, &constantBuffer);
        if (FAILED(hr)) {
            DEBUG_ERROR("Ошибка создания константного буфера");
            return false;
        }

        // Создаем rasterizer state (чтобы видеть обе стороны полигонов)
        D3D11_RASTERIZER_DESC rsDesc = {};
        rsDesc.FillMode = D3D11_FILL_SOLID;
        rsDesc.CullMode = D3D11_CULL_NONE; // Не отбрасывать обратные стороны
        rsDesc.FrontCounterClockwise = FALSE;
        rsDesc.DepthClipEnable = TRUE;

        hr = device->CreateRasterizerState(&rsDesc, &rasterizerState);
        if (FAILED(hr)) {
            DEBUG_ERROR("Ошибка создания rasterizer state");
            return false;
        }

        DEBUG_SUCCESS("Шейдеры инициализированы");
        return true;
    }

    void SetShaderParameters(ID3D11DeviceContext* context,
        const XMMATRIX& world,
        const XMMATRIX& view,
        const XMMATRIX& proj) {
        D3D11_MAPPED_SUBRESOURCE mapped;
        if (SUCCEEDED(context->Map(constantBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped))) {
            XMMATRIX* matrices = (XMMATRIX*)mapped.pData;
            matrices[0] = XMMatrixTranspose(world);
            matrices[1] = XMMatrixTranspose(view);
            matrices[2] = XMMatrixTranspose(proj);

            float* lightDir = (float*)(matrices + 3);
            lightDir[0] = 1.0f;
            lightDir[1] = 1.0f;
            lightDir[2] = 0.5f;
            lightDir[3] = 0.0f;

            context->Unmap(constantBuffer, 0);
        }

        context->VSSetConstantBuffers(0, 1, &constantBuffer);
    }

    void Apply(ID3D11DeviceContext* context) {
        context->VSSetShader(vertexShader, nullptr, 0);
        context->PSSetShader(pixelShader, nullptr, 0);
        context->IASetInputLayout(inputLayout);
        context->RSSetState(rasterizerState);
    }

    void Cleanup() {
        if (rasterizerState) rasterizerState->Release();
        if (constantBuffer) constantBuffer->Release();
        if (inputLayout) inputLayout->Release();
        if (pixelShader) pixelShader->Release();
        if (vertexShader) vertexShader->Release();
    }

};

// ==================== ИЗОМЕТРИЧЕСКАЯ КАМЕРА ====================
class IsometricCamera {
private:
    XMFLOAT3 target = { 0, 0, 0 };
    float angle = XM_PIDIV4; // 45 градусов
    float elevation = XM_PIDIV4; // 45 градусов вверх (истинная изометрия)
    float distance = 20.0f;
    float height = 10.0f;

public:
    XMMATRIX GetViewMatrix() const {
        XMVECTOR eye = XMVectorSet(
            target.x + distance * cosf(angle),
            target.y + height,
            target.z + distance * sinf(angle),
            0.0f
        );
        XMVECTOR at = XMVectorSet(target.x, target.y, target.z, 0.0f);
        XMVECTOR up = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);

        return XMMatrixLookAtLH(eye, at, up);
    }

    XMMATRIX GetProjectionMatrix(float aspectRatio) const {
        // ИЗОМЕТРИЧЕСКАЯ ПРОЕКЦИЯ (ортогональная)
        float viewWidth = 40.0f;
        float viewHeight = viewWidth / aspectRatio;
        return XMMatrixOrthographicLH(viewWidth, viewHeight, 0.1f, 100.0f);
    }

    void SetTarget(const XMFLOAT3& newTarget) {
        target = newTarget;
    }

    void Rotate(float deltaAngle) {
        angle += deltaAngle;
    }

    void Zoom(float delta) {
        distance += delta;
        distance = std::max<float>(5.0f, std::min<float>(50.0f, distance));
    }
};
// ==================== ИЗОМЕТРИЧЕСКИЙ ФОН (2D КАРТИНКА) ====================
class IsometricBackground {
private:
    ID3D11Buffer* vertexBuffer = nullptr;
    ID3D11Buffer* indexBuffer = nullptr;
    Texture2D backgroundTexture;
    XMFLOAT3 position = { 0, 0, 0 };
    float size = 40.0f; // Размер соответствует камере

public:
    bool Initialize(ID3D11Device* device, const wchar_t* textureFilename) {
        DEBUG_LOG("Инициализация фона с картинкой...");

        if (!textureFilename || wcslen(textureFilename) == 0) {
            DEBUG_WARNING("Не указано имя файла фона, создаем простой фон");
            // Создаем дебажную текстуру
            backgroundTexture.CreateDebugTexture(device, L"background");
        }
        else {
            // Ищем файл с разными расширениями
            std::wstring filename = textureFilename;
            std::vector<std::wstring> extensions = { L"", L".jpg", L".jpeg", L".png", L".bmp", L".dds" };

            bool textureLoaded = false;

            for (const auto& ext : extensions) {
                std::wstring fullFilename = filename + ext;
                std::wstring foundPath = FileSystemHelper::FindImageFile(fullFilename);
                if (foundPath.empty()) {
                    foundPath = FileSystemHelper::FindFile(fullFilename);
                }

                if (!foundPath.empty()) {
                    DEBUG_LOG_W(L"Найден файл фона: " + foundPath);

                    if (backgroundTexture.LoadFromFile(device, foundPath.c_str())) {
                        textureLoaded = true;
                        DEBUG_SUCCESS("Текстура фона успешно загружена");
                        break;
                    }
                }
            }

            if (!textureLoaded) {
                DEBUG_WARNING("Не удалось загрузить текстуру фона, создаем простой фон");
                backgroundTexture.CreateDebugTexture(device, L"background");
            }
        }

        // Создаем геометрию фона (большая плоскость)
        CreateGeometry(device);

        DEBUG_SUCCESS("Фон инициализирован");
        return true;
    }

    void CreateGeometry(ID3D11Device* device) {
        DEBUG_LOG("Создание геометрии фона...");

        std::vector<Vertex> vertices;
        std::vector<uint32_t> indices;

        float halfSize = size / 2.0f;
        float height = -1.0f; // Чуть ниже уровня пола для моделей

        // Создаем одну большую плоскость
        // Вершины в порядке против часовой стрелки
        vertices.push_back(Vertex(
            -halfSize, height, -halfSize,  // Позиция
            0, 1, 0,                       // Нормаль (вверх)
            0.0f, 1.0f,                    // UV координаты (нижний левый угол)
            1, 1, 1                        // Цвет (белый)
        ));

        vertices.push_back(Vertex(
            halfSize, height, -halfSize,
            0, 1, 0,
            1.0f, 1.0f,
            1, 1, 1
        ));

        vertices.push_back(Vertex(
            halfSize, height, halfSize,
            0, 1, 0,
            1.0f, 0.0f,
            1, 1, 1
        ));

        vertices.push_back(Vertex(
            -halfSize, height, halfSize,
            0, 1, 0,
            0.0f, 0.0f,
            1, 1, 1
        ));

        // Индексы для двух треугольников
        indices = { 0, 1, 2, 0, 2, 3 };

        // Создаем вершинный буфер
        D3D11_BUFFER_DESC vbd = {};
        vbd.Usage = D3D11_USAGE_DEFAULT;
        vbd.ByteWidth = (UINT)(sizeof(Vertex) * vertices.size());
        vbd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
        vbd.CPUAccessFlags = 0;

        D3D11_SUBRESOURCE_DATA vinit = {};
        vinit.pSysMem = vertices.data();

        HRESULT hr = device->CreateBuffer(&vbd, &vinit, &vertexBuffer);
        if (FAILED(hr)) {
            DEBUG_ERROR("Ошибка создания вершинного буфера фона");
            return;
        }

        // Создаем индексный буфер
        D3D11_BUFFER_DESC ibd = {};
        ibd.Usage = D3D11_USAGE_DEFAULT;
        ibd.ByteWidth = (UINT)(sizeof(uint32_t) * indices.size());
        ibd.BindFlags = D3D11_BIND_INDEX_BUFFER;
        ibd.CPUAccessFlags = 0;

        D3D11_SUBRESOURCE_DATA iinit = {};
        iinit.pSysMem = indices.data();

        hr = device->CreateBuffer(&ibd, &iinit, &indexBuffer);
        if (FAILED(hr)) {
            DEBUG_ERROR("Ошибка создания индексного буфера фона");
            vertexBuffer->Release();
            return;
        }

        DEBUG_LOG("Геометрия фона создана");
    }

    void Render(ID3D11DeviceContext* context) {
        if (!vertexBuffer || !indexBuffer || !backgroundTexture.srv) {
            return;
        }

        // Устанавливаем текстуру
        context->PSSetShaderResources(0, 1, &backgroundTexture.srv);
        context->PSSetSamplers(0, 1, &backgroundTexture.samplerState);

        // Устанавливаем буферы
        UINT stride = sizeof(Vertex);
        UINT offset = 0;
        context->IASetVertexBuffers(0, 1, &vertexBuffer, &stride, &offset);
        context->IASetIndexBuffer(indexBuffer, DXGI_FORMAT_R32_UINT, 0);
        context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

        // Рендерим
        context->DrawIndexed(6, 0, 0);
    }

    XMMATRIX GetWorldMatrix() const {
        // Фон всегда плоский и лежит на полу
        return XMMatrixTranslation(position.x, position.y, position.z);
    }

    void Cleanup() {
        backgroundTexture.Cleanup();
        if (indexBuffer) indexBuffer->Release();
        if (vertexBuffer) vertexBuffer->Release();
    }
};
// ==================== ИГРОВАЯ СЦЕНА ====================
class GameScene {
private:
    Model3D player;
    IsometricCamera camera;
    ShaderManager shader;
    TextureManager textures;
    ID3D11Device* device = nullptr;
    ID3D11DeviceContext* context = nullptr;

    // Добавляем фон
    IsometricBackground background;

    float playerSpeed = 10.0f;
    float rotationSpeed = 3.0f;
    float currentRotation = XM_PI; // Начинаем смотрит на камеру

public:
    bool Initialize(ID3D11Device* dev, ID3D11DeviceContext* ctx) {
        device = dev;
        context = ctx;

        DEBUG_LOG("=== ИНИЦИАЛИЗАЦИЯ ИГРОВОЙ СЦЕНЫ ===");

        // Выводим информацию о текущей директории
        std::wstring exeDir = FileSystemHelper::GetExecutableDirectory();
        DEBUG_LOG_W(L"Директория EXE: " + exeDir);

        // Показываем файлы в директории для отладки
        DEBUG_LOG("Содержимое директории:");
        FileSystemHelper::ListFilesInDirectory(exeDir);

        // Инициализируем шейдеры
        if (!shader.Initialize(device)) {
            DEBUG_ERROR("Ошибка инициализации шейдеров");
            return false;
        }

        // Инициализируем менеджер текстур
        textures.Initialize(device);

        // Инициализируем фон
        DEBUG_LOG("Загрузка фона...");
        background.Initialize(device, L"background");

        // Загружаем модель character2
        DEBUG_LOG("Попытка загрузки модели X_Bot.fbx...");
        std::wstring objFile = L"character2";

        // Загружаем модель (список текстур пустой, так как используем материалы из MTL)
        std::vector<std::wstring> textureFiles = {
            // Пусто - текстуры будут созданы из материалов
        };

        if (!player.LoadFromOBJ(device, textures, objFile, textureFiles)) {
            DEBUG_ERROR("Критическая ошибка загрузки модели");
            return false;
        }

        // Настраиваем игрока - ОЧЕНЬ МАЛЕНЬКИЙ МАСШТАБ для моделей из Blender!
        player.SetPosition(0, 0, 0);
        player.SetScale(0.001f, 0.001f, 0.001f); // 0.1% от исходного размера
        player.SetRotation(0, currentRotation, 0);

        // Настраиваем камеру
        camera.SetTarget(player.GetPosition());

        DEBUG_SUCCESS("Игровая сцена инициализирована");

        // Добавим подсказку для пользователя
        DEBUG_LOG("========================================");
        DEBUG_LOG("ЕСЛИ МОДЕЛЬ НЕ ВИДНА, ПОПРОБУЙТЕ:");
        DEBUG_LOG("1. Нажать 1/2/3/4 для изменения масштаба");
        DEBUG_LOG("  1 - 0.001, 2 - 0.01, 3 - 0.1, 4 - 1.0");
        DEBUG_LOG("2. Проверить Debug Output в Visual Studio");
        DEBUG_LOG("3. Убедиться что character2.obj и character2.mtl в папке с EXE");
        DEBUG_LOG("========================================");

        return true;
    }

    void Update(float deltaTime) {
        // Управление игроком (изометрическое)
        bool isMoving = false;
        XMFLOAT3 moveDir = { 0, 0, 0 };
        float targetRotation = currentRotation;

        // Определяем направление движения
        int dirX = 0, dirZ = 0;

        if (GetAsyncKeyState('W') & 0x8000) {
            moveDir.x += -playerSpeed * deltaTime;
            moveDir.z += -playerSpeed * deltaTime;
            dirZ -= 1;
            isMoving = true;
        }
        if (GetAsyncKeyState('S') & 0x8000) {
            moveDir.x += playerSpeed * deltaTime;
            moveDir.z += playerSpeed * deltaTime;
            dirZ += 1;
            isMoving = true;
        }
        if (GetAsyncKeyState('A') & 0x8000) {
            moveDir.x += playerSpeed * deltaTime;
            moveDir.z += -playerSpeed * deltaTime;
            dirX -= 1;
            isMoving = true;
        }
        if (GetAsyncKeyState('D') & 0x8000) {
            moveDir.x += -playerSpeed * deltaTime;
            moveDir.z += playerSpeed * deltaTime;
            dirX += 1;
            isMoving = true;
        }

        // Применяем движение
        if (isMoving) {
            player.Move(moveDir.x, moveDir.y, moveDir.z);

            // Вычисляем целевой поворот на основе направления движения
            if (dirX != 0 || dirZ != 0) {
                // Для изометрических направлений
                if (dirX == 0 && dirZ == -1) targetRotation = -XM_PIDIV2;      // Север
                else if (dirX == 1 && dirZ == -1) targetRotation = -XM_PIDIV4;    // Северо-восток
                else if (dirX == 1 && dirZ == 0) targetRotation = 0.0f;          // Восток
                else if (dirX == 1 && dirZ == 1) targetRotation = XM_PIDIV4;     // Юго-восток
                else if (dirX == 0 && dirZ == 1) targetRotation = XM_PIDIV2;     // Юг
                else if (dirX == -1 && dirZ == 1) targetRotation = XM_PIDIV4 * 3; // Юго-запад
                else if (dirX == -1 && dirZ == 0) targetRotation = XM_PI;        // Запад
                else if (dirX == -1 && dirZ == -1) targetRotation = -XM_PIDIV4 * 3; // Северо-запад

                // Плавный поворот
                float rotationSpeed = 10.0f * deltaTime;

                // Вычисляем разницу углов
                float angleDiff = targetRotation - currentRotation;

                // Нормализуем разницу в диапазон [-π, π]
                while (angleDiff > XM_PI) angleDiff -= XM_2PI;
                while (angleDiff < -XM_PI) angleDiff += XM_2PI;

                // Интерполируем поворот
                currentRotation += angleDiff * rotationSpeed;

                // Устанавливаем поворот
                player.SetRotation(0, currentRotation, 0);
            }
        }

        // Обновляем цель камеры
        camera.SetTarget(player.GetPosition());

        // Вращение камеры
        if (GetAsyncKeyState(VK_LEFT) & 0x8000) {
            camera.Rotate(-rotationSpeed * deltaTime);
        }
        if (GetAsyncKeyState(VK_RIGHT) & 0x8000) {
            camera.Rotate(rotationSpeed * deltaTime);
        }

        // Зум камеры
        if (GetAsyncKeyState(VK_UP) & 0x8000) {
            camera.Zoom(-playerSpeed * deltaTime);
        }
        if (GetAsyncKeyState(VK_DOWN) & 0x8000) {
            camera.Zoom(playerSpeed * deltaTime);
        }

        // Сброс позиции
        if (GetAsyncKeyState('R') & 0x8000) {
            player.SetPosition(0, 0, 0);
            currentRotation = XM_PI; // Сбрасываем поворот к камере
            player.SetRotation(0, currentRotation, 0);
            DEBUG_LOG("Позиция и поворот сброшены");
        }

        // Масштаб
        if (GetAsyncKeyState('1') & 0x8000) {
            player.SetScale(1.0f, 1.0f, 1.0f);
            DEBUG_LOG("Масштаб: 0.001");
        }
        if (GetAsyncKeyState('2') & 0x8000) {
            player.SetScale(5.0f, 5.0f, 5.0f);
            DEBUG_LOG("Масштаб: 0.01");
        }
        if (GetAsyncKeyState('3') & 0x8000) {
            player.SetScale(10.0f, 10.0f, 10.0f);
            DEBUG_LOG("Масштаб: 0.1");
        }

        // Отладочная информация
        static float debugTimer = 0.0f;
        debugTimer += deltaTime;
        if (debugTimer > 1.0f) {
            XMFLOAT3 pos = player.GetPosition();
            XMFLOAT3 rot = player.GetRotation();
            char buffer[256];
            sprintf_s(buffer, "Игрок: Pos(%.2f, %.2f, %.2f) RotY: %.1f° (%.2f рад) Масштаб: проверьте 1/2/3/4",
                pos.x, pos.y, pos.z, rot.y * 180.0f / XM_PI, rot.y);
            DEBUG_LOG(buffer);
            debugTimer = 0.0f;
        }
    }

    void Render(float aspectRatio) {
        // Получаем матрицы камеры
        XMMATRIX view = camera.GetViewMatrix();
        XMMATRIX proj = camera.GetProjectionMatrix(aspectRatio);

        // 1. Сначала рендерим фон
        XMMATRIX backgroundWorld = background.GetWorldMatrix();
        shader.SetShaderParameters(context, backgroundWorld, view, proj);
        shader.Apply(context);
        background.Render(context);

        // 2. Затем рендерим игрока поверх фона
        XMMATRIX playerWorld = player.GetWorldMatrix();
        shader.SetShaderParameters(context, playerWorld, view, proj);
        player.Render(context, textures);
    }

    void Cleanup() {
        DEBUG_LOG("Очистка игровой сцены...");
        background.Cleanup(); // Очищаем фон
        player.Cleanup();
        textures.Cleanup();
        shader.Cleanup();
        DEBUG_LOG("Игровая сцена очищена");
    }
};

// ==================== RENDERER ====================
class DX11Renderer {
private:
    ID3D11Device* device = nullptr;
    ID3D11DeviceContext* context = nullptr;
    IDXGISwapChain* swapChain = nullptr;
    ID3D11RenderTargetView* renderTargetView = nullptr;
    ID3D11DepthStencilView* depthStencilView = nullptr;
    D3D11_VIEWPORT viewport = {};
    HWND hwnd = nullptr;

public:
    bool Initialize(HWND window, int width, int height) {
        hwnd = window;

        DEBUG_LOG("Инициализация DirectX 11...");

        // Описание swap chain
        DXGI_SWAP_CHAIN_DESC scd = {};
        scd.BufferCount = 1;
        scd.BufferDesc.Width = width;
        scd.BufferDesc.Height = height;
        scd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        scd.BufferDesc.RefreshRate.Numerator = 60;
        scd.BufferDesc.RefreshRate.Denominator = 1;
        scd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
        scd.OutputWindow = hwnd;
        scd.SampleDesc.Count = 1;
        scd.SampleDesc.Quality = 0;
        scd.Windowed = TRUE;
        scd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

        // Создаем устройство и swap chain
        D3D_FEATURE_LEVEL featureLevel = D3D_FEATURE_LEVEL_11_0;
        HRESULT hr = D3D11CreateDeviceAndSwapChain(
            nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0,
            &featureLevel, 1, D3D11_SDK_VERSION, &scd,
            &swapChain, &device, nullptr, &context);

        if (FAILED(hr)) {
            DEBUG_ERROR("Ошибка создания устройства DirectX 11");
            return false;
        }
        TestAssimp();
        // Получаем back buffer
        ID3D11Texture2D* backBuffer = nullptr;
        hr = swapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), (void**)&backBuffer);
        if (FAILED(hr)) {
            DEBUG_ERROR("Ошибка получения back buffer");
            return false;
        }

        // Создаем render target view
        hr = device->CreateRenderTargetView(backBuffer, nullptr, &renderTargetView);
        backBuffer->Release();
        if (FAILED(hr)) {
            DEBUG_ERROR("Ошибка создания render target view");
            return false;
        }

        // Создаем depth stencil buffer
        D3D11_TEXTURE2D_DESC depthDesc = {};
        depthDesc.Width = width;
        depthDesc.Height = height;
        depthDesc.MipLevels = 1;
        depthDesc.ArraySize = 1;
        depthDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
        depthDesc.SampleDesc.Count = 1;
        depthDesc.SampleDesc.Quality = 0;
        depthDesc.Usage = D3D11_USAGE_DEFAULT;
        depthDesc.BindFlags = D3D11_BIND_DEPTH_STENCIL;

        ID3D11Texture2D* depthBuffer = nullptr;
        hr = device->CreateTexture2D(&depthDesc, nullptr, &depthBuffer);
        if (FAILED(hr)) {
            DEBUG_ERROR("Ошибка создания depth buffer");
            return false;
        }

        // Создаем depth stencil view
        D3D11_DEPTH_STENCIL_VIEW_DESC dsvDesc = {};
        dsvDesc.Format = depthDesc.Format;
        dsvDesc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
        dsvDesc.Texture2D.MipSlice = 0;

        hr = device->CreateDepthStencilView(depthBuffer, &dsvDesc, &depthStencilView);
        depthBuffer->Release();
        if (FAILED(hr)) {
            DEBUG_ERROR("Ошибка создания depth stencil view");
            return false;
        }

        // Настраиваем viewport
        viewport.Width = (float)width;
        viewport.Height = (float)height;
        viewport.MinDepth = 0.0f;
        viewport.MaxDepth = 1.0f;
        viewport.TopLeftX = 0;
        viewport.TopLeftY = 0;

        DEBUG_SUCCESS("DirectX 11 инициализирован");
        return true;
    }
    void TestAssimp() {
        Assimp::Importer importer;

        const aiScene* scene = importer.ReadFile(
            "C:/dev/GameDev/animation/Walking.fbx",
            aiProcess_Triangulate |
            aiProcess_JoinIdenticalVertices |
            aiProcess_LimitBoneWeights |
            aiProcess_CalcTangentSpace
        );

        if (!scene) {
            DEBUG_ERROR(importer.GetErrorString());
            return;
        }

        DEBUG_SUCCESS("Assimp FBX загружен успешно!");

        if (scene->HasAnimations()) {
            DEBUG_LOG("FBX содержит анимации");
        }
        else {
            DEBUG_WARNING("FBX без анимаций");
        }
    }

    void BeginFrame() {
        float clearColor[4] = { 0.1f, 0.2f, 0.3f, 1.0f }; // Темно-синий фон

        // Очищаем back buffer и depth buffer
        context->ClearRenderTargetView(renderTargetView, clearColor);
        context->ClearDepthStencilView(depthStencilView, D3D11_CLEAR_DEPTH, 1.0f, 0);

        // Устанавливаем render target и depth stencil
        context->OMSetRenderTargets(1, &renderTargetView, depthStencilView);
        context->RSSetViewports(1, &viewport);
    }

    void EndFrame() {
        swapChain->Present(1, 0);
    }

    ID3D11Device* GetDevice() { return device; }
    ID3D11DeviceContext* GetContext() { return context; }

    void Cleanup() {
        if (depthStencilView) depthStencilView->Release();
        if (renderTargetView) renderTargetView->Release();
        if (swapChain) swapChain->Release();
        if (context) context->Release();
        if (device) device->Release();
    }
};

// ==================== WINDOW ====================
LRESULT CALLBACK WindowProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_DESTROY:
        DEBUG_LOG("Окно закрывается...");
        PostQuitMessage(0);
        break;

    case WM_SIZE: {
        int width = LOWORD(lParam);
        int height = HIWORD(lParam);
        char buffer[128];
        sprintf_s(buffer, "Размер окна изменен: %dx%d", width, height);
        DEBUG_LOG(buffer);
        break;
    }

    default:
        return DefWindowProc(hwnd, msg, wParam, lParam);
    }
    return 0;
}

HWND CreateGameWindow(HINSTANCE hInstance, int width, int height) {
    WNDCLASS wc = {};
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = L"IsometricGameWindow";
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);

    RegisterClass(&wc);

    HWND hwnd = CreateWindow(
        L"IsometricGameWindow",
        L"Shadows Over The Thames - Изометрический прототип с поддержкой MTL (WASD - движение, Стрелки - камера, R - сброс, 1/2/3/4 - масштаб)",
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT,
        width, height,
        nullptr, nullptr, hInstance, nullptr
    );

    return hwnd;
}

// ==================== MAIN ====================
int WINAPI WinMain(
    _In_ HINSTANCE hInstance,
    _In_opt_ HINSTANCE hPrevInstance,
    _In_ LPSTR lpCmdLine,
    _In_ int nCmdShow)
{
    DEBUG_LOG("=== ЗАПУСК ИЗОМЕТРИЧЕСКОЙ ИГРЫ ===");

    // Выводим информацию о системе
    DEBUG_LOG("Системная информация:");
    DEBUG_LOG("  Windows версия: проверяется...");

    // Создаем окно
    HWND hwnd = CreateGameWindow(hInstance, SCREEN_WIDTH, SCREEN_HEIGHT);
    if (!hwnd) {
        MessageBox(nullptr, L"Ошибка создания окна", L"Ошибка", MB_OK | MB_ICONERROR);
        return 1;
    }

    ShowWindow(hwnd, nCmdShow);
    UpdateWindow(hwnd);

    // Инициализируем рендерер
    DX11Renderer renderer;
    if (!renderer.Initialize(hwnd, SCREEN_WIDTH, SCREEN_HEIGHT)) {
        MessageBox(hwnd, L"Ошибка инициализации DirectX 11", L"Ошибка", MB_OK | MB_ICONERROR);
        return 1;
    }

    // Инициализируем игровую сцену
    GameScene game;
    if (!game.Initialize(renderer.GetDevice(), renderer.GetContext())) {
        MessageBox(hwnd, L"Ошибка инициализации игры", L"Ошибка", MB_OK | MB_ICONERROR);
        renderer.Cleanup();
        return 1;
    }

    // Инициализируем таймер
    LARGE_INTEGER frequency, lastTime;
    QueryPerformanceFrequency(&frequency);
    QueryPerformanceCounter(&lastTime);

    float deltaTime = 0.0f;
    float totalTime = 0.0f;
    int frameCount = 0;

    DEBUG_LOG("=== ИГРА ЗАПУЩЕНА ===");
    DEBUG_LOG("Управление:");
    DEBUG_LOG("  W - Северо-запад");
    DEBUG_LOG("  S - Юго-восток");
    DEBUG_LOG("  A - Юго-запад");
    DEBUG_LOG("  D - Северо-восток");
    DEBUG_LOG("  Стрелки - вращение и зум камеры");
    DEBUG_LOG("  R - Сброс позиции");
    DEBUG_LOG("  1/2/3/4 - Изменение масштаба (особенно важно для моделей из Blender!)");
    DEBUG_LOG("  ESC - Выход");
    DEBUG_LOG("ВАЖНО: Модели из Blender обычно очень большие,");
    DEBUG_LOG("поэтому начинаем с масштаба 0.001 и увеличиваем при необходимости");

    // Главный игровой цикл
    MSG msg = {};
    while (true) {

        if (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {
            if (msg.message == WM_QUIT) break;

            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
        else {
            // Вычисляем deltaTime
            LARGE_INTEGER currentTime;
            QueryPerformanceCounter(&currentTime);
            deltaTime = (float)(currentTime.QuadPart - lastTime.QuadPart) / frequency.QuadPart;
            lastTime = currentTime;

            // Ограничиваем deltaTime для стабильности
            if (deltaTime > 0.1f) deltaTime = 0.1f;

            // Обновляем игру
            game.Update(deltaTime);

            // Рендерим
            renderer.BeginFrame();

            float aspectRatio = (float)SCREEN_WIDTH / SCREEN_HEIGHT;
            game.Render(aspectRatio);

            renderer.EndFrame();

            // Статистика FPS
            totalTime += deltaTime;
            frameCount++;
            if (totalTime >= 1.0f) {
                char fpsBuffer[64];
                sprintf_s(fpsBuffer, "FPS: %d", frameCount);
                DEBUG_LOG(fpsBuffer);
                totalTime = 0.0f;
                frameCount = 0;
            }

            // Проверяем выход
            if (GetAsyncKeyState(VK_ESCAPE) & 0x8000) {
                PostQuitMessage(0);
            }
        }
    }

    // Очистка
    DEBUG_LOG("=== ЗАВЕРШЕНИЕ ===");
    game.Cleanup();
    renderer.Cleanup();

    DEBUG_LOG("Игра завершена");

    return (int)msg.wParam;
}