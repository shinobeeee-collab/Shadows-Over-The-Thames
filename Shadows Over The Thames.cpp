// Shadows Over The Thames.cpp
#include <windows.h>
#include <string>
#include <vector>
#include <commctrl.h>
#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "msimg32.lib")

// Глобальные переменные
struct WindowContext {
    HWND hWnd = nullptr;
    HDC device_context = nullptr;
    HDC buffer_context = nullptr;
    HBITMAP buffer_bitmap = nullptr;
    int width = 1024;
    int height = 768;
    bool should_exit = false;
    HWND hStartButton = nullptr;
    HWND hExitButton = nullptr;
};

struct GameState {
    bool inMainMenu = true;
    bool inGame = false;
    int currentLevel = 0;
};

WindowContext g_window;
GameState g_gameState;

// Цвета
const COLORREF MENU_BG_COLOR = RGB(20, 25, 35); // Темный фон для меню
const COLORREF BUTTON_COLOR = RGB(86, 98, 246);
const COLORREF BUTTON_HOVER_COLOR = RGB(105, 116, 255);
const COLORREF TEXT_COLOR = RGB(255, 255, 255);

// Прототипы
LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
bool InitializeWindow(HINSTANCE hInstance, int nCmdShow);
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

// Точка входа
int WINAPI WinMain(_In_ HINSTANCE hInstance, _In_opt_ HINSTANCE hPrevInstance,
    _In_ LPSTR lpCmdLine, _In_ int nCmdShow)
{
    // Инициализация Common Controls ДО создания окна
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
    wc.hbrBackground = CreateSolidBrush(MENU_BG_COLOR); // Фон окна

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
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT,
        g_window.width, g_window.height,
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
            // Небольшая пауза чтобы не грузить процессор
            Sleep(10);
        }
    }

    Cleanup();
    return 0;
}

// Создание кнопок главного меню
void CreateMainMenuButtons()
{
    HINSTANCE hInstance = (HINSTANCE)GetWindowLongPtr(g_window.hWnd, GWLP_HINSTANCE);

    // Стили кнопок - делаем их плоскими и современными
    DWORD buttonStyle = WS_TABSTOP | WS_VISIBLE | WS_CHILD | BS_PUSHBUTTON | BS_FLAT;

    // Кнопка "Начать игру" - яркая и крупная
    g_window.hStartButton = CreateWindow(
        L"BUTTON",
        L"НАЧАТЬ ИГРУ",
        buttonStyle,
        g_window.width / 2 - 150,  // Центр по горизонтали
        g_window.height / 2 - 35,   // Выше центра
        300, 70,                   // Ширина и высота
        g_window.hWnd,
        (HMENU)1001,               // ID кнопки
        hInstance,
        nullptr
    );

    // Кнопка "Выйти" - под первой кнопкой
    g_window.hExitButton = CreateWindow(
        L"BUTTON",
        L"ВЫЙТИ",
        buttonStyle,
        g_window.width / 2 - 100,   // Центр по горизонтали
        g_window.height / 2 + 55,   // Ниже первой кнопки
        200, 50,                   // Ширина и высота
        g_window.hWnd,
        (HMENU)1002,               // ID кнопки
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
    // Устанавливаем современный шрифт
    HFONT hFont = CreateFont(
        24,                        // Высота
        0,                         // Ширина
        0,                         // Угол наклона
        0,                         // Ориентация
        FW_BOLD,                   // Жирный
        FALSE,                     // Не курсив
        FALSE,                     // Не подчеркнутый
        FALSE,                     // Не зачеркнутый
        DEFAULT_CHARSET,           // Кодировка
        OUT_DEFAULT_PRECIS,
        CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY,         // Качество отображения
        DEFAULT_PITCH | FF_DONTCARE,
        L"Segoe UI"                // Современный шрифт
    );

    if (hFont)
    {
        SendMessage(hwndButton, WM_SETFONT, (WPARAM)hFont, TRUE);
    }

    // Делаем кнопку непрозрачной
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

    // Получаем путь к папке с exe
    std::string exeDir = exePath;
    size_t pos = exeDir.find_last_of("\\/");
    if (pos != std::string::npos)
    {
        exeDir = exeDir.substr(0, pos + 1);
    }

    // Пробуем несколько возможных расположений
    std::string paths[] = {
        exeDir + filename,                        // 1. В папке с exe
        exeDir + "Debug\\" + filename,            // 2. В подпапке Debug
        exeDir + "..\\" + filename,               // 3. На уровень выше
        exeDir + "..\\..\\" + filename,           // 4. На два уровня выше
        exeDir + "..\\..\\..\\" + filename,       // 5. На три уровня выше
        filename                                   // 6. Текущая рабочая директория
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
            // Выводим отладочную информацию
            char debugMsg[512];
            sprintf_s(debugMsg, "Файл загружен успешно: %s", fullPath.c_str());
            OutputDebugStringA(debugMsg);

            return hBitmap;
        }
    }

    // Если файл не найден, выводим сообщение
    char debugMsg[512];
    sprintf_s(debugMsg, "Файл не найден: %s. EXE находится в: %s",
        filename, exeDir.c_str());
    OutputDebugStringA(debugMsg);

    return nullptr;
}

// Отрисовка главного меню (теперь принимает HDC)
void RenderMainMenu(HDC hdc)
{
    // Очищаем фон
    HBRUSH background = CreateSolidBrush(MENU_BG_COLOR);
    RECT fullRect = { 0, 0, g_window.width, g_window.height };
    FillRect(hdc, &fullRect, background);
    DeleteObject(background);

    // Заголовок игры
    SetBkMode(hdc, TRANSPARENT);

    // Большой заголовок
    LOGFONT lf = {};
    lf.lfHeight = 72;
    lf.lfWeight = FW_BOLD;
    lf.lfItalic = TRUE;
    wcscpy_s(lf.lfFaceName, L"Georgia");

    HFONT titleFont = CreateFontIndirect(&lf);
    HFONT oldFont = (HFONT)SelectObject(hdc, titleFont);

    SetTextColor(hdc, RGB(220, 180, 100));

    std::wstring title = L"SHADOWS OVER THE THAMES";
    RECT titleRect = { 0, 100, g_window.width, 250 };
    DrawText(hdc, title.c_str(), -1, &titleRect, DT_CENTER | DT_SINGLELINE);

    // Подзаголовок
    lf.lfHeight = 28;
    lf.lfWeight = FW_NORMAL;
    lf.lfItalic = FALSE;
    wcscpy_s(lf.lfFaceName, L"Segoe UI");

    HFONT subtitleFont = CreateFontIndirect(&lf);
    SelectObject(hdc, subtitleFont);

    SetTextColor(hdc, RGB(180, 180, 200));

    std::wstring subtitle = L"Приключенческая игра!";
    RECT subtitleRect = { 0, 200, g_window.width, 300 };
    DrawText(hdc, subtitle.c_str(), -1, &subtitleRect, DT_CENTER | DT_SINGLELINE);

    // Инструкция
    lf.lfHeight = 20;
    HFONT infoFont = CreateFontIndirect(&lf);
    SelectObject(hdc, infoFont);

    SetTextColor(hdc, RGB(150, 150, 180));

    std::wstring info = L"Нажмите кнопку 'НАЧАТЬ ИГРУ' чтобы начать";
    RECT infoRect = { 0, g_window.height - 100, g_window.width, g_window.height - 50 };
    DrawText(hdc, info.c_str(), -1, &infoRect, DT_CENTER | DT_SINGLELINE);

    // Восстанавливаем шрифт
    SelectObject(hdc, oldFont);
    DeleteObject(titleFont);
    DeleteObject(subtitleFont);
    DeleteObject(infoFont);
}

// Отрисовка игры
void RenderGame(HDC hdc)
{
    // Очищаем фон
    HBRUSH background = CreateSolidBrush(RGB(30, 30, 40));
    RECT fullRect = { 0, 0, g_window.width, g_window.height };
    FillRect(hdc, &fullRect, background);
    DeleteObject(background);

    // Пробуем загрузить фон уровня
    HBITMAP hBackground = LoadBmpFromDebug("office_bg.bmp");
    if (hBackground)
    {
        HDC memDC = CreateCompatibleDC(hdc);
        HBITMAP oldBmp = (HBITMAP)SelectObject(memDC, hBackground);

        BITMAP bm;
        GetObject(hBackground, sizeof(bm), &bm);

        // Рисуем фон
        StretchBlt(hdc, 0, 0, g_window.width, g_window.height,
            memDC, 0, 0, bm.bmWidth, bm.bmHeight, SRCCOPY);

        SelectObject(memDC, oldBmp);
        DeleteDC(memDC);
        DeleteObject(hBackground);
    }

    // Информация об уровне
    SetBkMode(hdc, TRANSPARENT);

    LOGFONT lf = {};
    lf.lfHeight = 24;
    lf.lfWeight = FW_BOLD;
    wcscpy_s(lf.lfFaceName, L"Arial");

    HFONT font = CreateFontIndirect(&lf);
    HFONT oldFont = (HFONT)SelectObject(hdc, font);

    SetTextColor(hdc, RGB(255, 255, 255));

    std::wstring levelText = L"Уровень " + std::to_wstring(g_gameState.currentLevel);
    RECT levelRect = { 20, 20, 300, 60 };
    DrawText(hdc, levelText.c_str(), -1, &levelRect, DT_LEFT | DT_SINGLELINE);

    std::wstring hintText = L"ESC - Вернуться в меню";
    RECT hintRect = { g_window.width - 300, 20, g_window.width - 20, 60 };
    DrawText(hdc, hintText.c_str(), -1, &hintRect, DT_RIGHT | DT_SINGLELINE);

    SelectObject(hdc, oldFont);
    DeleteObject(font);
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
    char exePath[MAX_PATH];
    GetModuleFileNameA(nullptr, exePath, MAX_PATH);

    char debugMsg[512];
    sprintf_s(debugMsg, "Инициализация уровня 1. EXE путь: %s", exePath);
    OutputDebugStringA(debugMsg);

    // Проверяем доступность ресурсов
    HBITMAP testBmp = LoadBmpFromDebug("office_bg.bmp");
    if (!testBmp)
    {
        std::string errorMsg = "Ресурсы уровня не найдены!\n";
        errorMsg += "EXE находится в: ";
        errorMsg += exePath;
        errorMsg += "\nПоложите office_bg.bmp в ту же папку";

        MessageBoxA(g_window.hWnd,
            errorMsg.c_str(),
            "Внимание", MB_OK | MB_ICONINFORMATION);
    }
    else
    {
        MessageBoxA(g_window.hWnd,
            "Фон офиса успешно загружен!",
            "Успех", MB_OK | MB_ICONINFORMATION);
        DeleteObject(testBmp);
    }
}

// Оконная процедура
LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg)
    {
    case WM_COMMAND:
        // Обработка нажатий кнопок
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
        // Обновляем размеры окна
        g_window.width = LOWORD(lParam);
        g_window.height = HIWORD(lParam);

        // Перепозиционируем кнопки если они видны
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

        InvalidateRect(hwnd, nullptr, TRUE);
        break;

    case WM_PAINT:
    {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);

        // Рисуем только в зависимости от состояния
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
        PostQuitMessage(0);
        break;

    case WM_ERASEBKGND:
        // Не даем системе стирать фон - делаем это сами в WM_PAINT
        return 1;
    }

    return DefWindowProc(hwnd, uMsg, wParam, lParam);
}

// Очистка ресурсов
void Cleanup()
{
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
}