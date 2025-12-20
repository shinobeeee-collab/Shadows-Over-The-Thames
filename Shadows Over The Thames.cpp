// Shadows Over The Thames.cpp
#include <windows.h>
#include <string>
#include <vector>
#include <commctrl.h>
#include <cmath>
#include <algorithm>
#include <mmsystem.h>
#include <map>
#include <sstream>
#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "msimg32.lib")
#pragma comment(lib, "winmm.lib")

// Структура для анимации
struct AnimationFrames
{
    std::vector<HBITMAP> frames;
    int currentFrame = 0;
    DWORD lastUpdateTime = 0;
    DWORD frameDelay = 120; // мс
    bool loaded = false;

    // Очистка кадров
    void Clear() {
        for (auto& frame : frames) {
            if (frame) DeleteObject(frame);
        }
        frames.clear();
        currentFrame = 0;
        loaded = false;
    }
};

struct DialogLine {
    std::wstring speaker;
    std::wstring text;
    HBITMAP speakerFace;    // Портрет говорящего
    int faceIndex;          // Индекс выражения лица
};

struct DialogWindow {
    bool isActive = false;
    std::vector<DialogLine> lines;
    int currentLine = 0;

    // Позиция и размеры
    int x = 100;
    int y = 600;
    int width = 1200;
    int height = 250;

    // Цвета
    COLORREF bgColor = RGB(30, 30, 40);
    COLORREF borderColor = RGB(100, 80, 60);
    COLORREF textColor = RGB(255, 255, 255);
    COLORREF speakerColor = RGB(220, 180, 100);

    // Анимация текста
    std::wstring displayedText;
    DWORD textStartTime = 0;
    int charsPerSecond = 30;  // Скорость появления текста
    bool textComplete = false;

    // Портрет
    int portraitWidth = 180;
    int portraitHeight = 200;
    int portraitPadding = 20;

    // Кнопка продолжения
    bool showContinueButton = true;
    std::wstring continueText = L"▶ ПРОДОЛЖИТЬ";
};

// Структуры для игрока
struct Player {
    float x = 100.0f;
    float y = 300.0f;
    float width = 180.0f;
    float height = 256.0f;
    float speed = 5.0f;
    bool facingRight = true;
    bool isMoving = false;
    bool isRunning = false;

    AnimationFrames idleAnimation;
    AnimationFrames walkAnimation;
    AnimationFrames runAnimation;

    // Для обратной совместимости 
    HBITMAP hSpriteRight = nullptr;
    HBITMAP hSpriteRunRight = nullptr;

    DWORD lastRunTime = 0;
    DWORD boostStartTime = 0;
    bool isRunningBoost = false;
    DWORD idleTimer = 0;
    bool isIdle = true;

    // Получение текущего спрайта
    HBITMAP GetCurrentSprite()
    {
        if (isRunning && runAnimation.loaded && runAnimation.frames.size() > 0) {
            return runAnimation.frames[runAnimation.currentFrame];
        }
        else if (isMoving && walkAnimation.loaded && walkAnimation.frames.size() > 0) {
            return walkAnimation.frames[walkAnimation.currentFrame];
        }
        else if (idleAnimation.loaded && idleAnimation.frames.size() > 0) {
            return idleAnimation.frames[idleAnimation.currentFrame];
        }
        return nullptr;
    }

    // Обновление анимации
    void UpdateAnimation(DWORD currentTime) {
        AnimationFrames* anim = &idleAnimation; // По умолчанию idle

        if (isRunning && runAnimation.loaded) {
            anim = &runAnimation;
            isIdle = false;
            idleTimer = currentTime;
        }
        else if (isMoving && walkAnimation.loaded) {
            anim = &walkAnimation;
            isIdle = false;
            idleTimer = currentTime;
        }
        else if (idleAnimation.loaded) {
            anim = &idleAnimation;

            // Проверяем, сколько времени игрок стоит
            if (currentTime - idleTimer > 1000) { // 1 секунда неподвижности
                isIdle = true;
            }
            else {
                // Ещё не прошло время, используем первый кадр idle
                anim->currentFrame = 0;
                return;
            }
        }

        if (!anim->loaded || anim->frames.empty()) return;

        // Для idle-анимации обновляем кадры только если действительно idle
        if (anim == &idleAnimation && !isIdle) {
            anim->currentFrame = 0;
            return;
        }

        // Обновляем кадр, если пришло время
        if (currentTime - anim->lastUpdateTime > anim->frameDelay) {
            anim->currentFrame = (anim->currentFrame + 1) % anim->frames.size();
            anim->lastUpdateTime = currentTime;
        }
    }

    // Очистка ресурсов
    void Cleanup() {
        idleAnimation.Clear();
        walkAnimation.Clear();
        runAnimation.Clear();
    }
};

// Структура для врага
struct Enemy
{
    float x = 1500.0f;  // Позиция на уровне
    float y = 800.0f;
    float width = 180.0f;
    float height = 256.0f;
    bool isVisible = false;  // Виден только при удержании Q
    HBITMAP idleSprite = nullptr;  // Статичная картинка
    bool loaded = false;

    // Для обратной совместимости можно добавить анимацию позже
    AnimationFrames idleAnimation;
};

// Состояния боя
enum BattleState {
    BATTLE_PLAYER_TURN,    // Ход игрока
    BATTLE_ENEMY_TURN,     // Ход врага
    BATTLE_VICTORY,        // Победа
    BATTLE_DEFEAT,         // Поражение
    BATTLE_ESCAPED         // Побег
};

// Действия в бою
enum BattleAction {
    ACTION_ATTACK,         // Атака
    ACTION_DEFEND,         // Защита
    ACTION_ITEM,           // Использовать предмет
    ACTION_ESCAPE          // Побег
};

// Участник боя
struct BattleParticipant {
    std::wstring name;
    int maxHealth = 100;
    int currentHealth = 100;
    int attack = 20;
    int defense = 10;
    int speed = 15;
    bool isDefending = false;
    HBITMAP portrait = nullptr;
    HBITMAP battleSprite = nullptr;
    AnimationFrames attackAnimation;
    AnimationFrames hurtAnimation;
    AnimationFrames idleAnimation;

    // Для отображения в бою
    float x = 0;           // Позиция на экране боя
    float y = 0;
    float width = 300;
    float height = 400;

    // Полоска здоровья
    float healthBarWidth = 200;
    float healthBarHeight = 20;

    void TakeDamage(int damage) {
        if (isDefending) {
            damage = max(1, damage / 2);  // Защита уменьшает урон вдвое
            isDefending = false;
        }

        currentHealth -= damage;
        if (currentHealth < 0) currentHealth = 0;
    }

    void Heal(int amount) {
        currentHealth += amount;
        if (currentHealth > maxHealth) currentHealth = maxHealth;
    }

    float GetHealthPercentage() const {
        return (float)currentHealth / (float)maxHealth;
    }
};

// Боевая сцена
struct BattleScene {
    bool isActive = false;
    BattleState state = BATTLE_PLAYER_TURN;
    BattleParticipant player;
    BattleParticipant enemy;

    // Позиции участников
    float playerBattleX = 200;     // Игрок слева
    float playerBattleY = 400;
    float enemyBattleX = 1400;     // Враг справа
    float enemyBattleY = 400;

    // Интерфейс боя
    int selectedAction = 0;        // Выбранное действие (0-3)
    std::vector<std::wstring> actions = {
        L"⚔  АТАКА",
        L"🛡  ЗАЩИТА",
        L"💊  ПРЕДМЕТ",
        L"🏃  ПОБЕГ"
    };

    // Анимации и эффекты
    bool showDamageText = false;
    std::wstring damageText;
    int damageValue = 0;
    float damageTextX = 0;
    float damageTextY = 0;
    DWORD damageTextStartTime = 0;

    // Фон боя
    HBITMAP background = nullptr;

    // Сообщение в центре экрана
    std::wstring centerMessage;
    DWORD messageStartTime = 0;
    bool showMessage = false;

    // Таймер для автоматического хода врага
    DWORD enemyTurnStartTime = 0;

    void Reset() {
        state = BATTLE_PLAYER_TURN;
        selectedAction = 0;
        showDamageText = false;
        showMessage = false;
        player.isDefending = false;
        enemy.isDefending = false;
        enemyTurnStartTime = 0;
    }

    void ShowMessage(const std::wstring& msg) {
        centerMessage = msg;
        messageStartTime = GetTickCount();
        showMessage = true;
    }

    void ShowDamage(float x, float y, int damage, const std::wstring& text = L"") {
        damageTextX = x;
        damageTextY = y;
        damageValue = damage;
        damageText = text.empty() ? std::to_wstring(damage) : text;
        damageTextStartTime = GetTickCount();
        showDamageText = true;
    }
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
    bool inBattle = false;
    int currentLevel = 0;
    Player player;
    Enemy enemy;
    float levelWidth = 3840;
    float levelHeight = 2000;
    HBITMAP hLevelBackground = nullptr;
    float cameraX = 0;
    float cameraY = 0;
    bool isMusicPlaying = false;

    // Диалоговая система
    DialogWindow dialog;
    HBITMAP playerPortrait = nullptr;
    HBITMAP npcPortrait = nullptr;
    bool isDialogActive = false;

    // Боевая система
    BattleScene battle;
};

WindowContext g_window;
GameState g_gameState;

// Цвета
const COLORREF MENU_BG_COLOR = RGB(20, 25, 35);
const COLORREF BUTTON_COLOR = RGB(86, 98, 246);
const COLORREF BUTTON_HOVER_COLOR = RGB(105, 116, 255);
const COLORREF TEXT_COLOR = RGB(255, 255, 255);
const COLORREF TRANSPARENT_COLOR = RGB(255, 0, 255); // Маджента - прозрачный

// Прототипы функций ДИАЛОГОВОЙ СИСТЕМЫ
void StartDialog(const std::wstring& speaker, const std::wstring& text, HBITMAP face = nullptr);
void StartDialogSequence(const std::vector<DialogLine>& dialogLines);
void UpdateDialog();
void RenderDialog(HDC hdc);
void NextDialogLine();
void CloseDialog();
bool IsDialogActive();
HBITMAP CreatePortrait(int width, int height, COLORREF hairColor, COLORREF skinColor,
    COLORREF eyeColor, const std::wstring& expression = L"neutral");
void DrawRoundedRect(HDC hdc, int x, int y, int width, int height,
    COLORREF fillColor, COLORREF borderColor, int radius = 15);
void DrawTextWithShadow(HDC hdc, const std::wstring& text, int x, int y,
    COLORREF textColor, COLORREF shadowColor = RGB(0, 0, 0),
    int shadowOffset = 2);

// ОСНОВНЫЕ ПРОТОТИПЫ
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
void LoadWalkAnimation(Player& player, const std::vector<std::string>& rightFiles, DWORD frameDelay = 120);
void LoadRunAnimation(Player& player, const std::vector<std::string>& rightFiles, DWORD frameDelay = 100);
void LoadIdleAnimation(Player& player, const std::vector<std::string>& rightFiles, DWORD frameDelay = 200);
void LoadEnemy(Enemy& enemy);
void RenderEnemy(HDC hdc);

// БОЕВАЯ СИСТЕМА
bool CheckCollision(float x1, float y1, float w1, float h1,
    float x2, float y2, float w2, float h2);
void UpdateGameLogic();
void LoadBattleResources();
void StartBattle();
void EndBattle(bool victory);
void ProcessPlayerTurn(int action);
void ProcessEnemyTurn();
void UpdateBattle();
void RenderBattle(HDC hdc);

// ==================== РЕАЛИЗАЦИЯ ДИАЛОГОВОЙ СИСТЕМЫ ====================

bool IsDialogActive() {
    return g_gameState.dialog.isActive;
}

void StartDialog(const std::wstring& speaker, const std::wstring& text, HBITMAP face) {
    DialogLine line;
    line.speaker = speaker;
    line.text = text;
    line.speakerFace = face;
    line.faceIndex = 0;

    g_gameState.dialog.lines.clear();
    g_gameState.dialog.lines.push_back(line);
    g_gameState.dialog.currentLine = 0;
    g_gameState.dialog.isActive = true;
    g_gameState.isDialogActive = true;
    g_gameState.dialog.displayedText = L"";
    g_gameState.dialog.textStartTime = GetTickCount();
    g_gameState.dialog.textComplete = false;
}

void StartDialogSequence(const std::vector<DialogLine>& dialogLines) {
    g_gameState.dialog.lines = dialogLines;
    g_gameState.dialog.currentLine = 0;
    g_gameState.dialog.isActive = true;
    g_gameState.isDialogActive = true;
    g_gameState.dialog.displayedText = L"";
    g_gameState.dialog.textStartTime = GetTickCount();
    g_gameState.dialog.textComplete = false;
}

void UpdateDialog() {
    if (!g_gameState.dialog.isActive || g_gameState.dialog.lines.empty()) return;

    DialogWindow& dialog = g_gameState.dialog;
    const DialogLine& currentLine = dialog.lines[dialog.currentLine];

    if (dialog.textComplete) return;

    DWORD currentTime = GetTickCount();
    DWORD elapsedTime = currentTime - dialog.textStartTime;

    // Вычисляем сколько символов должно отобразиться
    int targetChars = (elapsedTime * dialog.charsPerSecond) / 1000;

    if (targetChars > (int)currentLine.text.length()) {
        dialog.displayedText = currentLine.text;
        dialog.textComplete = true;
    }
    else {
        dialog.displayedText = currentLine.text.substr(0, targetChars);
    }
}

HBITMAP CreatePortrait(int width, int height, COLORREF hairColor,
    COLORREF skinColor, COLORREF eyeColor,
    const std::wstring& expression) {

    // Создаем DC и битмап
    HDC screenDC = GetDC(NULL);
    HDC memDC = CreateCompatibleDC(screenDC);

    // Создаем битмап с поддержкой альфа-канала (32-bit)
    BITMAPINFO bmi = { 0 };
    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = width;
    bmi.bmiHeader.biHeight = -height;  // Отрицательная высота = сверху вниз
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 32;     // 32 бита на пиксель
    bmi.bmiHeader.biCompression = BI_RGB;

    void* pBits = NULL;
    HBITMAP bitmap = CreateDIBSection(screenDC, &bmi, DIB_RGB_COLORS, &pBits, NULL, 0);

    if (!bitmap) {
        DeleteDC(memDC);
        ReleaseDC(NULL, screenDC);
        return NULL;
    }

    HBITMAP oldBmp = (HBITMAP)SelectObject(memDC, bitmap);

    // Заливаем фон прозрачным цветом (маджента)
    HBRUSH bgBrush = CreateSolidBrush(TRANSPARENT_COLOR);
    RECT rect = { 0, 0, width, height };
    FillRect(memDC, &rect, bgBrush);
    DeleteObject(bgBrush);

    // === Рисуем голову (овал) ===
    HBRUSH headBrush = CreateSolidBrush(skinColor);
    HPEN headPen = CreatePen(PS_SOLID, 2, RGB(
        GetRValue(skinColor) * 0.7f,
        GetGValue(skinColor) * 0.7f,
        GetBValue(skinColor) * 0.7f
    ));

    HPEN oldPen = (HPEN)SelectObject(memDC, headPen);
    HBRUSH oldBrush = (HBRUSH)SelectObject(memDC, headBrush);

    int headWidth = width * 2 / 3;
    int headHeight = height * 3 / 5;
    int headX = (width - headWidth) / 2;
    int headY = height / 6;

    // Рисуем голову
    Ellipse(memDC, headX, headY, headX + headWidth, headY + headHeight);

    // === Рисуем волосы ===
    HBRUSH hairBrush = CreateSolidBrush(hairColor);
    SelectObject(memDC, hairBrush);

    int hairTop = headY - headHeight / 8;
    int hairHeight = headHeight / 2;
    Ellipse(memDC, headX - 10, hairTop, headX + headWidth + 10, hairTop + hairHeight);

    // === Рисуем глаза ===
    HBRUSH eyeBrush = CreateSolidBrush(eyeColor);
    SelectObject(memDC, eyeBrush);

    int eyeSize = headWidth / 6;
    int eyeY = headY + headHeight / 3;

    // Левый глаз
    int leftEyeX = headX + headWidth / 3 - eyeSize / 2;
    Ellipse(memDC, leftEyeX, eyeY, leftEyeX + eyeSize, eyeY + eyeSize);

    // Правый глаз
    int rightEyeX = headX + headWidth * 2 / 3 - eyeSize / 2;
    Ellipse(memDC, rightEyeX, eyeY, rightEyeX + eyeSize, eyeY + eyeSize);

    // Зрачки
    HBRUSH pupilBrush = CreateSolidBrush(RGB(0, 0, 0));
    SelectObject(memDC, pupilBrush);
    int pupilSize = eyeSize / 2;

    // Левый зрачок
    Ellipse(memDC,
        leftEyeX + pupilSize / 2,
        eyeY + pupilSize / 2,
        leftEyeX + pupilSize / 2 + pupilSize,
        eyeY + pupilSize / 2 + pupilSize);

    // Правый зрачок
    Ellipse(memDC,
        rightEyeX + pupilSize / 2,
        eyeY + pupilSize / 2,
        rightEyeX + pupilSize / 2 + pupilSize,
        eyeY + pupilSize / 2 + pupilSize);

    // === Рисуем рот ===
    HPEN mouthPen = NULL;
    if (expression == L"happy") {
        // Улыбка
        mouthPen = CreatePen(PS_SOLID, 3, RGB(200, 100, 100));
        SelectObject(memDC, mouthPen);
        Arc(memDC,
            leftEyeX, eyeY + eyeSize,
            rightEyeX + eyeSize, eyeY + eyeSize * 2,
            leftEyeX, eyeY + eyeSize * 1.5,
            rightEyeX + eyeSize, eyeY + eyeSize * 1.5);
    }
    else if (expression == L"sad") {
        // Грустный рот
        mouthPen = CreatePen(PS_SOLID, 3, RGB(100, 100, 200));
        SelectObject(memDC, mouthPen);
        Arc(memDC,
            leftEyeX, eyeY + eyeSize * 2,
            rightEyeX + eyeSize, eyeY + eyeSize,
            leftEyeX, eyeY + eyeSize * 1.5,
            rightEyeX + eyeSize, eyeY + eyeSize * 1.5);
    }
    else {
        // Нейтральный рот
        mouthPen = CreatePen(PS_SOLID, 2, RGB(150, 80, 80));
        SelectObject(memDC, mouthPen);
        MoveToEx(memDC, leftEyeX + eyeSize / 3, eyeY + eyeSize * 1.5, NULL);
        LineTo(memDC, rightEyeX + eyeSize / 3, eyeY + eyeSize * 1.5);
    }

    // === Рисуем нос ===
    HPEN nosePen = CreatePen(PS_SOLID, 2, RGB(
        GetRValue(skinColor) * 0.7f,
        GetGValue(skinColor) * 0.7f,
        GetBValue(skinColor) * 0.7f
    ));
    SelectObject(memDC, nosePen);
    int noseX = headX + headWidth / 2;
    int noseY = eyeY + eyeSize + eyeSize / 4;
    MoveToEx(memDC, noseX, noseY, NULL);
    LineTo(memDC, noseX, noseY + eyeSize / 2);

    // === Одежда (простой воротник) ===
    HBRUSH clothesBrush = CreateSolidBrush(RGB(80, 60, 40));
    SelectObject(memDC, clothesBrush);

    RECT clothesRect = {
        headX - 5, headY + headHeight - 10,
        headX + headWidth + 5, headY + headHeight + 20
    };
    RoundRect(memDC, clothesRect.left, clothesRect.top,
        clothesRect.right, clothesRect.bottom, 10, 10);

    // === Очистка ===
    SelectObject(memDC, oldPen);
    SelectObject(memDC, oldBrush);

    // Удаляем GDI объекты
    DeleteObject(headBrush);
    DeleteObject(hairBrush);
    DeleteObject(eyeBrush);
    DeleteObject(pupilBrush);
    DeleteObject(clothesBrush);
    DeleteObject(headPen);
    DeleteObject(mouthPen);
    DeleteObject(nosePen);

    // Восстанавливаем и возвращаем
    SelectObject(memDC, oldBmp);
    DeleteDC(memDC);
    ReleaseDC(NULL, screenDC);

    return bitmap;
}

void DrawRoundedRect(HDC hdc, int x, int y, int width, int height,
    COLORREF fillColor, COLORREF borderColor, int radius) {
    // Создаем перо и кисть
    HPEN borderPen = CreatePen(PS_SOLID, 2, borderColor);
    HBRUSH fillBrush = CreateSolidBrush(fillColor);

    // Сохраняем старые
    HPEN oldPen = (HPEN)SelectObject(hdc, borderPen);
    HBRUSH oldBrush = (HBRUSH)SelectObject(hdc, fillBrush);

    // Рисуем закругленный прямоугольник
    RoundRect(hdc, x, y, x + width, y + height, radius, radius);

    // Восстанавливаем
    SelectObject(hdc, oldPen);
    SelectObject(hdc, oldBrush);

    // Удаляем GDI объекты
    DeleteObject(borderPen);
    DeleteObject(fillBrush);
}

void DrawTextWithShadow(HDC hdc, const std::wstring& text, int x, int y,
    COLORREF textColor, COLORREF shadowColor, int shadowOffset) {
    COLORREF oldColor = SetTextColor(hdc, shadowColor);
    TextOut(hdc, x + shadowOffset, y + shadowOffset, text.c_str(), (int)text.length());

    SetTextColor(hdc, textColor);
    TextOut(hdc, x, y, text.c_str(), (int)text.length());

    SetTextColor(hdc, oldColor);
}

void RenderDialog(HDC hdc) {
    if (!g_gameState.dialog.isActive || g_gameState.dialog.lines.empty()) return;

    DialogWindow& dialog = g_gameState.dialog;
    const DialogLine& currentLine = dialog.lines[dialog.currentLine];

    // Сохраняем текущие настройки DC
    int oldBkMode = SetBkMode(hdc, TRANSPARENT);
    COLORREF oldTextColor = SetTextColor(hdc, dialog.textColor);
    HFONT oldFont = (HFONT)GetCurrentObject(hdc, OBJ_FONT);  // Сохраняем текущий шрифт

    // === Отрисовка фона диалогового окна ===
    DrawRoundedRect(hdc, dialog.x, dialog.y, dialog.width, dialog.height,
        dialog.bgColor, dialog.borderColor);

    // === Отрисовка портрета говорящего ===
    int portraitX = dialog.x + dialog.portraitPadding;
    int portraitY = dialog.y + (dialog.height - dialog.portraitHeight) / 2;

    if (currentLine.speakerFace) {
        // Рисуем портрет
        HDC memDC = CreateCompatibleDC(hdc);
        HBITMAP oldBmp = (HBITMAP)SelectObject(memDC, currentLine.speakerFace);

        // Рамка вокруг портрета
        DrawRoundedRect(hdc, portraitX - 5, portraitY - 5,
            dialog.portraitWidth + 10, dialog.portraitHeight + 10,
            dialog.borderColor, RGB(150, 120, 90));

        // Сам портрет
        TransparentBlt(hdc, portraitX, portraitY, dialog.portraitWidth, dialog.portraitHeight,
            memDC, 0, 0, dialog.portraitWidth, dialog.portraitHeight, TRANSPARENT_COLOR);

        SelectObject(memDC, oldBmp);
        DeleteDC(memDC);
    }
    else {
        // Запасной портрет если нет изображения
        DrawRoundedRect(hdc, portraitX, portraitY,
            dialog.portraitWidth, dialog.portraitHeight,
            RGB(100, 100, 120), dialog.borderColor);

        // Имя говорящего в портрете (только если нет изображения)
        HFONT portraitFont = CreateFont(24, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
            DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
            DEFAULT_QUALITY, DEFAULT_PITCH, L"Arial");
        HFONT oldPortraitFont = (HFONT)SelectObject(hdc, portraitFont);

        SetTextColor(hdc, RGB(255, 255, 255));
        RECT portraitTextRect = { portraitX, portraitY + dialog.portraitHeight / 2 - 20,
                                portraitX + dialog.portraitWidth, portraitY + dialog.portraitHeight / 2 + 20 };
        DrawText(hdc, L"???", -1, &portraitTextRect, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

        SelectObject(hdc, oldPortraitFont);
        DeleteObject(portraitFont);
    }

    // === Отрисовка текстовой области ===
    int textAreaX = portraitX + dialog.portraitWidth + dialog.portraitPadding * 2;
    int textAreaY = dialog.y + dialog.portraitPadding;
    int textAreaWidth = dialog.width - textAreaX - dialog.portraitPadding;
    int textAreaHeight = dialog.height - dialog.portraitPadding * 2;

    // Фон текстовой области
    DrawRoundedRect(hdc, textAreaX, textAreaY, textAreaWidth, textAreaHeight,
        RGB(40, 40, 50), RGB(80, 60, 40));

    // === Отрисовка имени говорящего ===
    HFONT speakerFont = CreateFont(28, 0, 0, 0, FW_BOLD, TRUE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Georgia");
    SelectObject(hdc, speakerFont);

    SetTextColor(hdc, dialog.speakerColor);
    RECT speakerRect = { textAreaX + 20, textAreaY + 15,
                       textAreaX + textAreaWidth - 20, textAreaY + 60 };

    // Имя говорящего (только один раз!)
    DrawText(hdc, currentLine.speaker.c_str(), -1, &speakerRect,
        DT_LEFT | DT_VCENTER | DT_SINGLELINE);

    // === Отрисовка текста диалога ===
    HFONT dialogFont = CreateFont(22, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Segoe UI");
    SelectObject(hdc, dialogFont);
    DeleteObject(speakerFont);  // Удаляем шрифт имени после использования

    SetTextColor(hdc, dialog.textColor);
    RECT textRect = { textAreaX + 25, textAreaY + 70,
                    textAreaX + textAreaWidth - 25, textAreaY + textAreaHeight - 50 };

    // Рисуем анимированный текст
    if (dialog.displayedText.length() > 0) {
        DrawText(hdc, dialog.displayedText.c_str(), -1, &textRect,
            DT_LEFT | DT_TOP | DT_WORDBREAK);
    }

    // === Отрисовка индикатора продолжения ===
    if (dialog.textComplete && dialog.showContinueButton) {
        HFONT continueFont = CreateFont(18, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
            DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
            DEFAULT_QUALITY, DEFAULT_PITCH, L"Arial");
        SelectObject(hdc, continueFont);

        // Мигающая анимация
        bool showIndicator = ((GetTickCount() / 500) % 2) == 0;

        if (showIndicator) {
            SetTextColor(hdc, RGB(220, 180, 100));
            RECT continueRect = { textAreaX + textAreaWidth - 150, textAreaY + textAreaHeight - 40,
                                textAreaX + textAreaWidth - 20, textAreaY + textAreaHeight - 10 };
            DrawText(hdc, dialog.continueText.c_str(), -1, &continueRect,
                DT_RIGHT | DT_VCENTER | DT_SINGLELINE);
        }

        DeleteObject(continueFont);
    }

    // Восстанавливаем настройки
    SelectObject(hdc, oldFont);    // Восстанавливаем исходный шрифт
    DeleteObject(dialogFont);      // Удаляем диалоговый шрифт
    SetBkMode(hdc, oldBkMode);
    SetTextColor(hdc, oldTextColor);
}

void NextDialogLine() {
    DialogWindow& dialog = g_gameState.dialog;

    if (!dialog.textComplete) {
        // Пропускаем анимацию текста
        dialog.displayedText = dialog.lines[dialog.currentLine].text;
        dialog.textComplete = true;
        return;
    }

    dialog.currentLine++;

    if (dialog.currentLine >= (int)dialog.lines.size()) {
        CloseDialog();
    }
    else {
        // Начинаем новую строку
        dialog.displayedText = L"";
        dialog.textStartTime = GetTickCount();
        dialog.textComplete = false;
    }
}

void CloseDialog() {
    g_gameState.dialog.isActive = false;
    g_gameState.isDialogActive = false;
    g_gameState.dialog.lines.clear();
    g_gameState.dialog.currentLine = 0;
}

// ==================== ОСНОВНЫЕ ФУНКЦИИ ====================

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
}

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

void DrawMirroredBitmap(HDC hdc, int x, int y, int width, int height,
    HDC memDC, HBITMAP hBitmap, int srcWidth, int srcHeight)
{
    HDC mirrorDC = CreateCompatibleDC(hdc);
    HBITMAP hMirrorBmp = CreateCompatibleBitmap(hdc, srcWidth, srcHeight);
    HBITMAP hOldMirrorBmp = (HBITMAP)SelectObject(mirrorDC, hMirrorBmp);

    HBITMAP hOldBmp = (HBITMAP)SelectObject(memDC, hBitmap);
    BitBlt(mirrorDC, 0, 0, srcWidth, srcHeight, memDC, 0, 0, SRCCOPY);

    StretchBlt(mirrorDC, srcWidth - 1, 0, -srcWidth, srcHeight,
        mirrorDC, 0, 0, srcWidth, srcHeight, SRCCOPY);

    TransparentBlt(hdc, x, y, width, height,
        mirrorDC, 0, 0, srcWidth, srcHeight, TRANSPARENT_COLOR);

    SelectObject(mirrorDC, hOldMirrorBmp);
    SelectObject(memDC, hOldBmp);
    DeleteObject(hMirrorBmp);
    DeleteDC(mirrorDC);
}

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

// ==================== ВРАГ ====================

void LoadEnemy(Enemy& enemy)
{
    OutputDebugStringA("=== ЗАГРУЗКА ВРАГА ===\n");

    // Загружаем статичный спрайт врага
    enemy.idleSprite = LoadBmpFromDebug("enemy_idle.bmp");

    if (enemy.idleSprite) {
        enemy.loaded = true;
        OutputDebugStringA("Спрайт врага загружен успешно\n");

        // Проверяем размеры
        BITMAP bm;
        if (GetObject(enemy.idleSprite, sizeof(BITMAP), &bm)) {
            char debugMsg[512];
            sprintf_s(debugMsg, "Размер спрайта врага: %dx%d\n", bm.bmWidth, bm.bmHeight);
            OutputDebugStringA(debugMsg);
        }
    }
    else {
        OutputDebugStringA("Не удалось загрузить спрайт врага. Создаем временный...\n");

        // Создаем временный спрайт врага (красный прямоугольник)
        HDC screenDC = GetDC(NULL);
        HDC memDC = CreateCompatibleDC(screenDC);

        BITMAPINFO bmi = { 0 };
        bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
        bmi.bmiHeader.biWidth = (int)enemy.width;
        bmi.bmiHeader.biHeight = -(int)enemy.height;  // Отрицательная высота
        bmi.bmiHeader.biPlanes = 1;
        bmi.bmiHeader.biBitCount = 32;
        bmi.bmiHeader.biCompression = BI_RGB;

        void* pBits = NULL;
        enemy.idleSprite = CreateDIBSection(screenDC, &bmi, DIB_RGB_COLORS, &pBits, NULL, 0);

        if (enemy.idleSprite) {
            HBITMAP oldBmp = (HBITMAP)SelectObject(memDC, enemy.idleSprite);

            // Заливаем прозрачным цветом
            HBRUSH bgBrush = CreateSolidBrush(TRANSPARENT_COLOR);
            RECT rect = { 0, 0, (int)enemy.width, (int)enemy.height };
            FillRect(memDC, &rect, bgBrush);
            DeleteObject(bgBrush);

            // Рисуем красного врага
            HBRUSH enemyBrush = CreateSolidBrush(RGB(255, 50, 50));
            HPEN enemyPen = CreatePen(PS_SOLID, 3, RGB(200, 0, 0));

            HPEN oldPen = (HPEN)SelectObject(memDC, enemyPen);
            HBRUSH oldBrush = (HBRUSH)SelectObject(memDC, enemyBrush);

            // Тело врага
            Ellipse(memDC, 30, 80, 150, 250);

            // Голова
            HBRUSH headBrush = CreateSolidBrush(RGB(200, 150, 150));
            SelectObject(memDC, headBrush);
            Ellipse(memDC, 60, 30, 120, 90);

            // Глаза (злые)
            HBRUSH eyeBrush = CreateSolidBrush(RGB(255, 255, 0));
            SelectObject(memDC, eyeBrush);
            Ellipse(memDC, 75, 50, 85, 60);  // Левый глаз
            Ellipse(memDC, 95, 50, 105, 60); // Правый глаз

            // Рот (злой)
            HPEN mouthPen = CreatePen(PS_SOLID, 3, RGB(150, 0, 0));
            SelectObject(memDC, mouthPen);
            Arc(memDC, 70, 70, 110, 85, 70, 77, 110, 77);  // Злая ухмылка

            // Восстанавливаем и чистим
            SelectObject(memDC, oldPen);
            SelectObject(memDC, oldBrush);
            SelectObject(memDC, oldBmp);

            DeleteObject(enemyBrush);
            DeleteObject(headBrush);
            DeleteObject(eyeBrush);
            DeleteObject(enemyPen);
            DeleteObject(mouthPen);

            enemy.loaded = true;
            OutputDebugStringA("Временный спрайт врага создан\n");
        }

        DeleteDC(memDC);
        ReleaseDC(NULL, screenDC);
    }
}

void RenderEnemy(HDC hdc)
{
    Enemy& enemy = g_gameState.enemy;

    // Враг виден только при удержании Q
    if (!enemy.isVisible || !enemy.loaded || !enemy.idleSprite)
        return;

    int screenX = (int)enemy.x - (int)g_gameState.cameraX - (int)enemy.width / 2;
    int screenY = (int)enemy.y - (int)g_gameState.cameraY - (int)enemy.height / 2;

    HDC memDC = CreateCompatibleDC(hdc);
    HBITMAP oldBmp = (HBITMAP)SelectObject(memDC, enemy.idleSprite);

    BITMAP bm;
    GetObject(enemy.idleSprite, sizeof(bm), &bm);

    // Рисуем с прозрачностью
    TransparentBlt(hdc, screenX, screenY,
        (int)enemy.width, (int)enemy.height,
        memDC, 0, 0, bm.bmWidth, bm.bmHeight, TRANSPARENT_COLOR);

    SelectObject(memDC, oldBmp);
    DeleteDC(memDC);
}

// ==================== БОЕВАЯ СИСТЕМА ====================

bool CheckCollision(float x1, float y1, float w1, float h1,
    float x2, float y2, float w2, float h2)
{
    return (x1 < x2 + w2 &&
        x1 + w1 > x2 &&
        y1 < y2 + h2 &&
        y1 + h1 > y2);
}

void LoadBattleResources() {
    OutputDebugStringA("=== ЗАГРУЗКА РЕСУРСОВ БОЯ ===\n");

    BattleScene& battle = g_gameState.battle;

    // Загружаем фон боя
    battle.background = LoadBmpFromDebug("battle_background.bmp");
    if (!battle.background) {
        OutputDebugStringA("Не найден фон боя. Создаем временный...\n");

        // Создаем простой темный фон
        HDC screenDC = GetDC(NULL);
        HDC memDC = CreateCompatibleDC(screenDC);

        battle.background = CreateCompatibleBitmap(screenDC, g_window.width, g_window.height);
        HBITMAP oldBmp = (HBITMAP)SelectObject(memDC, battle.background);

        // Градиент от темно-красного к черному
        for (int y = 0; y < g_window.height; y++) {
            float t = (float)y / g_window.height;
            int r = (int)(80 * (1.0f - t));
            int g = (int)(20 * (1.0f - t));
            int b = (int)(20 * (1.0f - t));

            HPEN pen = CreatePen(PS_SOLID, 1, RGB(r, g, b));
            HPEN oldPen = (HPEN)SelectObject(memDC, pen);

            MoveToEx(memDC, 0, y, NULL);
            LineTo(memDC, g_window.width, y);

            SelectObject(memDC, oldPen);
            DeleteObject(pen);
        }

        // Рисуем арену (круг)
        HBRUSH arenaBrush = CreateSolidBrush(RGB(50, 30, 30));
        HPEN arenaPen = CreatePen(PS_SOLID, 3, RGB(100, 50, 50));

        HPEN oldPen = (HPEN)SelectObject(memDC, arenaPen);
        HBRUSH oldBrush = (HBRUSH)SelectObject(memDC, arenaBrush);

        int arenaSize = 600;
        int arenaX = g_window.width / 2 - arenaSize / 2;
        int arenaY = g_window.height / 2 - arenaSize / 2;
        Ellipse(memDC, arenaX, arenaY, arenaX + arenaSize, arenaY + arenaSize);

        // Восстанавливаем
        SelectObject(memDC, oldPen);
        SelectObject(memDC, oldBrush);
        SelectObject(memDC, oldBmp);

        DeleteObject(arenaBrush);
        DeleteObject(arenaPen);
        DeleteDC(memDC);
        ReleaseDC(NULL, screenDC);
    }

    // Загружаем спрайты для боя
    battle.player.battleSprite = LoadBmpFromDebug("player_battle.bmp");
    if (!battle.player.battleSprite) {
        // Используем обычный спрайт игрока
        battle.player.battleSprite = g_gameState.player.GetCurrentSprite();
        if (!battle.player.battleSprite && g_gameState.player.idleAnimation.frames.size() > 0) {
            battle.player.battleSprite = g_gameState.player.idleAnimation.frames[0];
        }
    }

    battle.enemy.battleSprite = LoadBmpFromDebug("enemy_battle.bmp");
    if (!battle.enemy.battleSprite) {
        battle.enemy.battleSprite = g_gameState.enemy.idleSprite;
    }

    // Настраиваем участников боя
    battle.player.name = L"АЛЕКС ХЭМПТОН";
    battle.player.maxHealth = 120;
    battle.player.currentHealth = 120;
    battle.player.attack = 25;
    battle.player.defense = 15;
    battle.player.speed = 20;
    battle.player.portrait = g_gameState.playerPortrait;
    battle.player.x = battle.playerBattleX;
    battle.player.y = battle.playerBattleY;

    battle.enemy.name = L"ТЕНЕВОЙ СТРАЖ";
    battle.enemy.maxHealth = 100;
    battle.enemy.currentHealth = 100;
    battle.enemy.attack = 20;
    battle.enemy.defense = 12;
    battle.enemy.speed = 18;
    battle.enemy.portrait = g_gameState.npcPortrait;
    battle.enemy.x = battle.enemyBattleX;
    battle.enemy.y = battle.enemyBattleY;

    OutputDebugStringA("Ресурсы боя загружены\n");
}

void StartBattle() {
    if (g_gameState.inBattle) return;

    OutputDebugStringA("=== НАЧАЛО БОЯ ===\n");

    g_gameState.inBattle = true;
    g_gameState.battle.isActive = true;
    g_gameState.battle.Reset();

    // Останавливаем музыку уровня
    StopBackgroundMusic();

    // Включаем музыку боя
    PlayBackgroundMusic("x64\\Debug\\battle_music.wav");

    // Показываем сообщение о начале боя
    g_gameState.battle.ShowMessage(L"⚔  НАЧАЛО БОЯ!");

    InvalidateRect(g_window.hWnd, nullptr, TRUE);
}

void EndBattle(bool victory) {
    g_gameState.inBattle = false;
    g_gameState.battle.isActive = false;

    // Останавливаем музыку боя
    StopBackgroundMusic();

    // Возвращаем музыку уровня
    PlayBackgroundMusic("x64\\Debug\\bazar.wav");

    // Если победа - враг исчезает
    if (victory) {
        g_gameState.enemy.x = -1000;
        g_gameState.enemy.y = -1000;

        // Показываем диалог победы
        StartDialog(L"АЛЕКС ХЭМПТОН",
            L"Еще один призрак лондонских улиц отправлен в небытие. Но что-то подсказывает мне, что это только начало...");
    }

    InvalidateRect(g_window.hWnd, nullptr, TRUE);
}

void ProcessPlayerTurn(int action) {
    BattleScene& battle = g_gameState.battle;

    switch (action) {
    case ACTION_ATTACK: {
        // Игрок атакует
        int damage = battle.player.attack + (rand() % 11) - 5;
        damage = max(5, damage - battle.enemy.defense / 2);

        battle.enemy.TakeDamage(damage);
        battle.ShowDamage(battle.enemy.x, battle.enemy.y - 100, damage, L"⚔");
        battle.ShowMessage(L"АЛЕКС атакует!");

        if (battle.enemy.currentHealth <= 0) {
            battle.state = BATTLE_VICTORY;
            battle.ShowMessage(L"ПОБЕДА!");
            return;
        }

        battle.state = BATTLE_ENEMY_TURN;
        battle.enemyTurnStartTime = GetTickCount();
        break;
    }

    case ACTION_DEFEND: {
        // Игрок защищается
        battle.player.isDefending = true;
        battle.ShowMessage(L"АЛЕКС готовится к защите!");

        battle.state = BATTLE_ENEMY_TURN;
        battle.enemyTurnStartTime = GetTickCount();
        break;
    }

    case ACTION_ITEM: {
        // Использование предмета (здоровье)
        int healAmount = 30;
        battle.player.Heal(healAmount);
        battle.ShowDamage(battle.player.x, battle.player.y - 100, healAmount, L"❤");
        battle.ShowMessage(L"АЛЕКС использует аптечку!");

        battle.state = BATTLE_ENEMY_TURN;
        battle.enemyTurnStartTime = GetTickCount();
        break;
    }

    case ACTION_ESCAPE: {
        // Побег из боя
        int escapeChance = 50 + battle.player.speed - battle.enemy.speed;
        if ((rand() % 100) < escapeChance) {
            battle.state = BATTLE_ESCAPED;
            battle.ShowMessage(L"УСПЕШНЫЙ ПОБЕГ!");
        }
        else {
            battle.ShowMessage(L"ПОБЕГ НЕ УДАЛСЯ!");
            battle.state = BATTLE_ENEMY_TURN;
            battle.enemyTurnStartTime = GetTickCount();
        }
        break;
    }
    }
}

void ProcessEnemyTurn() {
    BattleScene& battle = g_gameState.battle;

    // Простой ИИ врага
    int action;
    if (battle.enemy.currentHealth < 30 && (rand() % 100) < 40) {
        action = ACTION_DEFEND;
    }
    else if (battle.player.currentHealth < 50 && (rand() % 100) < 60) {
        action = ACTION_ATTACK;
    }
    else {
        action = rand() % 4;
    }

    switch (action) {
    case ACTION_ATTACK: {
        int damage = battle.enemy.attack + (rand() % 11) - 5;
        damage = max(5, damage - battle.player.defense / 2);

        battle.player.TakeDamage(damage);
        battle.ShowDamage(battle.player.x, battle.player.y - 100, damage, L"⚔");
        battle.ShowMessage(L"ТЕНЕВОЙ СТРАЖ атакует!");

        if (battle.player.currentHealth <= 0) {
            battle.state = BATTLE_DEFEAT;
            battle.ShowMessage(L"ПОРАЖЕНИЕ...");
            return;
        }
        break;
    }

    case ACTION_DEFEND: {
        battle.enemy.isDefending = true;
        battle.ShowMessage(L"ТЕНЕВОЙ СТРАЖ защищается!");
        break;
    }

    default: {
        int damage = battle.enemy.attack + (rand() % 11) - 5;
        damage = max(5, damage - battle.player.defense / 2);

        battle.player.TakeDamage(damage);
        battle.ShowDamage(battle.player.x, battle.player.y - 100, damage, L"⚔");
        battle.ShowMessage(L"ТЕНЕВОЙ СТРАЖ атакует!");

        if (battle.player.currentHealth <= 0) {
            battle.state = BATTLE_DEFEAT;
            battle.ShowMessage(L"ПОРАЖЕНИЕ...");
            return;
        }
        break;
    }
    }

    battle.state = BATTLE_PLAYER_TURN;
}

void UpdateBattle() {
    BattleScene& battle = g_gameState.battle;

    // Проверяем состояние боя
    switch (battle.state) {
    case BATTLE_VICTORY:
        if (battle.showMessage && (GetTickCount() - battle.messageStartTime) > 2000) {
            EndBattle(true);
        }
        break;

    case BATTLE_DEFEAT:
        if (battle.showMessage && (GetTickCount() - battle.messageStartTime) > 2000) {
            EndBattle(false);
        }
        break;

    case BATTLE_ESCAPED:
        if (battle.showMessage && (GetTickCount() - battle.messageStartTime) > 1500) {
            EndBattle(false);
        }
        break;

    case BATTLE_ENEMY_TURN:
        if (battle.enemyTurnStartTime == 0) {
            battle.enemyTurnStartTime = GetTickCount();
        }
        else if (GetTickCount() - battle.enemyTurnStartTime > 1000) {
            ProcessEnemyTurn();
            battle.enemyTurnStartTime = 0;
        }
        break;
    }

    // Скрываем сообщение через 1.5 секунды
    if (battle.showMessage && (GetTickCount() - battle.messageStartTime) > 1500) {
        battle.showMessage = false;
    }

    // Скрываем текст урона через 1 секунду
    if (battle.showDamageText && (GetTickCount() - battle.damageTextStartTime) > 1000) {
        battle.showDamageText = false;
    }
}

void RenderBattle(HDC hdc) {
    BattleScene& battle = g_gameState.battle;

    if (!battle.isActive) return;

    // Отрисовываем фон боя
    if (battle.background) {
        HDC memDC = CreateCompatibleDC(hdc);
        HBITMAP oldBmp = (HBITMAP)SelectObject(memDC, battle.background);

        BITMAP bm;
        GetObject(battle.background, sizeof(bm), &bm);

        StretchBlt(hdc, 0, 0, g_window.width, g_window.height,
            memDC, 0, 0, bm.bmWidth, bm.bmHeight, SRCCOPY);

        SelectObject(memDC, oldBmp);
        DeleteDC(memDC);
    }
    else {
        HBRUSH bgBrush = CreateSolidBrush(RGB(20, 10, 10));
        RECT rect = { 0, 0, g_window.width, g_window.height };
        FillRect(hdc, &rect, bgBrush);
        DeleteObject(bgBrush);
    }

    // === Отрисовка участников боя ===
    // Игрок (слева)
    if (battle.player.battleSprite) {
        HDC memDC = CreateCompatibleDC(hdc);
        HBITMAP oldBmp = (HBITMAP)SelectObject(memDC, battle.player.battleSprite);

        BITMAP bm;
        GetObject(battle.player.battleSprite, sizeof(bm), &bm);

        TransparentBlt(hdc,
            (int)battle.player.x - (int)battle.player.width / 2,
            (int)battle.player.y - (int)battle.player.height / 2,
            (int)battle.player.width, (int)battle.player.height,
            memDC, 0, 0, bm.bmWidth, bm.bmHeight, TRANSPARENT_COLOR);

        SelectObject(memDC, oldBmp);
        DeleteDC(memDC);
    }

    // Враг (справа)
    if (battle.enemy.battleSprite) {
        HDC memDC = CreateCompatibleDC(hdc);
        HBITMAP oldBmp = (HBITMAP)SelectObject(memDC, battle.enemy.battleSprite);

        BITMAP bm;
        GetObject(battle.enemy.battleSprite, sizeof(bm), &bm);

        TransparentBlt(hdc,
            (int)battle.enemy.x - (int)battle.enemy.width / 2,
            (int)battle.enemy.y - (int)battle.enemy.height / 2,
            (int)battle.enemy.width, (int)battle.enemy.height,
            memDC, 0, 0, bm.bmWidth, bm.bmHeight, TRANSPARENT_COLOR);

        SelectObject(memDC, oldBmp);
        DeleteDC(memDC);
    }

    // === Полоски здоровья ===
    // Полоска здоровья игрока
    int playerHealthX = 100;
    int playerHealthY = 100;

    HBRUSH bgHealth = CreateSolidBrush(RGB(50, 20, 20));
    RECT playerHealthBg = {
        playerHealthX, playerHealthY,
        playerHealthX + (int)battle.player.healthBarWidth,
        playerHealthY + (int)battle.player.healthBarHeight
    };
    FillRect(hdc, &playerHealthBg, bgHealth);

    float healthPercent = battle.player.GetHealthPercentage();
    int healthWidth = (int)(battle.player.healthBarWidth * healthPercent);

    COLORREF healthColor;
    if (healthPercent > 0.5) {
        healthColor = RGB(0, 200, 0);
    }
    else if (healthPercent > 0.25) {
        healthColor = RGB(200, 200, 0);
    }
    else {
        healthColor = RGB(200, 0, 0);
    }

    HBRUSH healthBrush = CreateSolidBrush(healthColor);
    RECT playerHealthRect = {
        playerHealthX, playerHealthY,
        playerHealthX + healthWidth,
        playerHealthY + (int)battle.player.healthBarHeight
    };
    FillRect(hdc, &playerHealthRect, healthBrush);

    HPEN healthPen = CreatePen(PS_SOLID, 2, RGB(255, 255, 255));
    HPEN oldPen = (HPEN)SelectObject(hdc, healthPen);
    HBRUSH oldBrush = (HBRUSH)SelectObject(hdc, GetStockObject(NULL_BRUSH));

    Rectangle(hdc, playerHealthX - 1, playerHealthY - 1,
        playerHealthX + (int)battle.player.healthBarWidth + 1,
        playerHealthY + (int)battle.player.healthBarHeight + 1);

    // Имя и цифры здоровья игрока
    SetBkMode(hdc, TRANSPARENT);
    HFONT font = CreateFont(20, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        DEFAULT_QUALITY, DEFAULT_PITCH, L"Arial");
    HFONT oldFont = (HFONT)SelectObject(hdc, font);

    std::wstring playerHealthText = battle.player.name + L"  " +
        std::to_wstring(battle.player.currentHealth) +
        L"/" + std::to_wstring(battle.player.maxHealth);

    SetTextColor(hdc, RGB(255, 255, 255));
    TextOut(hdc, playerHealthX, playerHealthY - 25,
        playerHealthText.c_str(), (int)playerHealthText.length());

    // Полоска здоровья врага (справа)
    int enemyHealthX = g_window.width - 100 - (int)battle.enemy.healthBarWidth;
    int enemyHealthY = 100;

    RECT enemyHealthBg = {
        enemyHealthX, enemyHealthY,
        enemyHealthX + (int)battle.enemy.healthBarWidth,
        enemyHealthY + (int)battle.enemy.healthBarHeight
    };
    FillRect(hdc, &enemyHealthBg, bgHealth);

    healthPercent = battle.enemy.GetHealthPercentage();
    healthWidth = (int)(battle.enemy.healthBarWidth * healthPercent);

    if (healthPercent > 0.5) {
        healthColor = RGB(200, 0, 0);
    }
    else if (healthPercent > 0.25) {
        healthColor = RGB(200, 100, 0);
    }
    else {
        healthColor = RGB(150, 50, 50);
    }

    DeleteObject(healthBrush);
    healthBrush = CreateSolidBrush(healthColor);

    RECT enemyHealthRect = {
        enemyHealthX, enemyHealthY,
        enemyHealthX + healthWidth,
        enemyHealthY + (int)battle.enemy.healthBarHeight
    };
    FillRect(hdc, &enemyHealthRect, healthBrush);

    Rectangle(hdc, enemyHealthX - 1, enemyHealthY - 1,
        enemyHealthX + (int)battle.enemy.healthBarWidth + 1,
        enemyHealthY + (int)battle.enemy.healthBarHeight + 1);

    std::wstring enemyHealthText = battle.enemy.name + L"  " +
        std::to_wstring(battle.enemy.currentHealth) +
        L"/" + std::to_wstring(battle.enemy.maxHealth);

    TextOut(hdc, enemyHealthX, enemyHealthY - 25,
        enemyHealthText.c_str(), (int)enemyHealthText.length());

    // === Интерфейс действий (только во время хода игрока) ===
    if (battle.state == BATTLE_PLAYER_TURN) {
        int actionsX = g_window.width / 2 - 200;
        int actionsY = g_window.height - 200;

        // Фон меню действий
        HBRUSH menuBg = CreateSolidBrush(RGB(40, 30, 40));
        RECT menuRect = { actionsX - 20, actionsY - 20,
                        actionsX + 400, actionsY + 160 };
        DrawRoundedRect(hdc, menuRect.left, menuRect.top,
            menuRect.right - menuRect.left,
            menuRect.bottom - menuRect.top,
            RGB(40, 30, 40), RGB(100, 80, 100), 10);

        // Заголовок
        HFONT titleFont = CreateFont(24, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
            DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
            DEFAULT_QUALITY, DEFAULT_PITCH, L"Arial");
        SelectObject(hdc, titleFont);

        SetTextColor(hdc, RGB(220, 180, 100));
        std::wstring turnText = L"ВАШ ХОД";
        RECT turnRect = { actionsX, actionsY - 50, actionsX + 400, actionsY - 10 };
        DrawText(hdc, turnText.c_str(), -1, &turnRect, DT_CENTER | DT_VCENTER);

        // Действия
        HFONT actionFont = CreateFont(22, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
            DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
            DEFAULT_QUALITY, DEFAULT_PITCH, L"Segoe UI");
        SelectObject(hdc, actionFont);

        for (int i = 0; i < battle.actions.size(); i++) {
            int actionY = actionsY + i * 40;

            if (i == battle.selectedAction) {
                HBRUSH selectedBrush = CreateSolidBrush(RGB(80, 60, 80));
                RECT selectedRect = { actionsX - 10, actionY - 5,
                                    actionsX + 380, actionY + 35 };
                FillRect(hdc, &selectedRect, selectedBrush);
                DeleteObject(selectedBrush);

                HPEN selectedPen = CreatePen(PS_SOLID, 2, RGB(220, 180, 100));
                HPEN oldActionPen = (HPEN)SelectObject(hdc, selectedPen);
                HBRUSH oldActionBrush = (HBRUSH)SelectObject(hdc, GetStockObject(NULL_BRUSH));

                Rectangle(hdc, selectedRect.left, selectedRect.top,
                    selectedRect.right, selectedRect.bottom);

                SelectObject(hdc, oldActionPen);
                SelectObject(hdc, oldActionBrush);
                DeleteObject(selectedPen);

                SetTextColor(hdc, RGB(255, 255, 200));
            }
            else {
                SetTextColor(hdc, RGB(200, 200, 200));
            }

            RECT actionRect = { actionsX, actionY, actionsX + 400, actionY + 40 };
            DrawText(hdc, battle.actions[i].c_str(), -1, &actionRect,
                DT_LEFT | DT_VCENTER | DT_SINGLELINE);
        }

        // Подсказка управления
        SetTextColor(hdc, RGB(150, 150, 150));
        std::wstring controls = L"↑↓ - Выбор | SPACE - Подтвердить";
        RECT controlsRect = { actionsX, actionsY + 170, actionsX + 400, actionsY + 200 };
        DrawText(hdc, controls.c_str(), -1, &controlsRect, DT_CENTER | DT_VCENTER);

        DeleteObject(titleFont);
        DeleteObject(actionFont);
    }

    // === Сообщение в центре экрана ===
    if (battle.showMessage) {
        HFONT messageFont = CreateFont(36, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
            DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
            DEFAULT_QUALITY, DEFAULT_PITCH, L"Arial");
        SelectObject(hdc, messageFont);

        SetBkMode(hdc, TRANSPARENT);
        SetTextColor(hdc, RGB(255, 255, 200));

        BLENDFUNCTION blend = { 0 };
        blend.BlendOp = AC_SRC_OVER;
        blend.SourceConstantAlpha = 200;
        blend.AlphaFormat = 0;

        SIZE textSize;
        GetTextExtentPoint32(hdc, battle.centerMessage.c_str(),
            (int)battle.centerMessage.length(), &textSize);

        int msgX = g_window.width / 2 - textSize.cx / 2 - 20;
        int msgY = g_window.height / 2 - 50;
        int msgWidth = textSize.cx + 40;
        int msgHeight = 80;

        HDC memDC = CreateCompatibleDC(hdc);
        HBITMAP msgBmp = CreateCompatibleBitmap(hdc, msgWidth, msgHeight);
        HBITMAP oldMsgBmp = (HBITMAP)SelectObject(memDC, msgBmp);

        HBRUSH msgBg = CreateSolidBrush(RGB(0, 0, 0));
        RECT msgRect = { 0, 0, msgWidth, msgHeight };
        FillRect(memDC, &msgRect, msgBg);
        DeleteObject(msgBg);

        SetTextColor(memDC, RGB(255, 255, 200));
        SetBkMode(memDC, TRANSPARENT);

        RECT textRect = { 0, 0, msgWidth, msgHeight };
        DrawText(memDC, battle.centerMessage.c_str(), -1, &textRect,
            DT_CENTER | DT_VCENTER | DT_SINGLELINE);

        AlphaBlend(hdc, msgX, msgY, msgWidth, msgHeight,
            memDC, 0, 0, msgWidth, msgHeight, blend);

        SelectObject(memDC, oldMsgBmp);
        DeleteObject(msgBmp);
        DeleteDC(memDC);
        DeleteObject(messageFont);
    }

    // === Текст урона ===
    if (battle.showDamageText) {
        HFONT damageFont = CreateFont(28, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
            DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
            DEFAULT_QUALITY, DEFAULT_PITCH, L"Arial");
        SelectObject(hdc, damageFont);

        SetBkMode(hdc, TRANSPARENT);

        if (battle.damageValue > 0 && battle.damageText != L"❤") {
            SetTextColor(hdc, RGB(255, 50, 50));
        }
        else {
            SetTextColor(hdc, RGB(50, 255, 50));
        }

        float timePassed = (GetTickCount() - battle.damageTextStartTime) / 1000.0f;
        int offsetY = (int)(-50 * timePassed);

        RECT damageRect = {
            (int)battle.damageTextX - 100,
            (int)battle.damageTextY + offsetY - 50,
            (int)battle.damageTextX + 100,
            (int)battle.damageTextY + offsetY + 50
        };

        DrawText(hdc, battle.damageText.c_str(), -1, &damageRect,
            DT_CENTER | DT_VCENTER | DT_SINGLELINE);

        DeleteObject(damageFont);
    }

    // === Статус защиты ===
    if (battle.player.isDefending) {
        SetTextColor(hdc, RGB(100, 150, 255));
        std::wstring defendText = L"🛡";
        TextOut(hdc, (int)battle.player.x - 15, (int)battle.player.y - 150,
            defendText.c_str(), (int)defendText.length());
    }

    if (battle.enemy.isDefending) {
        SetTextColor(hdc, RGB(255, 150, 100));
        std::wstring defendText = L"🛡";
        TextOut(hdc, (int)battle.enemy.x - 15, (int)battle.enemy.y - 150,
            defendText.c_str(), (int)defendText.length());
    }

    // Восстанавливаем GDI объекты
    SelectObject(hdc, oldFont);
    SelectObject(hdc, oldPen);
    SelectObject(hdc, oldBrush);

    DeleteObject(bgHealth);
    DeleteObject(healthBrush);
    DeleteObject(healthPen);
    DeleteObject(font);
}

// ==================== ГЛАВНЫЕ ФУНКЦИИ ====================

void ProcessInput()
{
    // Если в бою - обрабатываем только управление боем
    if (g_gameState.inBattle) {
        BattleScene& battle = g_gameState.battle;

        if (battle.state == BATTLE_PLAYER_TURN) {
            static bool keyDown = false;

            if (GetAsyncKeyState(VK_UP) & 0x8000) {
                if (!keyDown) {
                    battle.selectedAction--;
                    if (battle.selectedAction < 0)
                        battle.selectedAction = battle.actions.size() - 1;
                    keyDown = true;
                    InvalidateRect(g_window.hWnd, nullptr, FALSE);
                }
            }
            else if (GetAsyncKeyState(VK_DOWN) & 0x8000) {
                if (!keyDown) {
                    battle.selectedAction = (battle.selectedAction + 1) % battle.actions.size();
                    keyDown = true;
                    InvalidateRect(g_window.hWnd, nullptr, FALSE);
                }
            }
            else if (GetAsyncKeyState(VK_SPACE) & 0x8000) {
                if (!keyDown) {
                    ProcessPlayerTurn(battle.selectedAction);
                    keyDown = true;
                    InvalidateRect(g_window.hWnd, nullptr, FALSE);
                }
            }
            else {
                keyDown = false;
            }
        }
        return;
    }

    // Обычное управление
    Enemy& enemy = g_gameState.enemy;
    Player& player = g_gameState.player;
    DWORD currentTime = GetTickCount();
    bool moved = false;

    if (GetAsyncKeyState('W') & 0x8000) {
        player.y -= player.speed;
        moved = true;
    }
    if (GetAsyncKeyState('S') & 0x8000) {
        player.y += player.speed;
        moved = true;
    }
    if (GetAsyncKeyState('A') & 0x8000) {
        player.x -= player.speed;
        player.facingRight = false;
        moved = true;
    }
    if (GetAsyncKeyState('D') & 0x8000) {
        player.x += player.speed;
        player.facingRight = true;
        moved = true;
    }

    // Клавиша Q для показа врага
    if (GetAsyncKeyState('Q') & 0x8000) {
        enemy.isVisible = true;
    }
    else {
        enemy.isVisible = false;
    }

    player.isMoving = moved;

    if (moved) {
        player.idleTimer = currentTime;
        player.isIdle = false;
    }

    if ((GetAsyncKeyState(VK_SHIFT) & 0x8000) && player.runAnimation.loaded) {
        if (!player.isRunningBoost) {
            player.isRunningBoost = true;
            player.boostStartTime = currentTime;
        }

        player.speed = 12;
        player.isRunning = true;
        player.lastRunTime = currentTime;
        player.idleTimer = currentTime;
        player.isIdle = false;
    }
    else if (player.isRunningBoost) {
        player.isRunningBoost = false;
        player.speed = 5;
        player.isRunning = false;
    }

    if (player.isRunningBoost && (currentTime - player.boostStartTime >= 5000)) {
        player.isRunningBoost = false;
        player.speed = 5;
        player.isRunning = false;
    }
}

void UpdatePlayer()
{
    LimitPlayerOnGround();

    // Проверка столкновения с врагом
    if (!g_gameState.inBattle) {
        Player& player = g_gameState.player;
        Enemy& enemy = g_gameState.enemy;

        float playerLeft = player.x - player.width / 2 + 50;
        float playerRight = player.x + player.width / 2 - 50;
        float playerTop = player.y - player.height / 2 + 100;
        float playerBottom = player.y + player.height / 2 - 50;

        float enemyLeft = enemy.x - enemy.width / 2 + 50;
        float enemyRight = enemy.x + enemy.width / 2 - 50;
        float enemyTop = enemy.y - enemy.height / 2 + 100;
        float enemyBottom = enemy.y + enemy.height / 2 - 50;

        if (playerLeft < enemyRight &&
            playerRight > enemyLeft &&
            playerTop < enemyBottom &&
            playerBottom > enemyTop) {
            StartBattle();
            return; // Прерываем обновление, так как начался бой
        }
    }

    // Обновление анимации
    if (g_gameState.player.isRunning) {
        g_gameState.player.runAnimation.frameDelay = 80;
    }
    else if (g_gameState.player.isMoving) {
        g_gameState.player.walkAnimation.frameDelay = 120;
    }
    else {
        g_gameState.player.idleAnimation.frameDelay = 200;
    }

    g_gameState.player.UpdateAnimation(GetTickCount());

    // Обновляем диалог если он активен
    if (IsDialogActive()) {
        UpdateDialog();
    }
}

void UpdateGameLogic()
{
    if (g_gameState.inBattle) {
        UpdateBattle();
    }
    else {
        UpdatePlayer();
        UpdateCamera();
    }
}

void LimitPlayerOnGround()
{
    // Ограничения пока отключены
}

void UpdateCamera()
{
    float targetX = g_gameState.player.x - g_window.width / 2;
    float targetY = g_gameState.player.y - g_window.height / 2;

    g_gameState.cameraX += (targetX - g_gameState.cameraX) * 0.1f;
    g_gameState.cameraY += (targetY - g_gameState.cameraY) * 0.1f;

    if (g_gameState.cameraX < 0)
        g_gameState.cameraX = 0;

    if (g_gameState.cameraX > g_gameState.levelWidth - g_window.width)
        g_gameState.cameraX = g_gameState.levelWidth - g_window.width;

    if (g_gameState.cameraY < 0)
        g_gameState.cameraY = 0;

    if (g_gameState.cameraY > g_gameState.levelHeight - g_window.height) {
        g_gameState.cameraY = g_gameState.levelHeight - g_window.height;
    }
}

void RenderGame(HDC hdc)
{
    if (!g_window.hBufferDC || g_window.bufferWidth != g_window.width || g_window.bufferHeight != g_window.height)
    {
        InitBuffer(hdc);
    }

    HDC bufferDC = g_window.hBufferDC;

    // Если в бою - рисуем бой
    if (g_gameState.inBattle) {
        RenderBattle(bufferDC);
        BitBlt(hdc, 0, 0, g_window.width, g_window.height, bufferDC, 0, 0, SRCCOPY);
        return;
    }

    // Обычная отрисовка игры
    HBRUSH background = CreateSolidBrush(RGB(30, 30, 40));
    RECT fullRect = { 0, 0, g_window.width, g_window.height };
    FillRect(bufferDC, &fullRect, background);
    DeleteObject(background);

    if (g_gameState.hLevelBackground)
    {
        HDC memDC = CreateCompatibleDC(bufferDC);
        HBITMAP oldBmp = (HBITMAP)SelectObject(memDC, g_gameState.hLevelBackground);

        BITMAP bm;
        GetObject(g_gameState.hLevelBackground, sizeof(bm), &bm);

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
        HBRUSH skyBrush = CreateSolidBrush(RGB(135, 206, 235));
        RECT skyRect = { 0, 0, g_window.width, g_window.height - 150 };
        FillRect(bufferDC, &skyRect, skyBrush);
        DeleteObject(skyBrush);

        HBRUSH floorBrush = CreateSolidBrush(RGB(100, 70, 50));
        RECT floorRect = { 0, g_window.height - 150, g_window.width, g_window.height };
        FillRect(bufferDC, &floorRect, floorBrush);
        DeleteObject(floorBrush);
    }

    // Рисуем врага ПОД игроком
    RenderEnemy(bufferDC);
    // Рисуем игрока ПОВЕРХ врага
    RenderPlayer(bufferDC);

    // Рисуем диалог поверх всего
    if (IsDialogActive()) {
        RenderDialog(bufferDC);
    }

    SetBkMode(bufferDC, TRANSPARENT);

    LOGFONT lf = {};
    lf.lfHeight = 20;
    lf.lfWeight = FW_BOLD;
    wcscpy_s(lf.lfFaceName, L"Arial");

    HFONT font = CreateFontIndirect(&lf);
    HFONT oldFont = (HFONT)SelectObject(bufferDC, font);

    SetTextColor(bufferDC, RGB(255, 255, 255));

    std::wstring posText = L"X: " + std::to_wstring((int)g_gameState.player.x) +
        L" Y: " + std::to_wstring((int)g_gameState.player.y);
    RECT posRect = { 20, 20, 300, 50 };
    DrawText(bufferDC, posText.c_str(), -1, &posRect, DT_LEFT | DT_SINGLELINE);

    std::wstring animText;
    if (g_gameState.player.isRunning && g_gameState.player.runAnimation.loaded)
    {
        animText = L"БЕГ: " + std::to_wstring(g_gameState.player.runAnimation.currentFrame + 1) +
            L"/" + std::to_wstring(g_gameState.player.runAnimation.frames.size());
    }
    else if (g_gameState.player.isMoving && g_gameState.player.walkAnimation.loaded)
    {
        animText = L"ХОДЬБА: " + std::to_wstring(g_gameState.player.walkAnimation.currentFrame + 1) +
            L"/" + std::to_wstring(g_gameState.player.walkAnimation.frames.size());
    }
    else if (g_gameState.player.idleAnimation.loaded)
    {
        animText = L"IDLE: " + std::to_wstring(g_gameState.player.idleAnimation.currentFrame + 1) +
            L"/" + std::to_wstring(g_gameState.player.idleAnimation.frames.size());
    }
    else
    {
        animText = L"Анимация не загружена";
    }

    if (g_gameState.player.isIdle) {
        animText += L" [IDLE]";
    }

    RECT animRect = { 20, 50, 400, 80 };
    DrawText(bufferDC, animText.c_str(), -1, &animRect, DT_LEFT | DT_SINGLELINE);

    std::wstring controls = L"WASD - Движение | SHIFT - Бег | Q - Показать врага | ESC - Меню | SPACE - Диалог";
    RECT controlsRect = { g_window.width / 2 - 250, 20, g_window.width / 2 + 250, 50 };
    DrawText(bufferDC, controls.c_str(), -1, &controlsRect, DT_CENTER | DT_SINGLELINE);

    // Добавляем информацию о враге
    if (g_gameState.enemy.isVisible) {
        std::wstring enemyInfo = L"ВРАГ ОБНАРУЖЕН! Позиция: X=" +
            std::to_wstring((int)g_gameState.enemy.x) +
            L" Y=" + std::to_wstring((int)g_gameState.enemy.y);

        SetTextColor(bufferDC, RGB(255, 100, 100));
        RECT enemyRect = { 20, g_window.height - 50, 600, g_window.height - 20 };
        DrawText(bufferDC, enemyInfo.c_str(), -1, &enemyRect, DT_LEFT | DT_SINGLELINE);
        SetTextColor(bufferDC, RGB(255, 255, 255));
    }

    std::wstring speedText = L"Скорость: " + std::to_wstring((int)g_gameState.player.speed);
    RECT speedRect = { 20, 80, 300, 110 };
    DrawText(bufferDC, speedText.c_str(), -1, &speedRect, DT_LEFT | DT_SINGLELINE);

    SelectObject(bufferDC, oldFont);
    DeleteObject(font);

    BitBlt(hdc, 0, 0, g_window.width, g_window.height, bufferDC, 0, 0, SRCCOPY);
}

void RenderPlayer(HDC hdc)
{
    HBITMAP hSprite = g_gameState.player.GetCurrentSprite();

    if (!hSprite)
    {
        hSprite = g_gameState.player.isRunning ?
            g_gameState.player.hSpriteRunRight :
            g_gameState.player.hSpriteRight;
    }

    if (!hSprite)
    {
        HBRUSH playerBrush = CreateSolidBrush(RGB(0, 150, 255));

        int screenX = (int)g_gameState.player.x - (int)g_gameState.cameraX - (int)g_gameState.player.width / 2;
        int screenY = (int)g_gameState.player.y - (int)g_gameState.cameraY - (int)g_gameState.player.height / 2;

        RECT playerRect = {
            screenX,
            screenY,
            screenX + (int)g_gameState.player.width,
            screenY + (int)g_gameState.player.height
        };

        FillRect(hdc, &playerRect, playerBrush);

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
        HDC memDC = CreateCompatibleDC(hdc);
        HBITMAP oldBmp = (HBITMAP)SelectObject(memDC, hSprite);

        BITMAP bm;
        GetObject(hSprite, sizeof(bm), &bm);

        int screenX = (int)g_gameState.player.x - (int)g_gameState.cameraX - (int)g_gameState.player.width / 2;
        int screenY = (int)g_gameState.player.y - (int)g_gameState.cameraY - (int)g_gameState.player.height / 2;

        if (g_gameState.player.facingRight)
        {
            TransparentBlt(hdc, screenX, screenY,
                (int)g_gameState.player.width, (int)g_gameState.player.height,
                memDC, 0, 0, bm.bmWidth, bm.bmHeight, TRANSPARENT_COLOR);
        }
        else
        {
            DrawMirroredBitmap(hdc,
                screenX, screenY,
                (int)g_gameState.player.width, (int)g_gameState.player.height,
                memDC, hSprite, bm.bmWidth, bm.bmHeight);
        }

        SelectObject(memDC, oldBmp);
        DeleteDC(memDC);
    }
}

// ==================== ОСТАЛЬНЫЕ ФУНКЦИИ ====================

int WINAPI WinMain(_In_ HINSTANCE hInstance, _In_opt_ HINSTANCE hPrevInstance,
    _In_ LPSTR lpCmdLine, _In_ int nCmdShow)
{
    INITCOMMONCONTROLSEX icc = { sizeof(INITCOMMONCONTROLSEX), ICC_STANDARD_CLASSES };
    InitCommonControlsEx(&icc);

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

    RECT clientRect;
    GetClientRect(g_window.hWnd, &clientRect);
    g_window.width = clientRect.right - clientRect.left;
    g_window.height = clientRect.bottom - clientRect.top;

    CreateMainMenuButtons();

    ShowWindow(g_window.hWnd, nCmdShow);
    UpdateWindow(g_window.hWnd);

    SetTimer(g_window.hWnd, 1, 16, NULL);

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
            if (g_gameState.inGame)
            {
                ProcessInput();
                UpdateGameLogic();
                InvalidateRect(g_window.hWnd, nullptr, FALSE);
            }
            Sleep(1);
        }
    }

    Cleanup();
    return 0;
}

void CreateMainMenuButtons()
{
    HINSTANCE hInstance = (HINSTANCE)GetWindowLongPtr(g_window.hWnd, GWLP_HINSTANCE);

    DWORD buttonStyle = WS_TABSTOP | WS_VISIBLE | WS_CHILD | BS_PUSHBUTTON | BS_FLAT;

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

    CustomizeButton(g_window.hStartButton);
    CustomizeButton(g_window.hExitButton);
}

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

void ShowMainMenuButtons(bool show)
{
    if (g_window.hStartButton && IsWindow(g_window.hStartButton))
        ShowWindow(g_window.hStartButton, show ? SW_SHOW : SW_HIDE);

    if (g_window.hExitButton && IsWindow(g_window.hExitButton))
        ShowWindow(g_window.hExitButton, show ? SW_SHOW : SW_HIDE);
}

HBITMAP LoadBmpFromDebug(const char* filename)
{
    char exePath[MAX_PATH];
    GetModuleFileNameA(nullptr, exePath, MAX_PATH);

    char debugMsg[512];
    sprintf_s(debugMsg, "=== Поиск файла: %s ===\n", filename);
    OutputDebugStringA(debugMsg);

    std::string exeDir = exePath;
    size_t pos = exeDir.find_last_of("\\/");
    if (pos != std::string::npos)
    {
        exeDir = exeDir.substr(0, pos + 1);
    }

    sprintf_s(debugMsg, "Папка EXE: %s\n", exeDir.c_str());
    OutputDebugStringA(debugMsg);

    std::string paths[] = {
        exeDir + filename,
        exeDir + "Debug\\" + filename,
        exeDir + "x64\\Debug\\" + filename,
        exeDir + "..\\" + filename,
        filename
    };

    for (const auto& fullPath : paths)
    {
        sprintf_s(debugMsg, "Пробуем: %s\n", fullPath.c_str());
        OutputDebugStringA(debugMsg);

        HBITMAP hBitmap = (HBITMAP)LoadImageA(
            nullptr,
            fullPath.c_str(),
            IMAGE_BITMAP,
            0, 0,
            LR_LOADFROMFILE | LR_CREATEDIBSECTION
        );

        if (hBitmap)
        {
            sprintf_s(debugMsg, "✓ УСПЕХ! Загружен: %s\n", fullPath.c_str());
            OutputDebugStringA(debugMsg);
            return hBitmap;
        }
        else
        {
            DWORD err = GetLastError();
            sprintf_s(debugMsg, "✗ Ошибка: %lu\n", err);
            OutputDebugStringA(debugMsg);
        }
    }

    sprintf_s(debugMsg, "✗ Файл не найден ни в одном из мест: %s\n", filename);
    OutputDebugStringA(debugMsg);
    return nullptr;
}

void RenderMainMenu(HDC hdc)
{
    if (!g_window.hBufferDC || g_window.bufferWidth != g_window.width || g_window.bufferHeight != g_window.height)
    {
        InitBuffer(hdc);
    }

    HDC bufferDC = g_window.hBufferDC;

    HBRUSH background = CreateSolidBrush(MENU_BG_COLOR);
    RECT fullRect = { 0, 0, g_window.width, g_window.height };
    FillRect(bufferDC, &fullRect, background);
    DeleteObject(background);

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

    lf.lfHeight = 20;
    HFONT infoFont = CreateFontIndirect(&lf);
    SelectObject(bufferDC, infoFont);

    SetTextColor(bufferDC, RGB(150, 150, 180));

    std::wstring info = L"Нажмите кнопку 'НАЧАТЬ ИГРУ' чтобы начать";
    RECT infoRect = { 0, g_window.height - 100, g_window.width, g_window.height - 50 };
    DrawText(bufferDC, info.c_str(), -1, &infoRect, DT_CENTER | DT_SINGLELINE);

    SelectObject(bufferDC, oldFont);
    DeleteObject(titleFont);
    DeleteObject(subtitleFont);
    DeleteObject(infoFont);

    BitBlt(hdc, 0, 0, g_window.width, g_window.height, bufferDC, 0, 0, SRCCOPY);
}

void LoadWalkAnimation(Player& player, const std::vector<std::string>& rightFiles, DWORD frameDelay)
{
    player.walkAnimation.Clear();
    player.walkAnimation.frameDelay = frameDelay;

    for (const auto& filename : rightFiles) {
        HBITMAP frame = LoadBmpFromDebug(filename.c_str());
        if (frame) {
            player.walkAnimation.frames.push_back(frame);
        }
    }

    player.walkAnimation.loaded = !player.walkAnimation.frames.empty();

    if (!player.walkAnimation.frames.empty()) {
        player.hSpriteRight = player.walkAnimation.frames[0];
    }
}

void LoadRunAnimation(Player& player, const std::vector<std::string>& rightFiles, DWORD frameDelay)
{
    player.runAnimation.Clear();
    player.runAnimation.frameDelay = frameDelay;

    for (const auto& filename : rightFiles) {
        HBITMAP frame = LoadBmpFromDebug(filename.c_str());
        if (frame) {
            player.runAnimation.frames.push_back(frame);
        }
    }

    player.runAnimation.loaded = !player.runAnimation.frames.empty();

    if (!player.runAnimation.frames.empty()) {
        player.hSpriteRunRight = player.runAnimation.frames[0];
    }
}

void LoadIdleAnimation(Player& player, const std::vector<std::string>& rightFiles, DWORD frameDelay)
{
    player.idleAnimation.Clear();
    player.idleAnimation.frameDelay = frameDelay;

    for (const auto& filename : rightFiles) {
        HBITMAP frame = LoadBmpFromDebug(filename.c_str());
        if (frame) {
            player.idleAnimation.frames.push_back(frame);
        }
    }

    player.idleAnimation.loaded = !player.idleAnimation.frames.empty();

    if (!player.idleAnimation.frames.empty()) {
        player.hSpriteRight = player.idleAnimation.frames[0];
    }
}

void OnStartGame()
{
    g_gameState.inMainMenu = false;
    g_gameState.inGame = true;
    g_gameState.currentLevel = 1;

    ShowMainMenuButtons(false);
    InitLevel1();
    InvalidateRect(g_window.hWnd, nullptr, TRUE);
    PlayBackgroundMusic("x64\\Debug\\bazar.wav");
}

void OnExitGame()
{
    g_window.should_exit = true;
    PostQuitMessage(0);
}

void InitLevel1()
{
    OutputDebugStringA("=== ИНИЦИАЛИЗАЦИЯ УРОВНЯ 1 ===\n");

    // Загружаем врага
    LoadEnemy(g_gameState.enemy);

    // Загружаем ресурсы боя
    LoadBattleResources();

    // Загрузка фона
    g_gameState.hLevelBackground = LoadBmpFromDebug("level1.bmp");
    if (g_gameState.hLevelBackground) {
        OutputDebugStringA("Фон уровня загружен успешно\n");
    }
    else {
        OutputDebugStringA("Не удалось загрузить фон уровня\n");
    }

    // Загрузка анимаций
    std::vector<std::string> idleFrames = {
        "player_idle_1.bmp",
        "player_idle_2.bmp",
        "player_idle_3.bmp",
        "player_idle_4.bmp",
        "player_idle_5.bmp",
        "player_idle_6.bmp"
    };
    LoadIdleAnimation(g_gameState.player, idleFrames, 200);
    OutputDebugStringA("Idle анимация загружена\n");

    // ЗАГРУЗКА ПОРТРЕТА ИГРОКА
    OutputDebugStringA("Пробуем загрузить player_portrait_smoking.bmp...\n");
    g_gameState.playerPortrait = LoadBmpFromDebug("player_portrait_smoking.bmp");

    if (g_gameState.playerPortrait) {
        OutputDebugStringA("Портрет игрока загружен успешно!\n");

        BITMAP bm;
        if (GetObject(g_gameState.playerPortrait, sizeof(BITMAP), &bm)) {
            char debugMsg[512];
            sprintf_s(debugMsg, "Размер портрета: %dx%d, бит на пиксель: %d\n",
                bm.bmWidth, bm.bmHeight, bm.bmBitsPixel);
            OutputDebugStringA(debugMsg);
        }
    }
    else {
        OutputDebugStringA("Не удалось загрузить портрет игрока. Создаем временный...\n");

        g_gameState.playerPortrait = CreatePortrait(180, 200,
            RGB(60, 40, 20),
            RGB(255, 220, 180),
            RGB(0, 150, 200),
            L"neutral");

        if (g_gameState.playerPortrait) {
            OutputDebugStringA("Временный портрет создан успешно\n");
        }
        else {
            OutputDebugStringA("Не удалось создать временный портрет!\n");
        }
    }

    // Продолжение загрузки...
    std::vector<std::string> walkFrames = {
        "player_walking_1.bmp",
        "player_walking_2.bmp",
        "player_walking_3.bmp",
        "player_walking_4.bmp",
        "player_walking_5.bmp",
        "player_walking_6.bmp",
        "player_walking_7.bmp",
        "player_walking_8.bmp"
    };
    LoadWalkAnimation(g_gameState.player, walkFrames, 120);
    OutputDebugStringA("Walk анимация загружена\n");

    std::vector<std::string> runFrames = {
        "player_running_1.bmp",
        "player_running_2.bmp",
        "player_running_3.bmp",
        "player_running_4.bmp",
        "player_running_5.bmp",
        "player_running_6.bmp",
        "player_running_7.bmp",
        "player_running_8.bmp"
    };
    LoadRunAnimation(g_gameState.player, runFrames, 80);
    OutputDebugStringA("Run анимация загружена\n");

    // Инициализация игрока
    g_gameState.player.x = 500;
    g_gameState.player.y = g_gameState.levelHeight - 300;
    g_gameState.player.facingRight = true;
    g_gameState.player.isIdle = true;

    // Инициализация камеры
    g_gameState.cameraX = 0;
    g_gameState.cameraY = 0;

    // Таймеры анимации
    DWORD currentTime = GetTickCount();
    g_gameState.player.idleAnimation.lastUpdateTime = currentTime;
    g_gameState.player.walkAnimation.lastUpdateTime = currentTime;
    g_gameState.player.runAnimation.lastUpdateTime = currentTime;
    g_gameState.player.idleTimer = currentTime;

    // Инициализация диалогового окна
    g_gameState.dialog.x = 100;
    g_gameState.dialog.y = g_window.height - 280;
    g_gameState.dialog.width = g_window.width - 200;
    g_gameState.dialog.height = 250;

    // Создаем портрет NPC
    g_gameState.npcPortrait = CreatePortrait(180, 200,
        RGB(180, 180, 180),
        RGB(255, 200, 160),
        RGB(200, 100, 50),
        L"happy");

    OutputDebugStringA("=== ИНИЦИАЛИЗАЦИЯ УРОВНЯ 1 ЗАВЕРШЕНА ===\n");
}

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
            UpdateGameLogic();
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
        else if (wParam == VK_SPACE)  // ДИАЛОГИ
        {
            if (IsDialogActive()) {
                NextDialogLine();
                InvalidateRect(hwnd, nullptr, TRUE);
            }
            else {
                // Тестовый диалог при нажатии Space
                if (g_gameState.inGame && !IsDialogActive()) {
                    std::vector<DialogLine> testDialog;

                    DialogLine line1;
                    line1.speaker = L"АЛЕКС ХЭМПТОН";
                    line1.text = L"Еще одна ночь в объятиях лондонского кошмара. 'Кровавая Кукла'... Пять лет прошло, а твой почерк все так же свеж на стенах моей памяти. (добавить в размышление деталей то, что он видит на фотографии, дополнить контекст происходящего, немного расширить для игрока лорную часть и это добавит эмоциональной глубины) Кто ты? Призрак? Маньяк? Или... нечто большее?";
                    line1.speakerFace = g_gameState.playerPortrait;
                    testDialog.push_back(line1);

                    StartDialogSequence(testDialog);
                    InvalidateRect(hwnd, nullptr, TRUE);
                }
            }
        }
        break;

    case WM_DESTROY:
        KillTimer(hwnd, 1);
        PostQuitMessage(0);
        break;

    case WM_ERASEBKGND:
        return 1;
    }

    return DefWindowProc(hwnd, uMsg, wParam, lParam);
}

void Cleanup()
{
    StopBackgroundMusic();
    CleanupBuffer();

    g_gameState.player.Cleanup();

    // Очищаем ресурсы боя
    if (g_gameState.battle.background) {
        DeleteObject(g_gameState.battle.background);
    }
    if (g_gameState.battle.player.battleSprite &&
        g_gameState.battle.player.battleSprite != g_gameState.player.GetCurrentSprite()) {
        DeleteObject(g_gameState.battle.player.battleSprite);
    }
    if (g_gameState.battle.enemy.battleSprite &&
        g_gameState.battle.enemy.battleSprite != g_gameState.enemy.idleSprite) {
        DeleteObject(g_gameState.battle.enemy.battleSprite);
    }

    // Очищаем ресурсы врага
    if (g_gameState.enemy.idleSprite) {
        DeleteObject(g_gameState.enemy.idleSprite);
    }
    g_gameState.enemy.idleAnimation.Clear();

    // Очищаем диалоговые ресурсы
    if (g_gameState.playerPortrait) {
        DeleteObject(g_gameState.playerPortrait);
    }
    if (g_gameState.npcPortrait) {
        DeleteObject(g_gameState.npcPortrait);
    }

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

    if (g_gameState.hLevelBackground) DeleteObject(g_gameState.hLevelBackground);
    if (g_gameState.player.hSpriteRight) DeleteObject(g_gameState.player.hSpriteRight);
    if (g_gameState.player.hSpriteRunRight &&
        g_gameState.player.hSpriteRunRight != g_gameState.player.hSpriteRight)
    {
        DeleteObject(g_gameState.player.hSpriteRunRight);
    }
}