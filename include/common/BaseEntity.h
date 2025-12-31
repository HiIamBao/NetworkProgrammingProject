#pragma once
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

namespace phisics
{
	struct BaseEntity
	{
		glm::vec2 pos = {};
		glm::vec2 lastPos = {};
		glm::vec2 velocity = {}; 
		glm::vec2 dimensions = {0.9, 0.9}; // Default common dimensions
		bool isAlive = true;
		float health = 10;
		float maxHealth = 10;
		float speed = 1;

		// Common game logic references or flags could go here
		// Keeping it POD (Plain Old Data) for network safety if possible
	};
}
