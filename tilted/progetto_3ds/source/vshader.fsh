#version 120

varying vec2 v_texcoord;
uniform sampler2D tex;

void main() {
    // Se non hai una texture, sostituisci con: gl_FragColor = vec4(1.0, 1.0, 1.0, 1.0);
    gl_FragColor = texture2D(tex, v_texcoord);
}