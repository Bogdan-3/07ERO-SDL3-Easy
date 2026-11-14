#include <iostream>
#include <vector>
#include <SDL3/SDL.h>
#include "rectangle.h"
#include "circle.h"
#include <vector>
#include <algorithm>

constexpr float PLAYER_BULLET_SPEED{500.0f};
constexpr float ENEMY_BULLET_SPEED{PLAYER_BULLET_SPEED / 5.0f}; // 100 px/s
constexpr float PLAYER_FIRE_PERIOD{0.3f};
constexpr float BULLET_RADIUS{10.0f};

float fireTimer{0.0f};
bool fireHeld{false};

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

void Create()
{
    SDL_GetMouseState(&mouseX, &mouseY);
    BulletPlayer.push_back(circle);
    // define bullets
    for (int i = 0; i < BulletPlayer.size(); i++)
    {
        BulletPlayer[i].color = SDL_Color(255, 0, 0, 255);
        BulletPlayer[i].radius = 10;
        BulletPlayer[i].pos.x = mouseX;
        BulletPlayer[i].pos.y = mouseY - 30;
    }
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

void spawnPlayerBullet()
{
    Circle bullet;
    bullet.radius = BULLET_RADIUS;
    bullet.color = SDL_Color(255, 0, 0, 255);
    bullet.pos = Player.pos;
    bullet.vel = glm::vec2(0.0f, -1.0f) * PLAYER_BULLET_SPEED; // direction (length 1) * speed
    BulletPlayer.push_back(bullet);
}

void spawnEnemyBullet(const Rectangle &enemy)
{
    Circle bullet;
    bullet.radius = BULLET_RADIUS;
    bullet.color = SDL_Color(255, 0, 255, 255);
    bullet.pos = enemy.pos;

    glm::vec2 dir = Player.pos - enemy.pos;
    if (glm::length(dir) > 0.0001f)
        dir = glm::normalize(dir); // aimed once, here, at firing time
    else
        dir = glm::vec2(0.0f, 1.0f);
    bullet.vel = dir * ENEMY_BULLET_SPEED;
    BulletEnemy.push_back(bullet);
}

void updateFiring()
{
    const SDL_MouseButtonFlags buttons = SDL_GetMouseState(nullptr, nullptr);
    const bool lmbDown = (buttons & SDL_BUTTON_MASK(SDL_BUTTON_LEFT)) != 0;

    if (lmbDown)
    {
        fireTimer -= DT;
        while (fireTimer <= 0.0f)
        {
            spawnPlayerBullet();
            fireTimer += PLAYER_FIRE_PERIOD;
        }
    }
    else
        fireTimer = 0.0f; // next press fires immediately
}

void moveBullets(std::vector<Circle> &bullets)
{
    for (Circle &b : bullets)
        b.pos += b.vel * DT; // pixels/second * seconds

    bullets.erase(std::remove_if(bullets.begin(), bullets.end(),
                                 [](const Circle &b)
                                 {
                                     return b.pos.x < -BULLET_RADIUS || b.pos.x > WINDOW_WIDTH + BULLET_RADIUS ||
                                            b.pos.y < -BULLET_RADIUS || b.pos.y > WINDOW_HEIGHT + BULLET_RADIUS;
                                 }),
                  bullets.end());
}

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
                if (currentEvent.type == SDL_EVENT_MOUSE_BUTTON_DOWN && currentEvent.button.button == SDL_BUTTON_LEFT)
                    fireHeld = true;

                if (currentEvent.type == SDL_EVENT_MOUSE_BUTTON_UP && currentEvent.button.button == SDL_BUTTON_LEFT)
                    fireHeld = false;
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
}

void drawFrame()
{
    // Clear the background
    SDL_SetRenderDrawColor(renderer, backgroundColor.r, backgroundColor.g, backgroundColor.b, backgroundColor.a);
    SDL_RenderClear(renderer);

    // Draw scene
    Player.draw(renderer);
    Enemy1.draw(renderer);
    Enemy2.draw(renderer);
    Enemy3.draw(renderer);
    for (int i = 0; i < BulletPlayer.size(); i++)
        BulletPlayer[i].draw(renderer);

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
        MouseMovement();
        updateFiring();
        moveBullets(BulletPlayer);
        moveBullets(BulletEnemy);
        animate();
        drawFrame();

        endTime = SDL_GetTicks();
        DT = (endTime - startTime) / 1000.0f;
    }

    cleanup();
    return 0;
}
