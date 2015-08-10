/*
 * FragmentShader.h
 *
 *  Created on: 2015年8月10日
 *      Author: danielzhang
 */

#ifndef NATIVEPLAYER_FRAGMENTSHADER_H_
#define NATIVEPLAYER_FRAGMENTSHADER_H_

static const char gFragmentShader[] = "varying lowp vec2 textureOut;\n"
		"uniform sampler2D tex_y;\n"
		"uniform sampler2D tex_u;\n"
		"uniform sampler2D tex_v;\n"
		"void main(void)\n"
		"{\n"
		"mediump vec3 yuv;\n"
		"lowp vec3 rgb;\n"
		"yuv.x = texture2D(tex_y, textureOut).r;\n"
		"yuv.y = texture2D(tex_u, textureOut).r - 0.5;\n"
		"yuv.z = texture2D(tex_v, textureOut).r - 0.5;\n"
		"rgb = mat3(1,1,1,\n"
		"0,-0.39465,2.03211,\n"
		"1.13983,-0.58060,0) * yuv;\n"
		"gl_FragColor = vec4(rgb, 1);\n"
		"}\n";

#endif /* NATIVEPLAYER_FRAGMENTSHADER_H_ */
