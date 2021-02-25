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

#include <stdint.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>

#include "log.h"
#include "octtree.h"
#include "3dmaths.h"

struct octtree* octtree_init(uint32_t size)
{
	struct octtree *ret;
	ret = malloc(sizeof(struct octtree));
	if(ret == NULL)
	{
		log_error("malloc(octtree) %s", strerror(errno));
		return NULL;
	}
	ret->origin = (vec3){{0.0, 0.0, 0.0}};
	ret->volume = (vec3){{1.0, 1.0, 1.0}};
	ret->node_pool_size = size;
	ret->node_pool = malloc(size * sizeof(struct octtree_node));
	if(ret->node_pool == NULL)
	{
		log_error("malloc(node_pool) %s", strerror(errno));
		free(ret);
		return NULL;
	}
	memset(ret->node_pool, 0, size * sizeof(struct octtree_node));
	ret->node_count = 1; // there is always a root node
	return ret;
}

void octtree_free(struct octtree* octtree)
{
	free(octtree->node_pool);
	free(octtree);
}

void octtree_empty(struct octtree* octtree)
{
	memset(octtree->node_pool, 0, octtree->node_pool_size * sizeof(struct octtree_node));
	octtree->node_count = 1;
}



int octtree_child_position(vec3 position, vec3 half_volume)
{
	int offset = 0;
	if(position.x > half_volume.x)offset += 1;
	if(position.y > half_volume.y)offset += 2;
	if(position.z > half_volume.z)offset += 4;
	return offset;
}

vec3 octtree_child_relative(vec3 position, vec3 half_volume)
{
	vec3 ret = position;
	if(position.x > half_volume.x)ret.x = ret.x-half_volume.x;
	if(position.y > half_volume.y)ret.y = ret.z-half_volume.y;
	if(position.z > half_volume.z)ret.z = ret.z-half_volume.z;
	return ret;
}

int octtree_find(struct octtree* octtree, vec3 position, int depth)
{
	vec3 rel_position = sub(position, octtree->origin);
	if(vec3_lessthan_vec3(rel_position, (vec3){{0,0,0}}))
	{
		log_warning("Tested vector is less than Origin");
		return 0;
	}
	if(vec3_greaterthan_vec3(rel_position, octtree->volume))
	{
		log_warning("Tested vector is greater than Volume");
		return 0;
	}

	vec3 node_volume = octtree->volume;
	int current_node = 0;
	for(int i=0; i<=depth; i++)
	{
		node_volume = mul(node_volume, 0.5);
		int offset = octtree_child_position(rel_position, node_volume);
		rel_position = octtree_child_relative(rel_position, node_volume);
		// no node here, add one
		if(octtree->node_pool[current_node].node[offset] == 0)
		{
			if(octtree->node_count >= octtree->node_pool_size)
			{
				log_warning("Attempted to add too many nodes");
				return 0;
			}
			octtree->node_pool[current_node].node[offset] = octtree->node_count;
			octtree->node_count++;
		}
		current_node = octtree->node_pool[current_node].node[offset];
		if(i >= depth)
		{
			return current_node;
		}
	}
	return 0;
}
