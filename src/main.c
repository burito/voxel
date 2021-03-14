/*
Copyright (c) 2013 Daniel Burke

This software is provided 'as-is', without any express or implied
warranty. In no event will the authors be held liable for any damages
arising from the use of this software.

Permission is granted to anyone to use this software for any purpose,
including commercial applications, and to alter it and redistribute it
freely, subject to the following restrictions:

   1. The origin of this software must not be misrepresented; you must not
   claim that you wrote the original software. If you use this software
   in a product, an acknowledgment in the product documentation would be
   appreciated but is not required.

   2. Altered source versions must be plainly marked as such, and must not be
   misrepresented as being the original software.

   3. This notice may not be removed or altered from any source
   distribution.
*/
#ifdef __APPLE__
#define GL_SILENCE_DEPRECATION
#include <OpenGL/gl.h>
#else
#include <GL/glew.h>
#endif

#include <stdio.h>
#include <math.h>

#include "global.h"
#include "3dmaths.h"
#include "log.h"
#include "vr.h"
#include "fps_movement.h"
#include "shader.h"
#include "mesh_gl.h"
#include "spacemouse.h"

#include "gui.h"
#include "clvoxel.h"
#include "gpuinfo.h"

#include "fluidtest.h"

void gfx_init(void);
void gfx_end(void);
void gfx_swap(void);

long long time_start = 0;
float time = 0;

float step = 0.0f;

struct GLSLSHADER *mesh_shader;


void GLAPIENTRY glerror_callback( GLenum source, GLenum type, GLuint id,
 GLenum severity, GLsizei length, const GLchar* message, const void* userParam )
{
  log_error("GL CALLBACK: %s type = 0x%x, severity = 0x%x, message = %s",
           ( type == GL_DEBUG_TYPE_ERROR ? "** GL ERROR **" : "" ),
             type, severity, message );
}

int main_init(int argc, char *argv[])
{
	int gl_error;
	gfx_init();
	log_info("GL Vendor   : %s", glGetString(GL_VENDOR) );
	log_info("GL Renderer : %s", glGetString(GL_RENDERER) );
	log_info("GL Driver   : %s", glGetString(GL_VERSION) );
	log_info("SL Version  : %s", glGetString(GL_SHADING_LANGUAGE_VERSION) );

	// During init, enable debug output
	glEnable              ( GL_DEBUG_OUTPUT );
//	glDebugMessageCallback( glerror_callback, 0 );

	int gl_major_version = 0;
	int gl_minor_version = 0;
	glGetIntegerv(GL_MAJOR_VERSION, &gl_major_version);
	glGetIntegerv(GL_MAJOR_VERSION, &gl_minor_version);
	log_info("glGetIntVer : %d.%d", gl_major_version, gl_minor_version);

#ifndef __APPLE__
	if(!GLEW_VERSION_4_3)
	{
		log_fatal("OpenGL 4.3 Required");
		return 1;
	}
	gpuinfo_init();
#endif

	while( (gl_error = glGetError()) != GL_NO_ERROR )
	{
		log_warning("glGetError() = %d:%s", gl_error, glError(gl_error));
	}

	mesh_shader = shader_load(
		"data/shaders/default.vert",
		"data/shaders/default.frag" );
	shader_uniform(mesh_shader, "modelview");
	shader_uniform(mesh_shader, "projection");

	while( (gl_error = glGetError()) != GL_NO_ERROR )
	{
		log_warning("glGetError() = %d:%s", gl_error, glError(gl_error));
	}


	GLfloat mat_specular[] = { 1.0, 1.0, 1.0, 1.0 };
	GLfloat mat_shininess[] = { 50.0 };
	GLfloat light_position[] = { -100.0, -100.0, -100.0, 0.0 };
	glMaterialfv(GL_FRONT, GL_SPECULAR, mat_specular);
	glMaterialfv(GL_FRONT, GL_SHININESS, mat_shininess);
	glEnable(GL_LIGHT0);
	glLightfv(GL_LIGHT0, GL_POSITION, light_position);
	while( (gl_error = glGetError()) != GL_NO_ERROR )
	{
		log_warning("glGetError() = %d:%s", gl_error, glError(gl_error));
	}

	time_start = sys_time();
	voxel_init();

	while( (gl_error = glGetError()) != GL_NO_ERROR )
	{
		log_warning("glGetError() = %d:%s", gl_error, glError(gl_error));
	}

	fluidtest_init();

//	vr_init();
	spacemouse_init();

	while( (gl_error = glGetError()) != GL_NO_ERROR )
	{
		log_warning("glGetError() = %d:%s", gl_error, glError(gl_error));
	}


	time_start = sys_time();
	int ret =  gui_init(argc, argv);
	if(!ret)
	{
		log_info("Initialised : OK");
	}
	return ret;
}

void main_end(void)
{
	voxel_end();
	gui_end();

	spacemouse_shutdown();
	if(vr_using)
	{
		vr_end();
	}
	gfx_end();
	log_info("Shutdown    : OK");
}


int p_swim = 0;

// last digit of angle is x-fov, in radians
//vec4 position = {{0.619069, 0.644001, 1.457747, 0.0}};
//vec4 angle = {{-0.126000, -3.261001, 0.000000, M_PI*0.5}};

// fluid pos
vec4 position = {{1.636183, -0.490000, 1.073543, 0.0}};
vec4 angle = {{0.021000, 0.993001, 0.000000, M_PI*0.5}};

// glFrustum fluid pos
//vec4 position = {{1.562702, -0.869279, 3.507709, 0.0}};
//vec4 angle = {{-0.213255, 11.815352, 0.000000, M_PI*0.5}};

void render(mat4x4 view, mat4x4 projection)
{
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	glEnable(GL_DEPTH_TEST);
	glDepthFunc(GL_LESS);
	glDepthRangef( 0.1f, 30.0f);

	mat4x4 model = mat4x4_identity();
	model = mul( model, mat4x4_rot_y(step) );		// rotate the bunny
	model = mul( model, mat4x4_translate_float(-0.5, 0, -0.5) ); // around it's own origin
	model = mul( mat4x4_translate_float( 0, 0, -2), model );	// move it 2 metres infront of the origin

	model = mul(mat4x4_translate_vec3( position.xyz ), model);	// move to player position
	model = mul(mat4x4_rot_y(-angle.y ), model);
	model = mul(mat4x4_rot_x(-angle.x ), model);

	mat4x4 modelview = mul( view, model);

	glUseProgram(mesh_shader->program);
	glUniformMatrix4fv(mesh_shader->uniforms[0], 1, GL_TRUE, modelview.f);
	glUniformMatrix4fv(mesh_shader->uniforms[1], 1, GL_TRUE, projection.f);

	fluidtest_draw(modelview, projection);
	voxel_loop(modelview, projection);

//	glDrawElements( GL_LINES, (sim->cells+1)*(sim->cells+1), GL_UNSIGNED_INT, 0 );
	glBindVertexArray( 0 );

	glUseProgram(0);
}

void main_loop(void)
{
	glClearColor(0.2,0.2,0.3,1.0);
	angle.z = 0;
	spacemouse_tick();
	fluidtest_tick();

#ifndef __APPLE__
	gpuinfo_tick();
#endif

	if(!vr_using)
	{
		mat4x4 projection;
		projection = mat4x4_perspective(0.1, 300, ((float)vid_width / (float)vid_height)*0.1, 0.1);
//		projection = mat4x4_orthographic(0.1, 30, 1, (float)vid_height / (float)vid_width);
//		float ratio = (float)vid_height / (float)vid_width;
//		projection = mat4x4_glfrustum(-0.5, 0.5, -(ratio*0.5), ratio*0.5, 1, 30);
		mat4x4 view = mat4x4_translate_float(0, 0, 0); // move the camera 1m above ground
		render(view, projection);
	}
	else
	{
		vr_loop(render);
	}

	if(keys[KEY_ESCAPE])
	{
		log_info("Shutdown on : Button press (ESC)");
		killme=1;
	}

	if(keys[KEY_F9])
	{
		keys[KEY_F9] = 0;
		log_info("VR %s", (vr_using?"Shutdown":"Startup") );
		if(!vr_using)vr_init();
		else vr_end();
	}

	fps_movement(&position, &angle, 0.007);

	time = (float)(sys_time() - time_start)/(float)sys_ticksecond;

	glUseProgram(0);
	gui_input();
	gui_draw();
	gfx_swap();
}
