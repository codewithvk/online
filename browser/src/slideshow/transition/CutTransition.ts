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

class CutTransition extends SlideShow.Transition2d {
	constructor(
		canvas: HTMLCanvasElement,
		image1: HTMLImageElement,
		image2: HTMLImageElement,
		slideInfo: SlideInfo,
	) {
		super(canvas, image1, image2);
		this.prepareTransition();
		this.animationTime =
			slideInfo?.transitionDuration > 0 ? slideInfo.transitionDuration : 2000;
	}

	public start(): void {
		this.startTransition();
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
                    if (time < 1.0) {
                        outColor =  vec4(0.0f, 0.0f, 0.0f, 1.0f);
                    } else {
                        outColor = texture(enteringSlideTexture, v_texCoord);
                    }
                }
                `;
	}
}

SlideShow.CutTransition = CutTransition;
