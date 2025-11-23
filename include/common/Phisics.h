#pragma once
#include "glm/vec2.hpp"
#include "glm/vec4.hpp"
#include <algorithm>
#include <gl2d/gl2d.h>
#include "tiles.h"

#undef min
#undef max

constexpr float worldMagnification = 48;
constexpr int playerNameSize = 16;  // Increased to fit longer usernames

namespace phisics
{
	
	struct BlockInfo
	{
		
		char type;
		
		bool isCollidable();
	};
	
	struct MapData
	{

		BlockInfo* data;
		BlockInfo nullBlock = {};
	
		int w = 0;
		int h = 0;
	
		void create(int w, int h, const char* d);
		BlockInfo& get(int x, int y);
	
		void render(gl2d::Renderer2D &renderer, gl2d::Texture texture);

		void cleanup();
	
		bool load(const char *file);
		void save(const char *file);
	};
	
	struct Entity
	{
		char name[playerNameSize] = {};
		glm::vec2 pos = {};
		glm::vec2 lastPos = {};
		glm::vec2 input = {};

		glm::vec2 dimensions = {0.9,0.9};
		glm::vec3 color = {1,1,1};

		float hitTime = 0.f;
		int maxLife = 5;  // Maximum health (can be increased via upgrades)
		int life = 5;     // Current health
		
		// Game mode stats
		int kills = 0;
		int deaths = 0;
		
		// Horde Defense mode stats
		int money = 0;                  // Player's money for buying upgrades/items
		int damageUpgradeLevel = 0;     // 0-5: Damage upgrade level
		int fireRateUpgradeLevel = 0;   // 0-5: Fire rate upgrade level
		int healthUpgradeLevel = 0;     // 0-5: Health upgrade level
		int speedUpgradeLevel = 0;      // 0-5: Speed upgrade level
		int bulletSpeedUpgradeLevel = 0; // 0-5: Bullet speed upgrade level
		
	// Active buffs (temporary effects) - now wave-based instead of time-based
	int speedBoostWaves = 0;              // Remaining waves for speed boost
	int damageBoostWaves = 0;             // Remaining waves for damage boost
	int multiShotWaves = 0;               // Remaining waves for multi-shot
	float shieldHealth = 0.0f;            // Shield HP (absorbs damage) - kept for compatibility
	
	// Damage tracking for leaderboard (Horde Defense mode)
	int totalDamageDealt = 0;             // Total damage dealt to enemies
	int enemiesKilled = 0;                // Total enemies killed

	static constexpr float invincibilityTime = 0.10;  // Hit invincibility frames

		void resolveConstrains(MapData& mapData);
	
	
		bool moving = 0;
	
	
		// 0 1  -> used for animations
		bool movingRight = 0;
		
	
		void move(glm::vec2 dir);
		
		bool hit();
	
		//should be called only once per frame
		void updateMove(float deltaTime);
	
		void draw(gl2d::Renderer2D& renderer, float deltaTime, gl2d::Texture characterSprite, gl2d::Font font);
	
	
	private:
		void checkCollisionBrute(glm::vec2& pos, glm::vec2 lastPos, MapData& mapData,
			bool& upTouch, bool& downTouch, bool& leftTouch, bool& rightTouch);
		glm::vec2 performCollision(MapData& mapData, glm::vec2 pos, glm::vec2 size, glm::vec2 delta,
			bool& upTouch, bool& downTouch, bool& leftTouch, bool& rightTouch);
	};

	struct Bullet
	{
		glm::vec2 pos = {};
		glm::vec2 direction = {};
		glm::vec3 color = {1,1,1};
		int32_t cid = 0;
		float size = 0.4;

		bool checkCollisionMap(MapData &mapData);
		bool checkCollisionPlayer(Entity &e);
		void updateMove(float deltaTime);
		void draw(gl2d::Renderer2D &renderer, gl2d::Texture bulletSprite);
		glm::vec4 getTransform();
	};


	enum
	{
		itemTypeHealth = 1,
		itemTypeBatery,
		itemsCount = 2,
	};

	struct Item
	{
		Item() {};
		Item(glm::vec2 pos, uint32_t itemId, int itemType) 
			:pos(pos), itemId(itemId), itemType(itemType)
		{};
		
		glm::vec2 pos = {};
		uint32_t itemId = 0;
		int itemType = 0;

		bool checkCollisionPlayer(Entity &e);
		void draw(gl2d::Renderer2D &renderer, gl2d::Texture medkitTexture, gl2d::Texture bateryTexture);

	};
	
	
	//pos and size on on every component
	bool aabb(glm::vec4 b1, glm::vec4 b2);


};
