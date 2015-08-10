/*
 * VertexShader.h
 *
 *  Created on: 2015年8月10日
 *      Author: danielzhang
 */

#ifndef NATIVEPLAYER_VERTEXSHADER_H_
#define NATIVEPLAYER_VERTEXSHADER_H_

static const char gVertexShader[] = "attribute vec4 vertexIn;\n"
		"attribute vec2 textureIn;\n"
		"varying vec2 textureOut;\n"
		"void main(void)\n"
		"{\n"
		"gl_Position = vertexIn;\n"
		"textureOut = textureIn;\n"
		"}\n";

#endif /* NATIVEPLAYER_VERTEXSHADER_H_ */
