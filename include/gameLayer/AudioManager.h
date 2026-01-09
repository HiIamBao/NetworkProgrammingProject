#pragma once
#include <raudio.h>
#include <string>
#include <iostream>

class AudioManager {
public:
    static AudioManager& getInstance();
    
    // Initialize audio system and load sounds
    bool init();
    
    // Play sounds
    void playMusic();
    void playClick();
    void playShoot();
    
    // Update music stream (call in game loop)
    void update();
    
    // Cleanup
    void cleanup();
    
    // Destructor
    ~AudioManager();

private:
    AudioManager() = default;
    AudioManager(const AudioManager&) = delete;
    AudioManager& operator=(const AudioManager&) = delete;
    
    bool initialized = false;
    
    // Audio resources
    Music bgMusic = {};
    Sound clickSound = {};
    Sound shootSound = {};
};
