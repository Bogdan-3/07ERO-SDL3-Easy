#include <iostream>
#include <vector>
#include <SDL3/SDL.h>
#include "rectangle.h"
#include "circle.h"
#include <vector>
#include <cmath>     // NEW
#include <algorithm> // NEW

// define window dimensions
constexpr int WINDOW_WIDTH{700};
constexpr int WINDOW_HEIGHT{800};

// Vertical scrolling grid related variables
constexpr float VERTICAL_SCROLLING_SPEED{100};
constexpr int VERTICAL_SCROLLING_GRID_SQUARE_SIZE{20};
std::vector<Rectangle> scrollingGrid;

// Animation related variables
float DT{0.01f};
Uint64 startTime{0};
Uint64 endTime{0};

// define SDL Window related variables
SDL_Window *window{nullptr};
SDL_Renderer *renderer{nullptr};
SDL_Event currentEvent;
SDL_Color backgroundColor{255, 255, 255, 255};

bool quit{false};

float mouseX{WINDOW_WIDTH / 2}, mouseY{WINDOW_HEIGHT - 50};

float displayScale{1.0f};

// define shapes
Rectangle Player;
Rectangle Enemy1, Enemy2, Enemy3;
std::vector<Circle> BulletPlayer, BulletEnemy;
Circle circle;

// ---------------- NEW: bullet / enemy / game state ----------------
constexpr float PLAYER_BULLET_SPEED{500.0f};
constexpr float ENEMY_BULLET_SPEED{PLAYER_BULLET_SPEED / 5.0f}; // 100 px/s
constexpr float PLAYER_FIRE_PERIOD{0.3f};
constexpr float ENEMY_FIRE_PERIOD{PLAYER_FIRE_PERIOD * 10.0f}; // 3 s
constexpr float ENEMY_FIRST_SHOT{1.0f};
constexpr float BULLET_RADIUS{10.0f};

float fireTimer{0.0f};

Rectangle *Enemies[3]{&Enemy1, &Enemy2, &Enemy3}; // lets loops reach your Enemy1/2/3
bool enemyAlive[3]{true, true, true};
float enemyCooldown[3]{ENEMY_FIRST_SHOT, ENEMY_FIRST_SHOT, ENEMY_FIRST_SHOT};

bool gameOver{false};
// -------------------------------------------------------------------

void EdgeCheck(Rectangle &rectangle)
{
    if (rectangle.pos.x > WINDOW_WIDTH)
        rectangle.pos.x = WINDOW_WIDTH - 50;
}

void initShape()
{
    // Player
    Player.color = SDL_Color(0, 255, 0, 255);
    Player.height = Player.width = 50;

    // Enemy
    Enemy1.color = Enemy2.color = Enemy3.color = SDL_Color(255, 127, 0, 255);
    Enemy1.height = Enemy2.height = Enemy3.height = 50;
    Enemy1.width = Enemy2.width = Enemy3.width = 50;
    Enemy1.pos.y = SDL_rand(WINDOW_HEIGHT / 2);
    Enemy2.pos.y = SDL_rand(WINDOW_HEIGHT / 2);
    Enemy3.pos.y = SDL_rand(WINDOW_HEIGHT / 2);

    Enemy1.pos.x = SDL_rand(WINDOW_HEIGHT / 3);
    Enemy2.pos.x = SDL_rand(WINDOW_HEIGHT / 3) + WINDOW_HEIGHT / 3;
    Enemy3.pos.x = SDL_rand(WINDOW_HEIGHT / 3) + 2 * WINDOW_HEIGHT / 3;

    EdgeCheck(Enemy1);
    EdgeCheck(Enemy2);
    EdgeCheck(Enemy3);
}

// CHANGED: only the NEW bullet gets its position/color/radius/velocity
// (before, every click moved all existing bullets back to the mouse)
void Create()
{
    SDL_GetMouseState(&mouseX, &mouseY);
    BulletPlayer.push_back(circle);
    // define bullets
    Circle &b = BulletPlayer.back();
    b.color = SDL_Color(255, 0, 0, 255);
    b.radius = BULLET_RADIUS;
    b.pos.x = mouseX;
    b.pos.y = mouseY - 30;
    b.vel = glm::vec2(0.0f, -1.0f) * PLAYER_BULLET_SPEED; // NEW: direction (length 1) * speed
}

void initScollingRectangles()
{
    int numGridRectangles = (WINDOW_HEIGHT / VERTICAL_SCROLLING_GRID_SQUARE_SIZE) / 2 + 2;

    for (int i = 0; i < numGridRectangles; i++)
    {
        Rectangle newGridRectangleLeft, newGridRectangleRight;

        newGridRectangleLeft.width = newGridRectangleLeft.height = VERTICAL_SCROLLING_GRID_SQUARE_SIZE;
        newGridRectangleLeft.pos.x = newGridRectangleLeft.width / 2.0f;
        newGridRectangleLeft.pos.y = WINDOW_HEIGHT - newGridRectangleLeft.height / 2.0f - 2.0f * i * newGridRectangleLeft.height;
        newGridRectangleLeft.vel = glm::vec2(0.0f, VERTICAL_SCROLLING_SPEED);
        newGridRectangleLeft.color = SDL_Color{235, 235, 235, 255};

        newGridRectangleRight.width = newGridRectangleRight.height = VERTICAL_SCROLLING_GRID_SQUARE_SIZE;
        newGridRectangleRight.pos.x = WINDOW_WIDTH - newGridRectangleRight.width / 2.0f;
        newGridRectangleRight.pos.y = WINDOW_HEIGHT - newGridRectangleRight.height / 2.0f - 2.0f * i * newGridRectangleRight.height;
        newGridRectangleRight.vel = glm::vec2(0.0f, VERTICAL_SCROLLING_SPEED);
        newGridRectangleRight.color = SDL_Color{235, 235, 235, 255};

        scrollingGrid.push_back(newGridRectangleLeft);
        scrollingGrid.push_back(newGridRectangleRight);
    }
}

bool initWindow()
{
    bool success{true};

    // Try to initialize SDL
    if (!SDL_Init(SDL_INIT_VIDEO))
    {
        SDL_Log("SDL initialization failed: %s\n", SDL_GetError());
        success = false;
    }
    else
    {
        // Try to create the window and renderer
        displayScale = SDL_GetDisplayContentScale(1);

        if (!SDL_CreateWindowAndRenderer(
                "SDL Hello World Example",
                static_cast<int>(displayScale * WINDOW_WIDTH),
                static_cast<int>(displayScale * WINDOW_HEIGHT),
                0,
                &window, &renderer))
        {
            SDL_Log("Failed to create window and renderer: %s\n", SDL_GetError());
            success = false;
        }
        else
        {
            // Apply global display scaling to renderer
            SDL_SetRenderScale(renderer, displayScale, displayScale);

            // Set background color
            SDL_SetRenderDrawColor(renderer, backgroundColor.r, backgroundColor.g, backgroundColor.b,
                                   backgroundColor.a);

            // Apply background color
            SDL_RenderClear(renderer);
        }
    }

    return success;
}

// CHANGED: was "if (DT = 0.3f)", which overwrote DT. Now it fires only
// when the fire timer has run out.
void Up()
{
    if (fireTimer <= 0.0f)
    {
        Create();
        fireTimer = PLAYER_FIRE_PERIOD;
    }
}

// CHANGED: same name, now moves every bullet by velocity * DT and removes
// the ones that left the window
void speed()
{
    for (int i = 0; i < BulletPlayer.size(); i++)
        BulletPlayer[i].pos += BulletPlayer[i].vel * DT;
    for (int i = 0; i < BulletEnemy.size(); i++)
        BulletEnemy[i].pos += BulletEnemy[i].vel * DT;

    auto offscreen = [](const Circle &b)
    {
        return b.pos.x < -BULLET_RADIUS || b.pos.x > WINDOW_WIDTH + BULLET_RADIUS ||
               b.pos.y < -BULLET_RADIUS || b.pos.y > WINDOW_HEIGHT + BULLET_RADIUS;
    };
    BulletPlayer.erase(std::remove_if(BulletPlayer.begin(), BulletPlayer.end(), offscreen), BulletPlayer.end());
    BulletEnemy.erase(std::remove_if(BulletEnemy.begin(), BulletEnemy.end(), offscreen), BulletEnemy.end());
}

// ---------------- NEW: enemy fire, hitboxes, game flow ----------------
void spawnEnemyBullet(const Rectangle &enemy)
{
    Circle b = circle;
    b.color = SDL_Color(255, 0, 255, 255);
    b.radius = BULLET_RADIUS;
    b.pos = enemy.pos;

    glm::vec2 dir = Player.pos - enemy.pos;
    if (glm::length(dir) > 0.0001f)
        dir = glm::normalize(dir); // aimed once, when fired
    else
        dir = glm::vec2(0.0f, 1.0f);
    b.vel = dir * ENEMY_BULLET_SPEED;
    BulletEnemy.push_back(b);
}

void updateFiring()
{
    // player: timer counts down, LMB held keeps firing
    fireTimer = std::max(0.0f, fireTimer - DT);
    if (SDL_GetMouseState(nullptr, nullptr) & SDL_BUTTON_MASK(SDL_BUTTON_LEFT))
        Up();

    // enemies: first shot after 1 s, then every 3 s
    for (int i = 0; i < 3; i++)
    {
        if (!enemyAlive[i])
            continue;
        enemyCooldown[i] -= DT;
        if (enemyCooldown[i] <= 0.0f)
        {
            spawnEnemyBullet(*Enemies[i]);
            enemyCooldown[i] += ENEMY_FIRE_PERIOD;
        }
    }
}

// circle vs 50x50 box: clamp circle center into the box, compare distance with radius
bool circleHitsRect(const Circle &c, const Rectangle &r)
{
    glm::vec2 half(r.width / 2.0f, r.height / 2.0f);
    glm::vec2 closest = glm::clamp(c.pos, r.pos - half, r.pos + half);
    glm::vec2 d = c.pos - closest;
    return glm::dot(d, d) <= c.radius * c.radius;
}

// box vs box
bool rectHitsRect(const Rectangle &a, const Rectangle &b)
{
    return std::abs(a.pos.x - b.pos.x) < (a.width + b.width) / 2.0f &&
           std::abs(a.pos.y - b.pos.y) < (a.height + b.height) / 2.0f;
}

void endGame(bool win)
{
    gameOver = true;
    Player.color = win ? SDL_Color(0, 0, 255, 255) : SDL_Color(255, 0, 0, 255);
    for (int i = 0; i < 3; i++)
        enemyAlive[i] = false;
    BulletPlayer.clear();
    BulletEnemy.clear();
}

void resetGame()
{
    BulletPlayer.clear();
    BulletEnemy.clear();
    fireTimer = 0.0f;
    for (int i = 0; i < 3; i++)
    {
        enemyAlive[i] = true;
        enemyCooldown[i] = ENEMY_FIRST_SHOT;
    }
    gameOver = false;
    initShape(); // new enemy positions + player color back to green
}

void checkCollisions()
{
    // player bullets vs enemies: both disappear
    for (int i = (int)BulletPlayer.size() - 1; i >= 0; i--)
    {
        for (int j = 0; j < 3; j++)
        {
            if (enemyAlive[j] && circleHitsRect(BulletPlayer[i], *Enemies[j]))
            {
                enemyAlive[j] = false;
                BulletPlayer.erase(BulletPlayer.begin() + i);
                break;
            }
        }
    }

    if (!enemyAlive[0] && !enemyAlive[1] && !enemyAlive[2])
    {
        endGame(true); // win
        return;
    }

    // enemy touches player
    for (int j = 0; j < 3; j++)
        if (enemyAlive[j] && rectHitsRect(Player, *Enemies[j]))
        {
            endGame(false);
            return;
        }

    // enemy bullet hits player
    for (int i = 0; i < BulletEnemy.size(); i++)
        if (circleHitsRect(BulletEnemy[i], Player))
        {
            endGame(false);
            return;
        }
}
// ----------------------------------------------------------------------

void processEvents()
{
    // Check for events in queue
    while (SDL_PollEvent(&currentEvent))
    {
        // User requests quit
        if (currentEvent.type == SDL_EVENT_QUIT)
        {
            quit = true;
        }

        // Mouse event -> pressed button
        if (currentEvent.type == SDL_EVENT_MOUSE_BUTTON_DOWN)
        {
            if (currentEvent.button.button == SDL_BUTTON_LEFT)
            {
                SDL_GetMouseState(&mouseX, &mouseY);
                if (!gameOver) // NEW
                    Up();
                mouseX /= displayScale;
                mouseY /= displayScale;
                std::cout << "Left mouse click => " << "x: " << mouseX << ", y: " << mouseY << std::endl;
            }
        }

        // Mouse event -> mouse movement
        if (currentEvent.type == SDL_EVENT_MOUSE_MOTION)
        {
            if (const SDL_MouseButtonFlags mouseButtons = SDL_GetMouseState(nullptr, nullptr); mouseButtons & SDL_BUTTON_MASK(SDL_BUTTON_LEFT))
            {
                SDL_GetMouseState(&mouseX, &mouseY);
                mouseX /= displayScale;
                mouseY /= displayScale;
                std::cout << "Mouse move while left clicking => " << "x: " << mouseX << ", y: " << mouseY << std::endl;
            }
        }

        // Keyboard event
        if (currentEvent.type == SDL_EVENT_KEY_DOWN)
        {
            switch (currentEvent.key.key)
            {
            case SDLK_UP:
                std::cout << "UP arrow key was pressed" << std::endl;
                break;
            case SDLK_DOWN:
                std::cout << "DOWN arrow key was pressed" << std::endl;
                break;
            case SDLK_LEFT:
                std::cout << "LEFT arrow key was pressed" << std::endl;
                break;
            case SDLK_RIGHT:
                std::cout << "RIGHT arrow key was pressed" << std::endl;
                break;
            case SDLK_R:
                std::cout << "R key was pressed" << std::endl;
                if (gameOver) // NEW: reset only after the game ended
                    resetGame();
                break;
            case SDLK_S:
                std::cout << "S key was pressed" << std::endl;
                break;
            case SDLK_ESCAPE:
                quit = true;
                break;
            default:
                break;
            }
        }
    }
}

void animate()
{

    // Scrolling grid
    for (int i = 0; i < scrollingGrid.size(); i++)
    {
        Rectangle &gridRectangle = scrollingGrid.at(i);

        gridRectangle.pos += gridRectangle.vel * DT;

        if (gridRectangle.pos.y > (WINDOW_HEIGHT + gridRectangle.height / 2.0f))
        {
            gridRectangle.pos.y -= gridRectangle.height * scrollingGrid.size();
        }
    }
}

void MouseMovement()
{
    SDL_GetMouseState(&mouseX, &mouseY);
    Player.pos.x = mouseX;
    Player.pos.y = mouseY;

    // NEW: keep the ship inside the window
    const float half = Player.width / 2.0f;
    Player.pos.x = glm::clamp(Player.pos.x, half, WINDOW_WIDTH - half);
    Player.pos.y = glm::clamp(Player.pos.y, half, WINDOW_HEIGHT - half);
}

void drawFrame()
{
    // Clear the background
    SDL_SetRenderDrawColor(renderer, backgroundColor.r, backgroundColor.g, backgroundColor.b, backgroundColor.a);
    SDL_RenderClear(renderer);

    // Draw scene
    Player.draw(renderer);
    if (enemyAlive[0])
        Enemy1.draw(renderer); // CHANGED: only living enemies
    if (enemyAlive[1])
        Enemy2.draw(renderer);
    if (enemyAlive[2])
        Enemy3.draw(renderer);
    for (int i = 0; i < BulletPlayer.size(); i++)
        BulletPlayer[i].draw(renderer);
    for (int i = 0; i < BulletEnemy.size(); i++) // NEW
        BulletEnemy[i].draw(renderer);

    // Draw scrolling grid

    for (int i = 0; i < scrollingGrid.size(); i++)
    {
        Rectangle &gridRectangle = scrollingGrid.at(i);
        gridRectangle.draw(renderer);
    }

    // Update window
    SDL_RenderPresent(renderer);
}

void cleanup()
{
    // Destroy renderer
    if (renderer)
    {
        SDL_DestroyRenderer(renderer);
        renderer = nullptr;
    }

    // Destroy window
    if (window)
    {
        SDL_DestroyWindow(window);
        window = nullptr;
    }

    // Quit SDL
    SDL_Quit();
}

int main()
{
    // Initialize window
    if (!initWindow())
    {
        std::cout << "Failed to initialize" << std::endl;
        return -1;
    }

    initShape();

    SDL_zero(currentEvent);

    initScollingRectangles();

    SDL_SetWindowRelativeMouseMode(window, true);

    // Game loop
    while (!quit)
    {
        startTime = SDL_GetTicks();

        processEvents();
        if (!gameOver) // NEW: everything game-related freezes after win/loss
        {
            MouseMovement();
            updateFiring();
            speed();
            checkCollisions();
        }
        animate();
        drawFrame();

        endTime = SDL_GetTicks();
        DT = (endTime - startTime) / 1000.0f;
    }

    cleanup();
    return 0;
}