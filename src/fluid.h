/*
Copyright (c) 2011,2020 Daniel Burke

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
#include <stdint.h>
#include "3dmaths.h"
#include "octtree.h"

struct vorton {
	vec3 p;		// position
	vec3 w;		// vorticity
	vec3 v;		// velocity
	float weight;	// for weighted average
	float magnitude;
	int count;
};

struct fluid_sim {
	int max_depth;
	int max_vortons;
	int vorton_count;
	struct vorton *vortons;
	struct octtree *octtree;
};

struct particle {
	vec3 p;
	uint8_t r, g, b, a;
};

struct fluid_sim* fluid_init(float x, float y, float z, int depth);
void fluid_end(struct fluid_sim *sim);
void fluid_tree_update(struct fluid_sim *sim);


void fluid_tick(struct fluid_sim *sim);
void fluid_advect_tracers(struct fluid_sim *sim, struct particle *particles, int count);
void fluid_bound(struct fluid_sim *sim, vec3 position);
