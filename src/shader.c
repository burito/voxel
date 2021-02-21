/*
Copyright (c) 2012,2020 Daniel Burke

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
#include <OpenGL/gl3.h>
#else
#include <GL/glew.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <string.h>

#include "glerror.h"
#include "shader.h"
#include "log.h"

char *shader_header = NULL;
char shader_empty[] = "";

/*
 * Read a text file into a char* buffer, allocating the buffer
 */
char* textfile_load(char *filename)
{
	char *buffer = NULL;
	// open the file
	FILE *fptr = fopen(filename, "rb");
	if(!fptr)
	{
		log_error("fopen(%s) = %s", filename, strerror(errno));
		goto TEXTFILE_LOAD_FOPEN;
	}
	// find the files size
	struct stat stbuf;
	stat(filename, &stbuf);
	size_t size = stbuf.st_size;
	if(size <= 0)
	{
		log_warning("file is empty");
		goto TEXTFILE_LOAD_SUCCESS;
	}
	// allocate the buffer
	buffer = malloc(size+1);
	if( buffer == NULL )
	{
		log_fatal("malloc(buffer) = %s", strerror(errno));
		goto TEXTFILE_LOAD_MALLOC;
	}
	// place the terminating null byte
	buffer[size] = 0;
	size_t bytes_read = fread(buffer, 1, size, fptr);
	if( bytes_read == size )
	{
		goto TEXTFILE_LOAD_SUCCESS;
	}

	// something went wrong
	log_warning("Read %d/%d bytes", (int)bytes_read, (int)size);

	free(buffer);
	buffer = NULL;
TEXTFILE_LOAD_MALLOC:
TEXTFILE_LOAD_SUCCESS:
	fclose(fptr);
TEXTFILE_LOAD_FOPEN:
	return buffer;
}





/*
 * Load and compile a shader file
 */
static GLuint shader_fileload(GLenum shaderType, char * filename)
{
	GLuint id = 0;
	// load the file
	char *buffer;
	buffer = textfile_load(filename);
	if( buffer == NULL )
	{
		log_error("Loading %d failed", filename);
		goto SHADER_FILELOAD_TEXTFILE_LOAD;
	}

	// create the shader handle
	id = glCreateShader(shaderType);
	if( id == 0 )
	{
		log_error("glCreateShader(%s)", glerr_ShaderType(shaderType) );
		goto SHADER_FILELOAD_CREATESHADER;
	}

	// give the shader source to OpenGL
	glShaderSource(id, 1, (const GLchar**)&buffer, NULL);

	// compile the shader
	glCompileShader(id);

	// get the compile status
	GLint params = 0;
	glGetShaderiv(id, GL_COMPILE_STATUS, &params);

	// if it worked
	if(params == GL_TRUE)
	{
		goto SHADER_FILELOAD_SUCCESS;
	}

	// it didn't work
	char *infoLog = NULL;

	// get the length of the error log
	GLint infologLength = 0;
	glGetShaderiv(id, GL_INFO_LOG_LENGTH, &infologLength);
	if( infologLength <= 0 )
	{
		log_warning("Infolog empty");
		goto SHADER_FILELOAD_INFOLOG;
	}

	// allocate a buffer for the infolog
	infoLog = malloc(infologLength);
	if( infoLog == NULL )
	{
		log_fatal("malloc(infolog) = %s", strerror(errno));
		goto SHADER_FILELOAD_INFOLOG;
	}

	// get the infolog into our buffer
	GLsizei length;
	glGetShaderInfoLog(id, infologLength, &length, infoLog);
	if( infologLength != ((int)length + 1) )
	{
		log_warning("InfoLog was %d/%d", (int)length, (int)infologLength);
	}

	// output the infolog
	log_warning("%s compile for \"%s\" failed\n%s", glerr_ShaderType(shaderType), filename, infoLog);

	free(infoLog);

	// clean everything up
SHADER_FILELOAD_INFOLOG:
	glDeleteShader(id);
	id = 0;
SHADER_FILELOAD_CREATESHADER:
SHADER_FILELOAD_SUCCESS:
	free(buffer);
SHADER_FILELOAD_TEXTFILE_LOAD:
	return id;
}

static void shader_unifind(struct GLSLSHADER *shader)
{
	if(!shader)return;
	if(!shader->happy)return;
	for(int i=0; i<shader->uniform_num; i++)
	{
		shader->uniforms[i] = glGetUniformLocation(shader->program, shader->uniform_names[i]);
		if(-1 == shader->uniforms[i])
		{
			printf("Shader(\"%s\")uniform(\"%s\"):Not Found\n",
				shader->fragment_filename, shader->uniform_names[i]);
		}
	}
}

#ifndef __APPLE__
static void shader_buffind(struct GLSLSHADER *shader)
{
	if(!shader)return;
	if(!shader->happy)return;
	for(int i=0; i<shader->buffer_num; i++)
	{
		shader->buffers[i] = glGetUniformBlockIndex(shader->program, shader->buffer_names[i]);
		glUniformBlockBinding(shader->program, shader->buffers[i], i);
		if(-1 == shader->buffers[i])
		{
			printf("Shader(\"%s\")block(\"%s\"):Not Found\n",
				shader->fragment_filename, shader->buffer_names[i]);
		}
	}
}
#endif


/*
 * (Re)Loads and (Re)Builds the given shader.
 */
void shader_rebuild(struct GLSLSHADER *shader)
{
	// if we're rebuilding, release the old shader first
	if( shader->program )
	{
		glDeleteProgram(shader->program);
		shader->program = 0;
	}
	if(shader->vertex)
	{
		glDeleteShader(shader->vertex);
		shader->vertex = 0;
	}
	if(shader->fragment)
	{
		glDeleteShader(shader->fragment);
		shader->fragment = 0;
	}
	shader->happy = 0;

	// We need a program to attach the shaders to
	shader->program = glCreateProgram();

	// if there is a vertex shader, it's a normal shader
	if(shader->vertex_filename)
	{
		shader->vertex = shader_fileload(GL_VERTEX_SHADER, shader->vertex_filename);
		shader->fragment = shader_fileload(GL_FRAGMENT_SHADER, shader->fragment_filename);
		if(!shader->vertex)return;
		if(!shader->fragment)return;
		glAttachShader(shader->program, shader->vertex);
		glAttachShader(shader->program, shader->fragment);
	}
	else
	{ // else it's a compute shader
#ifndef __APPLE__
		shader->fragment = shader_fileload(GL_COMPUTE_SHADER, shader->fragment_filename);
#endif
		if(!shader->fragment)return;
		glAttachShader(shader->program, shader->fragment);
	}
	// attempt to link the shaders into a program
	glLinkProgram(shader->program);

	// get the status of the linking
	GLint params;
	glGetProgramiv(shader->program, GL_LINK_STATUS, &params);

	// if it worked
	if(params == GL_TRUE)
	{
		shader->happy = 1;
		shader_unifind(shader);
//		shader_buffind(shader);
		return;
	}

	// it didn't work
	char *infoLog = NULL;

	// get the length of the error log
	GLint infologLength = 0;
	glGetProgramiv(shader->program, GL_INFO_LOG_LENGTH, &infologLength);
	if( infologLength <= 0 )
	{
		log_warning("Link Infolog for (\"%s\", \"%s\") empty",
			shader->vertex_filename,
			shader->fragment_filename);
		return;
	}

	// allocate a buffer for the infolog
	infoLog = malloc(infologLength);
	if( infoLog == NULL )
	{
		log_fatal("malloc(infolog) = %s", strerror(errno));
		return;
	}

	// get the infolog into our buffer
	GLsizei length;
	glGetProgramInfoLog(shader->program, infologLength, &length, infoLog);
	if( infologLength != ((int)length + 1) )
	{
		log_warning("InfoLog was %d/%d", (int)length, (int)infologLength);
	}

	// output the infolog
	log_warning("Shader Linking for (%d, %d) failed\n%s", shader->vertex_filename, shader->fragment_filename, infoLog);

	free(infoLog);
}

/*
 * Load a Vertex & Fragment Shader, compile and link them
 */
struct GLSLSHADER* shader_load(char *vertex_filename, char *fragment_filename)
{
	// allocate memory for the structure
	struct GLSLSHADER *shader = NULL;
	shader = malloc(sizeof(struct GLSLSHADER));
	if( shader == NULL )
	{
		log_error("malloc(shader) = %s", strerror(errno));
		goto SHADER_LOAD_MALLOC;
	}
	// clear the newly allocated structure
	memset(shader, 0, sizeof(struct GLSLSHADER));

	// if present, copy the vertex shader filename
	if( vertex_filename != NULL )
	{
		shader->vertex_filename = strdup(vertex_filename);
		if( shader->vertex_filename == NULL )
		{
			log_error("strdup(vertex_filename) = %d", strerror(errno));
			goto SHADER_LOAD_STRDUP_VERTEX;
		}
	}

	// if present, copy the fragment shader filename
	if( fragment_filename != NULL )
	{
		shader->fragment_filename = strdup(fragment_filename);
		if( shader->fragment_filename == NULL )
		{
			log_error("strdup(fragment_filename) = %d", strerror(errno));
			goto SHADER_LOAD_STRDUP_FRAGMENT;
		}
	}

	// now build the shader
	shader_rebuild(shader);

	// everything worked
	goto SHADER_LOAD_SUCCESS;

	if( shader->fragment_filename != NULL)
	{
		free(shader->fragment_filename);
		shader->fragment_filename = NULL;
	}
SHADER_LOAD_STRDUP_FRAGMENT:

	if( shader->vertex_filename != NULL)
	{
		free(shader->vertex_filename);
		shader->vertex_filename = NULL;
	}
SHADER_LOAD_STRDUP_VERTEX:
	free(shader);
	shader = NULL;
SHADER_LOAD_MALLOC:
SHADER_LOAD_SUCCESS:
	return shader;
}

/*
 * Release the resources and memory associated with a shader
 */
void shader_free(struct GLSLSHADER* shader)
{
	if( shader == NULL )
	{
		log_warning("Tried to free NULL");
		return;
	}

	if(shader->vertex_filename)free(shader->vertex_filename);
	if(shader->fragment_filename)free(shader->fragment_filename);

	if(shader->vertex)glDeleteShader(shader->vertex);
	if(shader->fragment)glDeleteShader(shader->fragment);
	if(shader->program)glDeleteProgram(shader->program);

	if(shader->uniform_names)
	{
		for(int i=0; i<shader->uniform_num; i++)
			if(shader->uniform_names[i] != NULL)
				free(shader->uniform_names[i]);

		free(shader->uniform_names);
	}
	if(shader->uniforms)free(shader->uniforms);


	if(shader->buffer_names)
	{
		for(int i=0; i<shader->buffer_num; i++)
			if(shader->buffer_names[i] != NULL)
				free(shader->buffer_names[i]);

		free(shader->buffer_names);
	}
	if(shader->buffers)free(shader->buffers);

	free(shader);
}

/*
 * Search for a Shader Uniform by name, adding it to a list of uniforms
 * so that we can find it again if/when we recompile the shader
 */
void shader_uniform(struct GLSLSHADER *shader, char *name)
{
	if(!shader)return;
	int i = shader->uniform_num++;
	void* tmp;
	// Realloc the name array
	tmp = realloc(shader->uniform_names, sizeof(char*)*shader->uniform_num);
	if(tmp == NULL)
	{
		log_fatal("realloc(uniform_names) = %s", strerror(errno));
	}
	shader->uniform_names = tmp;
	shader->uniform_names[i] = strdup(name);
	// realloc the uniform location id array
	tmp = realloc(shader->uniforms, sizeof(GLint)*shader->uniform_num);
	if(tmp == NULL)
	{
		log_fatal("realloc(uniforms) = %s", strerror(errno));
	}
	shader->uniforms = tmp;
	shader->uniforms[i] = glGetUniformLocation(shader->program, name);
	// Check if we found the uniform
	if(-1 == shader->uniforms[i])
	{
		log_warning("Shader(\"%s\")uniform(\"%s\"):Not Found",
			shader->fragment_filename, shader->uniform_names[i]);
	}
}

#ifndef __APPLE__
/*
 * Search for a Shader Buffer by name, adding it to a list of uniforms
 * so that we can find it again if/when we recompile the shader
 */
void shader_buffer(struct GLSLSHADER *shader, char *name)
{
	if(!shader)return;
	int i = shader->buffer_num++;
	void* tmp;
	// Realloc the name array
	tmp = realloc(shader->buffer_names, sizeof(char*)*shader->buffer_num);
	if(tmp == NULL)
	{
		log_fatal("realloc(buffer_names) = %s", strerror(errno));
	}
	shader->buffer_names = tmp;
	shader->buffer_names[i] = strdup(name);
	// realloc the buffer location id array
	tmp = realloc(shader->buffers, sizeof(GLint)*shader->buffer_num);
	if(tmp == NULL)
	{
		log_fatal("realloc(buffers) = %s", strerror(errno));
	}
	shader->buffers = tmp;
	shader->buffers[i] = glGetUniformBlockIndex(shader->program, name);
	// Check if we found the buffer
	if(-1 == shader->buffers[i])
	{
		log_warning("Shader(\"%s\")buffer(\"%s\"):Not Found",
				shader->fragment_filename, shader->buffer_names[i]);
	}
	else
	{
		glUniformBlockBinding(shader->program, shader->buffers[i], i);
	}
}
#endif

int available_vram(void)
{
	int mem = 0;
	if(strstr((const char*)glGetString(GL_VENDOR), "NVIDIA"))
	{
//		glGetIntegerv(GL_GPU_MEMORY_INFO_CURRENT_AVAILABLE_VIDMEM_NVX, &mem);
		glGetIntegerv(0x9049, &mem);
		return mem;
	}
	else if(strstr((const char*)glGetString(GL_VENDOR), "ATI"))
	{
		/* Crashes on AMD devices :-/ */
//		  glGetIntegerv(GL_TEXTURE_FREE_MEMORY_ATI, &mem);
		mem = 2000000;
		return mem;
	}
	else
	{
		printf("I don't know if there is VRAM available\n");
	}
	return 0;
}

GLint glBaseFormat(GLint SizedInternalFormat)
{ // taken from https://www.opengl.org/sdk/docs/man4/xhtml/glTexImage3D.xml
	switch(SizedInternalFormat) {
#ifndef __APPLE__
	case GL_R8_SNORM:
	case GL_R16_SNORM:
#endif
	case GL_R8:
	case GL_R16:
	case GL_R16F:
	case GL_R32F:
	case GL_R8I:
	case GL_R8UI:
	case GL_R16I:
	case GL_R16UI:
	case GL_R32I:
	case GL_R32UI:
		return GL_RED;
#ifndef __APPLE__
	case GL_RG8_SNORM:
	case GL_RG16_SNORM:
#endif
	case GL_RG8:
	case GL_RG16:
	case GL_RG16F:
	case GL_RG32F:
	case GL_RG8I:
	case GL_RG8UI:
	case GL_RG16I:
	case GL_RG16UI:
	case GL_RG32I:
	case GL_RG32UI:
		return GL_RG;

#ifndef __APPLE__
	case GL_RGB8_SNORM:
	case GL_RGB16_SNORM:
	case GL_RGB16F:
	case GL_RGB32F:
	case GL_RGB8I:
	case GL_RGB8UI:
	case GL_RGB16I:
	case GL_RGB16UI:
	case GL_RGB32I:
	case GL_RGB32UI:
#endif
	case GL_R3_G3_B2:
	case GL_RGB4:
	case GL_RGB5:
	case GL_RGB8:
	case GL_RGB10:
	case GL_RGB12:
	case GL_SRGB8:
		return GL_RGB;

#ifndef __APPLE__
	case GL_RGBA8_SNORM:
	case GL_RGB10_A2UI:
	case GL_RGBA16F:
	case GL_RGBA32F:
	case GL_RGBA8I:
	case GL_RGBA8UI:
	case GL_RGBA16I:
	case GL_RGBA16UI:
	case GL_RGBA32I:
	case GL_RGBA32UI:
#endif
	case GL_RGBA2:
	case GL_RGBA4:
	case GL_RGB5_A1:
	case GL_RGBA8:
	case GL_RGB10_A2:
	case GL_RGBA12:
	case GL_RGBA16:
	case GL_SRGB8_ALPHA8:
		return GL_RGBA;

	default:
		return 0;
	}
}

GLint glBaseType(GLint SizedInternalFormat)
{ // taken from https://www.opengl.org/sdk/docs/man4/xhtml/glTexImage3D.xml
	switch(SizedInternalFormat) {

#ifndef __APPLE__
	case GL_RGB16F:
	case GL_RGB32F:
	case GL_RGBA16F:
	case GL_RGBA32F:
#endif
	case GL_R16F:
	case GL_R32F:
	case GL_RG16F:
	case GL_RG32F:
		return GL_FLOAT;

#ifndef __APPLE__
	case GL_R8_SNORM:
	case GL_RG8_SNORM:
	case GL_RGB8_SNORM:
	case GL_RGB8I:
	case GL_RGBA8_SNORM:
	case GL_RGBA8I:
#endif
	case GL_R8:
	case GL_R8I:
	case GL_RG8:
	case GL_RG8I:
	case GL_R3_G3_B2:
	case GL_RGB4:
	case GL_RGB5:
	case GL_RGB8:
	case GL_RGBA8:
	case GL_SRGB8:
	case GL_SRGB8_ALPHA8:
		return GL_BYTE;

#ifndef __APPLE__
	case GL_RGB8UI:
	case GL_RGBA8UI:
#endif
	case GL_R8UI:
	case GL_RG8UI:
		return GL_UNSIGNED_BYTE;

#ifndef __APPLE__
	case GL_R16_SNORM:
	case GL_RG16_SNORM:
	case GL_RGB16I:
	case GL_RGB16_SNORM:
	case GL_RGBA16I:
#endif
	case GL_R16:
	case GL_RG16:
	case GL_R16I:
	case GL_RG16I:
	case GL_RGBA16:
		return GL_SHORT;

#ifndef __APPLE__
	case GL_RGB16UI:
	case GL_RGBA16UI:
#endif
	case GL_R16UI:
	case GL_RG16UI:
		return GL_UNSIGNED_SHORT;

#ifndef __APPLE__
	case GL_RGB32I:
	case GL_RGBA32I:
#endif
	case GL_R32I:
	case GL_RG32I:
		return GL_INT;

#ifndef __APPLE__
	case GL_RGB32UI:
	case GL_RGBA32UI:
#endif
	case GL_R32UI:
	case GL_RG32UI:
		return GL_UNSIGNED_INT;

#ifndef __APPLE__
	case GL_RGB10_A2UI:
#endif
	case GL_RGB10:
	case GL_RGB12:
	case GL_RGBA2:
	case GL_RGBA4:
	case GL_RGB5_A1:
	case GL_RGB10_A2:
	case GL_RGBA12:
	default:
		return 0;
	}
}
