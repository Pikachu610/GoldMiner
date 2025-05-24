// main.cpp - Gold Miner with welcome -> goal screen -> gameplay + hook logic
#include <SDL3/SDL.h>
#include <SDL3/SDL_image.h>
#include <algorithm>
#include <cmath>
#include <iostream>
#include <vector>
#include <string>

#ifndef M_PI
#define M_PI 3.14159265358979323846 // Hằng số Pi để tính góc quay hook
#endif

const int SCREEN_WIDTH = 600;
const int SCREEN_HEIGHT = 450;

// Cửa sổ và renderer chính
SDL_Window *window = nullptr;
SDL_Renderer *renderer = nullptr;

// Hàm load ảnh thành texture
SDL_Texture *loadTexture(const char *path)
{
  SDL_Surface *surface = IMG_Load(path);
  if (!surface)
  {
    std::cerr << "IMG_Load failed: " << SDL_GetError() << "\n";
    return nullptr;
  }
  SDL_Texture *texture = SDL_CreateTextureFromSurface(renderer, surface);
  SDL_DestroySurface(surface);
  return texture;
}

// Cấu trúc vật thể game
struct GameObject
{
  SDL_Texture *texture = nullptr;
  SDL_FRect rect{};
  bool visible = true;
};

// Trạng thái game
enum class GameState
{
  Welcome,
  Ready,
  Playing,
  LevelComplete,
  GameOver
};

GameState gameState = GameState::Welcome; // Trạng thái khởi đầu

// Các đối tượng game chính
std::vector<GameObject> golds;
std::vector<GameObject> stones; // Danh sách các viên đá
GameObject background, goalImg, playBackground, winImg, loseImg,
    hook; // Loại bỏ levelCompleteImg

// Biến trạng thái móc và game
float angle = 0.0f, angleSpeed = 1.0f;
bool fang = false, shou = false, timeUp = false;
float timeLeft = 60.0f;
int money = 0;
int currentLevel = 1;                    // Thêm biến theo dõi màn chơi hiện tại
int targetMoney = 650;                   // Mục tiêu ban đầu
float levelCompleteTimer = 0.0f;         // Thời gian chờ trước khi chuyển màn
const float LEVEL_COMPLETE_DELAY = 3.0f; // Số giây chờ trước khi chuyển màn

// Biến điều khiển chuyển động của móc
float hookLength = 0.0f;
float hookMaxLength = 350.0f;
float hookSpeed = 4.0f;
GameObject *hookedGold = nullptr; // Cục vàng đang dính móc

// Khai báo digitSegments ở phạm vi toàn cục
bool digitSegments[10][7] = {
    {1, 1, 1, 1, 1, 1, 0}, // 0
    {0, 1, 1, 0, 0, 0, 0}, // 1
    {1, 1, 0, 1, 1, 0, 1}, // 2
    {1, 1, 1, 1, 0, 0, 1}, // 3
    {0, 1, 1, 0, 0, 1, 1}, // 4
    {1, 0, 1, 1, 0, 1, 1}, // 5
    {1, 0, 1, 1, 1, 1, 1}, // 6
    {1, 1, 1, 0, 0, 0, 0}, // 7
    {1, 1, 1, 1, 1, 1, 1}, // 8
    {1, 1, 1, 1, 0, 1, 1}  // 9
};

// Định nghĩa hàm vẽ đoạn số ở phạm vi toàn cục
void drawSegment(SDL_Renderer *renderer, float x1, float y1, float x2, float y2, int thickness = 2)
{
  // Vẽ đoạn thẳng với độ dày tùy chỉnh
  for (int t = -thickness / 2; t <= thickness / 2; t++)
  {
    SDL_RenderLine(renderer, x1 + (y2 > y1 ? t : 0), y1 + (x2 > x1 ? t : 0),
                   x2 + (y2 > y1 ? t : 0), y2 + (x2 > x1 ? t : 0));
  }
}

// Vẽ viền đen mỏng cho chữ số
void outlineDigit(SDL_Renderer *renderer, const bool segments[7], float posX, float posY, float w, float h)
{
  SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255); // Màu đen cho viền

  // Vẽ các segment với viền đen dày hơn
  if (segments[0]) // Trên
    drawSegment(renderer, posX - 1, posY - 1, posX + w + 1, posY - 1, 4);
  if (segments[1]) // Phải trên
    drawSegment(renderer, posX + w + 1, posY - 1, posX + w + 1, posY + h / 2 + 1, 4);
  if (segments[2]) // Phải dưới
    drawSegment(renderer, posX + w + 1, posY + h / 2 - 1, posX + w + 1, posY + h + 1, 4);
  if (segments[3]) // Dưới
    drawSegment(renderer, posX - 1, posY + h + 1, posX + w + 1, posY + h + 1, 4);
  if (segments[4]) // Trái dưới
    drawSegment(renderer, posX - 1, posY + h / 2 - 1, posX - 1, posY + h + 1, 4);
  if (segments[5]) // Trái trên
    drawSegment(renderer, posX - 1, posY - 1, posX - 1, posY + h / 2 + 1, 4);
  if (segments[6]) // Giữa
    drawSegment(renderer, posX - 1, posY + h / 2, posX + w + 1, posY + h / 2, 4);
}

// Kiểm tra chuột có trong vùng hình chữ nhật không
bool pointInRect(const SDL_FPoint *pt, const SDL_FRect *rect)
{
  return pt->x >= rect->x && pt->x <= (rect->x + rect->w) && pt->y >= rect->y &&
         pt->y <= (rect->y + rect->h);
}

// Kiểm tra va chạm giữa 2 vật thể
bool checkCollision(const SDL_FRect &a, const SDL_FRect &b)
{
  return a.x < b.x + b.w && a.x + a.w > b.x && a.y < b.y + b.h &&
         a.y + a.h > b.y;
}

// Khởi tạo SDL và tạo cửa sổ
bool init()
{
  if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO) < 0)
  {
    std::cerr << "SDL_Init failed: " << SDL_GetError() << std::endl;
    return false;
  }

  window = SDL_CreateWindow("Gold Miner", SCREEN_WIDTH, SCREEN_HEIGHT,
                            SDL_WINDOW_RESIZABLE);
  renderer = SDL_CreateRenderer(window, nullptr);
  SDL_SetRenderLogicalPresentation(renderer, SCREEN_WIDTH, SCREEN_HEIGHT,
                                   SDL_LOGICAL_PRESENTATION_LETTERBOX);
  return true;
}

// Load toàn bộ asset hình ảnh vào game
void loadAssets()
{
  background = {loadTexture("image/welcome.png"),
                {0, 0, SCREEN_WIDTH, SCREEN_HEIGHT}};
  goalImg = {loadTexture("image/goal.png"),
             {0, 0, SCREEN_WIDTH, SCREEN_HEIGHT}};
  playBackground = {loadTexture("image/game.png"),
                    {0, 0, SCREEN_WIDTH, SCREEN_HEIGHT}};
  hook = {loadTexture("image/hook.png"), {291, 63, 25, 10}}; // kich thuoc moc
  winImg = {loadTexture("image/win.png"), {0, 0, SCREEN_WIDTH, SCREEN_HEIGHT}};
  loseImg = {loadTexture("image/lose.png"),
             {0, 0, SCREEN_WIDTH, SCREEN_HEIGHT}};

  // Khởi tạo random
  srand((unsigned int)time(0));
  std::vector<SDL_FRect> allRects;

  // Hàm sinh vị trí ngẫu nhiên không trùng
  auto randomRect = [&](float w, float h)
  {
    SDL_FRect rect;
    int tries = 0;
    do
    {
      rect.x = 50 + rand() % (SCREEN_WIDTH - (int)w - 100);
      rect.y = 120 + rand() % (SCREEN_HEIGHT - (int)h - 150);
      rect.w = w;
      rect.h = h;
      tries++;
      if (tries > 100)
        break; // Tránh vòng lặp vô hạn
    } while (
        std::any_of(allRects.begin(), allRects.end(), [&](const SDL_FRect &r)
                    { return checkCollision(rect, r); }));
    allRects.push_back(rect);
    return rect;
  };

  // Thêm nhiều vàng và đá hơn
  golds.clear();
  stones.clear();

  // Thêm 5 cục vàng nhỏ
  for (int i = 0; i < 5; ++i)
  {
    golds.push_back(
        {loadTexture("image/goldsmall.png"), randomRect(19.0f, 18.0f)});
  }

  // Thêm 3 cục vàng vừa
  for (int i = 0; i < 3; ++i)
  {
    golds.push_back(
        {loadTexture("image/goldmid.png"), randomRect(33.0f, 30.0f)});
  }

  // Thêm 2 cục vàng to
  for (int i = 0; i < 2; ++i)
  {
    golds.push_back(
        {loadTexture("image/goldb.png"), randomRect(77.0f, 73.0f)});
  }

  // Thêm 6 viên đá loại A
  for (int i = 0; i < 6; ++i)
  {
    stones.push_back(
        {loadTexture("image/stonea.png"), randomRect(30.0f, 28.0f)});
  }

  // Thêm 4 viên đá loại B
  for (int i = 0; i < 4; ++i)
  {
    stones.push_back(
        {loadTexture("image/stoneb.png"), randomRect(47.0f, 43.0f)});
  }
}

// Cập nhật logic game mỗi frame
void updateGame(float deltaTime)
{
  // Nếu đã hoàn thành màn chơi, đếm ngược thời gian để chuyển màn
  if (gameState == GameState::LevelComplete)
  {
    levelCompleteTimer += deltaTime;
    if (levelCompleteTimer >= LEVEL_COMPLETE_DELAY)
    {
      // Chuẩn bị cho màn chơi tiếp theo
      currentLevel++;
      targetMoney += 200; // Tăng mục tiêu ở mỗi màn
      timeLeft = 60.0f;   // Đặt lại thời gian
      money = 0;          // Đặt lại tiền
      timeUp = false;
      fang = false;
      shou = false;
      hookLength = 0.0f;
      levelCompleteTimer = 0.0f;

      // Tải lại vị trí các vật phẩm
      loadAssets();

      // Chuyển sang chơi màn mới
      gameState = GameState::Playing;
    }
    return;
  }

  if (gameState != GameState::Playing || timeUp)
    return;

  // Giảm thời gian
  timeLeft -= deltaTime;
  if (timeLeft <= 0.0f)
  {
    timeLeft = 0.0f;
    timeUp = true;
    gameState = GameState::GameOver;
  }

  // Kiểm tra xem đã đạt mục tiêu chưa
  if (money >= targetMoney)
  {
    gameState = GameState::LevelComplete;
    return;
  }

  if (!fang)
  {
    // Móc quay trái phải
    angle += angleSpeed;
    if (angle > 75.0f || angle < -75.0f)
      angleSpeed = -angleSpeed;
  }
  else
  {
    // Góc quay -> hướng di chuyển
    float rad = (angle + 90.0f) * M_PI / 180.0f;
    float dx = cos(rad);
    float dy = sin(rad);

    // Tính độ dài móc tối đa (1 lần duy nhất)
    if (hookLength == 0.0f)
    {
      float t = (SCREEN_HEIGHT - 63.0f) / dy;
      float endX = 291.0f + dx * t;
      float endY = 63.0f + dy * t;

      if (endX < 0.0f || endX > SCREEN_WIDTH)
      {
        if (endX < 0.0f)
          t = (0.0f - 291.0f) / dx;
        else
          t = (SCREEN_WIDTH - 291.0f) / dx;

        endX = 291.0f + dx * t;
        endY = 63.0f + dy * t;
      }

      float diffX = endX - 291.0f;
      float diffY = endY - 63.0f;
      hookMaxLength = sqrtf(diffX * diffX + diffY * diffY);
    }

    // Đang đi xuống
    if (!shou)
    {
      hook.rect.x += dx * hookSpeed;
      hook.rect.y += dy * hookSpeed;

      SDL_FPoint tip = {hook.rect.x + hook.rect.w / 2.0f, hook.rect.y};
      SDL_FPoint start = {291.0f, 63.0f};
      float dxHook = tip.x - start.x;
      float dyHook = tip.y - start.y;
      hookLength = sqrtf(dxHook * dxHook + dyHook * dyHook);

      // Va chạm vàng
      for (auto &g : golds)
      {
        if (g.visible && checkCollision(hook.rect, g.rect))
        {
          shou = true;
          hookedGold = &g;
          // Điều chỉnh tốc độ kéo dựa trên kích thước vàng
          if (g.rect.w < 30.0f) // Vàng nhỏ
            hookSpeed = 2.0f;
          else if (g.rect.w < 50.0f) // Vàng vừa
            hookSpeed = 1.5f;
          else                // Vàng to
            hookSpeed = 1.0f; // Làm chậm vàng to hơn
          break;
        }
      }

      // Va chạm đá nếu chưa dính vàng
      if (!shou)
      {
        for (auto &s : stones)
        {
          if (s.visible && checkCollision(hook.rect, s.rect))
          {
            shou = true;
            hookedGold = &s;
            // Điều chỉnh tốc độ kéo đá chậm hơn
            if (s.rect.w < 35.0f) // Đá nhỏ
              hookSpeed = 0.8f;   // Chậm hơn trước (1.0f)
            else                  // Đá to
              hookSpeed = 0.6f;   // Chậm hơn nữa
            break;
          }
        }
      }

      // Không dính gì và chạm biên
      if (hookLength >= hookMaxLength - 1.0f)
      {
        shou = true;
        hookedGold = nullptr;
        hookSpeed = 4.0f;
      }
    }
    // Kéo về
    else
    {
      hook.rect.x -= dx * hookSpeed;
      hook.rect.y -= dy * hookSpeed;
      hookLength -= hookSpeed;

      // Di chuyển vật theo móc
      if (hookedGold)
      {
        // 🎯 Gốc xoay của móc
        SDL_FPoint pivot = {hook.rect.w / 2.0f, 0.0f};
        float rad = (angle + 90.0f) * M_PI / 180.0f;

        // 📏 Khoảng cách từ đỉnh móc tới tâm vật
        float baseOffset = 5.0f;
        float sizeAdjust = std::max(hookedGold->rect.w, hookedGold->rect.h) * 0.15f;
        float totalOffset = baseOffset + sizeAdjust;

        // 📍 Tính vị trí đúng theo hướng xoay
        float hookHeadX = hook.rect.x + pivot.x;
        float hookHeadY = hook.rect.y + pivot.y;

        SDL_FPoint clampPos;
        clampPos.x = hookHeadX + cos(rad) * totalOffset;
        clampPos.y = hookHeadY + sin(rad) * totalOffset;

        // 📦 Gắn vật vào đúng giữa vị trí kẹp
        hookedGold->rect.x = clampPos.x - hookedGold->rect.w / 2.0f;
        hookedGold->rect.y = clampPos.y - hookedGold->rect.h / 2.0f;
      }

      if (hookLength <= 0.0f)
      {
        fang = false;
        shou = false;
        hookLength = 0.0f;
        hook.rect.x = 270;
        hook.rect.y = 63;

        if (hookedGold)
        {
          hookedGold->visible = false;
          bool isGold = false;

          for (auto &g : golds)
          {
            if (&g == hookedGold)
            {
              money += 100;
              isGold = true;
              break;
            }
          }

          if (!isGold)
            money += 30;

          hookedGold = nullptr;
          hookSpeed = 4.0f;
        }
      }
    }
  }
}

// Vẽ toàn bộ giao diện game lên màn hình
void renderGame()
{
  SDL_RenderClear(renderer);

  if (gameState == GameState::Welcome)
  {
    SDL_RenderTexture(renderer, background.texture, nullptr, &background.rect);
  }
  else if (gameState == GameState::Ready)
  {
    SDL_RenderTexture(renderer, goalImg.texture, nullptr, &goalImg.rect);
  }
  else if (gameState == GameState::Playing)
  {
    // 1. Vẽ background của màn chơi
    SDL_RenderTexture(renderer, playBackground.texture, nullptr,
                      &playBackground.rect);

    // 2. Vẽ các cục vàng còn hiện
    for (auto &g : golds)
      if (g.visible)
        SDL_RenderTexture(renderer, g.texture, nullptr, &g.rect);
    for (auto &s : stones)
      if (s.visible)
        SDL_RenderTexture(renderer, s.texture, nullptr, &s.rect);

    // 3. Tính điểm pivot (gốc xoay) là đỉnh móc
    SDL_FPoint pivot = {hook.rect.w / 2.0f, 0.0f};

    // 4. Tính điểm đầu móc (nơi dây sẽ nối vào)
    float tipX = hook.rect.x + pivot.x;
    float tipY = hook.rect.y + pivot.y;

    // 5. Vẽ dây móc từ tời đến đỉnh móc
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255); // Dây màu đen
    if (!fang && hookLength <= 0.0f)
    {
      SDL_RenderLine(renderer, 291, 63, 291, 63); // Không kéo thì dây gọn
    }
    else
    {
      SDL_RenderLine(renderer, 291, 63, tipX, tipY);
    }

    // 6. Vẽ móc xoay quanh đỉnh
    SDL_RenderTextureRotated(renderer, hook.texture, nullptr, &hook.rect, angle, &pivot, SDL_FLIP_NONE);

    // 7. Hiển thị thông tin trực tiếp bằng các đường vẽ
    // Thiết lập màu cho chữ (màu đỏ)
    SDL_SetRenderDrawColor(renderer, 255, 0, 0, 255);

    // Hiển thị số vàng hiện tại (góc trên bên trái)
    int goldX = 15;
    int goldY = 15;
    int digitHeight = 14;  // Giảm chiều cao xuống
    int digitSpacing = 14; // Giảm khoảng cách giữa các chữ số
    int digitWidth = 8;    // Giảm kích thước chiều rộng chữ số
    int moneyStartX = goldX + 20;

    // Điều chỉnh vị trí số tiền xuống dưới một chút để không chồng lên chữ
    int currentValueY = goldY + 15; // Di chuyển lên cao hơn

    // Vẽ số tiền
    std::string moneyStr = std::to_string(money);

    for (int i = 0; i < moneyStr.length(); i++)
    {
      int digit = moneyStr[i] - '0';
      int posX = moneyStartX + i * digitSpacing;

      // Vẽ viền đen trước
      outlineDigit(renderer, digitSegments[digit], posX, currentValueY, digitWidth, digitHeight);

      // Thay đổi màu từ xanh đậm sang trắng sáng
      SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);

      // Vẽ các segment của số
      if (digitSegments[digit][0]) // Trên
        drawSegment(renderer, posX, currentValueY, posX + digitWidth, currentValueY, 3);
      if (digitSegments[digit][1]) // Phải trên
        drawSegment(renderer, posX + digitWidth, currentValueY, posX + digitWidth, currentValueY + digitHeight / 2, 3);
      if (digitSegments[digit][2]) // Phải dưới
        drawSegment(renderer, posX + digitWidth, currentValueY + digitHeight / 2, posX + digitWidth, currentValueY + digitHeight, 3);
      if (digitSegments[digit][3]) // Dưới
        drawSegment(renderer, posX, currentValueY + digitHeight, posX + digitWidth, currentValueY + digitHeight, 3);
      if (digitSegments[digit][4]) // Trái dưới
        drawSegment(renderer, posX, currentValueY + digitHeight / 2, posX, currentValueY + digitHeight, 3);
      if (digitSegments[digit][5]) // Trái trên
        drawSegment(renderer, posX, currentValueY, posX, currentValueY + digitHeight / 2, 3);
      if (digitSegments[digit][6]) // Giữa
        drawSegment(renderer, posX, currentValueY + digitHeight / 2, posX + digitWidth, currentValueY + digitHeight / 2, 3);
    }

    // Hiển thị dấu / và mục tiêu
    int slashX = moneyStartX + moneyStr.length() * digitSpacing + 8;
    SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255); // Trở lại màu trắng
    drawSegment(renderer, slashX, currentValueY + digitHeight, slashX + 5, currentValueY, 3);

    // Vẽ số mục tiêu màu trắng
    SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
    std::string targetStr = std::to_string(targetMoney);
    for (int i = 0; i < targetStr.length(); i++)
    {
      int digit = targetStr[i] - '0';
      int posX = slashX + 15 + i * digitSpacing;

      // Vẽ viền đen trước
      outlineDigit(renderer, digitSegments[digit], posX, currentValueY, digitWidth, digitHeight);

      // Màu trắng cho số mục tiêu
      SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);

      // Vẽ các segment của số
      if (digitSegments[digit][0]) // Trên
        drawSegment(renderer, posX, currentValueY, posX + digitWidth, currentValueY, 3);
      if (digitSegments[digit][1]) // Phải trên
        drawSegment(renderer, posX + digitWidth, currentValueY, posX + digitWidth, currentValueY + digitHeight / 2, 3);
      if (digitSegments[digit][2]) // Phải dưới
        drawSegment(renderer, posX + digitWidth, currentValueY + digitHeight / 2, posX + digitWidth, currentValueY + digitHeight, 3);
      if (digitSegments[digit][3]) // Dưới
        drawSegment(renderer, posX, currentValueY + digitHeight, posX + digitWidth, currentValueY + digitHeight, 3);
      if (digitSegments[digit][4]) // Trái dưới
        drawSegment(renderer, posX, currentValueY + digitHeight / 2, posX, currentValueY + digitHeight, 3);
      if (digitSegments[digit][5]) // Trái trên
        drawSegment(renderer, posX, currentValueY, posX, currentValueY + digitHeight / 2, 3);
      if (digitSegments[digit][6]) // Giữa
        drawSegment(renderer, posX, currentValueY + digitHeight / 2, posX + digitWidth, currentValueY + digitHeight / 2, 3);
    }

    // Hiển thị thời gian (góc trên bên phải)
    int secondsLeft = (int)timeLeft;

    // Điều chỉnh vị trí số giây xuống dưới một chút
    int timeStartX = SCREEN_WIDTH - 50; // Thay đổi vị trí
    int timeStartY = goldY + 15;        // Di chuyển lên cao hơn

    // Vẽ số giây
    std::string timeStr = std::to_string(secondsLeft);
    if (timeStr.length() == 1)
    {
      timeStr = "0" + timeStr; // Thêm số 0 phía trước nếu < 10
    }

    // Bỏ nền đen mờ phía sau thời gian

    if (secondsLeft < 10)
    {
      // Giữ màu đỏ khi < 10s
      SDL_SetRenderDrawColor(renderer, 255, 80, 80, 255);
    }
    else
    {
      // Trở lại màu trắng
      SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
    }

    // Vẽ các chữ số thời gian
    for (int i = 0; i < timeStr.length(); i++)
    {
      int digit = timeStr[i] - '0';
      int posX = timeStartX + i * digitSpacing;

      // Vẽ viền đen dày hơn
      outlineDigit(renderer, digitSegments[digit], posX, timeStartY, digitWidth, digitHeight);

      // Đặt lại màu
      if (secondsLeft < 10)
      {
        SDL_SetRenderDrawColor(renderer, 255, 80, 80, 255);
      }
      else
      {
        SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
      }

      // Vẽ các segment của số
      if (digitSegments[digit][0]) // Trên
        drawSegment(renderer, posX, timeStartY, posX + digitWidth, timeStartY, 3);
      if (digitSegments[digit][1]) // Phải trên
        drawSegment(renderer, posX + digitWidth, timeStartY, posX + digitWidth, timeStartY + digitHeight / 2, 3);
      if (digitSegments[digit][2]) // Phải dưới
        drawSegment(renderer, posX + digitWidth, timeStartY + digitHeight / 2, posX + digitWidth, timeStartY + digitHeight, 3);
      if (digitSegments[digit][3]) // Dưới
        drawSegment(renderer, posX, timeStartY + digitHeight, posX + digitWidth, timeStartY + digitHeight, 3);
      if (digitSegments[digit][4]) // Trái dưới
        drawSegment(renderer, posX, timeStartY + digitHeight / 2, posX, timeStartY + digitHeight, 3);
      if (digitSegments[digit][5]) // Trái trên
        drawSegment(renderer, posX, timeStartY, posX, timeStartY + digitHeight / 2, 3);
      if (digitSegments[digit][6]) // Giữa
        drawSegment(renderer, posX, timeStartY + digitHeight / 2, posX + digitWidth, timeStartY + digitHeight / 2, 3);
    }
  }
  else if (gameState == GameState::LevelComplete)
  {
    // Hiển thị hình ảnh hoàn thành màn chơi (dùng win.png)
    SDL_RenderTexture(renderer, winImg.texture, nullptr, &winImg.rect);

    // Hiển thị số màn chơi
    int digitHeight = 24; // Tăng kích thước chữ số
    int digitWidth = 14;

    std::string levelStr = std::to_string(currentLevel);
    float posX = SCREEN_WIDTH / 2.0f - levelStr.length() * 10;
    float posY = SCREEN_HEIGHT / 2.0f + 50;

    for (int i = 0; i < levelStr.length(); i++)
    {
      int digit = levelStr[i] - '0';
      float digitX = posX + i * 25;

      // Vẽ chữ số với viền đen
      SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
      outlineDigit(renderer, digitSegments[digit], digitX, posY, digitWidth, digitHeight);

      // Vẽ chữ số màu vàng
      SDL_SetRenderDrawColor(renderer, 255, 215, 0, 255);

      if (digitSegments[digit][0]) // Trên
        drawSegment(renderer, digitX, posY, digitX + digitWidth, posY, 3);
      if (digitSegments[digit][1]) // Phải trên
        drawSegment(renderer, digitX + digitWidth, posY, digitX + digitWidth, posY + digitHeight / 2, 3);
      if (digitSegments[digit][2]) // Phải dưới
        drawSegment(renderer, digitX + digitWidth, posY + digitHeight / 2, digitX + digitWidth, posY + digitHeight, 3);
      if (digitSegments[digit][3]) // Dưới
        drawSegment(renderer, digitX, posY + digitHeight, digitX + digitWidth, posY + digitHeight, 3);
      if (digitSegments[digit][4]) // Trái dưới
        drawSegment(renderer, digitX, posY + digitHeight / 2, digitX, posY + digitHeight, 3);
      if (digitSegments[digit][5]) // Trái trên
        drawSegment(renderer, digitX, posY, digitX, posY + digitHeight / 2, 3);
      if (digitSegments[digit][6]) // Giữa
        drawSegment(renderer, digitX, posY + digitHeight / 2, digitX + digitWidth, posY + digitHeight / 2, 3);
    }
  }
  else if (gameState == GameState::GameOver)
  {
    SDL_RenderTexture(renderer,
                      (money >= targetMoney ? winImg.texture : loseImg.texture), nullptr, &(money >= targetMoney ? winImg.rect : loseImg.rect));
  }

  SDL_RenderPresent(renderer);
}

// Giải phóng bộ nhớ khi kết thúc
void cleanup()
{
  for (auto &g : golds)
    SDL_DestroyTexture(g.texture);
  for (auto &s : stones)
    SDL_DestroyTexture(s.texture);
  SDL_DestroyTexture(background.texture);
  SDL_DestroyTexture(goalImg.texture);
  SDL_DestroyTexture(playBackground.texture);
  SDL_DestroyTexture(hook.texture);
  SDL_DestroyTexture(winImg.texture);
  SDL_DestroyTexture(loseImg.texture);

  SDL_DestroyRenderer(renderer);
  SDL_DestroyWindow(window);
  SDL_Quit();
}

// Hàm main - vòng lặp game chính
int main(int argc, char *argv[])
{
  if (!init())
    return -1;
  loadAssets();

  Uint64 lastTick = SDL_GetTicks();
  bool running = true;
  SDL_Event e;

  while (running)
  {
    Uint64 current = SDL_GetTicks();
    float deltaTime = (current - lastTick) / 1000.0f;
    lastTick = current;

    while (SDL_PollEvent(&e))
    {
      if (e.type == SDL_EVENT_QUIT)
        running = false;

      if (e.type == SDL_EVENT_MOUSE_BUTTON_DOWN)
      { // thay đổi làm hết lỗi nút // bắt đầu0000000000000
        // Chuyển đổi tọa độ chuột sang hệ logic
        float logicX = 0, logicY = 0;
        SDL_RenderCoordinatesFromWindow(renderer, e.button.x, e.button.y, &logicX, &logicY);

        SDL_FPoint mp = {logicX, logicY};

        if (gameState == GameState::Welcome)
          gameState = GameState::Ready;
        else if (gameState == GameState::Ready)
          gameState = GameState::Playing;
        else if (gameState == GameState::Playing && !fang)
          fang = true; // Bắt đầu móc
      }
    }

    updateGame(deltaTime);
    renderGame();
    SDL_Delay(16);
  }

  cleanup();
  return 0;
}
