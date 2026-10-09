#version 330

// Post-process pass over the glyph scene: lantern falloff around the cursor,
// flicker, vignette and film grain.

in vec2 fragTexCoord;
in vec4 fragColor;

uniform sampler2D texture0;
uniform vec2 resolution;   // screen size in pixels
uniform vec2 lightPos;     // lantern position, pixels, top-left origin
uniform float time;        // seconds
uniform float lightRadius; // base radius of the lantern pool, pixels

out vec4 finalColor;

float hash(vec2 p) {
    p = fract(p * vec2(123.34, 456.21));
    p += dot(p, p + 45.32);
    return fract(p.x * p.y);
}

float noise1(float x) {
    float i = floor(x);
    float f = fract(x);
    float a = hash(vec2(i, 0.0));
    float b = hash(vec2(i + 1.0, 0.0));
    return mix(a, b, f * f * (3.0 - 2.0 * f));
}

void main() {
    // fragTexCoord.y runs bottom-up because the render texture is drawn flipped.
    vec2 uv = vec2(fragTexCoord.x, 1.0 - fragTexCoord.y);
    vec2 px = uv * resolution;

    vec3 scene = texture(texture0, fragTexCoord).rgb;

    // Lantern flicker: slow breathing plus a fast, irregular gutter.
    float flicker = 0.88
        + 0.06 * sin(time * 2.3)
        + 0.08 * (noise1(time * 9.0) - 0.5)
        + 0.05 * (noise1(time * 23.0) - 0.5);

    float radius = lightRadius * flicker;
    float d = distance(px, lightPos) / radius;
    float light = clamp(1.0 - d, 0.0, 1.0);
    light = light * light * (3.0 - 2.0 * light);      // smooth edge
    float core = exp(-d * d * 6.0);                     // hot centre

    vec3 warm = vec3(1.00, 0.72, 0.42);
    vec3 cold = vec3(0.10, 0.14, 0.22);                 // moonless ambient
    vec3 lit = scene * (cold * 0.55 + warm * (light * 1.25 + core * 0.35) * flicker);

    // A faint halo so the darkness around the lantern isn't pure black.
    lit += warm * 0.025 * light * flicker;

    // Vignette.
    vec2 c = uv - 0.5;
    c.x *= resolution.x / resolution.y;
    float vig = smoothstep(1.05, 0.25, length(c));
    lit *= mix(0.35, 1.0, vig);

    // Film grain, re-seeded ~24 times a second like real stock.
    float frame = floor(time * 24.0);
    float g = hash(px + frame * 17.13) - 0.5;
    lit += g * 0.06;

    finalColor = vec4(max(lit, 0.0), 1.0);
}
