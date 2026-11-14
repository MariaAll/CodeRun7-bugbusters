#include <iostream>
#include <SDL3/SDL.h>
#include "circle.cpp"
#include "rectangle.cpp"

#include <cmath>
#include <cstdlib>
#include <ctime>
#include <vector>

constexpr int WINDOW_WIDTH{700};
constexpr int WINDOW_HEIGHT{800};
constexpr float SHIP_SIZE{50.0f};
constexpr float BULLET_RADIUS{10.0f};
constexpr float PLAYER_BULLET_SPEED{500.0f};
constexpr float ENEMY_BULLET_SPEED{PLAYER_BULLET_SPEED / 5.0f};
constexpr float PLAYER_FIRE_INTERVAL{0.3f};
constexpr float ENEMY_FIRE_INTERVAL{PLAYER_FIRE_INTERVAL * 10.0f};
constexpr float ENEMY_ATTACK_DELAY{1.0f};
constexpr int ENEMY_COUNT{3};

SDL_Window* window{nullptr};
SDL_Renderer* renderer{nullptr};
SDL_Event currentEvent;
SDL_Color backgroundColor{0, 0, 0, 255};

bool quit{false};
float displayScale{1.0f};

bool playing{true};
bool leftButtonDown{false};
float elapsed{0.0f};
float playerFireTimer{0.0f};
glm::vec2 mouseDelta{0.0f, 0.0f};

struct Bullet {
    Circle shape;
};

struct Enemy {
    Rectangle shape;
    float fireTimer{ENEMY_ATTACK_DELAY};
};

std::vector<Bullet> playerBullets;
std::vector<Bullet> enemyBullets;
std::vector<Enemy> enemies;
Rectangle playerShip(SHIP_SIZE, SHIP_SIZE,
                     glm::vec2(WINDOW_WIDTH / 2.0f,
                               WINDOW_HEIGHT - SHIP_SIZE / 2.0f),
                     SDL_Color{0, 255, 0, 255});

glm::vec2 logicalSize() {
    int w{WINDOW_WIDTH};
    int h{WINDOW_HEIGHT};
    if (window) {
        SDL_GetWindowSize(window, &w, &h);
    }
    return glm::vec2(static_cast<float>(w) / displayScale,
                     static_cast<float>(h) / displayScale);
}

float frand(float lo, float hi) {
    return lo + static_cast<float>(std::rand()) /
               (static_cast<float>(RAND_MAX) + 1.0f) * (hi - lo);
}

void spawnEnemies() {
    enemies.clear();
    const glm::vec2 size = logicalSize();
    const float sliceWidth = size.x / ENEMY_COUNT;
    for (int i = 0; i < ENEMY_COUNT; ++i) {
        Enemy e;
        e.shape = Rectangle(SHIP_SIZE, SHIP_SIZE,
                            glm::vec2(frand(i * sliceWidth + SHIP_SIZE / 2.0f,
                                            (i + 1) * sliceWidth - SHIP_SIZE / 2.0f),
                                      frand(SHIP_SIZE / 2.0f,
                                            size.y / 2.0f - SHIP_SIZE / 2.0f)),
                            SDL_Color{255, 165, 0, 255});
        e.fireTimer = ENEMY_ATTACK_DELAY;
        enemies.push_back(e);
    }
}

void resetGame() {
    const glm::vec2 size = logicalSize();
    playing = true;
    elapsed = 0.0f;
    playerFireTimer = 0.0f;
    playerBullets.clear();
    enemyBullets.clear();
    playerShip.color = SDL_Color{0, 255, 0, 255};
    playerShip.pos = glm::vec2(size.x / 2.0f, size.y - SHIP_SIZE / 2.0f);
    spawnEnemies();
}

void endGame(bool won) {
    playing = false;
    playerBullets.clear();
    enemyBullets.clear();
    enemies.clear();
    playerShip.color = won ? SDL_Color{0, 0, 255, 255} : SDL_Color{255, 0, 0, 255};
}

bool circleHitsRect(const glm::vec2& c, const Rectangle& r) {
    const glm::vec2 closest(std::max(r.pos.x - r.width / 2.0f,
                                     std::min(c.x, r.pos.x + r.width / 2.0f)),
                            std::max(r.pos.y - r.height / 2.0f,
                                     std::min(c.y, r.pos.y + r.height / 2.0f)));
    const glm::vec2 d = c - closest;
    return glm::dot(d, d) <= BULLET_RADIUS * BULLET_RADIUS;
}

bool rectsOverlap(const Rectangle& a, const Rectangle& b) {
    const glm::vec2 d = a.pos - b.pos;
    return std::abs(d.x) < (a.width + b.width) / 2.0f &&
           std::abs(d.y) < (a.height + b.height) / 2.0f;
}

void update(float dt) {
    if (!playing) {
        return;
    }

    elapsed += dt;

    const glm::vec2 size = logicalSize();
    playerShip.pos += mouseDelta;
    mouseDelta = glm::vec2(0.0f, 0.0f);
    playerShip.pos.x = glm::clamp(playerShip.pos.x, SHIP_SIZE / 2.0f, size.x - SHIP_SIZE / 2.0f);
    playerShip.pos.y = glm::clamp(playerShip.pos.y, SHIP_SIZE / 2.0f, size.y - SHIP_SIZE / 2.0f);

    playerFireTimer += dt;
    if (leftButtonDown && playerFireTimer >= PLAYER_FIRE_INTERVAL) {
        playerFireTimer -= PLAYER_FIRE_INTERVAL;
        Bullet b;
        b.shape = Circle(BULLET_RADIUS, playerShip.pos, SDL_Color{255, 0, 0, 255});
        b.shape.vel = glm::vec2(0.0f, -PLAYER_BULLET_SPEED);
        playerBullets.push_back(b);
    }

    for (auto& b : playerBullets) {
        b.shape.pos += b.shape.vel * dt;
    }
    for (auto& b : enemyBullets) {
        b.shape.pos += b.shape.vel * dt;
    }

    for (auto& e : enemies) {
        e.fireTimer += dt;
        if (e.fireTimer >= ENEMY_FIRE_INTERVAL) {
            e.fireTimer -= ENEMY_FIRE_INTERVAL;
            const glm::vec2 dir = playerShip.pos - e.shape.pos;
            const float len = glm::length(dir);
            Bullet b;
            b.shape = Circle(BULLET_RADIUS, e.shape.pos, SDL_Color{255, 0, 255, 255});
            b.shape.vel = (len > 0.0f) ? (dir / len) * ENEMY_BULLET_SPEED
                                       : glm::vec2(0.0f, ENEMY_BULLET_SPEED);
            enemyBullets.push_back(b);
        }
    }

    for (size_t i = 0; i < playerBullets.size();) {
        if (playerBullets[i].shape.pos.y + BULLET_RADIUS < 0.0f) {
            playerBullets.erase(playerBullets.begin() + i);
        } else {
            ++i;
        }
    }
    for (size_t i = 0; i < enemyBullets.size();) {
        const glm::vec2 p = enemyBullets[i].shape.pos;
        if (p.x + BULLET_RADIUS < 0.0f || p.x - BULLET_RADIUS > size.x ||
            p.y + BULLET_RADIUS < 0.0f || p.y - BULLET_RADIUS > size.y) {
            enemyBullets.erase(enemyBullets.begin() + i);
        } else {
            ++i;
        }
    }

    for (size_t i = 0; i < playerBullets.size(); ++i) {
        bool hit = false;
        for (size_t j = 0; j < enemies.size(); ++j) {
            if (circleHitsRect(playerBullets[i].shape.pos, enemies[j].shape)) {
                enemies.erase(enemies.begin() + j);
                hit = true;
                break;
            }
        }
        if (hit) {
            playerBullets.erase(playerBullets.begin() + i);
            --i;
        }
    }

    for (const auto& e : enemies) {
        if (rectsOverlap(e.shape, playerShip)) {
            endGame(false);
            return;
        }
    }
    for (const auto& b : enemyBullets) {
        if (circleHitsRect(b.shape.pos, playerShip)) {
            endGame(false);
            return;
        }
    }

    if (enemies.empty()) {
        endGame(true);
    }
}

void processEvents() {
    while (SDL_PollEvent(&currentEvent)) {
        if (currentEvent.type == SDL_EVENT_QUIT) {
            quit = true;
        }

        if (currentEvent.type == SDL_EVENT_MOUSE_BUTTON_DOWN &&
            currentEvent.button.button == SDL_BUTTON_LEFT) {
            leftButtonDown = true;
        }

        if (currentEvent.type == SDL_EVENT_MOUSE_BUTTON_UP &&
            currentEvent.button.button == SDL_BUTTON_LEFT) {
            leftButtonDown = false;
        }

        if (currentEvent.type == SDL_EVENT_MOUSE_MOTION) {
            mouseDelta += glm::vec2(currentEvent.motion.xrel,
                                    currentEvent.motion.yrel) / displayScale;
        }

        if (currentEvent.type == SDL_EVENT_KEY_DOWN) {
            switch (currentEvent.key.key) {
                case SDLK_R:
                    if (!playing) {
                        resetGame();
                    }
                    break;
                default:
                    break;
            }
        }
    }
}

void drawFrame() {
    SDL_SetRenderDrawColor(
        renderer,
        backgroundColor.r,
        backgroundColor.g,
        backgroundColor.b,
        backgroundColor.a
    );

    SDL_RenderClear(renderer);

    for (auto e : enemies) {
        e.shape.draw(renderer);
    }
    for (auto b : playerBullets) {
        b.shape.draw(renderer);
    }
    for (auto b : enemyBullets) {
        b.shape.draw(renderer);
    }
    playerShip.draw(renderer);

    SDL_RenderPresent(renderer);
}

bool initWindow() {
    if (!SDL_Init(SDL_INIT_VIDEO)) {
        SDL_Log("SDL initialization failed: %s\n", SDL_GetError());
        return false;
    }

    const SDL_DisplayID primaryDisplay = SDL_GetPrimaryDisplay();
    if (primaryDisplay != 0) {
        const float scale = SDL_GetDisplayContentScale(primaryDisplay);
        if (scale > 0.0f) {
            displayScale = scale;
        }
    }

    if (!SDL_CreateWindowAndRenderer(
            "Space Shooter",
            static_cast<int>(displayScale * WINDOW_WIDTH),
            static_cast<int>(displayScale * WINDOW_HEIGHT),
            0,
            &window,
            &renderer)) {
        SDL_Log("Failed to create window and renderer: %s\n", SDL_GetError());
        return false;
    }

    SDL_SetRenderScale(renderer, displayScale, displayScale);
    SDL_SetWindowRelativeMouseMode(window, true);

    return true;
}

void cleanup() {
    if (renderer) {
        SDL_DestroyRenderer(renderer);
        renderer = nullptr;
    }

    if (window) {
        SDL_DestroyWindow(window);
        window = nullptr;
    }

    SDL_Quit();
}

int main() {
    std::srand(static_cast<unsigned int>(std::time(nullptr)));

    if (!initWindow()) {
        std::cout << "Failed to initialize" << std::endl;
        cleanup();
        return 1;
    }

    resetGame();
    SDL_zero(currentEvent);

    Uint64 previousTicks = SDL_GetPerformanceCounter();
    const double ticksPerSecond = static_cast<double>(SDL_GetPerformanceFrequency());

    while (!quit) {
        processEvents();

        const Uint64 now = SDL_GetPerformanceCounter();
        const float dt = static_cast<float>((now - previousTicks) / ticksPerSecond);
        previousTicks = now;

        update(dt);
        drawFrame();
    }

    cleanup();
    return 0;
}
