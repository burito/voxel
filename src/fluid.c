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
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <stdio.h>
#include <errno.h>

#include "fluid.h"
#include "log.h"
#include "octtree.h"


void fluid_log_vorton(char *name, struct vorton vorton)
{
	log_info("Vorton %s\nvort p={{%f, %f, %f}}\nvort w={{%f, %f, %f}}\nmag=%f, count=%d, calc_mag=%f",
	name,
	vorton.p.x, vorton.p.y, vorton.p.z,
	vorton.w.x, vorton.w.y, vorton.w.z,
	vorton.magnitude, vorton.count, mag(vorton.w)
	);
}



// Initialise the Fluid Sim, allocating memory and all that.
struct fluid_sim* fluid_init(float x, float y, float z, int max_depth)
{
	struct fluid_sim *sim;

	sim = malloc(sizeof(struct fluid_sim));
	if(sim == NULL)
	{
		log_fatal("malloc(fluid_sim) %s", strerror(errno));
		return NULL;
	}
	memset(sim, 0, sizeof(struct fluid_sim));
	sim->octtree = octtree_init(20);
	if(sim->octtree == NULL)
	{
		log_fatal("octtree_init() failed");
		free(sim);
		return NULL;
	}
	sim->max_depth = max_depth; // chosen by fair dice roll
	sim->max_vortons = 20+10;	// why not
	sim->vortons = malloc(sim->max_vortons * sizeof(struct vorton));
	if(sim->vortons == NULL)
	{
		log_fatal("malloc(sim->vortons) %s", strerror(errno));
		free(sim);
		return NULL;
	}
	memset(sim->vortons, 0, sim->max_vortons * sizeof(struct vorton));

	// allocated all of our stuff
	return sim;
}

// Free the memory allocated by the sim
void fluid_end(struct fluid_sim *sim)
{
	octtree_free(sim->octtree);
	free(sim->vortons);
	free(sim);
}

// Used by fluid_sim_update()
// Adds a vorton to the octtree, adding it's values to each node
// in the octtree as it moves down
void fluid_octtree_add_vorton(struct fluid_sim *sim, int j)
{
	struct octtree* octtree = sim->octtree;
	struct vorton* vorton = &sim->vortons[j];
	vec3 position = vorton->p;
	vec3 rel_position = sub(position, octtree->origin);
	if(vec3_lessthan_vec3(rel_position, (vec3){{0,0,0}}))
	{
		log_warning("Tested vector is less than Origin");
		return;
	}
	if(vec3_greaterthan_vec3(rel_position, octtree->volume))
	{
		log_warning("Tested vector is greater than Volume");
		return;
	}

	vec3 node_volume = octtree->volume;
	int current_node_index = 0;
	// for each step down the octtree
	for(int i=0; i<=sim->max_depth; i++)
	{
		struct vorton *current_node = &sim->vortons[current_node_index];

		// add the current vorton to this node
		if(current_node->count == 0)
		{
			*current_node = *vorton;
			current_node->count = 1;
			float magnitude = (mag(vorton->w));//sqrt() may be optional
			current_node->p = mul(vorton->p, magnitude);
			current_node->magnitude = magnitude;
			sim->octtree->node_pool->leaf[0]=current_node_index;
		}
		else
		{
			// find the magnitude of the vorton
			float magnitude = (mag(vorton->w));//sqrt() may be optional
			// position is weighted proportionally to the magnitude of the vorton
			current_node->p = add(mul(vorton->p, magnitude), current_node->p);
			current_node->magnitude += magnitude;
			current_node->w = add(current_node->w, vorton->w);
			current_node->v = add(current_node->v, vorton->v);
			// add the index of the vorton to the leaf array, for diffusion later
			if(current_node->count<8)
			{
				sim->octtree->node_pool->leaf[current_node->count]=current_node_index;
			}
			current_node->count++;
		}

		// if we've reached max_depth, we're finished
		if(i >= sim->max_depth)
		{
			return;
		}

		// find the child in which the current vorton belongs
		node_volume = mul(node_volume, 0.5);
		int offset = octtree_child_position(rel_position, node_volume);
		rel_position = octtree_child_relative(rel_position, node_volume);
		// no node here, add one
		if(octtree->node_pool[current_node_index].node[offset] == 0)
		{
			if(octtree->node_count >= octtree->node_pool_size)
			{
				log_warning("Attempted to add too many nodes");
				return;
			}
			octtree->node_pool[current_node_index].node[offset] = octtree->node_count;
			octtree->node_count++;
		}
		current_node_index = octtree->node_pool[current_node_index].node[offset];
	}
}


// Adds all of the Vortons to the Octtree, ready for processing a frame
void fluid_tree_update(struct fluid_sim *sim)
{
	// delete the vorton list on each leaf node, and each vorton
	octtree_empty(sim->octtree);
	// reset the vortons that represent the octtree
	memset(sim->vortons, 0, sizeof(struct vorton)*sim->octtree->node_pool_size);

	// walk all vortons, adding them to each octtree node in their chain
	for(int i=sim->octtree->node_pool_size; i<sim->vorton_count+sim->octtree->node_pool_size; i++)
	{
		fluid_octtree_add_vorton(sim, i);
	}

	// average each branch of the oct-tree
	for(int i=0; i<sim->octtree->node_count; i++)
	{
		struct vorton* vorton = &sim->vortons[i];
		// position is weighted average, based on magnitude of w
		vorton->p = div(vorton->p, vorton->magnitude);
//		vorton->w = div(vorton->w, vorton->count);
//		vorton->v = div(vorton->v, vorton->count);
	}

}


// Check if a particle is inside a volume.
int particle_inside_bound(vec3 particle, vec3 origin, vec3 volume)
{
	vec3 rel_position = sub(particle, origin);
	if(vec3_lessthan_vec3(rel_position, (vec3){{0,0,0}}))
		return 0;
	if(vec3_greaterthan_vec3(rel_position, volume))
		return 0;
	return 1;
}

// determine the velocity imparted on a position by a single vorton
vec3 fluid_accumulate_velocity(struct vorton vorton, vec3 position)
{
	float distmag;
	float oneOverDist;
	float distLaw;
	float radius = 0.5f;
	float rad2 = radius * radius;
	vec3 distance = sub(position, vorton.p);

	distmag = mag(distance) + 0.001f;
	oneOverDist = finvsqrt(distmag);
//	vect_smul(&dir, &dist, oneOverDist);
	distLaw = (distmag < rad2)
		? (oneOverDist / rad2) : (oneOverDist / distmag);

	distance = mul(distance, distLaw);
	vec3 w = mul(vorton.w,
//		(1.0f / (4.0f * 3.1415926535f)) * (8.0f * rad2 * radius));
		0.636619772367f * rad2 * radius);
	return vec3_cross(w, distance);
}

// determine the velocity imparted on a position by a single vorton, minus a child
vec3 fluid_accumulate_part_velocity(struct fluid_sim *sim, int parent, int child, vec3 position)
{
	// if there is no child, the parent is the only one that matters
	if(child == 0)
	{
		return fluid_accumulate_velocity(sim->vortons[parent], position);
	}

	// if the parent has one child, we will take care of it next time around
	if(sim->vortons[parent].count == 1)
	{
		return (vec3){{0,0,0}};
	}

	// else subtract the childs contribution from the parent, and apply the parent
	struct vorton difference;
	struct vorton vorton;

	vorton.w = sub(sim->vortons[parent].w, sim->vortons[child].w);
	vorton.v = sub(sim->vortons[parent].v, sim->vortons[child].v);

	difference.p = mul(sim->vortons[parent].p, sim->vortons[parent].magnitude);
	difference.p = sub(difference.p, mul(sim->vortons[child].p, sim->vortons[child].magnitude));
	vorton.p = div(difference.p, sim->vortons[parent].magnitude-sim->vortons[child].magnitude);

	return fluid_accumulate_velocity(vorton, position);
}


// find the velocity of the fluid at a given position
vec3 fluid_tree_velocity(struct fluid_sim *sim, vec3 position)
{
	vec3 rel_position = sub(position, sim->octtree->origin);
	vec3 half_volume = sim->octtree->volume;
	vec3 result = (vec3){{0,0,0}};
	struct octtree_node* nodes = sim->octtree->node_pool;
	int here = 0;
	for(int i=0; i<=sim->max_depth; i++)
	{
		int parent = here;
		// find which child node to descend into
		half_volume = mul(half_volume, 0.5);
		int branch = octtree_child_position(rel_position, half_volume);
		here = nodes[here].node[branch];
		result = add(result, fluid_accumulate_part_velocity(sim, parent, here, rel_position));

		// does the child node exist?
		if(here == 0)
			break;
	}
	return result;
}


// exchange the vorticity between two vortons
void fluid_vorton_exchange(struct vorton *left, struct vorton *right)
{
	float viscosity = 0.01f;
	float deltatime = 1.0f / 60.0f;
	vec3 delta = sub(left->w, right->w);
	vec3 exchange = mul(delta, viscosity * deltatime);
	left->w = sub(left->w, exchange);
	right->w = add(right->w, exchange);
}

/*
void fluid_diffuse(struct fluid_sim *sim)
{
	vorton* this;
	vorton* other;
	int layer = fluid_tree_size(sim->depth - 1);

	int cells = sim->cells - 1;
	for(int x=0; x<cells; x++)
	for(int y=0; y<cells; y++)
	for(int z=0; z<cells; z++)
	{
		int step = 0;
		this = sim->tree[layer+cell_offset(x,y,z)].next;
		while(this)
		{
			switch(step)
			{
			case 0:		// loop over local cell
				other = this->next; // local exchange should be mul by 2
				break;
			case 1:		// loop over x+1 cell
				other = sim->tree[layer+cell_offset(x+1,y,z)].next;
				break;
			case 2:		// loop over y+1 cell
				other = sim->tree[layer+cell_offset(x,y+1,z)].next;
				break;
			case 3:		// loop over z+1 cell
				other = sim->tree[layer+cell_offset(x,y,z+1)].next;
				break;
			}

			while(other)
			{
				fluid_vorton_exchange(this, other);
				other = other->next;
			}

			if(step < 3)
			{
				step++;
			}
			else
			{ // viscosity
				vec3 temp = mul(this->w, (1.0f/60.0f) * 0.01f);
				this->w = sub(this->w, temp);
				step = 0;
				this = this->next;
			}
		}
	}
}
*/

/*
void fluid_velocity_grid(struct fluid_sim *sim)
{
	int layer = fluid_tree_size(sim->depth - 1);

	int cells = sim->cells;
	for(int y = 1; y < cells; y++)
	for(int x = 1; x < cells; x++)
	for(int z = 1; z < cells; z++)
	{
		vec3 *v = &sim->tree[layer + cell_offset(x,y,z)].v;
		vec3 pos = (vec3){{x,y,z}};
		pos = add(mul(pos, sim->step), sim->octtree->origin);
		*v = fluid_tree_velocity(sim, pos);
	}
}

float mix(float x, float y, float a)
{
	return x * (1.0-a) + y*a;
}


vec3 fluid_interpolate_velocity(struct fluid_sim *sim, vec3 pos)
{
	int layer = fluid_tree_size(sim->depth - 1);

	vec3 rpos = sub(pos, sim->octtree->origin);
	if( rpos.x < 0.0f || rpos.x > sim->octtree->volume.x )
	{
		log_warning("Flail x! x=%f, y=%f, z=%f", pos.x, pos.y, pos.z);
		return;
	}
	if( rpos.y < 0.0f || rpos.y > sim->octtree->volume.y )
	{
		log_warning("Flail y! x=%f, y=%f, z=%f", pos.x, pos.y, pos.z);
		return;
	}
	if( rpos.z < 0.0f || rpos.z > sim->octtree->volume.z )
	{
		log_warning("Flail z! x=%f, y=%f, z=%f", pos.x, pos.y, pos.z);
		return;
	}

	int xp, yp, zp;
	xp = (int)(rpos.x / sim->step.x);
	yp = (int)(rpos.y / sim->step.y);
	zp = (int)(rpos.z / sim->step.z);

	float xd, yd, zd;
	xd = fmod(rpos.x, sim->step.x) * sim->oneOverStep.x;
	yd = fmod(rpos.y, sim->step.y) * sim->oneOverStep.y;
	zd = fmod(rpos.z, sim->step.z) * sim->oneOverStep.z;

	float ixd, iyd, izd;
	ixd = 1.0f - xd;
	iyd = 1.0f - yd;
	izd = 1.0f - zd;

	// http://en.wikipedia.org/wiki/Trilinear_interpolation
	vec3 *a, *b, *c, *d, *e, *f, *g, *h;
	a = &sim->tree[layer+cell_offset(xp, yp, zp)].v;	// c000
	b = &sim->tree[layer+cell_offset(xp, yp, zp+1)].v;	// c001
	c = &sim->tree[layer+cell_offset(xp, yp+1, zp)].v;	// c010
	d = &sim->tree[layer+cell_offset(xp, yp+1, zp+1)].v;	// c011
	e = &sim->tree[layer+cell_offset(xp+1, yp, zp)].v;	// c100
	f = &sim->tree[layer+cell_offset(xp+1, yp, zp+1)].v;	// c101
	g = &sim->tree[layer+cell_offset(xp+1, yp+1, zp)].v;	// c110
	h = &sim->tree[layer+cell_offset(xp+1, yp+1, zp+1)].v;	// c111

	vec3 i1 = add(mul(*a, izd), mul(*b, zd));
	vec3 i2 = add(mul(*c, izd), mul(*d, zd));
	vec3 j1 = add(mul(*e, izd), mul(*f, zd));
	vec3 j2 = add(mul(*g, izd), mul(*h, zd));

	vec3 w1 = add(mul(i1, iyd), mul(i2, yd));
	vec3 w2 = add(mul(j1, iyd), mul(j2, yd));

	return add(mul(w1, ixd), mul(w2, xd));
}

void fluid_stretch_tilt(struct fluid_sim *sim)
{
	float deltatime = 1.0f / 60.0f;
	int layer = fluid_tree_size(sim->depth - 1);
	int offset;

	for(int i=sim->octtree->node_pool_size; i<sim->vorton_count; i++)
	{
		struct vorton* vorton = &sim->vortons[i];
		if( !particle_inside_bound(vorton->p, sim->octtree->origin, sim->octtree->volume) )
			continue;

		vec3 p = sub(vorton->p, sim->origin);

		int px = (int)(p.x / sim->step.x);
		int py = (int)(p.y / sim->step.y);
		int pz = (int)(p.z / sim->step.z);

		offset = layer+cell_offset(px,py,pz);
		// FIXME: this line causes a segfault
		// compute jacobian matrix
		vec3 diff;
		diff.x = sim->tree[offset].v.x - // TODO: segfault
			sim->tree[layer+cell_offset(px+1,py,pz)].v.x;
		diff.y = sim->tree[offset].v.y -
			sim->tree[layer+cell_offset(px,py+1,pz)].v.x;
		diff.z = sim->tree[offset].v.z -
			sim->tree[layer+cell_offset(px,py,pz+1)].v.x;

		mat3x3 jacobian = vec3_jacobian_vec3(diff, sim->step);
		// multiply jacobian by vorticity vector
		vec3 dw = mul(jacobian, vorton->w);

		// integrate with euler
		vec3 *w = &vorton->w;
		*w = add(*w, mul(dw, deltatime * 0.2));
	}
}
*/
void fluid_advect_tracers(struct fluid_sim *sim, struct particle *particles, int count)
{
	float deltatime = 1.0f / 60.0f;

	for(int i=0; i<count; i++)
	{
		if( particle_inside_bound(particles[i].p, sim->octtree->origin, sim->octtree->volume) )
		{
//			vec3 velocity = fluid_interpolate_velocity(sim, particles[i].p);
			vec3 velocity = fluid_tree_velocity(sim, particles[i].p);
			velocity = mul(velocity, deltatime);
			particles[i].p = add(particles[i].p, velocity);
		}
	}

}

/*
void fluid_advect_vortons(struct fluid_sim *sim)
{
	float deltatime = 1.0f / 60.0f;

	for(int i=sim->octtree->node_pool_size; i<sim->vorton_count; i++)
	{
		struct vorton* vorton = &sim->vortons[i];
		if( particle_inside_bound(vorton->p, sim->octtree->origin, sim->octtree->volume) )
		{
			vec3 velocity = fluid_interpolate_velocity(sim, vorton->p);
			velocity = mul(velocity, deltatime);
			vorton->p = add(vorton->p, velocity);
		}
	}
}
*/

// evolve the fluid simulation
void fluid_tick(struct fluid_sim *sim)
{
	fluid_tree_update(sim);
//	fluid_diffuse(sim);
//	fluid_velocity_grid(sim);
//	fluid_stretch_tilt(sim);
//	fluid_advect_vortons(sim);
}

// check that a position is inside the fluid volume, expanding it if not
void fluid_bound(struct fluid_sim *sim, vec3 position)
{
	vec3 *origin = &sim->octtree->origin;
	vec3 relative_position = sub(*origin, position);

	origin->y = nmin(origin->y, relative_position.y);
	origin->z = nmin(origin->z, relative_position.z);
	origin->x = nmin(origin->x, relative_position.x);

	vec3 *volume = &sim->octtree->volume;
	volume->x = nmax(volume->x, relative_position.x);
	volume->y = nmax(volume->y, relative_position.y);
	volume->z = nmax(volume->z, relative_position.z);
}
