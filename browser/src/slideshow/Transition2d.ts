/* -*- js-indent-level: 8 -*- */

/*
 * Copyright the Collabora Online contributors.
 *
 * SPDX-License-Identifier: MPL-2.0
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

declare var SlideShow: any;

class Transition2d extends SlideShow.TransitionBase {
	constructor(transitionParameters: TransitionParameters) {
		super(transitionParameters);
	}

	public getVertexShader(): string {
		return `#version 300 es
				in vec4 a_position;
				in vec2 a_texCoord;
				out vec2 v_texCoord;

				void main() {
					gl_Position = a_position;
					v_texCoord = a_texCoord;
				}
				`;
	}

	public getFragmentShader(): string {
		return `#version 300 es
				precision mediump float;

				uniform sampler2D leavingSlideTexture;
				uniform sampler2D enteringSlideTexture;
				uniform float time;

				in vec2 v_texCoord;
				out vec4 outColor;

				void main() {
					vec4 color0 = texture(leavingSlideTexture, v_texCoord);
					vec4 color1 = texture(enteringSlideTexture, v_texCoord);
					outColor = mix(color0, color1, time);
				}
				`;
	}

	public render() {
		if (!this.startTime) this.startTime = performance.now();
		this.time =
			(performance.now() - this.startTime) /
			(this.animationTime > 0 ? this.animationTime : 1500);

		if (this.time > 1) this.time = 1;
		const gl = this.gl;

		gl.viewport(0, 0, this.context.canvas.width, this.context.canvas.height);
		gl.clearColor(0.0, 0.0, 0.0, 1.0);
		gl.clear(gl.COLOR_BUFFER_BIT);

		gl.useProgram(this.program);
		gl.uniform1f(gl.getUniformLocation(this.program, 'time'), this.time);

		gl.activeTexture(gl.TEXTURE0);
		gl.bindTexture(gl.TEXTURE_2D, this.transitionParameters.current);
		gl.uniform1i(gl.getUniformLocation(this.program, 'leavingSlideTexture'), 0);

		gl.activeTexture(gl.TEXTURE1);
		gl.bindTexture(gl.TEXTURE_2D, this.transitionParameters.next);
		gl.uniform1i(
			gl.getUniformLocation(this.program, 'enteringSlideTexture'),
			1,
		);

		this.renderUniformValue();

		gl.bindVertexArray(this.vao);
		gl.drawArrays(gl.TRIANGLE_STRIP, 0, 4);

		if (!this.skip && this.time < 1) {
			requestAnimationFrame(this.render.bind(this));
		} else {
			this.finishTransition();
		}
	}
}

SlideShow.Transition2d = Transition2d;
