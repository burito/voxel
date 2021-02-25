/*
Copyright (c) 2017 Daniel Burke

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
#include <string.h>
#include <openvr/headers/openvr_capi.h>


#include "log.h"

#ifdef _WIN32
//#define WIN32_LEAN_AND_MEAN
//#include <windows.h>
//#include <synchapi.h>
void Sleep (int dwMilliseconds);
#else
// #include <unistd.h>	// but it defines sleep() too
int	usleep(unsigned int useconds);
#endif

#include "vr_helper.h"
#include "shader.h"
#include "3dmaths.h"
#include "glerror.h"
#include "global.h"


int vr_using = 0;

intptr_t VR_InitInternal( EVRInitError *peError, EVRApplicationType eType );
void VR_ShutdownInternal();
int VR_IsHmdPresent();
intptr_t VR_GetGenericInterface( const char *pchInterfaceVersion, EVRInitError *peError );
int VR_IsRuntimeInstalled();
const char * VR_GetVRInitErrorAsSymbol( EVRInitError error );
const char * VR_GetVRInitErrorAsEnglishDescription( EVRInitError error );

static void sleep(unsigned int msec)
{
#ifdef _WIN32
	Sleep(msec);
#else
	usleep(msec*1000);
#endif
}

struct VR_IVRSystem_FnTable * OVR = NULL;
struct VR_IVRCompositor_FnTable * OVRC;
struct VR_IVRRenderModels_FnTable * OVRM;
// k_unMaxTrackedDeviceCount = 16 // gcc doesn't like the way this is declared
TrackedDevicePose_t m_rTrackedDevicePose [16];

struct FramebufferDesc
{
	GLuint m_nDepthBufferId;
	GLuint m_nRenderTextureId;
	GLuint m_nRenderFramebufferId;
	GLuint m_nResolveTextureId;
	GLuint m_nResolveFramebufferId;
};
struct FramebufferDesc leftEyeDesc = {0};
struct FramebufferDesc rightEyeDesc = {0};

uint32_t m_nRenderWidth;
uint32_t m_nRenderHeight;
int m_iValidPoseCount;
char m_strPoseClasses[16+1]; // should never get above 16 = k_unMaxTrackedDeviceCount
char m_rDevClassChar[16+1];
mat4x4 vrdevice_poses[16]; // 16 = k_unMaxTrackedDeviceCount
mat4x4 hmdPose;
mat4x4 controller_left, controller_right;
int controller_left_id = -1;
int controller_right_id = -1;

mat4x4 eye_left, eye_left_proj;
mat4x4 eye_right, eye_right_proj;
struct GLSLSHADER *eye_prog = NULL;
GLuint eye_VAO = 0;
GLuint eye_VBO, eye_EAB;

char controller_filename[1024] = {0};

typedef struct {
	GLuint va, vb, eab, tex, vcount;
	char name[1024];
} ovr_mesh;

ovr_mesh ovr_models[10];
int ovr_model_count = 0;


void device_spam(int id)
{
	ETrackedPropertyError tp_error;

	char device_name[1024];
	int32_t role;
	int32_t class;

	OVR->GetStringTrackedDeviceProperty(id,
		ETrackedDeviceProperty_Prop_RenderModelName_String,
		device_name, 1024, &tp_error );
	if(tp_error) log_fatal("DeviceName:TP_Error = \"%s\"", OVR->GetPropErrorNameFromEnum(tp_error) );

	class = OVR->GetTrackedDeviceClass(id);

	// switch (class)
	// {
	// case ETrackedDeviceClass_TrackedDeviceClass_Controller:        class_letter = 'C'; break;
	// case ETrackedDeviceClass_TrackedDeviceClass_HMD:               class_letter = 'H'; break;
	// case ETrackedDeviceClass_TrackedDeviceClass_Invalid:           class_letter = 'I'; break;
	// case ETrackedDeviceClass_TrackedDeviceClass_GenericTracker:    class_letter = 'G'; break;
	// case ETrackedDeviceClass_TrackedDeviceClass_TrackingReference: class_letter = 'T'; break;
	// case ETrackedDeviceClass_TrackedDeviceClass_DisplayRedirect:   class_letter = 'D'; break;
	// default:                                                       class_letter = '?'; break;
	// }



//	log_info("%d:%c:%s:", id, class_letter, device_name);

	switch (class)
	{
	case ETrackedDeviceClass_TrackedDeviceClass_Controller:
		role = OVR->GetInt32TrackedDeviceProperty(id, ETrackedDeviceProperty_Prop_ControllerRoleHint_Int32, &tp_error );
		if(tp_error) log_warning("RoleHint:TP_Error = \"%s\"", OVR->GetPropErrorNameFromEnum(tp_error) );
		switch(role) {
		case ETrackedControllerRole_TrackedControllerRole_LeftHand:
			log_info("LeftHand:");
			break;
		case ETrackedControllerRole_TrackedControllerRole_RightHand:
			log_info("RightHand:");
			break;
		case ETrackedControllerRole_TrackedControllerRole_Invalid:
			log_info("InvalidHand:");
			break;
		default:
			log_warning("UndocumentedHand:");
			break;
		}

		break;
	case ETrackedDeviceClass_TrackedDeviceClass_HMD:
		break;
	case ETrackedDeviceClass_TrackedDeviceClass_Invalid:
		break;
	case ETrackedDeviceClass_TrackedDeviceClass_GenericTracker:
		break;
	case ETrackedDeviceClass_TrackedDeviceClass_TrackingReference:
		break;
	case ETrackedDeviceClass_TrackedDeviceClass_DisplayRedirect:
		break;
	}
}


void ovr_model_load( TrackedDeviceIndex_t di )
{
	ETrackedPropertyError tp_error;
	EVRRenderModelError rm_error;

	// we only want controller models
	if(OVR->GetTrackedDeviceClass(di) != ETrackedDeviceClass_TrackedDeviceClass_Controller)return;

	char device_name[1024];

	OVR->GetStringTrackedDeviceProperty(di,
		ETrackedDeviceProperty_Prop_RenderModelName_String,
		device_name, 1024, &tp_error );

	int32_t role = OVR->GetInt32TrackedDeviceProperty(di, ETrackedDeviceProperty_Prop_ControllerRoleHint_Int32, &tp_error );
	log_info("device_name = \"%s\", %d, %d", device_name, di, role);

	switch(role) {
	case ETrackedControllerRole_TrackedControllerRole_LeftHand:
		controller_left_id = di;
		break;
	case ETrackedControllerRole_TrackedControllerRole_RightHand:
		controller_right_id = di;
		break;
	case ETrackedControllerRole_TrackedControllerRole_Invalid:
	default:
		break;
	}

	for(int i=0; i<ovr_model_count; i++)
		if(0 == strstr(ovr_models[i].name, device_name) )
			return; // already loaded

	memcpy(ovr_models[ovr_model_count].name, device_name, 1024);

	RenderModel_t *ovr_rendermodel = NULL;
	do
	{
		rm_error = OVRM->LoadRenderModel_Async(ovr_models[ovr_model_count].name, &ovr_rendermodel);
		sleep(1);
	}
	while(rm_error == EVRRenderModelError_VRRenderModelError_Loading);

	if(rm_error != EVRRenderModelError_VRRenderModelError_None)
	{
		log_fatal("LoadRenderModel_Async(\"%s\"): %s", ovr_models[ovr_model_count].name, OVRM->GetRenderModelErrorNameFromEnum(rm_error) );
	}
	RenderModel_TextureMap_t *ovr_texture = NULL;

	do
	{
		rm_error = OVRM->LoadTexture_Async(ovr_rendermodel->diffuseTextureId, &ovr_texture);
		sleep(1);
	}
	while(rm_error == EVRRenderModelError_VRRenderModelError_Loading);

	if(rm_error != EVRRenderModelError_VRRenderModelError_None)
	{
		log_fatal("LoadRenderModel_Async(\"%s\"): %s", ovr_models[ovr_model_count].name, OVRM->GetRenderModelErrorNameFromEnum(rm_error) );
	}

//	GLuint va, vb, eab, tex, vcount;
	// create and bind a VAO to hold state for this model
	glGenVertexArrays( 1, &ovr_models[ovr_model_count].va );
	glBindVertexArray( ovr_models[ovr_model_count].va );

	// Populate a vertex buffer
	glGenBuffers( 1, &ovr_models[ovr_model_count].vb );
	glBindBuffer( GL_ARRAY_BUFFER, ovr_models[ovr_model_count].vb );
	glBufferData( GL_ARRAY_BUFFER, sizeof( RenderModel_Vertex_t ) * ovr_rendermodel->unVertexCount, ovr_rendermodel->rVertexData, GL_STATIC_DRAW );

	// Identify the components in the vertex buffer
	glEnableVertexAttribArray( 0 );
	glVertexAttribPointer( 0, 3, GL_FLOAT, GL_FALSE, 32, (GLvoid *)0  );
	glEnableVertexAttribArray( 1 );
	glVertexAttribPointer( 1, 3, GL_FLOAT, GL_FALSE, 32, (GLvoid *)12 );
	glEnableVertexAttribArray( 2 );
	glVertexAttribPointer( 2, 2, GL_FLOAT, GL_FALSE, 32, (GLvoid *)24 );

	// Create and populate the index buffer
	glGenBuffers( 1, &ovr_models[ovr_model_count].eab );
	glBindBuffer( GL_ELEMENT_ARRAY_BUFFER, ovr_models[ovr_model_count].eab );
	glBufferData( GL_ELEMENT_ARRAY_BUFFER, sizeof( uint16_t ) * ovr_rendermodel->unTriangleCount * 3, ovr_rendermodel->rIndexData, GL_STATIC_DRAW );

	glBindVertexArray( 0 );

	// create and populate the texture
	glGenTextures(1, &ovr_models[ovr_model_count].tex );
	glBindTexture( GL_TEXTURE_2D, ovr_models[ovr_model_count].tex );

	glTexImage2D( GL_TEXTURE_2D, 0, GL_RGBA, ovr_texture->unWidth, ovr_texture->unHeight,
		0, GL_RGBA, GL_UNSIGNED_BYTE, ovr_texture->rubTextureMapData );

	// If this renders black ask McJohn what's wrong.
	glGenerateMipmap(GL_TEXTURE_2D);

	glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE );
	glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE );
	glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR );
	glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR );

#ifndef __APPLE__
	GLfloat fLargest;
	glGetFloatv( GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT, &fLargest );
	glTexParameterf( GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY_EXT, fLargest );
#endif
	glBindTexture( GL_TEXTURE_2D, 0 );

	ovr_models[ovr_model_count].vcount = ovr_rendermodel->unTriangleCount * 3;
	ovr_model_count++;
//	return ovr_model_count - 1;
}

void ovr_draw(int i)
{
	glBindVertexArray( ovr_models[i].va );
	glActiveTexture( GL_TEXTURE0 );
	glBindTexture( GL_TEXTURE_2D, ovr_models[i].tex );
	glDrawElements( GL_TRIANGLES, ovr_models[i].vcount, GL_UNSIGNED_SHORT, 0 );
	glBindVertexArray( 0 );
	glBindTexture( GL_TEXTURE_2D, 0 );
}

//extern GLSLSHADER *shader;
void ovr_render(mat4x4 pos, mat4x4 proj)
{
/*
	mat4x4 w = mul(proj,pos );
	glUseProgram(shader->prog);
	glUniformMatrix4fv(shader->unif[1], 1, GL_FALSE, w.f);

	if(controller_left_id != -1)
	{
		glUniformMatrix4fv(shader->unif[0], 1, GL_FALSE, controller_left.f);
		ovr_draw(0);
	}
	if(controller_right_id != -1)
	{
		glUniformMatrix4fv(shader->unif[0], 1, GL_FALSE, controller_right.f);
		ovr_draw(0);
	}

	glUseProgram(0);
*/
}

//-----------------------------------------------------------------------------
// Purpose: Creates a frame buffer. Returns true if the buffer was set up.
//          Returns false if the setup failed.
//-----------------------------------------------------------------------------
bool CreateFrameBuffer( int nWidth, int nHeight, struct FramebufferDesc *framebufferDesc )
{
	glGenFramebuffers(1, &framebufferDesc->m_nRenderFramebufferId );
	glBindFramebuffer(GL_FRAMEBUFFER, framebufferDesc->m_nRenderFramebufferId);

	glGenRenderbuffers(1, &framebufferDesc->m_nDepthBufferId);
	glBindRenderbuffer(GL_RENDERBUFFER, framebufferDesc->m_nDepthBufferId);
	glRenderbufferStorageMultisample(GL_RENDERBUFFER, 4, GL_DEPTH_COMPONENT, nWidth, nHeight );
	glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER,	framebufferDesc->m_nDepthBufferId );

	glGenTextures(1, &framebufferDesc->m_nRenderTextureId );
	glBindTexture(GL_TEXTURE_2D_MULTISAMPLE, framebufferDesc->m_nRenderTextureId );
	glTexImage2DMultisample(GL_TEXTURE_2D_MULTISAMPLE, 4, GL_RGBA8, nWidth, nHeight, GL_TRUE);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D_MULTISAMPLE, framebufferDesc->m_nRenderTextureId, 0);

	GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
	if (status != GL_FRAMEBUFFER_COMPLETE)
	{
		log_error("framebuffer1 creation failed: %s", glErrorFb(status));
		return 1;
	}

	glGenFramebuffers(1, &framebufferDesc->m_nResolveFramebufferId );
	glBindFramebuffer(GL_FRAMEBUFFER, framebufferDesc->m_nResolveFramebufferId);

	glGenTextures(1, &framebufferDesc->m_nResolveTextureId );
	glBindTexture(GL_TEXTURE_2D, framebufferDesc->m_nResolveTextureId );
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 0);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, nWidth, nHeight, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, framebufferDesc->m_nResolveTextureId, 0);

	// check FBO status
	status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
	if (status != GL_FRAMEBUFFER_COMPLETE)
	{
		log_error("framebuffer2 creation failed");
		return 1;
	}

	glBindFramebuffer( GL_FRAMEBUFFER, 0 );
	return 0;
}

void hmd_eye_calc(EVREye eye, mat4x4 * dest_pose, mat4x4 *dest_proj)
{
	float fnear = 0.1;
	float ffar = 30.0f;

	mat4x4 proj = mov( OVR->GetProjectionMatrix( eye, fnear, ffar ) );
	mat4x4 pos = mat4x4_invert( mov( OVR->GetEyeToHeadTransform( eye ) ) );
	*dest_pose = pos;
	*dest_proj = proj;
}


int vr_init(void)
{
	EVRInitError eError;

	if( !VR_IsHmdPresent() )
	{
		log_warning("VR Headset is not present");
		return 1;
	}

	if( !VR_IsRuntimeInstalled() )
	{
		log_warning("VR Runtime is not installed");
		return 1;
	}

	uint32_t vrToken = VR_InitInternal(&eError, EVRApplicationType_VRApplication_Scene);
	if(vrToken != 1)
	{	// in testing this equals 1, but don't know what it means
		log_info("vrToken = %d", vrToken);
	}
	if (eError != EVRInitError_VRInitError_None)
	{
		log_fatal("VR_InitInternal: %s", VR_GetVRInitErrorAsSymbol(eError));
		return 2;
	}

	char fnTableName[128];

	sprintf(fnTableName, "FnTable:%s", IVRSystem_Version);
	OVR = (struct VR_IVRSystem_FnTable *)VR_GetGenericInterface(fnTableName, &eError);
	if (eError != EVRInitError_VRInitError_None)
	{
		log_fatal("VR_GetGenericInterface(\"%s\"): %s", IVRSystem_Version, VR_GetVRInitErrorAsSymbol(eError));
		return 2;
	}

	sprintf(fnTableName, "FnTable:%s", IVRCompositor_Version);
	OVRC = (struct VR_IVRCompositor_FnTable *)VR_GetGenericInterface(fnTableName, &eError);
	if (eError != EVRInitError_VRInitError_None)
	{
		log_fatal("VR_GetGenericInterface(\"%s\"): %s", IVRCompositor_Version, VR_GetVRInitErrorAsSymbol(eError));
		return 2;
	}

	sprintf(fnTableName, "FnTable:%s", IVRRenderModels_Version);
	OVRM = (struct VR_IVRRenderModels_FnTable *)VR_GetGenericInterface(fnTableName, &eError );
	if (eError != EVRInitError_VRInitError_None)
	{
		log_fatal("VR_GetGenericInterface(\"%s\"): %s", IVRRenderModels_Version, VR_GetVRInitErrorAsSymbol(eError));
		return 2;
	}

	// Get hmd position matrices
	hmd_eye_calc(EVREye_Eye_Left, &eye_left, &eye_left_proj);
	hmd_eye_calc(EVREye_Eye_Right, &eye_right, &eye_right_proj);

	//	ETrackedDeviceProperty_Prop_TrackingSystemName_String = 1000
	// ETrackedDeviceClass_TrackedDeviceClass_Controller
//	OVR->GetStringTrackedDeviceProperty()
/*
	bool result2 = OVR->IsDisplayOnDesktop();
	if (result2)
		log_warning("Display is on desktop");
	else
		log_warning("Display is NOT on desktop");
*/

	if(!leftEyeDesc.m_nDepthBufferId)
	{
		OVR->GetRecommendedRenderTargetSize( &m_nRenderWidth, &m_nRenderHeight );
		CreateFrameBuffer( m_nRenderWidth, m_nRenderHeight, &leftEyeDesc );
		CreateFrameBuffer( m_nRenderWidth, m_nRenderHeight, &rightEyeDesc );
	}

	if(!eye_prog) eye_prog = shader_load(
			"data/shaders/window.vert",
			"data/shaders/window.frag" );

	float eye_verts[] = {
		// left eye
		-0.9f, -0.9f, 0.0f, 0.0f,
		0.0f, -0.9f, 1.0f, 0.0f,
		0.0f, 0.9f, 1.0f, 1.0f,
		-0.9f, 0.9f, 0.0f, 1.0f,
		// right eye
		0.1f, -0.9f, 0.0f, 0.0f,
		0.9f, -0.9f, 1.0f, 0.0f,
		0.9f, 0.9f, 1.0f, 1.0f,
		0.1f, 0.9f, 0.0f, 1.0f
	};

	GLushort eye_ind[]  = { 0, 1, 2,   0, 2, 3,   4, 5, 6,   4, 6, 7};

	if(!eye_VAO)
	{
		glGenVertexArrays( 1, &eye_VAO );
		glBindVertexArray( eye_VAO );

		glGenBuffers( 1, &eye_VBO );
		glBindBuffer( GL_ARRAY_BUFFER, eye_VBO );
		glBufferData( GL_ARRAY_BUFFER, sizeof(eye_verts), &eye_verts[0], GL_STATIC_DRAW );

		glGenBuffers( 1, &eye_EAB );
		glBindBuffer( GL_ELEMENT_ARRAY_BUFFER, eye_EAB );
		glBufferData( GL_ELEMENT_ARRAY_BUFFER, sizeof(eye_ind), &eye_ind[0], GL_STATIC_DRAW );

		glEnableVertexAttribArray( 0 );
		glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 16, (void *)0 );

		glEnableVertexAttribArray( 1 );
		glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 16, (void *)8 );

		glBindVertexArray( 0 );
		glBindBuffer(GL_ARRAY_BUFFER, 0);
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
	}

	vr_using = 1;
	return 0;
}

void vr_end(void)
{
	VR_ShutdownInternal();
	vr_using = 0;
}

void vr_loop( void render(mat4x4, mat4x4) )
{
// Process OpenVR events
	struct VREvent_t vre;
	while( OVR->PollNextEvent(&vre, sizeof(vre)) )
	{
		switch(vre.eventType) {
			case EVREventType_VREvent_TrackedDeviceActivated:
//				log_info("device activated - load a model here");
				break;
			case EVREventType_VREvent_TrackedDeviceDeactivated:
				if( vre.trackedDeviceIndex == controller_left_id )
				{
					m_rDevClassChar[controller_left_id]=0;
					controller_left_id = -1;
				}
				else if( vre.trackedDeviceIndex == controller_right_id )
				{
					m_rDevClassChar[controller_right_id]=0;
					controller_right_id = -1;
				}
//				log_info("device deactivated");
				break;
			case EVREventType_VREvent_TrackedDeviceUpdated:
//				log_info("device updated");
				break;
		}
	}

	// Process OpenVR Controller // k_unMaxTrackedDeviceCount = 16
	for( TrackedDeviceIndex_t unDevice = 0; unDevice < 16; unDevice++)
	{
		struct VRControllerState_t state;
		if( OVR->GetControllerState( unDevice, &state, sizeof(state) ) )
		{
		//	log_debug(" it equals \"%d\"", state.ulButtonPressed );
		// it tracks buttons at least
			//log_debug(" it equals 0?");

		}
	}

// Process HMD Position
	// UpdateHMDMatrixPose() // k_unMaxTrackedDeviceCount = 16
	OVRC->WaitGetPoses(m_rTrackedDevicePose, 16, NULL, 0);
	m_iValidPoseCount = 0;
	m_strPoseClasses[0] = 0;

//	log_info("begin device spam");

	for(int nDevice = 0; nDevice < 16; nDevice++)
	if(m_rTrackedDevicePose[nDevice].bPoseIsValid)
	{
		device_spam(nDevice);
		m_iValidPoseCount++;
		// m_rmat4DevicePose[nDevice]
		vrdevice_poses[nDevice] = mov( m_rTrackedDevicePose[nDevice].mDeviceToAbsoluteTracking );
		if (m_rDevClassChar[nDevice]==0)
		{
			switch (OVR->GetTrackedDeviceClass(nDevice))
			{
			case ETrackedDeviceClass_TrackedDeviceClass_Controller:
				m_rDevClassChar[nDevice] = 'C';
				if(!ovr_model_count)
				{
					ovr_model_load(nDevice);
				}
				//break;
				if(controller_left_id == -1)
					controller_left_id = nDevice;
				else if(controller_right_id == -1)
					controller_right_id = nDevice;

/*				// Doesn't tell you which hand the controller is
				ETrackedPropertyError tp_error;
				int32_t role = OVR->GetInt32TrackedDeviceProperty(nDevice, ETrackedDeviceProperty_Prop_ControllerRoleHint_Int32, &tp_error );
				log_info("device_name = %d, %d", nDevice, role);

				switch(role) {
				case ETrackedControllerRole_TrackedControllerRole_LeftHand:
					controller_left_id = nDevice;
					break;
				case ETrackedControllerRole_TrackedControllerRole_RightHand:
					controller_right_id = nDevice;
					break;
				case ETrackedControllerRole_TrackedControllerRole_Invalid:
				default:
					break;
				}
*/				break;
			case ETrackedDeviceClass_TrackedDeviceClass_HMD:               m_rDevClassChar[nDevice] = 'H'; break;
			case ETrackedDeviceClass_TrackedDeviceClass_Invalid:           m_rDevClassChar[nDevice] = 'I'; break;
			case ETrackedDeviceClass_TrackedDeviceClass_GenericTracker:    m_rDevClassChar[nDevice] = 'G'; break;
			case ETrackedDeviceClass_TrackedDeviceClass_TrackingReference: m_rDevClassChar[nDevice] = 'T'; break;
			default:                                       m_rDevClassChar[nDevice] = '?'; break;
			}
		}
//		log_debug("%c", m_rDevClassChar[nDevice]);
		m_strPoseClasses[nDevice] += m_rDevClassChar[nDevice];
	}
//	log_debug(" - %d, %d", m_iValidPoseCount, controller_id);
//	log_info("end device spam");
	if ( m_rTrackedDevicePose[k_unTrackedDeviceIndex_Hmd].bPoseIsValid )
	{
		hmdPose = mat4x4_invert(vrdevice_poses[k_unTrackedDeviceIndex_Hmd]);
	}

	mat4x4 mat_l = mul(eye_left, hmdPose);
	mat4x4 mat_r = mul(eye_right, hmdPose);

	if(controller_left_id != -1)controller_left = vrdevice_poses[controller_left_id];
	if(controller_right_id != -1)controller_right = vrdevice_poses[controller_right_id];

// Render to the Headset
	// RenderStereoTargets();
	glEnable( GL_MULTISAMPLE );

	//Left eye
	glBindFramebuffer( GL_FRAMEBUFFER, leftEyeDesc.m_nRenderFramebufferId );
	glViewport(0, 0, m_nRenderWidth, m_nRenderHeight );
	render(mat_l, eye_left_proj);
	ovr_render(mat_l, eye_left_proj);
	glBindFramebuffer( GL_FRAMEBUFFER, 0 );

	glDisable( GL_MULTISAMPLE );

	glBindFramebuffer(GL_READ_FRAMEBUFFER, leftEyeDesc.m_nRenderFramebufferId);
	glBindFramebuffer(GL_DRAW_FRAMEBUFFER, leftEyeDesc.m_nResolveFramebufferId );

	glBlitFramebuffer( 0, 0, m_nRenderWidth, m_nRenderHeight, 0, 0, m_nRenderWidth, m_nRenderHeight,
		GL_COLOR_BUFFER_BIT,
		GL_LINEAR );

	glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);
	glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0 );

	glEnable( GL_MULTISAMPLE );

	// Right Eye
	glBindFramebuffer( GL_FRAMEBUFFER, rightEyeDesc.m_nRenderFramebufferId );
	glViewport(0, 0, m_nRenderWidth, m_nRenderHeight );
	render(mat_r, eye_right_proj);
	ovr_render(mat_r, eye_right_proj);
	glBindFramebuffer( GL_FRAMEBUFFER, 0 );

	glDisable( GL_MULTISAMPLE );

	glBindFramebuffer(GL_READ_FRAMEBUFFER, rightEyeDesc.m_nRenderFramebufferId );
	glBindFramebuffer(GL_DRAW_FRAMEBUFFER, rightEyeDesc.m_nResolveFramebufferId );

	glBlitFramebuffer( 0, 0, m_nRenderWidth, m_nRenderHeight, 0, 0, m_nRenderWidth, m_nRenderHeight,
		GL_COLOR_BUFFER_BIT,
		GL_LINEAR );

	glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);
	glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0 );
	// end RenderStereoTargets();

// render to the monitor
	// RenderCompanionWindow()
	glDisable(GL_DEPTH_TEST);
	glViewport( 0, 0, vid_width, vid_height );

	glBindVertexArray( eye_VAO );
	glUseProgram( eye_prog->program );

	// render left eye (first half of index array )
	glBindTexture(GL_TEXTURE_2D, leftEyeDesc.m_nResolveTextureId );
	glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE );
	glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE );
	glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR );
	glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR );
	glDrawElements( GL_TRIANGLES, 6, GL_UNSIGNED_SHORT, 0 );

	// render right eye (second half of index array )
	glBindTexture(GL_TEXTURE_2D, rightEyeDesc.m_nResolveTextureId  );
	glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE );
	glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE );
	glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR );
	glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR );
	glDrawElements( GL_TRIANGLES, 6, GL_UNSIGNED_SHORT, (const void *)(uintptr_t)(12) );

	glBindVertexArray( 0 );
	glUseProgram( 0 );
	// end RenderCompanionWindow()

	EVRCompositorError cErr;
	Texture_t leftEyeTexture = {(void*)(uintptr_t)leftEyeDesc.m_nResolveTextureId, ETextureType_TextureType_OpenGL, EColorSpace_ColorSpace_Gamma};
	cErr = OVRC->Submit(EVREye_Eye_Left, &leftEyeTexture, NULL, EVRSubmitFlags_Submit_Default);
	if(cErr)
	{
		log_error("OVRC->Submit(left) = %s", vrc_error(cErr));
		return;
	}
	Texture_t rightEyeTexture = {(void*)(uintptr_t)rightEyeDesc.m_nResolveTextureId, ETextureType_TextureType_OpenGL, EColorSpace_ColorSpace_Gamma};
	cErr = OVRC->Submit(EVREye_Eye_Right, &rightEyeTexture, NULL, EVRSubmitFlags_Submit_Default);
	if(cErr)
	{
		log_error("OVRC->Submit(right) = %s", vrc_error(cErr));
		return;
	}


// work around to force HMD vsync from official example
	glFinish();

	glFlush();
	glFinish();
}
