// Shadows Over The Thames.cpp
#include <windows.h>
#include <string>

// Структура для графического контекста

struct WindowContext 
{
    HWND hWnd;
    HDC device_context;
    HDC buffer_context;
    HBITMAP buffer_bitmap;
    int width;
    int height;
    bool should_exit = false;
} window;

// Прототипы функций
LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
void InitializeWindow(HINSTANCE hInstance, int nCmdShow);
void RenderFrame();
void Cleanup();

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance,
    LPSTR lpCmdLine, int nCmdShow)
{
    // Инициализация окна
    InitializeWindow(hInstance, nCmdShow);

    // Основной игровой цикл
    MSG msg = {};
    while (!window.should_exit)
    {
        // Обработка сообщений Windows
        while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
        {
            if (msg.message == WM_QUIT)
            {
                window.should_exit = true;
                break;
            }

            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }

        // Рендеринг кадра
        if (!window.should_exit)
        {
            RenderFrame();
        }
    }

    // Очистка
    Cleanup();

    return 0;
}

// Инициализация окна
void InitializeWindow(HINSTANCE hInstance, int nCmdShow)
{
    const wchar_t CLASS_NAME[] = L"ShadowsOverTheThamesClass";

    WNDCLASS wc = {};
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = CLASS_NAME;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hIcon = LoadIcon(NULL, IDI_APPLICATION);
    wc.style = CS_HREDRAW | CS_VREDRAW;

    RegisterClass(&wc);

    // Создаём окно
    window.hWnd = CreateWindowEx(
        0,
        CLASS_NAME,
        L"Shadows Over The Thames",
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT,
        1024, 768,  // Рекомендуемый размер для игры
        NULL,
        NULL,
        hInstance,
        NULL
    );

    // Получаем размеры клиентской области
    RECT clientRect;
    GetClientRect(window.hWnd, &clientRect);
    window.width = clientRect.right - clientRect.left;
    window.height = clientRect.bottom - clientRect.top;

    // Создаём графический контекст
    window.device_context = GetDC(window.hWnd);
    window.buffer_context = CreateCompatibleDC(window.device_context);
    window.buffer_bitmap = CreateCompatibleBitmap(window.device_context,
        window.width, window.height);
    SelectObject(window.buffer_context, window.buffer_bitmap);

    // Показываем окно
    ShowWindow(window.hWnd, nCmdShow);
    UpdateWindow(window.hWnd);
}

// Отрисовка одного кадра
void RenderFrame()
{
    if (window.buffer_context == NULL) return;

    // Очищаем буфер (заливаем тёмным цветом)
    HBRUSH background = CreateSolidBrush(RGB(20, 25, 35));
    RECT fullRect = { 0, 0, window.width, window.height };
    FillRect(window.buffer_context, &fullRect, background);
    DeleteObject(background);

    // Рисуем заголовок игры
    SetBkMode(window.buffer_context, TRANSPARENT);
    SetTextColor(window.buffer_context, RGB(220, 180, 100));

    LOGFONT lf = {};
    lf.lfHeight = 48;
    lf.lfWeight = FW_BOLD;
    wcscpy_s(lf.lfFaceName, L"Times New Roman");
    HFONT titleFont = CreateFontIndirect(&lf);
    HFONT oldFont = (HFONT)SelectObject(window.buffer_context, titleFont);

    std::wstring title = L"SHADOWS OVER THE THAMES";
    RECT titleRect = { 0, 100, window.width, 200 };
    DrawText(window.buffer_context, title.c_str(), -1, &titleRect,
        DT_CENTER | DT_SINGLELINE);

    // Меньший шрифт для подзаголовка
    lf.lfHeight = 24;
    lf.lfWeight = FW_NORMAL;
    HFONT subtitleFont = CreateFontIndirect(&lf);
    SelectObject(window.buffer_context, subtitleFont);

    std::wstring subtitle = L"Графический движок на Win32 API";
    RECT subtitleRect = { 0, 200, window.width, 300 };
    DrawText(window.buffer_context, subtitle.c_str(), -1, &subtitleRect,
        DT_CENTER | DT_SINGLELINE);

    // Информация
    lf.lfHeight = 18;
    HFONT infoFont = CreateFontIndirect(&lf);
    SelectObject(window.buffer_context, infoFont);
    SetTextColor(window.buffer_context, RGB(150, 150, 180));

    std::wstring info = L"Размер окна: " + std::to_wstring(window.width) +
        L" x " + std::to_wstring(window.height) +
        L"\nFPS будет реализован позднее";
    RECT infoRect = { 0, 350, window.width, 500 };
    DrawText(window.buffer_context, info.c_str(), -1, &infoRect,
        DT_CENTER | DT_WORDBREAK);

    // Восстанавливаем шрифт
    SelectObject(window.buffer_context, oldFont);
    DeleteObject(titleFont);
    DeleteObject(subtitleFont);
    DeleteObject(infoFont);

    // Копируем буфер на экран (двойная буферизация)
    BitBlt(window.device_context, 0, 0, window.width, window.height,
        window.buffer_context, 0, 0, SRCCOPY);
}

// Оконная процедура
LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg)
    {
    case WM_DESTROY:
        window.should_exit = true;
        PostQuitMessage(0);
        return 0;

    case WM_PAINT:
        // Перерисовываем окно
        RenderFrame();
        ValidateRect(hwnd, NULL);
        return 0;

    case WM_SIZE:
        // При изменении размера окна обновляем размеры буфера
        window.width = LOWORD(lParam);
        window.height = HIWORD(lParam);

        // Пересоздаём буферный битмап с новыми размерами
        if (window.buffer_bitmap)
        {
            DeleteObject(window.buffer_bitmap);
        }
        if (window.buffer_context)
        {
            window.buffer_bitmap = CreateCompatibleBitmap(window.device_context,
                window.width, window.height);
            SelectObject(window.buffer_context, window.buffer_bitmap);
        }

        // Запрашиваем перерисовку
        InvalidateRect(hwnd, NULL, FALSE);
        return 0;

    case WM_KEYDOWN:
        // Обработка клавиш (можно расширить)
        if (wParam == VK_ESCAPE)
        {
            DestroyWindow(hwnd);
        }
        return 0;

    case WM_CLOSE:
        DestroyWindow(hwnd);
        return 0;
    }

    return DefWindowProc(hwnd, uMsg, wParam, lParam);
}

// Очистка ресурсов
void Cleanup()
{
    if (window.buffer_bitmap)
        DeleteObject(window.buffer_bitmap);
    if (window.buffer_context)
        DeleteDC(window.buffer_context);
    if (window.device_context && window.hWnd)
        ReleaseDC(window.hWnd, window.device_context);
}