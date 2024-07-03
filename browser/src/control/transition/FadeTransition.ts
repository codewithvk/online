class FadeTransition extends Transition2d {
    private effectTransition : number = 0;
    constructor(canvas: HTMLCanvasElement, vertexShaderSource: string, fragmentShaderSource: string, image1: HTMLImageElement, image2: HTMLImageElement) {
        super(canvas, vertexShaderSource, fragmentShaderSource, image1, image2);
        this.prepareTransition();
        this.animationTime = 1500;  
    }

    public renderUniformValue(): void {
        this.gl.uniform1i(this.gl.getUniformLocation(this.program, 'effectType'), this.effectTransition);
    }

    public start(selectedEffectType : number): void {
        this.effectTransition = selectedEffectType;
        this.startTransition();
    }
}


app.definitions.FadeTransition = function(canvas: HTMLCanvasElement, image1: HTMLImageElement, image2: HTMLImageElement) {
    const vertexShaderSource = `#version 300 es
in vec4 a_position;
in vec2 a_texCoord;
out vec2 v_texCoord;

void main() {
    gl_Position = a_position;
    v_texCoord = a_texCoord;
}
`

    const fragmentShaderSource = `#version 300 es
precision mediump float;

uniform sampler2D leavingSlideTexture;
uniform sampler2D enteringSlideTexture;
uniform float time;
uniform int effectType; // 0: Fade through black, 1: Fade through white, 2: Smooth fade

in vec2 v_texCoord;
out vec4 outColor;

void main() {
    vec4 color0 = texture(leavingSlideTexture, v_texCoord);
    vec4 color1 = texture(enteringSlideTexture, v_texCoord);
    vec4 transitionColor;

    if (effectType == 0) {
        // Fade through black
        transitionColor = vec4(0.0, 0.0, 0.0, 1.0);
    } else if (effectType == 1) {
        // Fade through white
        transitionColor = vec4(1.0, 1.0, 1.0, 1.0);
    }

    if (effectType == 2) {
        // Smooth fade
        float smoothTime = smoothstep(0.0, 1.0, time);
        outColor = mix(color0, color1, smoothTime);
    } else {
        if (time < 0.5) {
            outColor = mix(color0, transitionColor, time * 2.0);
        } else {
            outColor = mix(transitionColor, color1, (time - 0.5) * 2.0);
        }
    }
}`

    return new FadeTransition(canvas, vertexShaderSource, fragmentShaderSource, image1, image2);
}
