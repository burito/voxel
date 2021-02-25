/*
Copyright (c) 2020 Daniel Burke

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

#ifndef __DPB_OCTTREE_H__
#define __DPB_OCTTREE_H__

#include <stdint.h>
#include "3dmaths.h"

struct octtree_node {
	uint32_t node[8];
	uint32_t leaf[8];
};

struct octtree {
	uint32_t node_pool_size;
	uint32_t node_count;
	struct octtree_node *node_pool;
	vec3 origin;
	vec3 volume;
};

struct octtree* octtree_init(uint32_t size);
void octtree_free(struct octtree* octtree);
void octtree_empty(struct octtree* octtree);
int octtree_find(struct octtree* octtree, vec3 position, int depth);
int octtree_child_position(vec3 position, vec3 half_volume) __attribute__((const));
vec3 octtree_child_relative(vec3 position, vec3 half_volume) __attribute__((const));

#endif
