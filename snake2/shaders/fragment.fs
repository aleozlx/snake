#version 330 core
in vec2 texCoord;
in vec2 fragTexCoord;
out vec4 FragColor;
uniform vec3 u_color;
uniform int u_shape_type; // 0 = rectangle, 1 = circle, 2 = ring, 3 = texture
uniform float u_inner_radius; // For ring shapes
uniform float u_aspect_ratio; // For correcting circle aspect ratio
uniform sampler2D u_texture; // For texture rendering
uniform bool u_use_texture; // Whether to use texture
void main() {
    if (u_shape_type == 3 || u_use_texture) {
        // Texture rendering
        vec4 texColor = texture(u_texture, fragTexCoord);
        if (texColor.a < 0.1) discard; // Alpha testing for transparency
        FragColor = texColor;
    } else if (u_shape_type == 0) {
        // Rectangle (default behavior)
        FragColor = vec4(u_color, 1.0);
    } else if (u_shape_type == 1) {
        // Circle with anti-aliasing and aspect ratio correction
        // Convert texture coordinates from [0,1] to [-1,1] centered
        vec2 uv = (texCoord - 0.5) * 2.0;
        
        // Apply aspect ratio correction to make perfect circles
        uv.y *= u_aspect_ratio;
        
        // Calculate distance from center
        float dist = length(uv);
        
        // Create smooth circle with anti-aliasing
        float radius = 1.0;
        float smoothness = 0.1;
        float alpha = 1.0 - smoothstep(radius - smoothness, radius + smoothness, dist);
        
        // Discard fragments outside the circle for performance
        if (alpha < 0.01) discard;
        
        FragColor = vec4(u_color, alpha);
    } else if (u_shape_type == 2) {
        // Ring (hollow circle) with aspect ratio correction
        vec2 uv = (texCoord - 0.5) * 2.0;
        uv.y *= u_aspect_ratio; // Apply aspect ratio correction
        float dist = length(uv);
        
        float outerRadius = 1.0;
        float innerRadius = u_inner_radius * 2.0;
        float smoothness = 0.1;
        
        float outerAlpha = 1.0 - smoothstep(outerRadius - smoothness, outerRadius + smoothness, dist);
        float innerAlpha = smoothstep(innerRadius - smoothness, innerRadius + smoothness, dist);
        
        float alpha = outerAlpha * innerAlpha;
        
        // Discard fragments outside the ring
        if (alpha < 0.01) discard;
        
        FragColor = vec4(u_color, alpha);
    }
} 