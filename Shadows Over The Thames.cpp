// Shadows Over The Thames.cpp
#include <windows.h>
#include <string>
#include <vector>
#include <commctrl.h>
#include <cmath>
#include <algorithm>
#include <mmsystem.h>
#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "msimg32.lib")
#pragma comment(lib, "winmm.lib")

// Структуры для игрока
struct Player {
    float x = 100.0f;
    float y = 300.0f;
    float width = 180.0f;
    float height = 128.0f;
    float speed = 5.0f;
    bool facingRight = true;
    bool isMoving = false;
    bool isRunning = false;
    HBITMAP hSpriteRight = nullptr;
    HBITMAP hSpriteRunRight = nullptr;
    int currentFrame = 0;
    DWORD lastFrameTime = 0;
    DWORD frameDelay = 150;
};

// Глобальные переменные
struct WindowContext {
    HWND hWnd = nullptr;
    int width = 1920;
    int height = 1080;
    bool should_exit = false;
    HWND hStartButton = nullptr;
    HWND hExitButton = nullptr;

    // Буфер для двойной буферизации
    HDC hBufferDC = nullptr;
    HBITMAP hBufferBitmap = nullptr;
    HBITMAP hOldBufferBitmap = nullptr;
    int bufferWidth = 0;
    int bufferHeight = 0;
};

struct GameState {
    bool inMainMenu = true;
    bool inGame = false;
    int currentLevel = 0;
    Player player;
    float levelWidth = 3840;
    float levelHeight = 2000;
    HBITMAP hLevelBackground = nullptr;
    DWORD lastRunTime = 0;
    DWORD boostStartTime = 0;
    bool isRunningBoost = false;
    float cameraX = 0;
    float cameraY = 0; 
    bool isMusicPlaying = false;
};

WindowContext g_window;
GameState g_gameState;

// Цвета
const COLORREF MENU_BG_COLOR = RGB(20, 25, 35);
const COLORREF BUTTON_COLOR = RGB(86, 98, 246);
const COLORREF BUTTON_HOVER_COLOR = RGB(105, 116, 255);
const COLORREF TEXT_COLOR = RGB(255, 255, 255);

// Прототипы функций
LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
void CreateMainMenuButtons();
void ShowMainMenuButtons(bool show);
void Cleanup();
void OnStartGame();
void OnExitGame();
HBITMAP LoadBmpFromDebug(const char* filename);
void InitLevel1();
void RenderMainMenu(HDC hdc);
void RenderGame(HDC hdc);
void CustomizeButton(HWND hwndButton);
void ProcessInput();
void UpdatePlayer();
void RenderPlayer(HDC hdc);
void LimitPlayerOnGround();
void UpdateCamera();
void InitBuffer(HDC hdc);
void CleanupBuffer();
void PlayBackgroundMusic(const char* filename);
void StopBackgroundMusic();
void ToggleMusicPause();

void PlayBackgroundMusic(const char* filename)
{
    StopBackgroundMusic();

    OutputDebugStringA("=== ЗАПУСК МУЗЫКИ ===\n");

    char debug[512];
    sprintf_s(debug, "Ищем файл: %s\n", filename);
    OutputDebugStringA(debug);

    // Проверяем существование файла
    DWORD attr = GetFileAttributesA(filename);
    if (attr == INVALID_FILE_ATTRIBUTES)
    {
        sprintf_s(debug, "Файл не найден напрямую: %s\n", filename);
        OutputDebugStringA(debug);
        return;
    }

    // Получаем размер файла
    HANDLE hFile = CreateFileA(filename, GENERIC_READ, FILE_SHARE_READ,
        NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile != INVALID_HANDLE_VALUE)
    {
        DWORD fileSize = GetFileSize(hFile, NULL);
        CloseHandle(hFile);
        sprintf_s(debug, "Размер файла: %lu байт (%.1f KB)\n", fileSize, fileSize / 1024.0f);
        OutputDebugStringA(debug);

        if (fileSize == 0)
        {
            OutputDebugStringA("Файл пустой!\n");
            return;
        }
    }

    // 1. Пробуем через PlaySound (самый простой способ)
    OutputDebugStringA("1. Пробуем PlaySound...\n");
    if (PlaySoundA(filename, NULL, SND_ASYNC | SND_LOOP | SND_FILENAME | SND_NODEFAULT))
    {
        OutputDebugStringA("PlaySound УСПЕХ! Музыка должна играть\n");
        g_gameState.isMusicPlaying = true;
        return;
    }
    else
    {
        DWORD err = GetLastError();
        sprintf_s(debug, "PlaySound ошибка: %lu\n", err);
        OutputDebugStringA(debug);
    }

    // 2. Пробуем через MCI
    OutputDebugStringA("2. Пробуем MCI...\n");

    // Сначала закрываем, если что-то было открыто
    mciSendStringA("close all", NULL, 0, NULL);

    // Открываем файл
    std::string openCmd = "open \"";
    openCmd += filename;
    openCmd += "\" type waveaudio alias bgmusic";

    MCIERROR mciError = mciSendStringA(openCmd.c_str(), NULL, 0, NULL);
    if (mciError != 0)
    {
        char mciErrorMsg[256];
        mciGetErrorStringA(mciError, mciErrorMsg, sizeof(mciErrorMsg));
        sprintf_s(debug, "MCI open ошибка: %s (код: %lu)\n", mciErrorMsg, mciError);
        OutputDebugStringA(debug);

        // Пробуем как mpegvideo (для mp3)
        openCmd = "open \"";
        openCmd += filename;
        openCmd += "\" type mpegvideo alias bgmusic";

        mciError = mciSendStringA(openCmd.c_str(), NULL, 0, NULL);
        if (mciError != 0)
        {
            mciGetErrorStringA(mciError, mciErrorMsg, sizeof(mciErrorMsg));
            sprintf_s(debug, "MCI mpegvideo тоже ошибка: %s\n", mciErrorMsg);
            OutputDebugStringA(debug);
            return;
        }
    }

    OutputDebugStringA("MCI файл открыт успешно\n");

    // Получаем информацию о файле
    char info[256];
    mciSendStringA("status bgmusic length", info, sizeof(info), NULL);
    sprintf_s(debug, "Длина трека: %s мс\n", info);
    OutputDebugStringA(debug);

    mciSendStringA("status bgmusic mode", info, sizeof(info), NULL);
    sprintf_s(debug, "Режим: %s\n", info);
    OutputDebugStringA(debug);

    // Устанавливаем громкость на максимум
    mciSendStringA("setaudio bgmusic volume to 1000", NULL, 0, NULL);

    // Запускаем с повторением
    mciError = mciSendStringA("play bgmusic repeat", NULL, 0, NULL);
    if (mciError != 0)
    {
        char mciErrorMsg[256];
        mciGetErrorStringA(mciError, mciErrorMsg, sizeof(mciErrorMsg));
        sprintf_s(debug, "MCI play ошибка: %s\n", mciErrorMsg);
        OutputDebugStringA(debug);

        // Пробуем без repeat
        mciError = mciSendStringA("play bgmusic", NULL, 0, NULL);
        if (mciError == 0)
        {
            OutputDebugStringA("MCI запущен без повторения\n");
            g_gameState.isMusicPlaying = true;
        }
        return;
    }

    OutputDebugStringA("MCI музыка запущена с повторением\n");
    g_gameState.isMusicPlaying = true;

    OutputDebugStringA("=== МУЗЫКА ЗАПУЩЕНА ===\n");

    // Тестовый звук через 2 секунды
    SetTimer(g_window.hWnd, 3, 2000, NULL);
}

// Остановка музыки
void StopBackgroundMusic()
{
    if (g_gameState.isMusicPlaying)
    {
        mciSendStringA("stop bgmusic", NULL, 0, NULL);
        mciSendStringA("close bgmusic", NULL, 0, NULL);
        g_gameState.isMusicPlaying = false;
        OutputDebugStringA("Музыка остановлена\n");
    }
}

// Пауза/возобновление музыки
void ToggleMusicPause()
{
    if (g_gameState.isMusicPlaying)
    {
        static bool isPaused = false;
        if (isPaused)
        {
            mciSendStringA("play bgmusic", NULL, 0, NULL);
            OutputDebugStringA("Музыка возобновлена\n");
        }
        else
        {
            mciSendStringA("pause bgmusic", NULL, 0, NULL);
            OutputDebugStringA("Музыка на паузе\n");
        }
        isPaused = !isPaused;
    }
}
// Функция для зеркального отображения битмапа
void DrawMirroredBitmap(HDC hdc, int x, int y, int width, int height,
    HDC memDC, HBITMAP hBitmap, int srcWidth, int srcHeight)
{
    // Создаем временный DC для зеркального отображения
    HDC mirrorDC = CreateCompatibleDC(hdc);
    HBITMAP hMirrorBmp = CreateCompatibleBitmap(hdc, srcWidth, srcHeight);
    HBITMAP hOldMirrorBmp = (HBITMAP)SelectObject(mirrorDC, hMirrorBmp);

    // Копируем исходный битмап
    HBITMAP hOldBmp = (HBITMAP)SelectObject(memDC, hBitmap);
    BitBlt(mirrorDC, 0, 0, srcWidth, srcHeight, memDC, 0, 0, SRCCOPY);

    // Зеркалим по горизонтали
    StretchBlt(mirrorDC, srcWidth - 1, 0, -srcWidth, srcHeight,
        mirrorDC, 0, 0, srcWidth, srcHeight, SRCCOPY);

    // Рисуем на экран
    TransparentBlt(hdc, x, y, width, height,
        mirrorDC, 0, 0, srcWidth, srcHeight, RGB(255, 0, 255));

    // Очистка
    SelectObject(mirrorDC, hOldMirrorBmp);
    SelectObject(memDC, hOldBmp);
    DeleteObject(hMirrorBmp);
    DeleteDC(mirrorDC);
}

// Инициализация буфера
void InitBuffer(HDC hdc)
{
    if (g_window.hBufferDC)
        CleanupBuffer();

    g_window.hBufferDC = CreateCompatibleDC(hdc);
    g_window.hBufferBitmap = CreateCompatibleBitmap(hdc, g_window.width, g_window.height);
    g_window.hOldBufferBitmap = (HBITMAP)SelectObject(g_window.hBufferDC, g_window.hBufferBitmap);
    g_window.bufferWidth = g_window.width;
    g_window.bufferHeight = g_window.height;
}

// Очистка буфера
void CleanupBuffer()
{
    if (g_window.hBufferDC)
    {
        if (g_window.hOldBufferBitmap)
        {
            SelectObject(g_window.hBufferDC, g_window.hOldBufferBitmap);
            g_window.hOldBufferBitmap = nullptr;
        }

        if (g_window.hBufferBitmap)
        {
            DeleteObject(g_window.hBufferBitmap);
            g_window.hBufferBitmap = nullptr;
        }

        DeleteDC(g_window.hBufferDC);
        g_window.hBufferDC = nullptr;
    }
}

// Точка входа
int WINAPI WinMain(_In_ HINSTANCE hInstance, _In_opt_ HINSTANCE hPrevInstance,
    _In_ LPSTR lpCmdLine, _In_ int nCmdShow)
{
    // Инициализация Common Controls
    INITCOMMONCONTROLSEX icc = { sizeof(INITCOMMONCONTROLSEX), ICC_STANDARD_CLASSES };
    InitCommonControlsEx(&icc);

    // Регистрация класса окна
    const wchar_t CLASS_NAME[] = L"ShadowsOverTheThamesClass";

    WNDCLASS wc = {};
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = CLASS_NAME;
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.hIcon = LoadIcon(nullptr, IDI_APPLICATION);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.hbrBackground = CreateSolidBrush(MENU_BG_COLOR);

    if (!RegisterClass(&wc))
    {
        MessageBox(nullptr, L"Не удалось зарегистрировать класс окна", L"Ошибка", MB_OK | MB_ICONERROR);
        return 1;
    }

    // Создаём окно
    g_window.hWnd = CreateWindowEx(
        0,
        CLASS_NAME,
        L"Shadows Over The Thames",
        WS_POPUP | WS_VISIBLE,  
        0, 0,                   
        GetSystemMetrics(SM_CXSCREEN),  
        GetSystemMetrics(SM_CYSCREEN),  
        nullptr,
        nullptr,
        hInstance,
        nullptr
    );

    if (!g_window.hWnd)
    {
        MessageBox(nullptr, L"Не удалось создать окно", L"Ошибка", MB_OK | MB_ICONERROR);
        return 1;
    }

    // Получаем реальные размеры клиентской области
    RECT clientRect;
    GetClientRect(g_window.hWnd, &clientRect);
    g_window.width = clientRect.right - clientRect.left;
    g_window.height = clientRect.bottom - clientRect.top;

    // Создаём кнопки ПЕРЕД показом окна
    CreateMainMenuButtons();

    // Показываем окно
    ShowWindow(g_window.hWnd, nCmdShow);
    UpdateWindow(g_window.hWnd);

    // Инициализация таймера для обновления игры
    SetTimer(g_window.hWnd, 1, 16, NULL); // ~60 FPS

    // Основной цикл
    MSG msg = {};
    while (!g_window.should_exit)
    {
        if (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE))
        {
            if (msg.message == WM_QUIT)
                g_window.should_exit = true;

            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
        else
        {
            // Обработка ввода и обновление состояния игры
            if (g_gameState.inGame)
            {
                ProcessInput();
                UpdatePlayer();
                UpdateCamera();
                InvalidateRect(g_window.hWnd, nullptr, FALSE);
            }

            // Небольшая пауза
            Sleep(1);
        }
    }

    Cleanup();
    return 0;
}

// Создание кнопок главного меню
void CreateMainMenuButtons()
{
    HINSTANCE hInstance = (HINSTANCE)GetWindowLongPtr(g_window.hWnd, GWLP_HINSTANCE);

    // Стили кнопок
    DWORD buttonStyle = WS_TABSTOP | WS_VISIBLE | WS_CHILD | BS_PUSHBUTTON | BS_FLAT;

    // Кнопка "Начать игру"
    g_window.hStartButton = CreateWindow(
        L"BUTTON",
        L"НАЧАТЬ ИГРУ",
        buttonStyle,
        g_window.width / 2 - 150,
        g_window.height / 2 - 35,
        300, 70,
        g_window.hWnd,
        (HMENU)1001,
        hInstance,
        nullptr
    );

    // Кнопка "Выйти"
    g_window.hExitButton = CreateWindow(
        L"BUTTON",
        L"ВЫЙТИ",
        buttonStyle,
        g_window.width / 2 - 100,
        g_window.height / 2 + 55,
        200, 50,
        g_window.hWnd,
        (HMENU)1002,
        hInstance,
        nullptr
    );

    // Кастомизируем кнопки
    CustomizeButton(g_window.hStartButton);
    CustomizeButton(g_window.hExitButton);
}

// Кастомизация внешнего вида кнопки
void CustomizeButton(HWND hwndButton)
{
    HFONT hFont = CreateFont(
        24,
        0,
        0,
        0,
        FW_BOLD,
        FALSE,
        FALSE,
        FALSE,
        DEFAULT_CHARSET,
        OUT_DEFAULT_PRECIS,
        CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY,
        DEFAULT_PITCH | FF_DONTCARE,
        L"Segoe UI"
    );

    if (hFont)
    {
        SendMessage(hwndButton, WM_SETFONT, (WPARAM)hFont, TRUE);
    }

    SetWindowLongPtr(hwndButton, GWL_EXSTYLE,
        GetWindowLongPtr(hwndButton, GWL_EXSTYLE) & ~WS_EX_TRANSPARENT);
}

// Показать/скрыть кнопки меню
void ShowMainMenuButtons(bool show)
{
    if (g_window.hStartButton && IsWindow(g_window.hStartButton))
        ShowWindow(g_window.hStartButton, show ? SW_SHOW : SW_HIDE);

    if (g_window.hExitButton && IsWindow(g_window.hExitButton))
        ShowWindow(g_window.hExitButton, show ? SW_SHOW : SW_HIDE);
}

// Загрузка BMP из папки Debug
HBITMAP LoadBmpFromDebug(const char* filename)
{
    char exePath[MAX_PATH];
    GetModuleFileNameA(nullptr, exePath, MAX_PATH);

    std::string exeDir = exePath;
    size_t pos = exeDir.find_last_of("\\/");
    if (pos != std::string::npos)
    {
        exeDir = exeDir.substr(0, pos + 1);
    }

    // Пробуем несколько путей
    std::string paths[] = {
        exeDir + filename,
        exeDir + "Debug\\" + filename,
        exeDir + "..\\" + filename,
        filename
    };

    for (const auto& fullPath : paths)
    {
        HBITMAP hBitmap = (HBITMAP)LoadImageA(
            nullptr,
            fullPath.c_str(),
            IMAGE_BITMAP,
            0, 0,
            LR_LOADFROMFILE | LR_CREATEDIBSECTION
        );

        if (hBitmap)
        {
            char debugMsg[512];
            sprintf_s(debugMsg, "Файл загружен: %s", fullPath.c_str());
            OutputDebugStringA(debugMsg);
            return hBitmap;
        }
    }

    char debugMsg[512];
    sprintf_s(debugMsg, "Файл не найден: %s", filename);
    OutputDebugStringA(debugMsg);
    return nullptr;
}

// Отрисовка главного меню с буферизацией
void RenderMainMenu(HDC hdc)
{
    // Инициализируем буфер если нужно
    if (!g_window.hBufferDC || g_window.bufferWidth != g_window.width || g_window.bufferHeight != g_window.height)
    {
        InitBuffer(hdc);
    }

    HDC bufferDC = g_window.hBufferDC;

    // Очищаем фон буфера
    HBRUSH background = CreateSolidBrush(MENU_BG_COLOR);
    RECT fullRect = { 0, 0, g_window.width, g_window.height };
    FillRect(bufferDC, &fullRect, background);
    DeleteObject(background);

    // Заголовок игры
    SetBkMode(bufferDC, TRANSPARENT);

    LOGFONT lf = {};
    lf.lfHeight = 72;
    lf.lfWeight = FW_BOLD;
    lf.lfItalic = TRUE;
    wcscpy_s(lf.lfFaceName, L"Georgia");

    HFONT titleFont = CreateFontIndirect(&lf);
    HFONT oldFont = (HFONT)SelectObject(bufferDC, titleFont);

    SetTextColor(bufferDC, RGB(220, 180, 100));

    std::wstring title = L"SHADOWS OVER THE THAMES";
    RECT titleRect = { 0, 100, g_window.width, 250 };
    DrawText(bufferDC, title.c_str(), -1, &titleRect, DT_CENTER | DT_SINGLELINE);

    // Подзаголовок
    lf.lfHeight = 28;
    lf.lfWeight = FW_NORMAL;
    lf.lfItalic = FALSE;
    wcscpy_s(lf.lfFaceName, L"Segoe UI");

    HFONT subtitleFont = CreateFontIndirect(&lf);
    SelectObject(bufferDC, subtitleFont);

    SetTextColor(bufferDC, RGB(180, 180, 200));

    std::wstring subtitle = L"Приключенческая игра!";
    RECT subtitleRect = { 0, 200, g_window.width, 300 };
    DrawText(bufferDC, subtitle.c_str(), -1, &subtitleRect, DT_CENTER | DT_SINGLELINE);

    // Инструкция
    lf.lfHeight = 20;
    HFONT infoFont = CreateFontIndirect(&lf);
    SelectObject(bufferDC, infoFont);

    SetTextColor(bufferDC, RGB(150, 150, 180));

    std::wstring info = L"Нажмите кнопку 'НАЧАТЬ ИГРУ' чтобы начать";
    RECT infoRect = { 0, g_window.height - 100, g_window.width, g_window.height - 50 };
    DrawText(bufferDC, info.c_str(), -1, &infoRect, DT_CENTER | DT_SINGLELINE);

    // Восстанавливаем шрифт
    SelectObject(bufferDC, oldFont);
    DeleteObject(titleFont);
    DeleteObject(subtitleFont);
    DeleteObject(infoFont);

    // Копируем буфер на экран
    BitBlt(hdc, 0, 0, g_window.width, g_window.height, bufferDC, 0, 0, SRCCOPY);
}

// Отрисовка игры с буферизацией
void RenderGame(HDC hdc)
{
    // Инициализируем буфер если нужно
    if (!g_window.hBufferDC || g_window.bufferWidth != g_window.width || g_window.bufferHeight != g_window.height)
    {
        InitBuffer(hdc);
    }

    HDC bufferDC = g_window.hBufferDC;

    // Очищаем фон буфера
    HBRUSH background = CreateSolidBrush(RGB(30, 30, 40));
    RECT fullRect = { 0, 0, g_window.width, g_window.height };
    FillRect(bufferDC, &fullRect, background);
    DeleteObject(background);

    // Рисуем фон уровня в буфер
    if (g_gameState.hLevelBackground)
    {
        HDC memDC = CreateCompatibleDC(bufferDC);
        HBITMAP oldBmp = (HBITMAP)SelectObject(memDC, g_gameState.hLevelBackground);

        BITMAP bm;
        GetObject(g_gameState.hLevelBackground, sizeof(bm), &bm);

        // Рисуем фон с учетом позиции камеры
        int srcX = (int)g_gameState.cameraX;
        int srcY = (int)g_gameState.cameraY;
        srcX = max(0, min(srcX, (int)g_gameState.levelWidth - g_window.width));
        srcY = max(0, min(srcY, (int)g_gameState.levelHeight - g_window.height));

        StretchBlt(bufferDC, 0, 0, g_window.width, g_window.height,
            memDC, srcX, srcY, g_window.width, g_window.height, SRCCOPY);

        SelectObject(memDC, oldBmp);
        DeleteDC(memDC);
    }
    else
    {
        // Если фон не загружен, рисуем простой
        HBRUSH skyBrush = CreateSolidBrush(RGB(135, 206, 235)); // Голубое небо
        RECT skyRect = { 0, 0, g_window.width, g_window.height - 150 };
        FillRect(bufferDC, &skyRect, skyBrush);
        DeleteObject(skyBrush);

        HBRUSH floorBrush = CreateSolidBrush(RGB(100, 70, 50)); // Коричневый пол
        RECT floorRect = { 0, g_window.height - 150, g_window.width, g_window.height };
        FillRect(bufferDC, &floorRect, floorBrush);
        DeleteObject(floorBrush);
    }


    // Рисуем игрока в буфер
    RenderPlayer(bufferDC);

    // Информация об уровне в буфер
    SetBkMode(bufferDC, TRANSPARENT);

    LOGFONT lf = {};
    lf.lfHeight = 20;
    lf.lfWeight = FW_BOLD;
    wcscpy_s(lf.lfFaceName, L"Arial");

    HFONT font = CreateFontIndirect(&lf);
    HFONT oldFont = (HFONT)SelectObject(bufferDC, font);

    SetTextColor(bufferDC, RGB(255, 255, 255));

    // Позиция игрока (для отладки)
    std::wstring posText = L"X: " + std::to_wstring((int)g_gameState.player.x) +
        L" Y: " + std::to_wstring((int)g_gameState.player.y);
    RECT posRect = { 20, 20, 300, 50 };
    DrawText(bufferDC, posText.c_str(), -1, &posRect, DT_LEFT | DT_SINGLELINE);

    // Подсказки управления
    std::wstring controls = L"WASD - Движение | SHIFT - Бег | ESC - Меню";
    RECT controlsRect = { g_window.width / 2 - 200, 20, g_window.width / 2 + 200, 50 };
    DrawText(bufferDC, controls.c_str(), -1, &controlsRect, DT_CENTER | DT_SINGLELINE);

    SelectObject(bufferDC, oldFont);
    DeleteObject(font);

    // Копируем буфер на экран
    BitBlt(hdc, 0, 0, g_window.width, g_window.height, bufferDC, 0, 0, SRCCOPY);
}

// Рендеринг игрока с зеркалированием
void RenderPlayer(HDC hdc)
{
    // Выбираем спрайт
    HBITMAP hSprite = g_gameState.player.isRunning ?
        g_gameState.player.hSpriteRunRight :
        g_gameState.player.hSpriteRight;

    // Если нет спрайтов, рисуем простого человечка
    if (!hSprite)
    {
        HBRUSH playerBrush = CreateSolidBrush(RGB(0, 150, 255));

        // Позиция на экране с учетом камеры
        int screenX = (int)g_gameState.player.x - (int)g_gameState.cameraX - (int)g_gameState.player.width / 2;
        int screenY = (int)g_gameState.player.y - (int)g_gameState.cameraY - (int)g_gameState.player.height / 2; 

        RECT playerRect = {
            screenX,
            screenY,
            screenX + (int)g_gameState.player.width,
            screenY + (int)g_gameState.player.height
        };

        FillRect(hdc, &playerRect, playerBrush);

        // Голова
        RECT headRect = {
            screenX + (int)g_gameState.player.width / 4,
            screenY,
            screenX + (int)g_gameState.player.width * 3 / 4,
            screenY + (int)g_gameState.player.height / 3
        };
        HBRUSH headBrush = CreateSolidBrush(RGB(255, 220, 180));
        FillRect(hdc, &headRect, headBrush);
        DeleteObject(headBrush);

        DeleteObject(playerBrush);
    }
    else
    {
        // Рисуем спрайт с зеркалированием
        HDC memDC = CreateCompatibleDC(hdc);
        HBITMAP oldBmp = (HBITMAP)SelectObject(memDC, hSprite);

        BITMAP bm;
        GetObject(hSprite, sizeof(bm), &bm);

        // Позиция на экране с учетом камеры
        int screenX = (int)g_gameState.player.x - (int)g_gameState.cameraX - (int)g_gameState.player.width / 2;
        int screenY = (int)g_gameState.player.y - (int)g_gameState.cameraY - (int)g_gameState.player.height / 2;  

        if (g_gameState.player.facingRight)
        {
            // Рисуем вправо (оригинальный спрайт)
            StretchBlt(hdc,
                screenX, screenY,
                (int)g_gameState.player.width, (int)g_gameState.player.height,
                memDC,
                0, 0,
                bm.bmWidth, bm.bmHeight,
                SRCCOPY);
        }
        else
        {
            // Рисуем зеркально влево
            DrawMirroredBitmap(hdc,
                screenX, screenY,
                (int)g_gameState.player.width, (int)g_gameState.player.height,
                memDC, hSprite, bm.bmWidth, bm.bmHeight);
        }

        SelectObject(memDC, oldBmp);
        DeleteDC(memDC);
    }
}

// Начать игру
void OnStartGame()
{
    g_gameState.inMainMenu = false;
    g_gameState.inGame = true;
    g_gameState.currentLevel = 1;

    // Скрываем кнопки меню
    ShowMainMenuButtons(false);

    // Инициализируем уровень
    InitLevel1();

    // Перерисовываем
    InvalidateRect(g_window.hWnd, nullptr, TRUE);

    PlayBackgroundMusic("x64\\Debug\\bazar.wav");
}

// Выйти из игры
void OnExitGame()
{
    g_window.should_exit = true;
    PostQuitMessage(0);
}

// Инициализация уровня 1
void InitLevel1()
{
    // Загружаем фон уровня
    g_gameState.hLevelBackground = LoadBmpFromDebug("level1.bmp");
    
    // Загружаем спрайт игрока
    g_gameState.player.hSpriteRight = LoadBmpFromDebug("player_walking_2.bmp");
    g_gameState.player.hSpriteRunRight = LoadBmpFromDebug("player_run.bmp");

    // Если нет спрайта бега, используем обычный
    if (!g_gameState.player.hSpriteRunRight)
        g_gameState.player.hSpriteRunRight = g_gameState.player.hSpriteRight;

    // Начальная позиция игрока
    g_gameState.player.x = 500;
    g_gameState.player.y = g_gameState.levelHeight - 300;
    g_gameState.player.facingRight = true;

    // Сбрасываем камеру
    g_gameState.cameraX = 0;
}

// Обработка ввода
void ProcessInput()
{
    DWORD currentTime = GetTickCount64();

    // Движение
    bool moved = false;
    if (GetAsyncKeyState('W') & 0x8000) {
        g_gameState.player.y -= g_gameState.player.speed;
        moved = true;
    }
    if (GetAsyncKeyState('S') & 0x8000) {
        g_gameState.player.y += g_gameState.player.speed;
        moved = true;
    }
    if (GetAsyncKeyState('A') & 0x8000)
    {
        g_gameState.player.x -= g_gameState.player.speed;
        g_gameState.player.facingRight = false;
        moved = true;
    }
    if (GetAsyncKeyState('D') & 0x8000) {
        g_gameState.player.x += g_gameState.player.speed;
        g_gameState.player.facingRight = true;
        moved = true;
    }
    g_gameState.player.isMoving = moved;

    // Бег
    if (GetAsyncKeyState(VK_SHIFT) & 0x8000 && currentTime - g_gameState.lastRunTime >= 1000)
    {
        g_gameState.isRunningBoost = true;
        g_gameState.boostStartTime = currentTime;
        g_gameState.player.speed = 12;
        g_gameState.player.isRunning = true;
        g_gameState.lastRunTime = currentTime;
    }

    // Сброс бега через 3 секунды
    if (g_gameState.isRunningBoost && currentTime - g_gameState.boostStartTime >= 3000)
    {
        g_gameState.player.speed = 5;
        g_gameState.player.isRunning = false;
        g_gameState.isRunningBoost = false;
    }
}

// Обновление состояния игрока
void UpdatePlayer()
{
    LimitPlayerOnGround();

    // Обновление анимации
    if (g_gameState.player.isMoving)
    {
        DWORD currentTime = GetTickCount64();
        if (currentTime - g_gameState.player.lastFrameTime > g_gameState.player.frameDelay)
        {
            g_gameState.player.currentFrame = (g_gameState.player.currentFrame + 1) % 4;
            g_gameState.player.lastFrameTime = currentTime;
        }
    }
    else
    {
        g_gameState.player.currentFrame = 0;
    }
}

// Ограничения перемещения игрока
void LimitPlayerOnGround()
{
    //// Левая граница уровня
    //if (g_gameState.player.x < g_gameState.player.width / 2)
    //    g_gameState.player.x = g_gameState.player.width / 2;

    //// Правая граница уровня
    //if (g_gameState.player.x > g_gameState.levelWidth - g_gameState.player.width / 2)
    //    g_gameState.player.x = g_gameState.levelWidth - g_gameState.player.width / 2;

    //// Верхняя граница
    //if (g_gameState.player.y < g_gameState.player.height / 2)
    //    g_gameState.player.y = g_gameState.player.height / 2;

    //// Нижняя граница (пол)
    //if (g_gameState.player.y > g_gameState.levelHeight - g_gameState.player.height / 2)
    //    g_gameState.player.y = g_gameState.levelHeight - g_gameState.player.height / 2;
}

// Обновление камеры
void UpdateCamera()
{
    // Плавное следование камеры
    float targetX = g_gameState.player.x - g_window.width / 2;
    float targetY = g_gameState.player.y - g_window.height / 2;

    // Отладка - выводим значения
    char debug[256];
    sprintf_s(debug, "PlayerY: %.0f, WindowH: %d, TargetY: %.0f, CameraY: %.0f, LevelH: %.0f\n",
        g_gameState.player.y, g_window.height, targetY, g_gameState.cameraY, g_gameState.levelHeight);
    OutputDebugStringA(debug);

    g_gameState.cameraX += (targetX - g_gameState.cameraX) * 0.1f;
    g_gameState.cameraY += (targetY - g_gameState.cameraY) * 0.1f;

    // Ограничиваем камеру границами уровня по X
    if (g_gameState.cameraX < 0)
        g_gameState.cameraX = 0;

    if (g_gameState.cameraX > g_gameState.levelWidth - g_window.width)
        g_gameState.cameraX = g_gameState.levelWidth - g_window.width;

    // Ограничиваем камеру границами уровня по Y
    if (g_gameState.cameraY < 0)
        g_gameState.cameraY = 0;

    // ВАЖНО: Проверяем это условие!
    if (g_gameState.cameraY > g_gameState.levelHeight - g_window.height)
    {
        g_gameState.cameraY = g_gameState.levelHeight - g_window.height;
        OutputDebugStringA("CameraY ограничена сверху!\n");
    }
}

// Оконная процедура
LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg)
    {
    case WM_COMMAND:
        switch (LOWORD(wParam))
        {
        case 1001: // Начать игру
            OnStartGame();
            break;
        case 1002: // Выйти
            OnExitGame();
            break;
        }
        break;

    case WM_SIZE:
        g_window.width = LOWORD(lParam);
        g_window.height = HIWORD(lParam);

        if (g_gameState.inMainMenu)
        {
            if (g_window.hStartButton && IsWindow(g_window.hStartButton))
            {
                SetWindowPos(g_window.hStartButton, nullptr,
                    g_window.width / 2 - 150,
                    g_window.height / 2 - 35,
                    300, 70, SWP_NOZORDER);
            }

            if (g_window.hExitButton && IsWindow(g_window.hExitButton))
            {
                SetWindowPos(g_window.hExitButton, nullptr,
                    g_window.width / 2 - 100,
                    g_window.height / 2 + 55,
                    200, 50, SWP_NOZORDER);
            }
        }

        // При изменении размера окна пересоздаем буфер
        if (g_window.hBufferDC)
        {
            HDC hdc = GetDC(hwnd);
            InitBuffer(hdc);
            ReleaseDC(hwnd, hdc);
        }

        InvalidateRect(hwnd, nullptr, TRUE);
        break;

    case WM_TIMER:
        if (g_gameState.inGame)
        {
            ProcessInput();
            UpdatePlayer();
            UpdateCamera();
            InvalidateRect(hwnd, nullptr, FALSE);
        }
        break;

    case WM_PAINT:
    {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);

        if (g_gameState.inMainMenu)
        {
            RenderMainMenu(hdc);
        }
        else if (g_gameState.inGame)
        {
            RenderGame(hdc);
        }

        EndPaint(hwnd, &ps);
        break;
    }

    case WM_KEYDOWN:
        if (wParam == VK_ESCAPE)
        {
            if (g_gameState.inGame)
            {
                // Возвращаемся в меню
                g_gameState.inGame = false;
                g_gameState.inMainMenu = true;
                StopBackgroundMusic();
                ShowMainMenuButtons(true);
                InvalidateRect(hwnd, nullptr, TRUE);
            }
            else
            {
                OnExitGame();
            }
        }
        break;

    case WM_DESTROY:
        KillTimer(hwnd, 1);
        PostQuitMessage(0);
        break;

    case WM_ERASEBKGND:
        return 1; // Говорим Windows, что сами обработали очистку фона
    }

    return DefWindowProc(hwnd, uMsg, wParam, lParam);
}

// Очистка ресурсов
void Cleanup()
{
    StopBackgroundMusic();
    // Очищаем буфер
    CleanupBuffer();

    // Удаляем шрифты кнопок
    if (g_window.hStartButton && IsWindow(g_window.hStartButton))
    {
        HFONT hFont = (HFONT)SendMessage(g_window.hStartButton, WM_GETFONT, 0, 0);
        if (hFont) DeleteObject(hFont);
    }

    if (g_window.hExitButton && IsWindow(g_window.hExitButton))
    {
        HFONT hFont = (HFONT)SendMessage(g_window.hExitButton, WM_GETFONT, 0, 0);
        if (hFont) DeleteObject(hFont);
    }

    // Удаляем игровые ресурсы
    if (g_gameState.hLevelBackground) DeleteObject(g_gameState.hLevelBackground);
    if (g_gameState.player.hSpriteRight) DeleteObject(g_gameState.player.hSpriteRight);
    if (g_gameState.player.hSpriteRunRight &&
        g_gameState.player.hSpriteRunRight != g_gameState.player.hSpriteRight)
    {
        DeleteObject(g_gameState.player.hSpriteRunRight);
    }
}