#version 450

layout(location = 0) in vec2 aPosition;
layout(location = 1) in vec4 aColor;  // R8G8B8A8_UNORM otomatik [0,1]'e normalize edilir

/// Push constant bloğu — tüm per-draw verisi buradan taşınır.
layout(push_constant) uniform PushConstants {
    mat4 uTransform;   // Projeksiyon × model (column-major), CPU'da çarpılır
    vec4 uTintColor;   // DrawTriangles color parametresi [0,1]
    float uOpacity;    // Global opaklık [0,1]
} pc;

layout(location = 0) out vec4 vColor;

void main() {
    gl_Position = pc.uTransform * vec4(aPosition, 0.0, 1.0);
    // Vertex rengi ile tint çarpılır; opacity alpha'ya uygulanır.
    vColor = aColor * pc.uTintColor * vec4(1.0, 1.0, 1.0, pc.uOpacity);
}
