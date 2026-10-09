// foghoge: an ASCII horror JRPG. Glyphs live at free sub-pixel positions,
// not on a terminal grid, and are lit by a post-process lantern shader.

#include "raylib.h"

#include <algorithm>
#include <cmath>
#include <random>
#include <string>
#include <vector>

namespace {

constexpr int kStartWidth = 1280;
constexpr int kStartHeight = 720;
constexpr int kFontBaseSize = 64;   // rasterised once; drawn smaller with bilinear filtering
constexpr float kLanternRadius = 300.0f;

// Every codepoint the game may draw. Anything not listed here renders as '?'.
std::vector<int> BuildCodepoints() {
    std::vector<int> cps;
    for (int c = 0x20; c <= 0x7E; ++c) cps.push_back(c);       // ASCII
    for (int c = 0x2500; c <= 0x257F; ++c) cps.push_back(c);   // box drawing
    for (int c = 0x2580; c <= 0x259F; ++c) cps.push_back(c);   // block elements
    for (int c = 0x25A0; c <= 0x25FF; ++c) cps.push_back(c);   // geometric shapes
    for (int c : {0x00B7, 0x00B0, 0x2020, 0x2021, 0x2022, 0x2026, 0x2219, 0x2234, 0x2235,
                  0x2620, 0x263D, 0x263E, 0x2666, 0x2669, 0x266A, 0x2691, 0x2694, 0x26B0, 0x26B1}) {
        cps.push_back(c);
    }
    return cps;
}

std::string ResolveData(const char* rel) {
    std::string beside = std::string(GetApplicationDirectory()) + rel;
    return FileExists(beside.c_str()) ? beside : std::string(rel);
}

float Rand(std::mt19937& rng, float lo, float hi) {
    return std::uniform_real_distribution<float>(lo, hi)(rng);
}

template <typename T>
const T& Pick(std::mt19937& rng, const std::vector<T>& v) {
    return v[std::uniform_int_distribution<size_t>(0, v.size() - 1)(rng)];
}

Color Shade(unsigned char r, unsigned char g, unsigned char b, float a) {
    return Color{r, g, b, static_cast<unsigned char>(std::clamp(a, 0.0f, 1.0f) * 255.0f)};
}

// A glyph that is placed once and only wobbles a little.
struct Fixture {
    int cp;
    Vector2 pos;
    float size;
    Color color;
    float phase;
    float wobble;   // pixels of idle sway
};

// A glyph that drifts across the room and wraps at the edges.
struct Drifter {
    int cp;
    Vector2 pos;
    Vector2 vel;
    float size;
    Color color;
    float phase;
};

// A pair of eyes in the dark. Drawn after the lighting pass so they glow,
// and they close when the lantern gets close.
struct Watcher {
    Vector2 pos;
    float spacing;
    float blinkAt;
    float open;     // 0 closed, 1 open
};

struct Ember {
    Vector2 pos;
    Vector2 vel;
    float life;
    float maxLife;
};

struct World {
    std::vector<Fixture> fixtures;
    std::vector<Drifter> fog;
    std::vector<Drifter> motes;
    std::vector<Watcher> watchers;
    std::vector<Ember> embers;
};

// Lay out a ruined chamber in box-drawing characters, slightly out of true.
void BuildChamber(World& w, std::mt19937& rng, float width, float height) {
    const float cell = 22.0f;
    const float left = width * 0.18f, right = width * 0.82f;
    const float top = height * 0.20f, bottom = height * 0.80f;
    const Color stone = Shade(150, 140, 125, 1.0f);

    auto wall = [&](int cp, float x, float y) {
        if (Rand(rng, 0, 1) < 0.12f) return;   // crumbled away
        float jitter = 1.6f;
        w.fixtures.push_back({cp, {x + Rand(rng, -jitter, jitter), y + Rand(rng, -jitter, jitter)},
                              24.0f, stone, Rand(rng, 0, 6.28f), 0.3f});
    };

    for (float x = left + cell; x < right; x += cell * 0.62f) {
        wall(0x2550, x, top);      // ═
        wall(0x2550, x, bottom);
    }
    for (float y = top + cell; y < bottom; y += cell) {
        wall(0x2551, left, y);     // ║
        wall(0x2551, right, y);
    }
    w.fixtures.push_back({0x2554, {left, top}, 24.0f, stone, 0, 0.2f});      // ╔
    w.fixtures.push_back({0x2557, {right, top}, 24.0f, stone, 0, 0.2f});     // ╗
    w.fixtures.push_back({0x255A, {left, bottom}, 24.0f, stone, 0, 0.2f});   // ╚
    w.fixtures.push_back({0x255D, {right, bottom}, 24.0f, stone, 0, 0.2f});  // ╝

    // Rubble heaped where the walls gave way.
    const std::vector<int> rubble = {0x2591, 0x2592, 0x2593, 0x2580, 0x2584, 0x258C, 0x2590};
    for (int i = 0; i < 70; ++i) {
        bool horizontal = Rand(rng, 0, 1) < 0.5f;
        float x = horizontal ? Rand(rng, left, right) : (Rand(rng, 0, 1) < 0.5f ? left : right);
        float y = horizontal ? (Rand(rng, 0, 1) < 0.5f ? top : bottom) : Rand(rng, top, bottom);
        x += Rand(rng, -18, 18);
        y += Rand(rng, -14, 14);
        w.fixtures.push_back({Pick(rng, rubble), {x, y}, Rand(rng, 12, 20),
                              Shade(110, 100, 92, Rand(rng, 0.4f, 0.9f)), Rand(rng, 0, 6.28f), 0.0f});
    }

    // Floor grit: scattered, not gridded.
    const std::vector<int> grit = {'.', ',', '\'', '`', 0x00B7, ':', ';'};
    for (int i = 0; i < 420; ++i) {
        w.fixtures.push_back({Pick(rng, grit), {Rand(rng, left + 12, right - 12), Rand(rng, top + 12, bottom - 12)},
                              Rand(rng, 12, 18), Shade(120, 115, 105, Rand(rng, 0.25f, 0.7f)),
                              Rand(rng, 0, 6.28f), 0.0f});
    }

    // Set dressing.
    const float cx = (left + right) * 0.5f, cy = (top + bottom) * 0.5f;
    w.fixtures.push_back({0x26B0, {cx - 12, cy - 18}, 40.0f, Shade(170, 150, 120, 1.0f), 0, 0.0f});  // ⚰
    w.fixtures.push_back({0x2620, {right - 70, bottom - 60}, 26.0f, Shade(210, 205, 190, 1.0f), 0, 0.0f});  // ☠
    w.fixtures.push_back({0x2020, {left + 50, top + 40}, 28.0f, Shade(160, 150, 140, 1.0f), 0, 0.0f});  // †
    w.fixtures.push_back({0x2020, {left + 90, top + 52}, 22.0f, Shade(160, 150, 140, 1.0f), 0, 0.0f});
    for (int i = 0; i < 6; ++i) {   // candle stubs, swaying
        w.fixtures.push_back({'i', {cx - 120 + i * 48.0f + Rand(rng, -6, 6), cy + 70 + Rand(rng, -6, 6)},
                              20.0f, Shade(230, 200, 150, 1.0f), Rand(rng, 0, 6.28f), 1.2f});
    }

    // Writing on the floor.
    const char* scrawl = "IT  WAS  NOT  THE  DARK  THAT  ATE  THEM";
    float sx = cx - 200;
    for (const char* p = scrawl; *p; ++p, sx += 10.5f) {
        if (*p == ' ') continue;
        w.fixtures.push_back({*p, {sx + Rand(rng, -1.5f, 1.5f), cy + 120 + Rand(rng, -3, 3)}, 16.0f,
                              Shade(140, 40, 35, 0.85f), Rand(rng, 0, 6.28f), 0.0f});
    }
}

World BuildWorld(float width, float height) {
    std::mt19937 rng(0xF06);
    World w;
    BuildChamber(w, rng, width, height);

    const std::vector<int> fogGlyphs = {0x2591, 0x2591, 0x2592, '~'};
    for (int i = 0; i < 160; ++i) {
        w.fog.push_back({Pick(rng, fogGlyphs), {Rand(rng, -80, width + 80), Rand(rng, -40, height + 40)},
                         {Rand(rng, 8, 26), Rand(rng, -3, 3)}, Rand(rng, 28, 72),
                         Shade(150, 165, 180, Rand(rng, 0.05f, 0.16f)), Rand(rng, 0, 6.28f)});
    }

    const std::vector<int> moteGlyphs = {0x00B7, '.', 0x2219, '\''};
    for (int i = 0; i < 90; ++i) {
        w.motes.push_back({Pick(rng, moteGlyphs), {Rand(rng, 0, width), Rand(rng, 0, height)},
                           {Rand(rng, -6, 6), Rand(rng, -10, -2)}, Rand(rng, 10, 16),
                           Shade(200, 190, 170, Rand(rng, 0.2f, 0.6f)), Rand(rng, 0, 6.28f)});
    }

    const Vector2 eyeSpots[] = {{width * 0.07f, height * 0.30f}, {width * 0.93f, height * 0.62f},
                                {width * 0.40f, height * 0.08f}, {width * 0.64f, height * 0.93f},
                                {width * 0.10f, height * 0.88f}};
    for (Vector2 p : eyeSpots) {
        w.watchers.push_back({p, Rand(rng, 14, 22), Rand(rng, 1, 6), 1.0f});
    }
    return w;
}

void Wrap(Vector2& p, float width, float height, float margin) {
    if (p.x > width + margin) p.x -= width + margin * 2;
    if (p.x < -margin) p.x += width + margin * 2;
    if (p.y > height + margin) p.y -= height + margin * 2;
    if (p.y < -margin) p.y += height + margin * 2;
}

}  // namespace

int main(int argc, char** argv) {
    // --shot <file.png>: render a few seconds with the lantern parked, save a frame, quit.
    const char* shotPath = nullptr;
    for (int i = 1; i + 1 < argc; ++i) {
        if (std::string(argv[i]) == "--shot") shotPath = argv[i + 1];
    }

    SetConfigFlags(FLAG_WINDOW_RESIZABLE | FLAG_VSYNC_HINT | FLAG_MSAA_4X_HINT);
    InitWindow(kStartWidth, kStartHeight, "foghoge");
    SetTargetFPS(60);
    HideCursor();

    std::vector<int> codepoints = BuildCodepoints();
    const std::string fontPath = ResolveData("assets/fonts/DejaVuSansMono.ttf");
    // Block and box glyphs overshoot the em box; raylib warns per glyph but renders them fine.
    SetTraceLogLevel(LOG_ERROR);
    Font font = LoadFontEx(fontPath.c_str(), kFontBaseSize, codepoints.data(), static_cast<int>(codepoints.size()));
    SetTraceLogLevel(LOG_INFO);
    GenTextureMipmaps(&font.texture);
    SetTextureFilter(font.texture, TEXTURE_FILTER_TRILINEAR);

    const std::string shaderPath = ResolveData("shaders/lantern.fs");
    Shader lantern = LoadShader(nullptr, shaderPath.c_str());
    const int locResolution = GetShaderLocation(lantern, "resolution");
    const int locLight = GetShaderLocation(lantern, "lightPos");
    const int locTime = GetShaderLocation(lantern, "time");
    const int locRadius = GetShaderLocation(lantern, "lightRadius");

    int width = GetScreenWidth();
    int height = GetScreenHeight();
    RenderTexture2D scene = LoadRenderTexture(width, height);
    World world = BuildWorld(static_cast<float>(width), static_cast<float>(height));

    Vector2 lanternPos = {width * 0.5f, height * 0.5f};
    Vector2 heroPos = lanternPos;
    std::mt19937 fx(1234);
    float emberTimer = 0.0f;

    while (!WindowShouldClose()) {
        const float dt = GetFrameTime();
        const float t = static_cast<float>(GetTime());

        if (IsWindowResized()) {
            width = GetScreenWidth();
            height = GetScreenHeight();
            UnloadRenderTexture(scene);
            scene = LoadRenderTexture(width, height);
            world = BuildWorld(static_cast<float>(width), static_cast<float>(height));
        }

        // --- update ----------------------------------------------------------
        const Vector2 mouse = shotPath ? Vector2{width * 0.42f, height * 0.55f} : GetMousePosition();
        lanternPos.x += (mouse.x - lanternPos.x) * std::min(1.0f, dt * 14.0f);
        lanternPos.y += (mouse.y - lanternPos.y) * std::min(1.0f, dt * 14.0f);
        // The hero trails the lantern they are holding out in front of them.
        heroPos.x += (lanternPos.x - 22.0f - heroPos.x) * std::min(1.0f, dt * 4.0f);
        heroPos.y += (lanternPos.y + 26.0f - heroPos.y) * std::min(1.0f, dt * 4.0f);

        for (Drifter& d : world.fog) {
            d.pos.x += (d.vel.x + std::sin(t * 0.3f + d.phase) * 6.0f) * dt;
            d.pos.y += (d.vel.y + std::cos(t * 0.21f + d.phase) * 4.0f) * dt;
            Wrap(d.pos, static_cast<float>(width), static_cast<float>(height), 90.0f);
        }
        for (Drifter& m : world.motes) {
            m.pos.x += (m.vel.x + std::sin(t * 1.3f + m.phase) * 8.0f) * dt;
            m.pos.y += m.vel.y * dt;
            Wrap(m.pos, static_cast<float>(width), static_cast<float>(height), 20.0f);
        }

        emberTimer -= dt;
        while (emberTimer <= 0.0f) {
            emberTimer += 0.05f;
            float life = Rand(fx, 0.6f, 1.6f);
            world.embers.push_back({{lanternPos.x + Rand(fx, -4, 4), lanternPos.y - 6},
                                    {Rand(fx, -14, 14), Rand(fx, -60, -25)}, life, life});
        }
        for (Ember& e : world.embers) {
            e.pos.x += (e.vel.x + std::sin(t * 6.0f + e.maxLife * 10.0f) * 10.0f) * dt;
            e.pos.y += e.vel.y * dt;
            e.life -= dt;
        }
        std::erase_if(world.embers, [](const Ember& e) { return e.life <= 0.0f; });

        for (Watcher& wch : world.watchers) {
            float dist = std::hypot(wch.pos.x - lanternPos.x, wch.pos.y - lanternPos.y);
            float target = dist < kLanternRadius * 1.1f ? 0.0f : 1.0f;   // they hide from the light
            if (t > wch.blinkAt) {
                target = 0.0f;
                if (t > wch.blinkAt + 0.14f) wch.blinkAt = t + Rand(fx, 2.0f, 7.0f);
            }
            wch.open += (target - wch.open) * std::min(1.0f, dt * 10.0f);
        }

        // --- draw the unlit scene -------------------------------------------
        BeginTextureMode(scene);
        ClearBackground(Color{8, 7, 10, 255});

        for (const Drifter& d : world.fog) {
            DrawTextCodepoint(font, d.cp, d.pos, d.size, d.color);
        }
        for (const Fixture& f : world.fixtures) {
            Vector2 p = f.pos;
            if (f.wobble > 0.0f) {
                p.x += std::sin(t * 1.7f + f.phase) * f.wobble;
                p.y += std::cos(t * 1.3f + f.phase) * f.wobble * 0.5f;
            }
            DrawTextCodepoint(font, f.cp, p, f.size, f.color);
        }
        for (const Drifter& m : world.motes) {
            DrawTextCodepoint(font, m.cp, m.pos, m.size, m.color);
        }

        DrawTextCodepoint(font, '@', {heroPos.x - 9, heroPos.y - 14}, 30.0f, Color{235, 225, 205, 255});
        DrawTextCodepoint(font, 0x25CA /* ◊ */, {lanternPos.x - 7, lanternPos.y - 11}, 22.0f, Color{255, 220, 150, 255});

        for (const Ember& e : world.embers) {
            float k = e.life / e.maxLife;
            DrawTextCodepoint(font, k > 0.5f ? '*' : 0x00B7, e.pos, 10.0f + 6.0f * k,
                      Shade(255, static_cast<unsigned char>(140 + 80 * k), 60, k));
        }
        EndTextureMode();

        // --- light it --------------------------------------------------------
        BeginDrawing();
        ClearBackground(BLACK);

        const float resolution[2] = {static_cast<float>(width), static_cast<float>(height)};
        const float light[2] = {lanternPos.x, lanternPos.y};
        SetShaderValue(lantern, locResolution, resolution, SHADER_UNIFORM_VEC2);
        SetShaderValue(lantern, locLight, light, SHADER_UNIFORM_VEC2);
        SetShaderValue(lantern, locTime, &t, SHADER_UNIFORM_FLOAT);
        SetShaderValue(lantern, locRadius, &kLanternRadius, SHADER_UNIFORM_FLOAT);

        BeginShaderMode(lantern);
        // Render textures are stored bottom-up, so flip the source rect.
        DrawTextureRec(scene.texture, Rectangle{0, 0, static_cast<float>(width), -static_cast<float>(height)},
                       Vector2{0, 0}, WHITE);
        EndShaderMode();

        // Eyes glow on top of the darkness, untouched by the lantern.
        for (const Watcher& wch : world.watchers) {
            if (wch.open < 0.05f) continue;
            Color eye = Shade(200, 30, 25, wch.open * (0.65f + 0.35f * std::sin(t * 0.8f + wch.pos.x)));
            float squint = 18.0f * wch.open;
            DrawTextCodepoint(font, wch.open > 0.5f ? 0x25CF : '-', {wch.pos.x - wch.spacing, wch.pos.y}, squint, eye);
            DrawTextCodepoint(font, wch.open > 0.5f ? 0x25CF : '-', {wch.pos.x + wch.spacing, wch.pos.y}, squint, eye);
        }

        const char* hint = "move the lantern. the fog remembers your name.";
        Vector2 hs = MeasureTextEx(font, hint, 16, 1);
        DrawTextEx(font, hint, {(width - hs.x) * 0.5f, height - 34.0f}, 16, 1, Shade(120, 110, 100, 0.55f));

        EndDrawing();

        if (shotPath && t > 3.0f) {
            TakeScreenshot(shotPath);
            break;
        }
    }

    UnloadRenderTexture(scene);
    UnloadShader(lantern);
    UnloadFont(font);
    CloseWindow();
    return 0;
}
